/* expansion_v121.c — 중심 좌표·실근무·성별 이름과 공용 수치 계산 */
#include "expansion.h"
#include <string.h>

int expansion_center_distance(const Building *b, int x, int y) {
    int lx = b->x + (b->w - 1) / 2, rx = b->x + b->w / 2;
    int ty = b->y + (b->h - 1) / 2, by = b->y + b->h / 2;
    return (x < lx ? lx - x : x > rx ? x - rx : 0) + (y < ty ? ty - y : y > by ? y - by : 0);
}
int expansion_centers_distance(const Building *a, const Building *b) {
    int x = exp_clamp(a->x + (a->w - 1) / 2, b->x + (b->w - 1) / 2, b->x + b->w / 2);
    int y = exp_clamp(a->y + (a->h - 1) / 2, b->y + (b->h - 1) / 2, b->y + b->h / 2);
    return expansion_center_distance(a, x, y);
}
int expansion_working_present(const World *w, const Building *b, const Villager *v) {
    (void)w;
    if (!b || !v || b->state != CORE_BSTATE_ACTIVE || v->health <= 0 || v->workplace != b ||
        v->incoming ||
        (v->state != CORE_VS_WORKING && v->state != CORE_VS_GUARD_POST))
        return 0;
    CoreWorkSchedule schedule;if(!expansion_schedule(w,v,&schedule))return 0;
    if(v->mission==CORE_MISSION_SCOUT||v->mission==CORE_MISSION_HUNT||v->mission==CORE_MISSION_PATROL||v->mission==CORE_MISSION_COMBAT)return 1;
    if (v->boat_port_id == b->id && b->type == CORE_BLD_PORT)
        return 1;
    if(v->target!=b||!v->at_target)return 0;
    int x = v->current_x_milli / 1000, y = v->current_y_milli / 1000;
    return v->inside_building == b->id || (x == b->door_x && y == b->door_y) ||
           (x >= b->x && x < b->x + b->w && y >= b->y && y < b->y + b->h);
}
int expansion_working_count(const World *w, const Building *b) {
    int result = 0;
    for (int i = 0; i < b->worker_count; i++)
        result += expansion_working_present(w, b, b->workers[i]);
    return result;
}
int expansion_labor_milli(const World *w, const Building *b) {
    int n = w->defs.buildings[b->type].worker_slots;
    return n ? exp_clamp((int)((int64_t)b->daily_work_milli * 1000 / (n * 540000)), 0, 7500)
             : b->type == CORE_BLD_WINDMILL ? 1000 : 0;
}
int expansion_sqrt_milli(int value) {
    int64_t n = (int64_t)exp_clamp(value, 0, 7500) * 1000;
    int low = 0, high = 3000;
    while (low < high) {
        int mid = (low + high + 1) / 2;
        if ((int64_t)mid * mid <= n)
            low = mid;
        else
            high = mid - 1;
    }
    return low;
}
int expansion_event_active(const World *w, int event) {
    if (w->nature.event == event)
        return 1;
    if (!w->expansion)
        return 0;
    if (event == CORE_NATURE_PLAGUE)
        return w->expansion->plague_days > 0;
    return w->expansion->economic_event == event && w->expansion->economic_days > 0;
}
int expansion_agriculture_event(const World *w, int type) {
    int irrigated = type == CORE_BLD_IRRIGATED_FARM;
    if (expansion_event_active(w, CORE_NATURE_STORM))
        return irrigated ? 900 : type == CORE_BLD_GREENHOUSE ? 1000 : 800;
    if (expansion_event_active(w, CORE_NATURE_DROUGHT))
        return irrigated ? (expansion_has_research(w, 2) ? 750 : 650)
                         : type == CORE_BLD_GREENHOUSE ? 850 : 250;
    return expansion_event_active(w, CORE_NATURE_BUMPER) ? 2400 : 1000;
}
int expansion_service_base(const Building *b, int workers) {
    switch (b->type) {
    case CORE_BLD_HEALER: return workers ? 2 : 0;
    case CORE_BLD_CLINIC: return workers >= 2 ? 4 : workers ? 3 : 0;
    case CORE_BLD_HOSPITAL: return workers >= 3 ? 8 : workers == 2 ? 5 : 0;
    case CORE_BLD_TAVERN: return 8;
    case CORE_BLD_BATHHOUSE: return 12;
    case CORE_BLD_THEATRE: return 15;
    case CORE_BLD_ART_MUSEUM:
        return workers >= 2 ? 30 : workers ? 15 : 0;
    case CORE_BLD_CHURCH: return 12;
    case CORE_BLD_SCHOOL: return 8;
    case CORE_BLD_LIBRARY: return 6;
    case CORE_BLD_ACADEMY: return 12;
    default: return 0;
    }
}
int expansion_service_shift(int type) {
    return type == CORE_BLD_TAVERN || type == CORE_BLD_BATHHOUSE || type == CORE_BLD_THEATRE ||
           type == CORE_BLD_CHURCH || type == CORE_BLD_SCHOOL || type == CORE_BLD_LIBRARY ||
           type == CORE_BLD_ACADEMY || type == CORE_BLD_ART_MUSEUM;
}
int expansion_begin_work(World *w, Villager *v, Building *b) {
    if (v->work_day != exp_day(w)) {
        v->work_day = exp_day(w);
        v->work_day_minutes = 0;
    }
    int minutes = exp_clamp(540 - v->work_day_minutes, 0, 10);
    int effective = expansion_work_percent(w, v, b->type) * minutes * 10;
    if (b->type == CORE_BLD_OBSERVATORY && v->stats.intellect < 5)
        effective = 0;
    int n = w->defs.buildings[b->type].worker_slots;
    if (!minutes || !n)
        return 0;
    int previous_minutes=v->work_day_minutes;
    v->work_day_minutes += minutes;
    if(previous_minutes/180<v->work_day_minutes/180 && v->work_day_minutes<=360)expansion_mood_event(w,v,7,1,b->id,1,2);
    b->actual_work_milli += minutes * 1000;
    b->daily_work_milli += effective;
    if (b->type == CORE_BLD_OBSERVATORY)
        b->research_work_milli += effective;
    if (b->type == CORE_BLD_OBSERVATORY && effective > 0) expansion_forecast_refresh(w,b);
    if (expansion_spec(b->type)->field == 0) {
        int event = expansion_agriculture_event(w, b->type);
        int credit = (int)((int64_t)effective * event / 1000);
        b->agriculture_work_milli += credit;
        w->expansion->farm_minutes += minutes;
        if (exp_season(w) < 2 && expansion_spec(b->type)->annual_yield) {
            int64_t amount=(int64_t)effective*event*(expansion_has_research(w,17)?108:100)+b->crop_work_carry;
            b->crop_labor_milli += (int)(amount/100000);
            b->crop_work_carry=(int)(amount%100000);
        }
    }
    int service_n = expansion_working_count(w,b);
    int medical=expansion_is_medical(b->type),highest=0,sum=0;
    if (medical) {
        service_n=expansion_working_count(w,b);
        for(int i=0;i<b->worker_count;i++)if(expansion_working_present(w,b,b->workers[i])) {
            int value=b->workers[i]->stats.intellect;sum+=value;if(value>highest)highest=value;
        }
    }
    int base = expansion_service_base(b, service_n);
    if(medical && (b->type==CORE_BLD_HEALER?highest<4:b->type==CORE_BLD_CLINIC?highest<5||sum<7:service_n<2||highest<6||sum<10))base=0;
    int education=b->type==CORE_BLD_SCHOOL || b->type==CORE_BLD_LIBRARY || b->type==CORE_BLD_ACADEMY;
    int teaching=education && b->service_credit_milli<base*1000;
    if(education)for(int i=0;i<w->villager_count;i++)
        if(w->villagers[i]->leisure_building==b->id && w->villagers[i]->at_target)teaching=1;
    if(education && !teaching)b->research_work_milli+=effective;
    if (base && service_n > 0 && (!education || teaching) && expansion_qualified_present(w, b)) {
        int64_t numerator = (int64_t)effective * base * (2520 / service_n);
        if (expansion_is_medical(b->type) && expansion_has_research(w, 18))
            numerator = numerator * 125 / 100;
        numerator += b->service_carry;
        int denominator = b->type == CORE_BLD_ART_MUSEUM ? 1058400 : 1360800;
        b->service_credit_milli += (int)(numerator / denominator);
        if (b->type == CORE_BLD_ART_MUSEUM)
            b->service_credit_milli =
                exp_clamp(b->service_credit_milli, 0, exp_clamp(45000 - b->visits_today * 1000, 0, 45000));
        b->service_carry = (int)(numerator % denominator);
    }
    int level = b->type == CORE_BLD_STALL ? 1 : b->type == CORE_BLD_MARKET ? 2
                                              : b->type == CORE_BLD_TRADING_POST ? 3 : 0;
    if (level) {
        w->expansion->market_staff_efficiency=expansion_trade_efficiency(w);
        w->expansion->market_staff_day=exp_day(w);
        int cap = (int[]){0, 20, 80, 260}[level];
        int64_t numerator = (int64_t)effective * cap * (2520 / n) + w->expansion->trade_capacity_carry;
        w->expansion->trade_capacity_milli = exp_clamp(w->expansion->trade_capacity_milli +
                                                     (int)(numerator / 1360800), 0, cap * 1000);
        w->expansion->trade_capacity_carry = (int)(numerator % 1360800);
    }
    return minutes;
}
int expansion_trade_efficiency(const World *w) {
    int level=expansion_market_level(w),total=0,count=0;
    if(!level)return 0;
    for(int i=0;i<w->building_count;i++) {
        const Building *b=w->buildings[i];
        int n=b->type==CORE_BLD_STALL?1:b->type==CORE_BLD_MARKET?2:b->type==CORE_BLD_TRADING_POST?3:0;
        if(n!=level||b->state!=CORE_BSTATE_ACTIVE)continue;
        for(int j=0;j<b->worker_count;j++)if(expansion_working_present(w,b,b->workers[j])) {
            total+=expansion_work_percent(w,b->workers[j],b->type);count++;
        }
    }
    if(!count)return 0;
    return total/count;
}
int expansion_trade_limit(const World *w) {
    int average=expansion_trade_efficiency(w),level=expansion_market_level(w);
    if(!average || !level)return 0;
    int base=(int[]){0,20,80,260}[level];
    return base+base*exp_clamp(average-100,0,200)/800-base*exp_clamp(100-average,0,75)/750;
}

