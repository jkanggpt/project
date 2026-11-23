/* expansion_v122.c — 공통 일정·상태 질의·종류 번호·새 지도와 v9 이관 */
#include "expansion.h"
#include "villager_ai.h"
#include "placement.h"
#include "rank.h"
#include <stdlib.h>
#include <string.h>

CORE_API int32_t core_expansion_tree_mature_age(int32_t kind) {
    static const int ages[4]={16000,28000,48000,72000};
    return kind>=1 && kind<=4?ages[kind-1]:0;
}
int expansion_military(int type) {return type==CORE_BLD_BARRACKS || type==CORE_BLD_GARRISON;}
void expansion_initialize_shift(World *w,Building *b,Villager *v) {
    v->mission=CORE_MISSION_NONE;v->leisure_building=0;v->leisure_elapsed=0;
    if(!expansion_tower(b->type))return;
    Villager *other=NULL;for(int i=0;i<b->worker_count;i++)if(b->workers[i]!=v){other=b->workers[i];break;}
    v->shift_kind=other?1-other->shift_kind:0;v->shift_changed_day=v->shift_kind?exp_day(w):0;
    v->shift_adapt_days=0;v->shift_sleep_day=0;
}
int expansion_defend_damage(const Villager *v,int damage) {
    if(damage<=0 || !v->workplace || !expansion_military(v->workplace->type))return damage;
    int armor=exp_clamp(2+v->stats.vitality/4,2,4)+(v->workplace->type==CORE_BLD_GARRISON);
    int maximum=exp_clamp(100+8*(v->stats.vitality-4),76,148)*(v->workplace->type==CORE_BLD_GARRISON?150:125)/100;
    int reduced=exp_clamp(damage-armor,1,10000);
    return exp_clamp((reduced*100+maximum/2)/maximum,1,10000);
}
CORE_API int32_t core_expansion_building_hit(CoreWorld *w,int32_t id,int32_t damage,int32_t kind,int32_t source) {
    if(!w)return CORE_ERR_NULL_ARG;
    Building *b=entity_find_building(w,id);if(!w->expansion||!b)return CORE_ERR_NOT_FOUND;
    if(damage<1||damage>10000||kind<0||kind>3)return CORE_ERR_INVALID_ARG;
    if(b->state==CORE_BSTATE_RUINED || (kind==1 && b->state==CORE_BSTATE_CONSTRUCTING))return CORE_IGNORED;
    b->damage_kind=kind;b->damage_source=source;b->damage_tick=w->clock.tick;
    if(kind==1)return core_report_building_damage(w,w->threat.raid.id,id,damage);
    placement_damage(w,b,damage);
    if(b->type==CORE_BLD_KEEP&&b->state==CORE_BSTATE_RUINED)rank_lose(w,CORE_LOSE_KEEP);
    return CORE_OK;
}
CORE_API int32_t core_expansion_person_hit(CoreWorld *w,int32_t id,int32_t damage,int32_t kind,int32_t source) {
    if(!w)return CORE_ERR_NULL_ARG;
    Villager *v=entity_find_villager(w,id);if(!w->expansion||!v)return CORE_ERR_NOT_FOUND;
    v->damage_kind=kind;v->damage_source=source;
    return core_report_villager_damage(w,w->threat.raid.id,id,damage);
}
int expansion_tower(int type) {return expansion_spec(type)->category==CORE_CAT_TOWERS;}
static int32_t *runtime_slot(const World *w,int kind,int id,int *count) {
    *count=0;if(!w->expansion)return NULL;
    if(kind==CORE_RUNTIME_ACTOR){Villager *v=entity_find_villager(w,id);*count=20;return v?v->frontend_actor:NULL;}
    if(kind==CORE_RUNTIME_TOWER){Building *b=entity_find_building(w,id);*count=2;return b?b->frontend_tower:NULL;}
    if(kind==CORE_RUNTIME_CLOCK && !id){*count=2;return w->expansion->frontend_clock;}
    if(kind==CORE_RUNTIME_PROJECTILE && id>=0 && id<512){*count=13;return w->expansion->frontend_projectiles[id];}
    return NULL;
}
CORE_API int32_t core_expansion_copy_runtime(const CoreWorld *w,int32_t kind,int32_t id,int32_t *values,int32_t count) {
    if(!w||!values)return CORE_ERR_NULL_ARG;
    int expected=0;int32_t *slot=runtime_slot(w,kind,id,&expected);
    if(!slot)return CORE_ERR_NOT_FOUND;
    if(count!=expected)return CORE_ERR_BUFFER_TOO_SMALL;
    memcpy(values,slot,(size_t)count*sizeof(*values));return CORE_OK;
}
CORE_API int32_t core_expansion_report_runtime(CoreWorld *w,int32_t kind,int32_t id,const int32_t *values,int32_t count) {
    if(!w||!values)return CORE_ERR_NULL_ARG;
    int expected=0;int32_t *slot=runtime_slot(w,kind,id,&expected);
    if(!slot)return CORE_ERR_NOT_FOUND;
    if(count!=expected)return CORE_ERR_INVALID_ARG;
    if(kind==CORE_RUNTIME_CLOCK && (values[0]<0||values[0]>10000||values[1]<0||values[1]>512))return CORE_ERR_INVALID_ARG;
    if(kind==CORE_RUNTIME_ACTOR && (values[0]<0||values[0]>1||values[1]<0||values[1]>=w->grid.width*1000||values[2]<0||values[2]>=w->grid.height*1000))return CORE_ERR_INVALID_ARG;
    memcpy(slot,values,(size_t)count*sizeof(*values));return CORE_OK;
}
CORE_API int32_t core_expansion_build_locked(const CoreWorld *w) {
    return w && w->expansion && threat_raid_active(w);
}
CORE_API int32_t core_expansion_enemy_runtime(CoreWorld *w,int32_t id,const CoreEnemyRuntime *state) {
    if(!w||!state)return CORE_ERR_NULL_ARG;
    Raid *r=&w->threat.raid;
    if(!w->expansion||!r->id||id<1||id>r->enemy_count)return CORE_ERR_NOT_FOUND;
    if(state->inbound_count<0||state->inbound_count>64||state->hp<0||state->hp>100000||state->x_milli<0||state->y_milli<0||state->x_milli>=w->grid.width*1000||state->y_milli>=w->grid.height*1000)return CORE_ERR_INVALID_ARG;
    r->enemies[id-1].runtime=*state;return CORE_OK;
}
void expansion_v122_numbers(World *w) {
    ExpansionState *e=w->expansion;
    for(int i=0;i<w->building_count;i++) {
        Building *b=w->buildings[i];
        if(b->type_serial>e->next_type_serial[b->type])e->next_type_serial[b->type]=b->type_serial;
        if(!b->type_serial)b->type_serial=++e->next_type_serial[b->type];
    }
}
static uint32_t map_noise(int seed,int x,int y) {
    uint32_t n=(uint32_t)seed*2654435761u+(uint32_t)x*374761393u+(uint32_t)y*668265263u;
    n=(n^(n>>13))*1274126177u;return n^(n>>16);
}
static int stone_base(const World *w,int p) {
    int weight=(int[]){6,6,6,10,5,8,7,4,5,6}[w->expansion->tiles[p].biome];
    return (8000+(int)(map_noise(w->seed,p%w->grid.width,p/w->grid.width)%10001))*weight/6;
}
void expansion_v122_generate(World *w) {
    ExpansionState *e=w->expansion;
    /* 연결 성분별로 호수를 배치하여 물의 수와 실제 수역 수가 일치하게 합니다. */
    for(int p=0;p<e->cells;p++)if(w->grid.terrain[p]==CORE_TERRAIN_WATER) {
        w->grid.terrain[p]=CORE_TERRAIN_GRASS;e->tiles[p].biome=CORE_BIOME_GRASS;
    }
    static const int positions[7][2]={{1,1},{3,1},{5,1},{1,3},{5,3},{1,5},{5,5}};
    static const int radii[7]={4,5,6,7,8,10,12};
    for(int k=0;k<7;k++) {
        uint32_t n=map_noise(w->seed,k,701);
        int cx=positions[k][0]*w->grid.width/6+(int)(n%9)-4;
        int cy=positions[k][1]*w->grid.height/6+(int)((n>>8)%9)-4,r=radii[k];
        for(int y=cy-r-2;y<=cy+r+2;y++)for(int x=cx-r-2;x<=cx+r+2;x++) {
            if(!grid_in_bounds(&w->grid,x,y)||exp_dist(x,y,w->keep_x,w->keep_y)<12)continue;
            int dx=x-cx,dy=y-cy;
            int wave=(int)(map_noise(w->seed,x/3+k*31,y/3)%9)-4;
            if(dx*dx*100+dy*dy*135>r*r*100+wave*r*12)continue;
            int p=grid_index(&w->grid,x,y);
            if(w->grid.occupant[p])continue;
            e->tiles[p].biome=CORE_BIOME_LAKE;e->tiles[p].tree_kind=0;e->tiles[p].tree_logs_milli=0;
            e->tiles[p].tree_age=0;e->tiles[p].regrow_milli=0;w->grid.terrain[p]=CORE_TERRAIN_WATER;
        }
    }
    for(int p=0;p<e->cells;p++) {
        e->tiles[p].stone_milli=stone_base(w,p);e->tiles[p].ore_kind=-1;e->tiles[p].ore_milli=0;
    }
    static const int resources[5]={CORE_RES_STONE,CORE_RES_COAL,CORE_RES_IRON_ORE,CORE_RES_GOLD_ORE,CORE_RES_MANA};
    static const int lengths[5][2]={{12,38},{9,30},{8,26},{4,14},{3,12}},widths[5]={3,2,2,1,1},clusters[5]={40,34,34,16,20};
    static const int weights[5][10]={{6,6,6,10,5,8,7,3,5,6},{3,4,6,8,4,5,2,0,3,3},{3,4,5,9,2,6,4,0,2,3},{1,1,2,7,1,4,3,0,1,2},{1,3,4,5,3,3,1,0,4,2}};
    static const int dx[4]={0,1,0,-1},dy[4]={-1,0,1,0};
    for(int kind=0;kind<5;kind++)for(int c=0;c<clusters[kind];c++) {
        int x=0,y=0;
        for(int trial=0;trial<100;trial++) {
            x=rng_range(&w->rng,3,w->grid.width-4);y=rng_range(&w->rng,3,w->grid.height-4);
            if(rng_range(&w->rng,0,9)<weights[kind][e->tiles[grid_index(&w->grid,x,y)].biome])break;
        }
        int length=rng_range(&w->rng,lengths[kind][0],lengths[kind][1]),direction=rng_range(&w->rng,0,3);
        for(int step=0;step<length;step++) {
            int r=kind==0?rng_range(&w->rng,1,widths[kind]):rng_range(&w->rng,0,widths[kind]-1);
            for(int yy=y-r;yy<=y+r;yy++)for(int xx=x-r;xx<=x+r;xx++) {
                if(!grid_in_bounds(&w->grid,xx,yy)||exp_dist(xx,yy,x,y)>r)continue;
                CoreExpansionTileView *t=&e->tiles[grid_index(&w->grid,xx,yy)];
                int amount=(int[]){45000,30000,24000,7000,5000}[kind]*(850+(int)(map_noise(w->seed,xx,yy)%301))/1000;
                if(kind==0)t->stone_milli+=amount;
                else if(t->biome!=CORE_BIOME_LAKE && (t->ore_kind<0 || t->ore_kind==resources[kind])) {
                    t->ore_kind=resources[kind];t->ore_milli+=amount;
                }
            }
            if(step%3==0)direction=(direction+(rng_range(&w->rng,0,2)==0?1:0))%4;
            int xx=x+dx[direction],yy=y+dy[direction];
            if(xx>2&&yy>2&&xx<w->grid.width-3&&yy<w->grid.height-3){x=xx;y=yy;}
            else direction=(direction+2)%4;
        }
    }
    int stone=grid_index(&w->grid,w->keep_x-10,w->keep_y-8),iron=grid_index(&w->grid,w->keep_x+20,w->keep_y-4);
    e->tiles[stone].stone_milli+=750000;e->tiles[iron].ore_kind=CORE_RES_IRON_ORE;e->tiles[iron].ore_milli+=500000;
    for(int p=0;p<e->cells;p++) {
        CoreExpansionTileView *t=&e->tiles[p];
        if((t->ore_milli>0||t->stone_milli>50000) && map_noise(w->seed,p%w->grid.width/2,p/w->grid.width/2)%4==0)
            t->rock_variant=1+(int)(map_noise(w->seed,p,21)%3);
    }
    expansion_water_bodies(w);expansion_v122_numbers(w);
    e->market_next_restock_day=7;e->loaded_version=10;
}
void expansion_v122_migrate(World *w) {
    ExpansionState *e=w->expansion;
    if(e->loaded_version>=10)return;
    for(int p=0;p<e->cells;p++) {
        CoreExpansionTileView *t=&e->tiles[p];
        if(t->ore_kind==CORE_RES_STONE){t->stone_milli=t->ore_milli;t->ore_milli=0;t->ore_kind=-1;}
        else t->stone_milli=stone_base(w,p);
        if(t->tree_kind==4 && t->regrow_milli>0)t->regrow_milli=exp_clamp(t->regrow_milli*18/48,1000,24000);
        if((t->ore_milli>0||t->stone_milli>50000) && map_noise(w->seed,p/2,p/192/2)%4==0)t->rock_variant=1+p%3;
    }
    for(int i=0;i<w->building_count;i++)if(w->buildings[i]->type==CORE_BLD_COTTAGE)w->buildings[i]->rotation^=1;
    for(int i=0;i<w->villager_count;i++)expansion_migrate_xp(w->villagers[i]);
    for(int i=0;i<w->building_count;i++)if(expansion_tower(w->buildings[i]->type))
        for(int j=0;j<w->buildings[i]->worker_count;j++)w->buildings[i]->workers[j]->shift_kind=j%2;
    expansion_v122_numbers(w);
    e->immigration_day=e->last_arrival_day+2;e->market_next_restock_day=exp_day(w)+6;e->loaded_version=10;
}
int expansion_duty(World *w,Villager *v,Building *b) {
    CoreWorkSchedule schedule;expansion_schedule(w,v,&schedule);
    if(!schedule.on_duty)return 0;
    if(!v->mission && (v->target!=b || !v->at_target))return 0;
    if(v->duty_day!=exp_day(w)){v->duty_day=exp_day(w);v->duty_minutes=0;}
    v->duty_minutes+=10;
    if(v->duty_minutes>=(expansion_tower(b->type)?720:550))expansion_mood_event(w,v,8,expansion_tower(b->type)?-2:-1,0,144,1);
    if(expansion_tower(b->type))return 0;
    int hunting=b->type==CORE_BLD_HUNTER_CAMP||b->type==CORE_BLD_HUNTING_LODGE;
    if(!hunting&&!expansion_military(b->type))return 0;
    if(!v->mission && (v->target!=b||!v->at_target))return 0;
    if(v->health<=35 || (v->mission==CORE_MISSION_RETURN&&v->health<70)) {
        v->mission=CORE_MISSION_RETURN;vil_set_intent(v,CORE_VS_GUARD_POST,b,b->door_x,b->door_y,0);
        if(v->at_target) {v->inside_building=b->id;v->health=exp_clamp(v->health+1,0,100);}
        return 1;
    }
    v->inside_building=0;
    if(w->expansion->alarm && !hunting){v->mission=CORE_MISSION_COMBAT;return 0;}
    int sight=4;
    int radius=hunting?exp_clamp(expansion_spec(b->type)->radius-sight,0,15):b->type==CORE_BLD_GARRISON?4:3;
    if(w->clock.tick>=v->mission_next_tick || !v->mission) {
        int x=b->door_x,y=b->door_y;
        for(int attempt=0;attempt<40;attempt++) {
            uint32_t n=map_noise(v->id,v->mission_waypoint++,exp_day(w));
            int xx=b->x+b->w/2+(int)(n%(unsigned)(radius*2+1))-radius;
            int yy=b->y+b->h/2+(int)((n>>8)%(unsigned)(radius*2+1))-radius;
            if(grid_in_bounds(&w->grid,xx,yy)&&expansion_center_distance(b,xx,yy)<=radius&&w->grid.cost[grid_index(&w->grid,xx,yy)]&&w->grid.block[grid_index(&w->grid,xx,yy)]!=GRID_BLOCK_WALL){x=xx;y=yy;break;}
        }
        v->mission=hunting?CORE_MISSION_SCOUT:CORE_MISSION_PATROL;
        vil_set_intent(v,CORE_VS_GUARD_POST,NULL,x,y,0);v->mission_next_tick=w->clock.tick+12;
    }
    return 1;
}
int expansion_leisure(World *w,Villager *v) {
    int minute=exp_minute(w),day=exp_day(w);
    if(minute<420||minute>=1320||v->needs.hunger<40||v->needs.rest<30||w->expansion->alarm)return 0;
    if(v->leisure_building) {
        Building *b=entity_find_building(w,v->leisure_building);
        if(!b||b->state!=CORE_BSTATE_ACTIVE||!expansion_working_count(w,b)||!expansion_qualified_present(w,b)||v->target!=b) {
            v->leisure_building=0;v->leisure_elapsed=0;v->mission_next_tick=w->clock.tick+12;return 0;
        }
        if(v->at_target) {
            v->inside_building=b->id;v->leisure_elapsed+=10;
            if(v->leisure_elapsed>=v->leisure_duration && b->service_credit_milli>=1000) {
                expansion_leisure_reward(w,v,b,day);
                if(b->type==CORE_BLD_SCHOOL||b->type==CORE_BLD_LIBRARY||b->type==CORE_BLD_ACADEMY) {
                    v->education_day=day;expansion_mood_event(w,v,10,2,b->id,144,1);
                } else v->leisure_day=day;
                v->leisure_building=0;v->leisure_elapsed=0;v->mission_next_tick=w->clock.tick+12;
                return 0;
            }
        }
        return 1;
    }
    if(v->mission==CORE_MISSION_WALK) {
        int x=v->current_x_milli/1000,y=v->current_y_milli/1000;
        int p=grid_index(&w->grid,x,y),origin=grid_index(&w->grid,v->walk_origin_x,v->walk_origin_y);
        if(!w->expansion->protected_cells[p] || !w->expansion->protected_cells[origin]){v->mission=CORE_MISSION_NONE;return 0;}
        if(!v->at_target)return 1;
        if(x!=v->walk_origin_x || y!=v->walk_origin_y) {
            vil_set_intent(v,CORE_VS_IDLE,NULL,v->walk_origin_x,v->walk_origin_y,0);return 1;
        }
        if(w->clock.tick>=v->mission_next_tick) {
            v->mission=CORE_MISSION_NONE;
            if(v->walk_reward_day!=day){expansion_mood_event(w,v,9,1,0,144,1);v->walk_reward_day=day;}
            v->mission_next_tick=w->clock.tick+12;return 0;
        }
        static const int dx[4]={0,1,0,-1},dy[4]={-1,0,1,0};
        int start=rng_range(&w->rng,0,3);
        for(int k=0;k<4;k++) {
            int xx=x+dx[(start+k)%4],yy=y+dy[(start+k)%4];if(!grid_in_bounds(&w->grid,xx,yy))continue;
            int q=grid_index(&w->grid,xx,yy);
            if(w->expansion->protected_cells[q] && w->expansion->tiles[q].explored && w->grid.cost[q] && !w->grid.block[q]) {
                vil_set_intent(v,CORE_VS_IDLE,NULL,xx,yy,0);return 1;
            }
        }
        v->mission=CORE_MISSION_NONE;return 0;
    }
    if(w->clock.tick<v->mission_next_tick)return 0;
    v->mission_next_tick=w->clock.tick+12;
    int weight_sum=20,choice=-1;
    for(int i=0;i<w->building_count;i++) {
        Building *b=w->buildings[i];int education=b->type==CORE_BLD_SCHOOL||b->type==CORE_BLD_LIBRARY||b->type==CORE_BLD_ACADEMY;
        if(!expansion_service_shift(b->type)||b->state!=CORE_BSTATE_ACTIVE||!expansion_working_count(w,b)||!expansion_qualified_present(w,b)||b->service_credit_milli<1000)continue;
        if(education?v->education_day==day:v->leisure_day==day)continue;
        int index=b->type==CORE_BLD_TAVERN?0:b->type==CORE_BLD_BATHHOUSE?1:b->type==CORE_BLD_CHURCH?3:2;
        if(!education && day<v->leisure_days[index])continue;
        int guests=0;for(int j=0;j<w->villager_count;j++)guests+=w->villagers[j]->leisure_building==b->id;
        if(guests>=expansion_service_base(b,expansion_working_count(w,b)))continue;
        int distance=exp_dist(v->current_x_milli/1000,v->current_y_milli/1000,b->door_x,b->door_y);
        int weight=exp_clamp(70-v->mood/5-distance*2-guests*5,1,120);
        if(v->needs.rest<60)weight/=2;
        weight_sum+=weight;if(rng_range(&w->rng,1,weight_sum)<=weight)choice=i;
    }
    if(choice>=0) {
        Building *b=w->buildings[choice];v->leisure_building=b->id;v->leisure_elapsed=0;
        v->leisure_duration=b->type==CORE_BLD_TAVERN?60:b->type==CORE_BLD_BATHHOUSE?80:b->type==CORE_BLD_CHURCH?70:b->type==CORE_BLD_ACADEMY?110:100;
        vil_set_intent(v,CORE_VS_IDLE,b,b->door_x,b->door_y,0);return 1;
    }
    /* 단일 인접 칸의 경로이므로 보호구역 바깥을 가로지르는 산책이 없습니다. */
    if(minute<1140 && expansion_person_protected(w,v) && rng_range(&w->rng,0,3)==0) {
        int x=v->current_x_milli/1000,y=v->current_y_milli/1000;
        static const int dx[4]={0,1,0,-1},dy[4]={-1,0,1,0};
        int start=rng_range(&w->rng,0,3);
        for(int k=0;k<4;k++) {
            int xx=x+dx[(start+k)%4],yy=y+dy[(start+k)%4];if(!grid_in_bounds(&w->grid,xx,yy))continue;
            int p=grid_index(&w->grid,xx,yy);
            if(w->expansion->protected_cells[p] && w->expansion->tiles[p].explored && w->grid.cost[p] && !w->grid.block[p]) {
                v->walk_origin_x=x;v->walk_origin_y=y;v->mission=CORE_MISSION_WALK;
                vil_set_intent(v,CORE_VS_IDLE,NULL,xx,yy,0);v->mission_next_tick=w->clock.tick+rng_range(&w->rng,2,6);return 1;
            }
        }
    }
    return 0;
}
int expansion_schedule(const World *w,const Villager *v,CoreWorkSchedule *out) {
    *out=(CoreWorkSchedule){480,1020,0,0,1320,360,0,0};
    const Building *b=v->workplace;if(!b || b->state!=CORE_BSTATE_ACTIVE)return 0;
    int type=b->type,field=expansion_spec(type)->field,minute=exp_minute(w);
    if(field==0||field==1||field==5){out->start=420;out->end=960;}
    if(field==3||type==CORE_BLD_RANCH||type==CORE_BLD_PASTURE){out->start=360;out->end=900;}
    if(field==6||field==8){out->start=540;out->end=1080;}
    if(expansion_service_shift(type)) {
        if(b->worker_count==1){out->start=420;out->end=660;out->second_start=1020;out->second_end=1320;}
        else {
            int slot=0;for(int i=0;i<b->worker_count;i++)if(b->workers[i]==v)slot=i;
            out->start=(slot&1)?780:420;out->end=(slot&1)?1320:960;
        }
    }
    if(expansion_military(type)){out->start=480;out->end=1080;}
    if(expansion_tower(type)) {
        out->shift=v->shift_kind;
        out->start=out->shift?1080:360;out->end=out->shift?360:1080;
        out->sleep_start=out->shift?480:1320;out->sleep_end=out->shift?960:360;
    }
    out->on_duty=out->start<out->end ? minute>=out->start&&minute<out->end : minute>=out->start||minute<out->end;
    out->on_duty|=out->second_end && minute>=out->second_start && minute<out->second_end;
    if(expansion_tower(type)) {
        if(b->combat_guard_id && b->combat_until_tick>w->clock.tick)out->on_duty=b->combat_guard_id==v->id;
        else if(w->expansion->alarm && b->worker_count==1)out->on_duty=1;
    }
    return out->on_duty;
}
CORE_API int32_t core_expansion_schedule(const CoreWorld *w,int32_t id,CoreWorkSchedule *out) {
    if(!w||!out)return CORE_ERR_NULL_ARG;
    const Villager *v=entity_find_villager(w,id);if(!w->expansion||!v)return CORE_ERR_NOT_FOUND;
    expansion_schedule(w,v,out);return CORE_OK;
}
CORE_API int32_t core_expansion_set_shift(CoreWorld *w,int32_t id,int32_t shift) {
    if(!w)return CORE_ERR_NULL_ARG;
    Villager *v=entity_find_villager(w,id);
    if(!w->expansion||!v||!v->workplace||!expansion_tower(v->workplace->type)||shift<0||shift>1)return CORE_ERR_INVALID_ARG;
    if(v->shift_kind==shift)return CORE_OK;
    if(v->shift_changed_day==exp_day(w))return CORE_ERR_REJECTED;
    Building *b=v->workplace;
    if(b->combat_guard_id && b->combat_until_tick>w->clock.tick)return CORE_ERR_REJECTED;
    for(int i=0;i<b->worker_count;i++)if(b->workers[i]!=v && b->workers[i]->shift_kind==shift) {
        Villager *other=b->workers[i];if(other->shift_changed_day==exp_day(w))return CORE_ERR_REJECTED;
        other->shift_kind=v->shift_kind;other->shift_changed_day=exp_day(w);other->shift_adapt_days=0;other->shift_sleep_day=0;
    }
    v->shift_kind=shift;v->shift_changed_day=exp_day(w);v->shift_adapt_days=0;v->shift_sleep_day=0;return CORE_OK;
}
CORE_API int32_t core_expansion_assign_shift(CoreWorld *w,int32_t id,int32_t building_id,int32_t shift) {
    if(!w)return CORE_ERR_NULL_ARG;
    Villager *v=entity_find_villager(w,id);Building *b=entity_find_building(w,building_id);
    if(!w->expansion || !v || !b)return CORE_ERR_NOT_FOUND;
    if(!expansion_tower(b->type) || shift<0 || shift>1)return CORE_ERR_INVALID_ARG;
    if(v->workplace==b)return core_expansion_set_shift(w,id,shift);
    for(int i=0;i<b->worker_count;i++)if(b->workers[i]->shift_kind==shift)return CORE_ERR_REJECTED;
    int rc=core_assign_worker(w,id,building_id);if(rc!=CORE_OK)return rc;
    v->shift_kind=shift;v->shift_changed_day=exp_day(w);v->shift_adapt_days=0;v->shift_sleep_day=0;
    return CORE_OK;
}
void expansion_mood_event(World *w,Villager *v,int kind,int amount,int source,int cooldown,int cap) {
    if(kind<0||kind>=32||!amount)return;
    if(v->mood_day!=exp_day(w)){memset(v->mood_counts,0,sizeof(v->mood_counts));v->mood_day=exp_day(w);}
    if(kind==17 && v->mood_ticks[kind] && w->clock.tick+1-v->mood_ticks[kind]<cooldown)return;
    if(source) {
        for(int i=0;i<32;i++)if(v->mood_recent[i][0]==kind+1 && v->mood_recent[i][1]==source &&
            w->clock.tick+1-v->mood_recent[i][2]<cooldown)return;
    } else if(v->mood_ticks[kind] && w->clock.tick+1-v->mood_ticks[kind]<cooldown)return;
    if(cap>0 && v->mood_counts[kind]>=cap)return;
    v->mood=exp_clamp(v->mood+amount,-200,200);v->mood_ticks[kind]=w->clock.tick+1;v->mood_counts[kind]++;
    if(source){int fixed=kind==15?0:kind==24?1:kind==26?2:-1;int i=fixed>=0?fixed:3+v->mood_recent_cursor%29;
        if(fixed<0){v->mood_recent_cursor=(v->mood_recent_cursor+1)%29;}
        v->mood_recent[i][0]=kind+1;v->mood_recent[i][1]=source;v->mood_recent[i][2]=w->clock.tick+1;}
    v->mood_cause=32+kind;v->mood_event_tick=w->clock.tick;v->mood_event_amount=amount;v->mood_event_source=source;    v->mood_source_kind=0;v->mood_source_type=0;
    if(kind==17 || kind==18)v->mood_source_kind=v->damage_kind;
    else if(kind==13)v->mood_source_kind=2;
    else if(kind==7 || kind==10 || kind==14 || kind==21)v->mood_source_kind=3;
    else if(kind==19 || kind==27)v->mood_source_kind=4;
    else if(kind==15 || kind==20 || kind==24 || kind==26)v->mood_source_kind=5;
    if(v->mood_source_kind==1 && source>=1 && source<=w->threat.raid.enemy_count)v->mood_source_type=w->threat.raid.enemies[source-1].type+1;
    if(v->mood_source_kind==2)for(int j=0;j<w->expansion->animal_count;j++)
        if(w->expansion->animals[j].id==source)v->mood_source_type=w->expansion->animals[j].kind+1;

}
void expansion_v122_life(World *w,Villager *v) {
    int minute=exp_minute(w),h=v->needs.hunger,t=v->water;
    int asleep=v->state==CORE_VS_SLEEP && v->at_target;
    if(minute%60==0) {
        if(!asleep) {
            if(t<40)expansion_mood_event(w,v,3,-1,0,12,6);
            if(h<20)expansion_mood_event(w,v,5,-2,0,6,24);
            else if(h<40)expansion_mood_event(w,v,5,-1,0,6,24);
            else if(h<70)expansion_mood_event(w,v,5,-1,0,12,6);
            if(v->needs.rest<30)expansion_mood_event(w,v,12,-1,0,12,4);
        }
        if(expansion_person_protected(w,v))expansion_mood_event(w,v,11,1,0,144,1);
        if(asleep && v->needs.rest>=90)expansion_mood_event(w,v,6,2,0,144,1);
    }
    if(!asleep && t<40 && !v->thirst_stage)expansion_mood_event(w,v,2,-2,0,12,1);
    if(!asleep && h<70 && !v->hunger_stage)expansion_mood_event(w,v,4,-1,0,12,1);
    v->thirst_stage=t<40;v->hunger_stage=h<70;
    if(w->threat.raid.id && w->threat.raid.first_discovered) {
        expansion_mood_event(w,v,15,-3,w->threat.raid.id,1000000000,0);
        if(asleep)expansion_mood_event(w,v,26,-2,w->threat.raid.id,1000000000,0);
    }
    if(asleep || v->inside_building)return;
    int x=v->current_x_milli/1000,y=v->current_y_milli/1000;
    Raid *raid=&w->threat.raid;
    if(raid->id) {
        for(int i=0;i<raid->enemy_count;i++) {
            const Enemy *enemy=&raid->enemies[i];const CoreEnemyRuntime *r=&enemy->runtime;
            if(enemy->state!=CORE_ENEMY_ALIVE || !r->valid)continue;
            int distance=exp_dist(x,y,r->x_milli/1000,r->y_milli/1000);
            if(distance<=4)expansion_mood_event(w,v,24,-5,raid->id,1000000000,0);
            if(distance<=1 && grid_in_bounds(&w->grid,r->x_milli/1000,r->y_milli/1000) &&
               w->grid.cost[grid_index(&w->grid,r->x_milli/1000,r->y_milli/1000)])
                expansion_mood_event(w,v,16,-4,enemy->id,18,4);
        }
    }
    if(!expansion_person_protected(w,v))for(int i=0;i<w->expansion->animal_count;i++) {
        const CoreAnimalView *animal=&w->expansion->animals[i];
        if(!animal->alive || animal->owner || EXP_ANIMALS[animal->kind].behavior!=2)continue;
        int distance=exp_dist(x,y,animal->x_milli/1000,animal->y_milli/1000);
        if(distance<=4)expansion_mood_event(w,v,13,-2,animal->id,18,4);
        if(distance<=1 && grid_in_bounds(&w->grid,animal->x_milli/1000,animal->y_milli/1000) &&
           w->grid.cost[grid_index(&w->grid,animal->x_milli/1000,animal->y_milli/1000)])
            expansion_mood_event(w,v,16,-4,animal->id,18,4);
    }
}
CORE_API int32_t core_expansion_facility(const CoreWorld *w,int32_t id,int32_t type,int32_t x,int32_t y,int32_t rotation,CoreFacilityView *out) {
    if(!w||!out)return CORE_ERR_NULL_ARG;
    if(!w->expansion)return CORE_ERR_INVALID_ARG;
    const Building *b=id?entity_find_building(w,id):NULL;Building candidate={0};
    if(id&&!b)return CORE_ERR_NOT_FOUND;
    if(!b) {
        const ExpansionBuildingSpec *s=expansion_spec(type);if(!s)return CORE_ERR_INVALID_ARG;
        candidate.type=type;candidate.x=x;candidate.y=y;candidate.w=(rotation&1)?s->h:s->w;candidate.h=(rotation&1)?s->w:s->h;b=&candidate;
    }
    const ExpansionBuildingSpec *s=expansion_spec(b->type);*out=(CoreFacilityView){0};
    out->id=b->id;out->type_serial=b->type_serial;out->radius=s->radius;out->sight=s->sight;
    out->attack=w->defs.buildings[b->type].attack;out->range=w->defs.buildings[b->type].attack_range;out->cooldown=s->cooldown;
    if(b->type==CORE_BLD_KEEP){out->attack=(int[]){6,9,13,18,26,35}[w->rank];out->range=4+w->rank;out->cooldown=25;}
    out->assigned=b->worker_count;out->present=expansion_working_count(w,b);out->labor_milli=id?expansion_labor_milli(w,b):1000;
    out->combat_guard_id=b->combat_guard_id;out->stopped_reason=id?b->blocked_reason:0;
    out->nominal_milli=b->type==CORE_BLD_QUARRY?8000:b->type==CORE_BLD_MINE?24000:b->type==CORE_BLD_MANA_MINE?50000:s->output[CORE_RES_LOGS];
    out->annual_yield=s->annual_yield;
    int nominal_fuel=0;
    (void)expansion_recipe_plan(w,b,1000,1000,0,out->nominal_input,out->nominal_output,&nominal_fuel);
    int factor=id?expansion_production_factor(w,b):0;
    int attendance=s->workers?exp_clamp(b->actual_work_milli/(s->workers*540),0,1000):0;
    int ratio=expansion_recipe_plan(w,b,factor,attendance,1,out->actual_input,out->actual_output,&out->fuel_milli);
    for(int res=0;res<CORE_RES_COUNT;res++){out->actual_input[res]=out->actual_input[res]*ratio/1000;out->actual_output[res]=out->actual_output[res]*ratio/1000;}
    out->fuel_milli=out->fuel_milli*ratio/1000;
    int level=b->type==CORE_BLD_STALL?1:b->type==CORE_BLD_MARKET?2:b->type==CORE_BLD_TRADING_POST?3:0;
    if(level){out->trade_base=(int[]){0,20,80,260}[level];out->trade_limit=expansion_trade_limit(w);out->trade_used=w->expansion->trade_used;out->trade_efficiency=expansion_trade_efficiency(w);out->trade_next_day=w->expansion->market_next_restock_day;}
    int r=s->radius;
    for(int yy=b->y-r;yy<b->y+b->h+r;yy++)for(int xx=b->x-r;xx<b->x+b->w+r;xx++) {
        if(!grid_in_bounds(&w->grid,xx,yy)||expansion_center_distance(b,xx,yy)>r)continue;
        int p=grid_index(&w->grid,xx,yy);const CoreExpansionTileView *t=&w->expansion->tiles[p];
        if(!t->explored)continue;
        if(t->tree_kind && t->tree_age>=core_expansion_tree_mature_age(t->tree_kind) && t->tree_logs_milli>0) {
            out->mature_trees++;if(t->tree_kind==4)out->magic_logs_milli+=t->tree_logs_milli;else out->logs_milli+=t->tree_logs_milli;
        }
        out->deposits[CORE_RES_STONE]+=t->stone_milli;
        if(t->ore_kind>=0&&t->ore_kind<CORE_RES_COUNT)out->deposits[t->ore_kind]+=t->ore_milli;
    }
    return CORE_OK;
}
