/* test_native.c — 실제 C 길찾기·이동·전투·설정·저장 교체를 화면 없이 검사합니다. */
#include "session.h"
#include "storage.h"
#include "platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static int failed=0;
#define EXPECT(x) do { if (!(x)) { fprintf(stderr,"native FAIL %s:%d: %s\n",__FILE__,__LINE__,#x); failed++; } } while(0)
static void paths(void)
{
    uint8_t cost[25]; GamePathfinder finder={0}; GamePath path={0}; int i;
    memset(cost,10,sizeof(cost));
    for (i=0;i<5;i++) cost[2+i*5]=CORE_COST_WALL;
    EXPECT(game_path_find(&finder,cost,5,5,0,2,4,2,0,&path)==0);
    EXPECT(game_path_find(&finder,cost,5,5,0,2,4,2,1,&path)==1);
    EXPECT(path.count==5);
    cost[12]=CORE_COST_GATE;
    EXPECT(game_path_find(&finder,cost,5,5,0,2,4,2,0,&path)==1);
    EXPECT(game_path_find(&finder,cost,5,5,-1,0,1,1,0,&path)==0);
    EXPECT(game_path_find(&finder,cost,5,5,0,0,0,0,0,&path)==1 && path.count==1);
    memset(cost,10,sizeof(cost)); cost[1]=30; cost[6]=30;
    EXPECT(game_path_find(&finder,cost,5,5,0,0,4,0,0,&path)==1);
    EXPECT(path.count==5); /* 일곱 칸 우회보다 네 번 움직이는 길이 쌉니다. */
    game_path_free(&path); game_pathfinder_free(&finder);
}
static void settings(void)
{
    GameSettings a,b; const char *path="native_settings.cfg";
    game_settings_default(&a); a.sfx_volume=0.25f; a.fullscreen=1; a.ui_scale=2;
    EXPECT(game_settings_save(&a,path)); EXPECT(game_settings_load(&b,path));
    EXPECT(fabsf(b.sfx_volume-0.25f)<0.001f && b.fullscreen==1 && b.ui_scale==2);
    EXPECT(game_write_bytes(path,"master=nan\nsfx=4\nui_scale=999\n",strlen("master=nan\nsfx=4\nui_scale=999\n")));
    EXPECT(game_settings_load(&b,path)); EXPECT(b.master_volume==0.8f && b.sfx_volume==0.8f && b.ui_scale==1);
    (void)platform_remove_file(path);
}
static void session(const char *data)
{
    GameSession *s=(GameSession *)calloc(1,sizeof(*s)); uint64_t before=0,after=0; int line=0,i;
    EXPECT(s!=NULL); if (!s) return;
    EXPECT(game_session_init(s,data)==CORE_OK);
    EXPECT(game_session_new(s,u8"한글 성채",42,CORE_DIFF_NORMAL)==CORE_OK);
    EXPECT(s->summary.population==6 && s->cells==96*96);
    EXPECT(game_session_step(s,60)==CORE_OK);
    EXPECT(s->arrivals>0 && core_validate(s->world)==0);
    EXPECT(core_state_hash(s->world,&before)==CORE_OK);
    EXPECT(core_save(s->world,"native_roundtrip.sav")==CORE_OK);
    EXPECT(game_session_load(s,"native_missing.sav",&line)==CORE_ERR_FILE_OPEN);
    EXPECT(core_state_hash(s->world,&after)==CORE_OK && before==after);
    EXPECT(game_session_load(s,"native_roundtrip.sav",&line)==CORE_OK);
    EXPECT(core_state_hash(s->world,&after)==CORE_OK && before==after);
    EXPECT(game_session_step(s,NAN)==CORE_ERR_INVALID_ARG);
    EXPECT(core_debug_stress(s->world,20,2)==CORE_OK);
    EXPECT(game_session_refresh(s)==CORE_OK && s->enemy_count==2);
    EXPECT(core_save(s->world,"native_raid.sav")==CORE_ERR_REJECTED);
    for (i=0;i<s->enemy_count;i++) {
        s->enemies[i].actor.position=(GamePoint){s->summary.keep_x+0.5f,s->summary.keep_y+3.5f};
        s->enemies[i].hp=1;
    }
    EXPECT(game_session_step(s,100)==CORE_OK);
    EXPECT(s->shots>0 && s->kills>0);
    EXPECT(core_validate(s->world)==0);
    game_session_free(s); free(s);
    (void)platform_remove_file("native_roundtrip.sav");
}
int main(int argc,char **argv)
{
    if (argc!=2) return 2;
    paths(); settings(); session(argv[1]);
    if (!failed) puts("C native runtime tests passed");
    return failed?1:0;
}