void expansion_choose_identity(World *w, Villager *v) {
    const char *entry = NULL, *name = NULL;
    int sex = 0;
    int start=rng_range(&w->rng,0,w->defs.name_count-1);
    for (int attempt = 0; attempt < w->defs.name_count; attempt++) {
        entry = w->defs.names[(start+attempt)%w->defs.name_count].text;
        sex = entry[0] == 'F' && entry[1] == ':';
        name = ((entry[0] == 'M' || entry[0] == 'F') && entry[1] == ':') ? entry + 2 : entry;
        int in_use = 0;
        for (int i = 0; i < w->villager_count; i++)
            in_use |= w->villagers[i] != v && strcmp(w->villagers[i]->name, name) == 0;
        if (!in_use)
            break;
    }
    if (!name)
        name = u8"주민";
    strcpy(v->name, name);
    v->sex = sex;
    v->appearance = rng_range(&w->rng, 0, 23) * 2 + sex;
    for(int attempt=0;attempt<4;attempt++) {
        int recent=0,checked=0;
        for(int i=w->villager_count-1;i>=0 && checked<8;i--)if(w->villagers[i]!=v&&w->villagers[i]->sex==sex) {
            checked++;recent|=w->villagers[i]->appearance==v->appearance;
        }
        if(!recent||rng_range(&w->rng,0,3)==0)break;
        v->appearance=rng_range(&w->rng,0,23)*2+sex;
    }
}
CORE_API int32_t core_expansion_events(const CoreWorld *w,CoreEventLayers *out) {
    if(!w || !out)return CORE_ERR_NULL_ARG;
    if(!w->expansion)return CORE_ERR_INVALID_ARG;
    *out=(CoreEventLayers){w->expansion->plague_days,w->expansion->economic_event,w->expansion->economic_days};
    return CORE_OK;
}
CORE_API int32_t core_expansion_efficiency(const CoreWorld *w,int32_t id,int32_t type,int32_t *out) {
    if(!w || !out)return CORE_ERR_NULL_ARG;
    if(!w->expansion || !expansion_spec(type))return CORE_ERR_INVALID_ARG;
    Villager *v=entity_find_villager(w,id);if(!v)return CORE_ERR_NOT_FOUND;
    *out=expansion_work_percent(w,v,type);return CORE_OK;
}

