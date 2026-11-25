/* check_v122_worlds.c — 여러 시드의 지도와 실제 v9 저장의 비파괴 이관을 검사합니다. */
#include "expansion.h"
#include "platform.h"
#include "storage.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int legacy(const char *path) {
    World *w=NULL,*again=NULL;int32_t line=0;uint64_t before=0,after=0;
    int n=0,m=0,errors=0,tiles=0;
    unsigned char *original=game_read_bytes(path,&n);
    if(!original || core_load(path,NULL,&w,&line)!=CORE_OK){fprintf(stderr,"v9 load failed line=%d\n",line);free(original);return 1;}
    FILE *f=platform_fopen_utf8(path,"rb");char text[4096];int version=0;
    if(!f){core_destroy(w);free(original);return 1;}
    if(fgets(text,sizeof(text),f))sscanf(text,"MAGICCASTLE_SAVE %d",&version);
    errors+=version!=9;
    while(fgets(text,sizeof(text),f)) {
        int p,biome,kind,age,logs,used,regrow,ore,stock,water,explored;
        if(sscanf(text,"tile=%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",&p,&biome,&kind,&age,&logs,&used,&regrow,&ore,&stock,&water,&explored)!=11)continue;
        if(p<0||p>=w->expansion->cells){errors++;continue;}
        const CoreExpansionTileView *t=&w->expansion->tiles[p];tiles++;
        errors+=t->biome!=biome || t->tree_kind!=kind || t->tree_age!=age || t->tree_logs_milli!=logs ||
            t->water_id!=water || t->explored!=explored;
        if(ore==CORE_RES_STONE)errors+=t->stone_milli!=stock || t->ore_milli!=0;
        else errors+=t->ore_kind!=ore || t->ore_milli!=stock;
    }
    fclose(f);errors+=tiles!=w->expansion->cells;
    errors+=core_state_hash(w,&before)!=CORE_OK || core_save(w,"v122_legacy_roundtrip.sav")!=CORE_OK;
    if(core_load("v122_legacy_roundtrip.sav",NULL,&again,&line)!=CORE_OK)errors++;
    else {core_state_hash(again,&after);errors+=before!=after || core_validate(again)!=0;core_destroy(again);}
    unsigned char *preserved=game_read_bytes(path,&m);
    errors+=!preserved || n!=m || (preserved&&n==m&&memcmp(original,preserved,(size_t)n));
    free(original);free(preserved);core_destroy(w);
    if(!errors){remove("v122_legacy_roundtrip.sav");remove("v122_legacy_roundtrip.sav.bak");}
    printf("legacy tiles=%d errors=%d hash=%llu\n",tiles,errors,(unsigned long long)after);
    return errors!=0;
}
int main(int argc,char **argv) {
    GameArguments args;int seeds=100,errors=0,large_worlds=0;const char *fixture=NULL;
    if(!game_arguments(&args,argc,argv))return 2;
    for(int i=1;i<args.count;i++) {
        if(i+1>=args.count){game_arguments_free(&args);return 2;}
        if(!strcmp(args.values[i],"--seeds"))seeds=atoi(args.values[++i]);
        else if(!strcmp(args.values[i],"--legacy"))fixture=args.values[++i];
        else {game_arguments_free(&args);return 2;}
    }
    if(seeds<1||seeds>1000){game_arguments_free(&args);return 2;}
    puts("seed,lakes,large_lakes,water_tiles,coal_tiles,iron_tiles,gold_tiles,mana_tiles,errors");
    for(int seed=1;seed<=seeds;seed++) {
        CoreNewGameParams p={0};World *w=NULL;int large=0,water=0,ore[CORE_RES_COUNT]={0},bad=0;
        strcpy(p.castle_name,u8"v122 지도 검사");p.seed=seed;p.ruleset=1;p.difficulty=0;
        if(core_create(&p,NULL,&w)!=CORE_OK){errors++;continue;}
        const ExpansionState *e=w->expansion;
        bad += e->water_count < 18 || e->water_count > 22;
        for(int i=0;i<e->water_count;i++)large+=e->water_area[i]>=250&&e->water_area[i]<=550;
        large_worlds+=large>0;
        for(int i=0;i<e->cells;i++) {
            const CoreExpansionTileView *t=&e->tiles[i];bad+=t->stone_milli<=0;
            if(t->biome==CORE_BIOME_LAKE){water++;bad+=exp_dist(i%w->grid.width,i/w->grid.width,w->keep_x,w->keep_y)<=10;}
            if(t->ore_kind>=0&&t->ore_kind<CORE_RES_COUNT&&t->ore_milli>0)ore[t->ore_kind]++;
        }
        bad+=!ore[CORE_RES_COAL]||!ore[CORE_RES_IRON_ORE]||!ore[CORE_RES_GOLD_ORE]||!ore[CORE_RES_MANA];
        bad+=core_validate(w)!=0;errors+=bad;
        printf("%d,%d,%d,%d,%d,%d,%d,%d,%d\n",seed,e->water_count,large,water,ore[CORE_RES_COAL],ore[CORE_RES_IRON_ORE],ore[CORE_RES_GOLD_ORE],ore[CORE_RES_MANA],bad);
        core_destroy(w);
    }
    if(large_worlds*100<seeds*90)errors++;
    printf("worlds=%d large_worlds=%d errors=%d\n",seeds,large_worlds,errors);
    if(fixture)errors+=legacy(fixture);
    game_arguments_free(&args);return errors?1:0;
}
