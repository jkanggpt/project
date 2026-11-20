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
        v->target != b || !v->at_target || v->incoming ||
        (v->state != CORE_VS_WORKING && v->state != CORE_VS_GUARD_POST))
        return 0;
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
    case CORE_BLD_HOSPITAL: return workers >= 3 ? 8 : workers == 2 ? 5 : 0;
    case CORE_BLD_TAVERN: return 8;
    case CORE_BLD_BATHHOUSE: return 12;
    case CORE_BLD_THEATRE: return 15;
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
           type == CORE_BLD_ACADEMY;
}
int expansion_begin_work(World *w, Villager *v, Building *b) {
    if (v->work_day != exp_day(w)) {
        v->work_day = exp_day(w);
        v->work_day_minutes = 0;
    }
    int minutes = exp_clamp(540 - v->work_day_minutes, 0, 10);
    int effective = expansion_work_percent(w, v, b->type) * minutes * 10;
    int n = w->defs.buildings[b->type].worker_slots;
    if (!minutes || !n)
        return 0;
    v->work_day_minutes += minutes;
    b->actual_work_milli += minutes * 1000;
    b->daily_work_milli += effective;
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
    int service_n = n;
    int medical=b->type==CORE_BLD_HEALER || b->type==CORE_BLD_HOSPITAL,highest=0,sum=0;
    if (medical) {
        service_n=expansion_working_count(w,b);
        for(int i=0;i<b->worker_count;i++)if(expansion_working_present(w,b,b->workers[i])) {
            int value=b->workers[i]->stats.intellect;sum+=value;if(value>highest)highest=value;
        }
    }
    int base = expansion_service_base(b, service_n);
    if(medical && (b->type==CORE_BLD_HEALER?highest<4:service_n<2||highest<6||sum<10))base=0;
    if (base && service_n > 0 && expansion_qualified(w, b)) {
        int64_t numerator = (int64_t)effective * base * (2520 / service_n);
        if ((b->type == CORE_BLD_HEALER || b->type == CORE_BLD_HOSPITAL) && expansion_has_research(w, 18))
            numerator = numerator * 125 / 100;
        numerator += b->service_carry;
        b->service_credit_milli += (int)(numerator / 1360800);
        b->service_carry = (int)(numerator % 1360800);
    }
    int level = b->type == CORE_BLD_STALL ? 1 : b->type == CORE_BLD_MARKET ? 2
                                              : b->type == CORE_BLD_TRADING_POST ? 3 : 0;
    if (level) {
        int cap = (int[]){0, 20, 80, 260}[level];
        int64_t numerator = (int64_t)effective * cap * (2520 / n) + w->expansion->trade_capacity_carry;
        w->expansion->trade_capacity_milli = exp_clamp(w->expansion->trade_capacity_milli +
                                                     (int)(numerator / 1360800), 0, cap * 1000);
        w->expansion->trade_capacity_carry = (int)(numerator % 1360800);
    }
    return minutes;
}
int expansion_trade_limit(const World *w) {
    int level = expansion_market_level(w);
    if (!level)
        return 0;
    return exp_clamp(w->expansion->trade_capacity_milli / 1000, 0, (int[]){0, 20, 80, 260}[level]);
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
    v->appearance = rng_range(&w->rng, 0, 5) * 2 + sex;
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