CORE_API int32_t core_expansion_profession_efficiency(const CoreWorld *w,int32_t id,int32_t field,int32_t *out) {
    if(!w||!out)return CORE_ERR_NULL_ARG;
    if(!w->expansion||field<0||field>=12)return CORE_ERR_INVALID_ARG;
    const Villager *v=entity_find_villager(w,id);if(!v)return CORE_ERR_NOT_FOUND;
    static const int types[12]={CORE_BLD_FARM,CORE_BLD_LUMBER,CORE_BLD_MINE,CORE_BLD_RANCH,CORE_BLD_PORT,CORE_BLD_HUNTER_CAMP,CORE_BLD_SAWMILL,CORE_BLD_BARRACKS,CORE_BLD_HEALER,CORE_BLD_MARKET,CORE_BLD_LIBRARY,CORE_BLD_MAGE_TOWER};
    int type=v->workplace && expansion_spec(v->workplace->type)->field==field ? v->workplace->type : types[field];
    *out=expansion_work_percent(w,v,type);return CORE_OK;
}
CORE_API int32_t core_expansion_daily_requirements(const CoreWorld *w,int32_t out[CORE_RES_COUNT]) {
    if(!w||!out)return CORE_ERR_NULL_ARG;
    if(!w->expansion)return CORE_ERR_INVALID_ARG;
    memset(out,0,CORE_RES_COUNT*sizeof(*out));int fuel=0;
    for(int i=0;i<w->villager_count;i++) {
        const Villager *v=w->villagers[i];if(v->incoming||v->state==CORE_VS_LEAVING)continue;
        out[CORE_RES_FOOD]+=1000;
        if(v->workplace && !v->contract_end_tick)out[CORE_RES_GOLD]+=w->defs.buildings[v->workplace->type].wage*1000;
    }
    for(int i=0;i<w->expansion->animal_count;i++) {
        const CoreAnimalView *a=&w->expansion->animals[i];if(!a->alive||!a->owner)continue;
        out[CORE_RES_FOOD]+=expansion_animal_feed(w,a,entity_find_building(w,a->owner));
    }
    for(int i=0;i<w->building_count;i++) {
        const Building *b=w->buildings[i];if(b->state!=CORE_BSTATE_ACTIVE)continue;
        const ExpansionBuildingSpec *spec=expansion_spec(b->type);
        if(!(expansion_is_medical(b->type)&&!b->operating))out[CORE_RES_GOLD]+=spec->upkeep*1000;
        if(exp_season(w)==3 && spec->heating_milli && (!spec->residents||b->resident_count)) {
            int need=spec->heating_milli*(expansion_has_research(w,19)?80:100)/100;
            if(expansion_event_active(w,CORE_NATURE_COLD_SNAP))need=need*130/100;
            fuel+=need;
        }
    }
    fuel=exp_clamp(fuel-w->expansion->fuel_milli,0,INT32_MAX);
    int coal=exp_clamp((fuel+1999)/2000,0,w->bank.amount[CORE_RES_COAL]);
    out[CORE_RES_COAL]=coal*1000;fuel=exp_clamp(fuel-coal*2000,0,INT32_MAX);
    int logs=exp_clamp((fuel+999)/1000,0,w->bank.amount[CORE_RES_LOGS]);
    out[CORE_RES_LOGS]=logs*1000;fuel=exp_clamp(fuel-logs*1000,0,INT32_MAX);
    out[CORE_RES_WOOD]=((fuel+999)/1000)*1000;
    return CORE_OK;
}
