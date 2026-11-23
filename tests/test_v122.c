/* test_v122.c — 변경 규칙의 경계와 저장 이관 회귀 검사 */
#include "expansion.h"
#include "test_util.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
void test_v122_discovery_timer_and_shift(void) {
    CoreNewGameParams p={0};World *w=NULL,*copy=NULL;int32_t line=0;
    strcpy(p.castle_name,u8"발견 시점과 최초 근무조");p.seed=42;p.ruleset=1;
    CHECK_EQ(core_create(&p,NULL,&w),CORE_OK);if(!w)return;
    CHECK_EQ(threat_debug_raid(w,CORE_THREAT_BANDITS,CORE_DIR_EAST,2),CORE_OK);
    expansion_raids_tick(w);Raid *r=&w->threat.raid;
    CHECK_EQ(r->phase,CORE_RAID_APPROACH);CHECK_EQ(r->assault_end_tick,0);
    CHECK(!core_expansion_build_locked(w));CHECK(!threat_raid_active(w));
    w->clock.tick+=500;expansion_raids_tick(w);
    CHECK_EQ(r->phase,CORE_RAID_APPROACH);CHECK_EQ(r->assault_end_tick,0);
    int started=w->clock.tick;
    CHECK_EQ(core_expansion_building_hit(w,w->keep_id,1,1,1),CORE_OK);
    CHECK_EQ(r->phase,CORE_RAID_ASSAULT);CHECK_EQ(r->started_tick,started);CHECK_EQ(r->assault_end_tick,started+288);
    CHECK(core_expansion_build_locked(w));
    w->clock.tick++;expansion_raid_discover(w);CHECK_EQ(r->assault_end_tick,started+288);
    CHECK_EQ(core_save(w,"v122_discovery.sav"),CORE_OK);
    CHECK_EQ(core_load("v122_discovery.sav",NULL,&copy,&line),CORE_OK);
    if(copy){uint64_t a,b;core_state_hash(w,&a);core_state_hash(copy,&b);CHECK_EQ(a,b);core_destroy(copy);}
    w->clock.tick=started+287;expansion_raids_tick(w);CHECK_EQ(r->phase,CORE_RAID_ASSAULT);
    w->clock.tick++;expansion_raids_tick(w);CHECK_EQ(r->phase,CORE_RAID_RETREAT);
    CHECK(core_expansion_build_locked(w));
    CHECK_EQ(core_report_enemy_escaped(w,r->id,1),CORE_OK);
    CHECK_EQ(core_report_enemy_escaped(w,r->id,2),CORE_OK);CHECK(!core_expansion_build_locked(w));
    /* 다음 적 배열을 재사용할 때 이전 HP·철수·공격 상태를 물려주지 않습니다. */
    r->enemies[0].runtime.valid=1;r->enemies[0].runtime.fleeing=1;r->enemies[0].runtime.hp=17;
    CHECK_EQ(threat_debug_raid(w,CORE_THREAT_BANDITS,CORE_DIR_EAST,1),CORE_OK);
    CHECK_EQ(r->enemies[0].runtime.valid,0);CHECK_EQ(r->enemies[0].runtime.fleeing,0);
    CHECK_EQ(core_expansion_engaged(w,1),CORE_OK);CHECK_EQ(r->assault_end_tick,w->clock.tick+288);
    CHECK_EQ(core_report_enemy_escaped(w,r->id,1),CORE_OK);
    CHECK_EQ(threat_debug_raid(w,CORE_THREAT_BANDITS,CORE_DIR_EAST,1),CORE_OK);
    CHECK_EQ(core_expansion_spotted(w,w->keep_x,w->keep_y),CORE_OK);
    CHECK_EQ(r->first_discovered,1);CHECK_EQ(r->assault_end_tick,w->clock.tick+288);
    Building *post=entity_add_building(w,CORE_BLD_OUTPOST);CHECK(post!=NULL);
    if(post) {
        post->x=90;post->y=90;post->w=post->h=1;post->door_x=90;post->door_y=91;post->hp=110;post->state=CORE_BSTATE_ACTIVE;
        Villager *v=w->villagers[0],*other=w->villagers[1];
        CHECK_EQ(core_expansion_assign_shift(w,v->id,post->id,1),CORE_OK);CHECK_EQ(v->shift_kind,1);
        Building *previous=other->workplace;
        CHECK_EQ(core_expansion_assign_shift(w,other->id,post->id,1),CORE_ERR_REJECTED);CHECK(other->workplace==previous);
        CHECK_EQ(core_expansion_assign_shift(w,other->id,post->id,0),CORE_OK);CHECK_EQ(other->shift_kind,0);
    }
    core_destroy(w);remove("v122_discovery.sav");remove("v122_discovery.sav.bak");
}
void test_v122_rules_and_save(void) {
    CoreNewGameParams p={0};World *w=NULL,*loaded=NULL;int32_t line=0;
    strcpy(p.castle_name,u8"v122 저장 시험");p.seed=7;p.difficulty=1;p.ruleset=1;
    CHECK_EQ(core_create(&p,NULL,&w),CORE_OK);if(!w)return;
    CHECK_EQ(CORE_BLD_COUNT,66);CHECK_EQ(CORE_SAVE_VERSION,10);
    CHECK_EQ(core_expansion_tree_mature_age(3),48000);
    CHECK_EQ(expansion_spec(CORE_BLD_COTTAGE)->w,2);CHECK_EQ(expansion_spec(CORE_BLD_COTTAGE)->h,3);
    CHECK_EQ(expansion_spec(CORE_BLD_BARRACKS)->rank,1);CHECK_EQ(expansion_spec(CORE_BLD_GARRISON)->workers,6);
    for(int i=0;i<w->expansion->cells;i++)CHECK(w->expansion->tiles[i].stone_milli>0);
    CHECK(w->expansion->water_count>=5);int large=0;
    for(int i=0;i<w->expansion->water_count;i++)large+=w->expansion->water_area[i]>=250;
    CHECK(large>0);CHECK_EQ(core_validate(w),0);
    CHECK_EQ(core_tick(w,50),CORE_OK);
    Villager *v=w->villagers[0];int baseline[5];memcpy(baseline,v->stat_xp,sizeof(baseline));
    expansion_gain_xp(v,7,CORE_BLD_TOWER,180);
    int32_t actor[20]={1,v->current_x_milli,v->current_y_milli,12000,42000,1,0,0,0,0,0,2100,5,0,2,1,3,96,97,1};
    int32_t clock[2]={1250,1},projectile[13]={95000,98000,96000,99000,400,1600,0,0,1,1,6,50,1};
    CHECK_EQ(core_expansion_report_runtime(w,CORE_RUNTIME_ACTOR,v->id,actor,20),CORE_OK);
    CHECK_EQ(core_expansion_report_runtime(w,CORE_RUNTIME_CLOCK,0,clock,2),CORE_OK);
    CHECK_EQ(core_expansion_report_runtime(w,CORE_RUNTIME_PROJECTILE,0,projectile,13),CORE_OK);
    CHECK_EQ(core_save(w,"v122_roundtrip.sav"),CORE_OK);
    int rc=core_load("v122_roundtrip.sav",NULL,&loaded,&line);
    if(rc!=CORE_OK)fprintf(stderr,"v122 load result=%d line=%d\n",rc,line);
    CHECK_EQ(rc,CORE_OK);
    if(loaded){
        uint64_t a,b;core_state_hash(w,&a);core_state_hash(loaded,&b);CHECK_EQ(a,b);
        int32_t actual[20]={0};CHECK_EQ(core_expansion_copy_runtime(loaded,CORE_RUNTIME_ACTOR,v->id,actual,20),CORE_OK);CHECK(!memcmp(actor,actual,sizeof(actor)));
        CHECK_EQ(core_expansion_copy_runtime(loaded,CORE_RUNTIME_PROJECTILE,0,actual,13),CORE_OK);CHECK(!memcmp(projectile,actual,sizeof(projectile)));
        Villager *resumed=loaded->villagers[0];expansion_gain_xp(resumed,7,CORE_BLD_BARRACKS,180);
        for(int i=0;i<5;i++)CHECK_EQ(resumed->stat_xp[i]-baseline[i],((int[]){1050,2100,1350,1500,0})[i]);
        core_destroy(loaded);
    }
    Building *post=entity_add_building(w,CORE_BLD_OUTPOST);CHECK(post!=NULL);
    if(post) {
        post->state=CORE_BSTATE_ACTIVE;post->x=90;post->y=90;post->w=post->h=1;post->door_x=90;post->door_y=91;post->hp=110;
        Villager *day=w->villagers[0],*night=w->villagers[1];CoreWorkSchedule schedule;
        CHECK_EQ(core_assign_worker(w,day->id,post->id),CORE_OK);CHECK_EQ(core_assign_worker(w,night->id,post->id),CORE_OK);
        CHECK_EQ(day->shift_kind,0);CHECK_EQ(night->shift_kind,1);
        w->clock.tick=108;expansion_schedule(w,day,&schedule);CHECK(!schedule.on_duty);
        expansion_schedule(w,night,&schedule);CHECK(schedule.on_duty);
        w->clock.tick=144+54;CHECK_EQ(core_expansion_set_shift(w,day->id,1),CORE_OK);
        CHECK_EQ(night->shift_kind,0);CHECK_EQ(core_expansion_set_shift(w,day->id,0),CORE_ERR_REJECTED);
        w->clock.tick=144+107;night->state=CORE_VS_WORKING;night->target=post;night->at_target=1;
        night->current_x_milli=90500;night->current_y_milli=91500;
        CHECK_EQ(core_expansion_fire(w,post->id),CORE_OK);
        w->clock.tick=144+108;expansion_schedule(w,night,&schedule);CHECK(schedule.on_duty);
        expansion_schedule(w,day,&schedule);CHECK(!schedule.on_duty);
        w->clock.tick=144+114;expansion_schedule(w,day,&schedule);CHECK(schedule.on_duty);
    }
    /* 나무 갱신 뒤에도 재로드와 같은 생물군계 이동 비용을 유지합니다. */
    expansion_rebuild(w);
    expansion_growth(w);
    uint8_t *cost=malloc((size_t)w->expansion->cells);CHECK(cost!=NULL);
    if(cost){memcpy(cost,w->grid.cost,(size_t)w->expansion->cells);expansion_terrain_costs(w);CHECK(!memcmp(cost,w->grid.cost,(size_t)w->expansion->cells));free(cost);}
    w->clock.tick=400;v=w->villagers[0];v->mood=0;
    expansion_mood_event(w,v,13,-2,10001,18,0);expansion_mood_event(w,v,13,-2,10001,18,0);CHECK_EQ(v->mood,-2);
    expansion_mood_event(w,v,13,-2,10002,18,0);CHECK_EQ(v->mood,-4);
    w->clock.tick+=18;expansion_mood_event(w,v,13,-2,10001,18,0);CHECK_EQ(v->mood,-6);
    expansion_mood_event(w,v,17,-5,10001,12,0);expansion_mood_event(w,v,17,-5,10002,12,0);CHECK_EQ(v->mood,-11);
    expansion_mood_event(w,v,15,-3,77,1000000000,0);
    for(int n=0;n<40;n++)expansion_mood_event(w,v,13,-1,20000+n,18,0);
    int once=v->mood;expansion_mood_event(w,v,15,-3,77,1000000000,0);CHECK_EQ(v->mood,once);
    Building plan={0};plan.type=CORE_BLD_SMELTER;int in[CORE_RES_COUNT],out[CORE_RES_COUNT],fuel=0;
    CHECK_EQ(expansion_recipe_plan(w,&plan,1000,1000,0,in,out,&fuel),1000);
    CHECK_EQ(in[CORE_RES_IRON_ORE],8000);CHECK_EQ(out[CORE_RES_IRON],4000);
    /* 최근 사건과 노동 분수는 새 저장에도 그대로 남습니다. */
    CHECK_EQ(core_save(w,"v122_events.sav"),CORE_OK);loaded=NULL;
    CHECK_EQ(core_load("v122_events.sav",NULL,&loaded,&line),CORE_OK);
    if(loaded){uint64_t a,b;core_state_hash(w,&a);core_state_hash(loaded,&b);CHECK_EQ(a,b);core_destroy(loaded);remove("v122_events.sav");remove("v122_events.sav.bak");}
    core_destroy(w);
    if(rc==CORE_OK){remove("v122_roundtrip.sav");remove("v122_roundtrip.sav.bak");}
}
