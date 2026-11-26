/* work — 현재 기능의 단일 구현 */
#include "expansion.h"
#include "placement.h"
#include "rank.h"
#include "villager_ai.h"
#include <stdlib.h>
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
    if (!b || !v || b->state != CORE_BSTATE_ACTIVE || v->health <= 0 || v->workplace != b || v->incoming ||
        (v->state != CORE_VS_WORKING && v->state != CORE_VS_GUARD_POST))
        return 0;
    CoreWorkSchedule schedule;
    if (!expansion_schedule(w, v, &schedule))
        return 0;
    if (v->mission == CORE_MISSION_SCOUT || v->mission == CORE_MISSION_HUNT ||
        v->mission == CORE_MISSION_PATROL || v->mission == CORE_MISSION_COMBAT)
        return 1;
    if (v->boat_port_id == b->id && b->type == CORE_BLD_PORT)
        return 1;
    if (v->target != b || !v->at_target)
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
           : b->type == CORE_BLD_WINDMILL ? 1000
                                          : 0;
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
    int previous_minutes = v->work_day_minutes;
    v->work_day_minutes += minutes;
    if (previous_minutes / 180 < v->work_day_minutes / 180 && v->work_day_minutes <= 360)
        expansion_mood_event(w, v, 7, 1, b->id, 1, 2);
    b->actual_work_milli += minutes * 1000;
    b->daily_work_milli += effective;
    if (b->type == CORE_BLD_OBSERVATORY)
        b->research_work_milli += effective;
    if (b->type == CORE_BLD_OBSERVATORY && effective > 0)
        expansion_forecast_refresh(w, b);
    if (expansion_spec(b->type)->field == 0) {
        int event = expansion_agriculture_event(w, b->type);
        int credit = (int)((int64_t)effective * event / 1000);
        b->agriculture_work_milli += credit;
        w->expansion->farm_minutes += minutes;
        if (exp_season(w) < 2 && expansion_spec(b->type)->annual_yield) {
            int64_t amount =
                (int64_t)effective * event * (expansion_has_research(w, 17) ? 108 : 100) + b->crop_work_carry;
            b->crop_labor_milli += (int)(amount / 100000);
            b->crop_work_carry = (int)(amount % 100000);
        }
    }
    int service_n = expansion_working_count(w, b);
    int medical = expansion_is_medical(b->type), highest = 0, sum = 0;
    if (medical) {
        service_n = expansion_working_count(w, b);
        for (int i = 0; i < b->worker_count; i++)
            if (expansion_working_present(w, b, b->workers[i])) {
                int value = b->workers[i]->stats.intellect;
                sum += value;
                if (value > highest)
                    highest = value;
            }
    }
    int base = expansion_service_base(b, service_n);
    if (medical && (b->type == CORE_BLD_HEALER   ? highest < 4
                    : b->type == CORE_BLD_CLINIC ? highest < 5 || sum < 7
                                                 : service_n < 2 || highest < 6 || sum < 10))
        base = 0;
    int education = b->type == CORE_BLD_SCHOOL || b->type == CORE_BLD_LIBRARY || b->type == CORE_BLD_ACADEMY;
    int teaching = education && b->service_credit_milli < base * 1000;
    if (education)
        for (int i = 0; i < w->villager_count; i++)
            if (w->villagers[i]->leisure_building == b->id && w->villagers[i]->at_target)
                teaching = 1;
    if (education && !teaching)
        b->research_work_milli += effective;
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
    int level = b->type == CORE_BLD_STALL          ? 1
                : b->type == CORE_BLD_MARKET       ? 2
                : b->type == CORE_BLD_TRADING_POST ? 3
                                                   : 0;
    if (level) {
        w->expansion->market_staff_efficiency = expansion_trade_efficiency(w);
        w->expansion->market_staff_day = exp_day(w);
        int cap = (int[]){0, 20, 80, 260}[level];
        int64_t numerator = (int64_t)effective * cap * (2520 / n) + w->expansion->trade_capacity_carry;
        w->expansion->trade_capacity_milli =
            exp_clamp(w->expansion->trade_capacity_milli + (int)(numerator / 1360800), 0, cap * 1000);
        w->expansion->trade_capacity_carry = (int)(numerator % 1360800);
    }
    return minutes;
}

CORE_API int32_t core_expansion_efficiency(const CoreWorld *w, int32_t id, int32_t type, int32_t *out) {
    if (!w || !out)
        return CORE_ERR_NULL_ARG;
    if (!w->expansion || !expansion_spec(type))
        return CORE_ERR_INVALID_ARG;
    Villager *v = entity_find_villager(w, id);
    if (!v)
        return CORE_ERR_NOT_FOUND;
    *out = expansion_work_percent(w, v, type);
    return CORE_OK;
}

CORE_API int32_t core_expansion_profession_efficiency(const CoreWorld *w, int32_t id, int32_t field,
                                                      int32_t *out) {
    if (!w || !out)
        return CORE_ERR_NULL_ARG;
    if (!w->expansion || field < 0 || field >= 12)
        return CORE_ERR_INVALID_ARG;
    const Villager *v = entity_find_villager(w, id);
    if (!v)
        return CORE_ERR_NOT_FOUND;
    static const int types[12] = {CORE_BLD_FARM,    CORE_BLD_LUMBER,   CORE_BLD_MINE,
                                  CORE_BLD_RANCH,   CORE_BLD_PORT,     CORE_BLD_HUNTER_CAMP,
                                  CORE_BLD_SAWMILL, CORE_BLD_BARRACKS, CORE_BLD_HEALER,
                                  CORE_BLD_MARKET,  CORE_BLD_LIBRARY,  CORE_BLD_MAGE_TOWER};
    int type = v->workplace && expansion_spec(v->workplace->type)->field == field ? v->workplace->type
                                                                                  : types[field];
    *out = expansion_work_percent(w, v, type);
    return CORE_OK;
}
