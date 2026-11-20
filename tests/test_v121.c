/* test_v121.c — 실근무·저장·개체 수·편성 및 추가 요청 회귀 검사 */
#include "expansion.h"
#include "population.h"
#include "test_util.h"
#include <stdio.h>
#include <string.h>
static World *world121(void) {
    CoreNewGameParams p={0};World *w=NULL;
    strcpy(p.castle_name,u8"개정 시험");p.seed=42;p.ruleset=1;p.difficulty=1;
    if(core_create(&p,NULL,&w)!=CORE_OK)test_fail(__FILE__,__LINE__,"v121 world");
    return w;
}
static Building *building121(World *w,int type,int x,int y) {
    Building *b=entity_add_building(w,type);const ExpansionBuildingSpec *s=expansion_spec(type);
    if(!b)return NULL;
    b->x=x;b->y=y;b->w=s->w;b->h=s->h;b->door_x=x+b->w/2;b->door_y=y+b->h;
    b->hp=s->hp;b->state=CORE_BSTATE_ACTIVE;b->completed_day=1;b->auto_assign=0;return b;
}
static void present121(Villager *v,Building *b) {
    v->target=b;v->state=CORE_VS_WORKING;v->at_target=1;v->incoming=0;
    v->target_x=b->door_x;v->target_y=b->door_y;v->loc_x=b->door_x;v->loc_y=b->door_y;
    v->current_x_milli=b->door_x*1000+500;v->current_y_milli=b->door_y*1000+500;
}
void test_v121_identity_and_save(void) {
    World *w=world121(),*loaded=NULL;if(!w)return;
    CHECK_EQ(w->defs.name_count,324);
    int male=0,female=0,hair=0;
    for(int i=0;i<80;i++) {
        Villager *v=population_create(w,w->keep_x,w->keep_y+4);CHECK(v!=NULL);
        CHECK_EQ(v->appearance%2,v->sex);CHECK(!strchr(v->name,':'));
        male+=!v->sex;female+=v->sex;hair|=1<<(v->appearance/2);
        for(int n=0;n<w->defs.name_count;n++)if(strcmp(v->name,w->defs.names[n].text+2)==0)CHECK_EQ(w->defs.names[n].text[0],v->sex?'F':'M');
    }
    CHECK(male&&female);CHECK_EQ(hair,63);
    w->villagers[0]->xp_work_minutes[5]=359;w->villagers[0]->healing_progress_milli=12345;
    w->expansion->plague_days=3;w->expansion->economic_event=CORE_NATURE_TRADE_BOOM;w->expansion->economic_days=4;
    w->expansion->phantom_next_day[4]=123;w->expansion->next_raid_tick=5678;
    expansion_rebuild(w);expansion_visibility(w);
    uint64_t before,after;CHECK_EQ(core_state_hash(w,&before),CORE_OK);
    CHECK_EQ(core_save(w,"v121_roundtrip.sav"),CORE_OK);CHECK_EQ(core_load("v121_roundtrip.sav",NULL,&loaded,NULL),CORE_OK);
    if(loaded){CHECK_EQ(core_state_hash(loaded,&after),CORE_OK);CHECK_EQ(before,after);core_destroy(loaded);}
    core_destroy(w);remove("v121_roundtrip.sav");remove("v121_roundtrip.sav.bak");
}
void test_v121_actual_labor_and_xp(void) {
    World *w=world121();if(!w)return;
    Building *b=building121(w,CORE_BLD_GARDEN,80,80);Villager *v=w->villagers[0];
    CHECK_EQ(entity_link_worker(b,v,2),CORE_OK);v->meal_day=1;v->traits[0]=v->traits[1]=-1;
    for(int i=0;i<36;i++) {
        w->clock.tick=48+i;present121(v,b);
        v->target_x=b->x+v->id%b->w;v->target_y=b->y+(v->id/b->w)%b->h;
        expansion_people_tick(w);
    }
    CHECK_EQ(v->skill_minutes[0],360);
    for(int i=0;i<5;i++)CHECK_EQ(v->stat_xp[i],((int[]){1750,0,1500,1000,750}[i]));
    CHECK_EQ(v->xp_work_minutes[0],0);CHECK_EQ(v->work_day_minutes,360);
    CHECK(expansion_labor_milli(w,b)>0);v->at_target=0;CHECK_EQ(expansion_working_count(w,b),0);
    b->daily_work_milli=7500*b->worker_count*540;
    CHECK(expansion_labor_milli(w,b)<=7500);
    core_destroy(w);
}
void test_v121_event_labor_and_services(void) {
    World *w=world121();if(!w)return;
    Building *b=building121(w,CORE_BLD_CROPLAND,80,80);Villager *v=w->villagers[0];
    entity_link_worker(b,v,2);present121(v,b);v->traits[0]=v->traits[1]=-1;
    int e=expansion_work_percent(w,v,b->type)*100;
    w->nature.event=CORE_NATURE_DROUGHT;CHECK_EQ(expansion_begin_work(w,v,b),10);CHECK_EQ(b->crop_labor_milli,e/4);
    w->nature.event=CORE_NATURE_BUMPER;expansion_begin_work(w,v,b);CHECK_EQ(b->crop_labor_milli,e/4+e*24/10);
    w->research.done|=1u<<17;w->nature.event=CORE_NATURE_NONE;expansion_begin_work(w,v,b);CHECK_EQ(b->crop_labor_milli,e/4+e*24/10+e*108/100);
    CHECK_EQ(expansion_agriculture_event(w,CORE_BLD_IRRIGATED_FARM),1000);
    w->nature.event=CORE_NATURE_DROUGHT;CHECK_EQ(expansion_agriculture_event(w,CORE_BLD_IRRIGATED_FARM),650);
    w->research.done|=1u<<2;CHECK_EQ(expansion_agriculture_event(w,CORE_BLD_IRRIGATED_FARM),750);
    Building *school=building121(w,CORE_BLD_SCHOOL,70,70);
    entity_link_worker(school,w->villagers[1],3);w->villagers[1]->stats.intellect=5;CHECK(!expansion_qualified(w,school));
    entity_link_worker(school,w->villagers[2],3);w->villagers[2]->stats.intellect=2;CHECK(expansion_qualified(w,school));
    core_destroy(w);
}
void test_v121_visibility_trade_and_centers(void) {
    World *w=world121();if(!w)return;
    Building *b=building121(w,CORE_BLD_WATCHTOWER,40,40);Villager *v=w->villagers[0];entity_link_worker(b,v,2);
    expansion_rebuild(w);expansion_visibility(w);int p=grid_index(&w->grid,40,35);
    CHECK(!w->expansion->visible[p]);present121(v,b);expansion_visibility(w);CHECK(w->expansion->visible[p]);
    v->at_target=0;expansion_visibility(w);CHECK(!w->expansion->visible[p]);CHECK(w->expansion->tiles[p].explored);
    CHECK_EQ(expansion_center_distance(b,40,40),0);CHECK_EQ(expansion_center_distance(b,40,35),5);
    Building dummy={0};dummy.x=10;dummy.y=20;dummy.w=4;dummy.h=2;
    CHECK_EQ(expansion_center_distance(&dummy,11,20),0);CHECK_EQ(expansion_center_distance(&dummy,12,21),0);CHECK_EQ(expansion_center_distance(&dummy,9,19),3);
    Building *trade=building121(w,CORE_BLD_STALL,60,60);trade->state=CORE_BSTATE_CONSTRUCTING;
    CorePlacementView result;expansion_place_check(w,CORE_BLD_MARKET,65,65,0,0,&result);CHECK_EQ(result.result,CORE_PLACE_TRADE_EXISTS);
    entity_remove_building(w,trade);w->rank=5;expansion_place_check(w,CORE_BLD_MARKET,65,65,0,0,&result);CHECK_EQ(result.result,CORE_PLACE_NOT_VISIBLE);
    core_destroy(w);
}
void test_v121_livestock_age_and_breeding(void) {
    World *w=world121();if(!w)return;
    Building *b=building121(w,CORE_BLD_RANCH,80,80);w->bank.amount[CORE_RES_GOLD]=1000;
    CHECK_EQ(core_expansion_buy_animal(w,b->id,10),CORE_OK);CHECK_EQ(core_expansion_buy_animal(w,b->id,10),CORE_OK);
    int first=w->expansion->animal_count-2;CHECK_EQ(w->expansion->animals[first].age,0);
    for(int i=first;i<first+2;i++)w->expansion->animals[i].age=7;
    b->daily_work_milli=w->defs.buildings[b->type].worker_slots*540000;
    b->breeding_progress[3]=7000;w->bank.amount[CORE_RES_FOOD]=100;
    expansion_animal_daily(w);
    int adults=0,babies=0;for(int i=0;i<w->expansion->animal_count;i++)if(w->expansion->animals[i].alive&&w->expansion->animals[i].owner==b->id){adults+=w->expansion->animals[i].age>=8;babies+=w->expansion->animals[i].age==0;}
    CHECK_EQ(adults,2);CHECK_EQ(babies,2);CHECK_EQ(b->breeding_progress[3],0);CHECK_EQ(b->feed_milli,150);
    CHECK_EQ(expansion_bank_have(w,CORE_RES_FOOD),100550);
    for(int i=first;i<first+2;i++)w->expansion->animals[i].age=27;
    b->daily_work_milli=0;expansion_animal_daily(w);CHECK(!w->expansion->animals[first].alive);
    core_destroy(w);
}
void test_v121_raids_summons_and_drops(void) {
    World *w=world121();if(!w)return;
    CoreRaiderInfo info;for(int i=0;i<28;i++){CHECK_EQ(core_expansion_raider_info(i,&info),CORE_OK);CHECK(info.cost>0);CHECK(w->defs.enemies[i].hp>0);}
    w->clock.tick=120*144+40;w->expansion->next_raid_tick=w->clock.tick;expansion_raids_tick(w);
    Raid *r=&w->threat.raid;CHECK(r->id);CHECK(r->enemy_count>=3&&r->enemy_count<=5);
    CHECK(expansion_center_distance(world_keep(w),r->spawn_x,r->spawn_y)>=25);
    CHECK(expansion_center_distance(world_keep(w),r->spawn_x,r->spawn_y)<=40);
    CHECK(!w->expansion->protected_cells[grid_index(&w->grid,r->spawn_x,r->spawn_y)]);
    r->enemies[0].type=13;
    for(int i=0;i<80;i++){int32_t id=0;CHECK_EQ(core_expansion_summon(w,1,&id),CORE_OK);CHECK(id>0);}
    CHECK(r->enemy_count>64);CHECK_EQ(core_validate(w),0);
    CHECK_EQ(threat_report_enemy_killed(w,r->id,1),CORE_OK);
    for(int i=0;i<r->enemy_count;i++)if(r->enemies[i].summoner_id==1)CHECK_EQ(r->enemies[i].state,CORE_ENEMY_ESCAPED);
    int gold=w->bank.amount[CORE_RES_GOLD];CHECK_EQ(threat_report_enemy_killed(w,r->id,1),CORE_IGNORED);CHECK_EQ(w->bank.amount[CORE_RES_GOLD],gold);
    core_destroy(w);
}
void test_v121_exact_crop_and_magic_cost(void) {
    World *w=world121();if(!w)return;
    Building *farm=building121(w,CORE_BLD_FARM,80,80);
    int baseline=3*540*24*1000;
    w->clock.tick=24*144+48;
    farm->crop_labor_milli=baseline/24*21+baseline/24*3/4;
    expansion_produce(w);CHECK_EQ(farm->pending_harvest_milli,543000);
    farm->crop_year=0;farm->pending_harvest_milli=0;
    farm->crop_labor_milli=baseline/24*21+baseline/24*3*24/10;
    expansion_produce(w);CHECK_EQ(farm->pending_harvest_milli,705000);
    Building *magic=building121(w,CORE_BLD_MAGIC_WORKSHOP,70,70);
    int n=w->defs.buildings[magic->type].worker_slots;
    magic->daily_work_milli=n*540000*75/10;magic->actual_work_milli=n*540000;
    w->expansion->distance[grid_index(&w->grid,magic->door_x,magic->door_y)]=0;
    w->research.active=0;w->bank.amount[CORE_RES_MANA]=10;
    expansion_daily(w);CHECK_EQ(w->bank.amount[CORE_RES_MANA],9);CHECK_EQ(w->research.progress_milli,22500);
    core_destroy(w);
}
void test_v121_medical_actual_staff(void) {
    World *w=world121();if(!w)return;
    Building *hospital=building121(w,CORE_BLD_HOSPITAL,70,70);
    for(int i=0;i<3;i++){Villager *v=w->villagers[i];entity_link_worker(hospital,v,3);v->stats.intellect=i?4:6;present121(v,hospital);}
    Villager *a=w->villagers[0],*b=w->villagers[1],*off=w->villagers[2];off->at_target=0;
    int sum=expansion_work_percent(w,a,hospital->type)+expansion_work_percent(w,b,hospital->type);
    expansion_begin_work(w,a,hospital);expansion_begin_work(w,b,hospital);
    CHECK_EQ(hospital->service_credit_milli,sum*100*5/(2*540));
    off->at_target=1;CHECK_EQ(expansion_working_count(w,hospital),3);
    core_destroy(w);
}
