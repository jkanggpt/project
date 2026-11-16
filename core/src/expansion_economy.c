/* expansion_economy.c — 실제 노동, 공유 자연자원, 연료와 가을 일괄 수확 */
#include "expansion.h"
#include <stdlib.h>

static int labor(const World *w,const Building *b)
{
    int slots=w->defs.buildings[b->type].worker_slots;
    return slots?exp_clamp((int)((int64_t)b->daily_work_milli*1000/(slots*540000)),0,4000):1000;
}
static int logistics(const World *w,const Building *b)
{
    const ExpansionState *e=w->expansion;int p=grid_index(&w->grid,b->door_x,b->door_y),d=e->distance[p],value,i;
    if(expansion_spec(b->type)->rank==0 && d<0)return 100;
    if(d<0)return 0;value=d<=20?100:d<=50?95:d<=80?85:70;
    if(expansion_has_research(w,3))value+=15;
    for(i=0;i<w->building_count;i++){
        const Building *store=w->buildings[i];
        if(store->state==CORE_BSTATE_ACTIVE && store->type==CORE_BLD_LOGISTICS && e->connected[grid_index(&w->grid,store->door_x,store->door_y)] && exp_dist(store->door_x,store->door_y,b->door_x,b->door_y)<=15){value+=20;break;}
    }
    return value>100?100:value;
}
static int local_bonus(const World *w,const Building *b)
{
    int i,bonus=100,wind=0,magic=0;
    for(i=0;i<w->building_count;i++){
        const Building *a=w->buildings[i];
        if(a->state!=CORE_BSTATE_ACTIVE || !a->worker_count)continue;
        if(a->type==CORE_BLD_WINDMILL && expansion_spec(b->type)->field==0 && exp_dist(a->x,a->y,b->x,b->y)<=10)wind=40;
        if(a->type==CORE_BLD_MAGIC_WORKSHOP && a->operating && exp_dist(a->x,a->y,b->x,b->y)<=8)magic=10+(expansion_has_research(w,17)?5:0);
    }
    bonus+=wind+magic;return bonus;
}
static int room(const World *w,int resource)
{
    int64_t value=(int64_t)eco_bank_limit(&w->bank,resource)*1000-expansion_bank_have(w,resource);
    return value>INT32_MAX?INT32_MAX:value<0?0:(int)value;
}
static void logging(World *w,Building *b,int factor)
{
    static const int ages[4]={28000,48000,84000,112000},caps[4]={600,800,1000,400};
    int budget=(int)((int64_t)expansion_spec(b->type)->output[CORE_RES_LOGS]*factor/1000),i,r=expansion_spec(b->type)->radius;
    if(budget>room(w,CORE_RES_LOGS))budget=room(w,CORE_RES_LOGS);
    /* 건물 ID 순으로 호출하며 모든 벌목 시설이 나무별 하루 한도를 공유합니다. */
    for(i=0;i<w->expansion->cells && budget>0;i++){
        CoreExpansionTileView *t=&w->expansion->tiles[i];int take,kind=t->tree_kind;
        if(kind<1 || kind>4 || t->tree_age<ages[kind-1] || exp_dist(i%w->grid.width,i/w->grid.width,b->x+b->w/2,b->y+b->h/2)>r)continue;
        take=caps[kind-1]-t->tree_used_milli;if(take>t->tree_logs_milli)take=t->tree_logs_milli;if(take>budget)take=budget;if(take<=0)continue;
        take=expansion_bank_add(w,CORE_RES_LOGS,take,CORE_LEDGER_PRODUCTION);budget-=take;t->tree_logs_milli-=take;t->tree_used_milli+=take;
        if(kind==4)w->expansion->magic_logs_milli+=take;
        if(!t->tree_logs_milli){
            static const int lo[4]={8,12,20,35},hi[4]={16,24,35,60};
            t->regrow_day=exp_day(w)+rng_range(&w->rng,lo[kind-1],hi[kind-1]);t->tree_age=0;
            w->grid.terrain[i]=CORE_TERRAIN_GRASS;
            if(kind==4)(void)expansion_bank_add(w,CORE_RES_HERBS,1000,CORE_LEDGER_PRODUCTION);
        }
    }
}
static int ore_value(int res)
{
    return res==CORE_RES_STONE?1:res==CORE_RES_COAL?2:res==CORE_RES_IRON_ORE?3:res==CORE_RES_GOLD_ORE?8:res==CORE_RES_MANA?10:0;
}
static int mine_accept(int type,int res)
{
    return type==CORE_BLD_QUARRY?res==CORE_RES_STONE:type==CORE_BLD_MINE?(res==CORE_RES_STONE || res==CORE_RES_COAL || res==CORE_RES_IRON_ORE):ore_value(res)>0;
}
static void mining(World *w,Building *b,int factor)
{
    int i,r=expansion_spec(b->type)->radius,budget=(b->type==CORE_BLD_QUARRY?8000:b->type==CORE_BLD_MINE?24000:50000)*factor/1000;int64_t weight=0;
    for(i=0;i<w->expansion->cells;i++){
        CoreExpansionTileView *t=&w->expansion->tiles[i];if(t->ore_milli>0 && mine_accept(b->type,t->ore_kind) && exp_dist(i%w->grid.width,i/w->grid.width,b->x,b->y)<=r)weight+=(int64_t)t->ore_milli*ore_value(t->ore_kind);
    }
    if(!weight)return;
    for(i=0;i<w->expansion->cells;i++){
        CoreExpansionTileView *t=&w->expansion->tiles[i];int take;
        if(t->ore_milli<=0 || !mine_accept(b->type,t->ore_kind) || exp_dist(i%w->grid.width,i/w->grid.width,b->x,b->y)>r)continue;
        take=(int)((int64_t)budget*t->ore_milli/weight);if(take>t->ore_milli)take=t->ore_milli;
        take=expansion_bank_add(w,t->ore_kind,take,CORE_LEDGER_PRODUCTION);t->ore_milli-=take;
    }
}
static int water_for(const World *w,const Building *b,int radius)
{
    int x,y,best=0,area=0;
    for(y=b->y-radius;y<b->y+b->h+radius;y++)for(x=b->x-radius;x<b->x+b->w+radius;x++)if(grid_in_bounds(&w->grid,x,y)){
        int id=w->expansion->tiles[grid_index(&w->grid,x,y)].water_id;
        if(id && w->expansion->water_area[id-1]>area){area=w->expansion->water_area[id-1];best=id;}
    }return best;
}
static int fish_request(const World *w,const Building *b,int *water)
{
    int x,y,near=0,desire;
    *water=water_for(w,b,b->type==CORE_BLD_PORT?2:5);if(!*water)return 0;
    for(y=b->y-5;y<=b->y+5;y++)for(x=b->x-5;x<=b->x+5;x++)if(grid_in_bounds(&w->grid,x,y) && w->expansion->tiles[grid_index(&w->grid,x,y)].water_id==*water)near++;
    desire=b->type==CORE_BLD_PORT?exp_clamp(3000+w->expansion->water_area[*water-1]*120,0,15000):exp_clamp(near*250,0,5000);
    return desire*labor(w,b)/1000*logistics(w,b)/100;
}
static void fishing(World *w)
{
    int i,j;
    for(i=0;i<w->building_count;i++){
        Building *b=w->buildings[i];int water,request,total=0,cap;
        if(b->state!=CORE_BSTATE_ACTIVE || (b->type!=CORE_BLD_FISHING_SPOT && b->type!=CORE_BLD_PORT))continue;
        request=fish_request(w,b,&water);if(!request)continue;
        for(j=0;j<w->building_count;j++){Building *other=w->buildings[j];int id,n;if(other->state!=CORE_BSTATE_ACTIVE || (other->type!=CORE_BLD_FISHING_SPOT && other->type!=CORE_BLD_PORT))continue;n=fish_request(w,other,&id);if(id==water)total+=n;}
        cap=exp_clamp(3000+w->expansion->water_area[water-1]*120,8000,70000);cap=cap*(exp_season(w)==1?110:exp_season(w)==3?75:100)/100;
        if(total>cap)request=(int)((int64_t)request*cap/total);
        (void)expansion_bank_add(w,CORE_RES_FOOD,request,CORE_LEDGER_PRODUCTION);
    }
}
static void recipe(World *w,Building *b,int factor)
{
    const ExpansionBuildingSpec *s=expansion_spec(b->type);int in[CORE_RES_COUNT],out[CORE_RES_COUNT],fuel=s->fuel,r,ratio=factor;
    for(r=0;r<CORE_RES_COUNT;r++){in[r]=s->input[r];out[r]=s->output[r];}
    if(b->type==CORE_BLD_SMELTER && b->mode){in[CORE_RES_IRON_ORE]=0;in[CORE_RES_GOLD_ORE]=4000;out[CORE_RES_IRON]=0;out[CORE_RES_GOLD_BAR]=2000;}
    if(b->type==CORE_BLD_SMELTER && !b->mode && expansion_has_research(w,5))out[CORE_RES_IRON]=5000;
    if(b->type==CORE_BLD_GARDEN && exp_season(w)==3)out[CORE_RES_FOOD]/=2;
    if(b->type==CORE_BLD_ORCHARD && (exp_day(w)-b->completed_day<24 || (exp_season(w)!=1 && exp_season(w)!=2)))return;
    if(b->type==CORE_BLD_GREENHOUSE && exp_season(w)==3)fuel=1000;
    if(expansion_has_research(w,22))for(r=0;r<CORE_RES_COUNT;r++)if(r==CORE_RES_MANA)in[r]=in[r]*90/100;
    for(r=0;r<CORE_RES_COUNT;r++){
        if(in[r]>0 && (int64_t)in[r]*ratio/1000>expansion_bank_have(w,r))ratio=expansion_bank_have(w,r)*1000/in[r];
        if(out[r]>0 && (int64_t)out[r]*ratio/1000>room(w,r))ratio=room(w,r)*1000/out[r];
    }
    if(fuel && (int64_t)fuel*ratio/1000>expansion_fuel(w,0,0))ratio=expansion_fuel(w,0,0)*1000/fuel;
    if(ratio<=0){b->operating=0;b->blocked_reason=CORE_DENY_NO_RESOURCES;return;}
    b->operating=1;
    for(r=0;r<CORE_RES_COUNT;r++)if(in[r]){
        int amount=in[r]*ratio/1000;
        (void)expansion_bank_add(w,r,-amount,CORE_LEDGER_PRODUCTION);
        if(r==CORE_RES_LOGS && b->type==CORE_BLD_SAWMILL){int magic=amount<w->expansion->magic_logs_milli?amount:w->expansion->magic_logs_milli;w->expansion->magic_logs_milli-=magic;b->processing_milli[CORE_RES_MANA]+=magic;while(b->processing_milli[CORE_RES_MANA]>=10000){b->processing_milli[CORE_RES_MANA]-=10000;(void)expansion_bank_add(w,CORE_RES_MANA,1000,CORE_LEDGER_PRODUCTION);}}
    }
    if(fuel)(void)expansion_fuel(w,fuel*ratio/1000,1);
    for(r=0;r<CORE_RES_COUNT;r++)if(out[r])(void)expansion_bank_add(w,r,out[r]*ratio/1000,CORE_LEDGER_PRODUCTION);
    if(s->research && w->research.active>=0){
        int points=s->research*ratio/1000;const ResearchDef *research=&w->defs.research[w->research.active];
        if(b->type==CORE_BLD_MAGE_TOWER && expansion_has_research(w,20))points=points*120/100;
        if(b->type==CORE_BLD_GRAND_MAGE_TOWER && expansion_has_research(w,23))points=points*120/100;
        w->research.progress_milli+=points;
        if(w->research.progress_milli>=research->points*1000){int index=w->research.active;w->research.done|=1u<<index;w->research.done_count++;w->research.active=-1;w->research.progress_milli=0;expansion_apply_research(w);events_push_progress(&w->events,CORE_EVT_RESEARCH_DONE,index,w->research.done_count,0,NULL);}
    }
}
void expansion_produce(World *w)
{
    int i;
    for(i=0;i<w->building_count;i++){
        Building *b=w->buildings[i];const ExpansionBuildingSpec *s=expansion_spec(b->type);int64_t amount,baseline;
        if(!s->annual_yield || b->crop_year==exp_year(w)+1)continue;
        baseline=(int64_t)s->workers*540*24*1000;
        amount=(int64_t)s->annual_yield*1000*exp_clamp((int)((int64_t)b->crop_labor_milli*1000/baseline),0,2500)/1000;
        if(b->crop_drought)amount=amount*(b->type==CORE_BLD_IRRIGATED_FARM?(expansion_has_research(w,2)?95:90):55)/100;
        amount=amount*local_bonus(w,b)/100;if(expansion_has_research(w,0))amount=amount*112/100;
        if(expansion_has_research(w,4) && b->type!=CORE_BLD_IRRIGATED_FARM)amount=amount*115/100;
        b->pending_harvest_milli+=(int)amount;b->harvest_expiry=exp_day(w)+4;b->crop_labor_milli=0;b->crop_drought=0;b->crop_year=exp_year(w)+1;
    }
}
static void release_harvest(World *w)
{
    int i;for(i=0;i<w->building_count;i++){Building *b=w->buildings[i];if(!b->pending_harvest_milli)continue;if(exp_day(w)>=b->harvest_expiry){b->pending_harvest_milli=0;continue;}b->pending_harvest_milli-=expansion_bank_add(w,CORE_RES_FOOD,b->pending_harvest_milli,CORE_LEDGER_PRODUCTION);}
}
void expansion_daily(World *w)
{
    int id,i;release_harvest(w);w->expansion->trade_used=0;
    /* 사료는 당일 생산 전에 차감해 같은 날의 축산물로 사료를 복제하지 않습니다. */
    expansion_animal_daily(w);
    for(id=1;id<w->next_building_id;id++){
        Building *b=entity_find_building(w,id);const ExpansionBuildingSpec *s;int factor;
        if(!b || b->state!=CORE_BSTATE_ACTIVE)continue;s=expansion_spec(b->type);b->blocked_reason=0;b->operating=expansion_qualified(w,b);
        if(s->upkeep){if(w->bank.amount[CORE_RES_GOLD]>=s->upkeep)(void)expansion_bank_add(w,CORE_RES_GOLD,-s->upkeep*1000,CORE_LEDGER_WAGES);else{b->blocked_reason=CORE_DENY_NO_GOLD;b->operating=0;}}
        b->flags&=~CORE_BF_COLD;
        if(exp_season(w)==3 && s->heating_milli){int fuel=s->heating_milli*(expansion_has_research(w,19)?80:100)/100;if(expansion_fuel(w,0,0)<fuel)b->flags|=CORE_BF_COLD;else(void)expansion_fuel(w,fuel,1);}
        factor=labor(w,b)*logistics(w,b)/100;
        if(!b->operating)factor=0;
        if(s->field!=0)factor=factor*local_bonus(w,b)/100;
        if(factor>0){
            if(b->type==CORE_BLD_LOGGING_SITE || b->type==CORE_BLD_LUMBER)logging(w,b,factor);
            else if(s->field==2)mining(w,b,factor);
            else if(b->type==CORE_BLD_GATHERER){b->hunts_milli+=4*factor;while(b->hunts_milli>=1000){int roll=rng_range(&w->rng,0,99),swamp=w->expansion->tiles[grid_index(&w->grid,b->x,b->y)].biome==4,res=swamp?(roll<20?CORE_RES_FOOD:roll<60?CORE_RES_HERBS:CORE_RES_CLAY):(roll<60?CORE_RES_FOOD:CORE_RES_HERBS);b->hunts_milli-=1000;(void)expansion_bank_add(w,res,1000,CORE_LEDGER_PRODUCTION);}}
            else if(s->field==5){b->hunts_milli+=factor;for(i=0;i<w->expansion->animal_count && b->hunts_milli>=1000;i++){
                CoreAnimalView *a=&w->expansion->animals[i];if(a->alive && !a->owner && exp_dist(a->x_milli/1000,a->y_milli/1000,b->x,b->y)<=s->radius){static const int meat[16]={2,10,7,2,3,7,9,30,12,18,2,5,20,10,22,1},leather[16]={0,2,2,0,1,2,3,6,3,3,0,2,6,4,5,0};a->alive=0;b->hunts_milli-=1000;(void)expansion_bank_add(w,CORE_RES_FOOD,meat[a->kind]*1000,CORE_LEDGER_PRODUCTION);(void)expansion_bank_add(w,CORE_RES_LEATHER,leather[a->kind]*1000,CORE_LEDGER_PRODUCTION);}
            }}
            else if(b->type!=CORE_BLD_FISHING_SPOT && b->type!=CORE_BLD_PORT && !s->annual_yield)recipe(w,b,factor);
        }
    }
    fishing(w);
    for(i=0;i<w->building_count;i++)w->buildings[i]->daily_work_milli=0;
    release_harvest(w);
}
void expansion_growth(World *w)
{
    static const int ages[4]={28000,48000,84000,112000},logs[4]={12000,28000,50000,24000};int i,j;
    for(i=0;i<w->expansion->cells;i++){
        CoreExpansionTileView *t=&w->expansion->tiles[i];int boost=1000;
        if(t->tree_kind<1 || t->tree_kind>4)continue;t->tree_used_milli=0;
        for(j=0;j<w->building_count;j++){Building *b=w->buildings[j];if(b->type==CORE_BLD_FORESTRY && b->state==CORE_BSTATE_ACTIVE && b->worker_count && exp_dist(i%w->grid.width,i/w->grid.width,b->x,b->y)<=8){boost=1800;break;}}
        if(t->regrow_day){if(exp_day(w)<t->regrow_day)continue;t->regrow_day=0;t->tree_age=1000;t->tree_logs_milli=logs[t->tree_kind-1];w->grid.terrain[i]=CORE_TERRAIN_FOREST;}
        else t->tree_age=exp_clamp(t->tree_age+boost,0,ages[t->tree_kind-1]);
    }
    for(j=0;j<w->building_count;j++){
        Building *b=w->buildings[j];int planted=0;
        if(b->type!=CORE_BLD_FORESTRY || b->state!=CORE_BSTATE_ACTIVE || !b->worker_count)continue;
        for(i=0;i<w->expansion->cells && planted<3;i++){
            CoreExpansionTileView *t=&w->expansion->tiles[i];
            if(!t->tree_kind && w->grid.terrain[i]==CORE_TERRAIN_GRASS && !w->grid.occupant[i] && !w->grid.road[i] && exp_dist(i%w->grid.width,i/w->grid.width,b->x,b->y)<=8){t->tree_kind=2;t->tree_age=1000;t->tree_logs_milli=28000;w->grid.terrain[i]=CORE_TERRAIN_FOREST;planted++;}
        }
    }
    grid_rebuild_costs(&w->grid);
}
