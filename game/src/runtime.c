/* runtime.c — 이동·복장·공격·발사체·공격 대기시간을 저장 가능한 정수 상태로 연결합니다. */
#include "session.h"
#include <math.h>
#include <stdlib.h>

static int milli(float value){return (int)lroundf(value*1000.0f);}
int game_session_checkpoint(GameSession *s,int restore) {
    if(!s->summary.ruleset)return CORE_OK;
    int32_t clock[2]={milli(s->tick_minutes),s->projectile_count};
    if(restore) {
        int rc=core_expansion_copy_runtime(s->world,CORE_RUNTIME_CLOCK,0,clock,2);if(rc!=CORE_OK)return rc;
        if(clock[0]<0||clock[0]>10000||clock[1]<0||clock[1]>GAME_MAX_PROJECTILES)return CORE_ERR_FORMAT;
        s->tick_minutes=clock[0]/1000.0f;s->projectile_count=clock[1];
    } else {
        int rc=core_expansion_report_runtime(s->world,CORE_RUNTIME_CLOCK,0,clock,2);if(rc!=CORE_OK)return rc;
    }
    for(int i=0;i<s->actor_count;i++) {
        GameActor *a=&s->actors[i];CoreVillagerView *v=game_villager(s,a->id);if(!v)continue;
        int32_t r[20]={1,milli(a->position.x),milli(a->position.y),milli(a->cooldown),milli(a->walk_distance),a->facing,
            milli(a->indoor),a->indoor_building,a->chasing,milli(a->chase_goal.x),milli(a->chase_goal.y),milli(a->attack_elapsed),
            a->attack_serial,a->attack_applied,a->attack_kind,a->attack_target,a->outfit,a->target_x,a->target_y,a->revision};
        if(restore) {
            if(core_expansion_copy_runtime(s->world,CORE_RUNTIME_ACTOR,a->id,r,20)!=CORE_OK||!r[0])continue;
            if(r[1]<0||r[2]<0||r[1]>=s->summary.map_width*1000||r[2]>=s->summary.map_height*1000)return CORE_ERR_FORMAT;
            a->position=(GamePoint){v->current_x_milli/1000.0f,v->current_y_milli/1000.0f};a->cooldown=r[3]/1000.0f;a->walk_distance=r[4]/1000.0f;
            a->facing=r[5];a->indoor=r[6]/1000.0f;a->indoor_building=r[7];a->chasing=r[8];
            a->chase_goal=(GamePoint){r[9]/1000.0f,r[10]/1000.0f};a->attack_elapsed=r[11]/1000.0f;
            a->attack_serial=r[12];a->attack_applied=r[13];a->attack_kind=r[14];a->attack_target=r[15];a->outfit=r[16];
            (void)game_actor_plan(s,a,r[17],r[18],0);a->revision=r[19];
        } else {
            int rc=core_expansion_report_runtime(s->world,CORE_RUNTIME_ACTOR,a->id,r,20);if(rc!=CORE_OK)return rc;
        }
    }
    if(restore) {
        GameTower *states=realloc(s->towers,(size_t)(s->building_count+1)*sizeof(*states));if(!states)return CORE_ERR_OUT_OF_MEMORY;
        s->towers=states;s->tower_capacity=s->building_count+1;s->tower_count=0;
        for(int i=0;i<s->building_count;i++) {
            int32_t r[2]={0};int id=s->buildings[i].id;
            if(core_expansion_copy_runtime(s->world,CORE_RUNTIME_TOWER,id,r,2)==CORE_OK && r[0])
                s->towers[s->tower_count++]=(GameTower){id,r[1]/1000.0f};
        }
    } else for(int i=0;i<s->tower_count;i++) {
        int32_t r[2]={1,milli(s->towers[i].cooldown)};
        (void)core_expansion_report_runtime(s->world,CORE_RUNTIME_TOWER,s->towers[i].id,r,2);
    }
    for(int i=0;i<s->projectile_count;i++) {
        GameProjectile *p=&s->projectiles[i];
        int32_t r[13]={milli(p->from.x),milli(p->from.y),milli(p->to.x),milli(p->to.y),milli(p->elapsed),milli(p->duration),milli(p->radius),p->raid_id,p->target_id,p->target_kind,p->damage,p->source_type,p->source_id};
        if(restore) {
            if(core_expansion_copy_runtime(s->world,CORE_RUNTIME_PROJECTILE,i,r,13)!=CORE_OK||r[5]<1)return CORE_ERR_FORMAT;
            *p=(GameProjectile){{r[0]/1000.0f,r[1]/1000.0f},{r[2]/1000.0f,r[3]/1000.0f},r[4]/1000.0f,r[5]/1000.0f,r[6]/1000.0f,r[7],r[8],r[9],r[10],r[11],r[12]};
            if(p->target_kind==1 && p->raid_id==s->raid_id){GameEnemy *enemy=game_enemy(s,p->target_id);if(enemy)enemy->incoming+=p->damage;}
        } else {
            int rc=core_expansion_report_runtime(s->world,CORE_RUNTIME_PROJECTILE,i,r,13);if(rc!=CORE_OK)return rc;
        }
    }
    return CORE_OK;
}
