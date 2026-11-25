/* expansion_v123_services.c — 실제 사건과 같은 날짜별 계획을 복제해 예측합니다.
 */
#include "expansion.h"
#include "nature.h"
#include <string.h>
void expansion_event_rng(const World *w, int day, int stream, Rng *out) {
    rng_seed(out, (uint64_t)(uint32_t)w->seed ^ ((uint64_t)(uint32_t)day << 32),
             (uint64_t)(unsigned)(stream + 123));
}
typedef struct Plan {
    int event, days, calm, plague, plague_cd, economic, economic_days, economic_cd;
} Plan;
static Plan plan_read(const World *w) {
    return (Plan){w->nature.event,
                  w->nature.event_days,
                  w->nature.calm_days,
                  w->expansion->plague_days,
                  w->expansion->plague_cooldown,
                  w->expansion->economic_event,
                  w->expansion->economic_days,
                  w->expansion->economic_cooldown};
}
static void plan_day(const World *w, int day, Plan *p, int starts[3], int ends[3]) {
    Rng rng;
    expansion_event_rng(w, day, 1, &rng);
    int season = ((day - 1) / 12) % 4;
    memset(starts, 0, 3 * sizeof(int));
    memset(ends, 0, 3 * sizeof(int));
    if (p->event && --p->days <= 0) {
        ends[0] = p->event;
        p->event = p->days = p->calm = 0;
    } else if (!p->event)
        p->calm++;
    if (!p->event && p->calm >= 3 && rng_chance_permille(&rng, 35)) {
        int choices[4] = {CORE_NATURE_STORM}, count = 1;
        if (season < 2) {
            choices[count++] = CORE_NATURE_DROUGHT;
            choices[count++] = CORE_NATURE_BUMPER;
        }
        if (season == 3)
            choices[count++] = CORE_NATURE_COLD_SNAP;
        p->event = choices[rng_range(&rng, 0, count - 1)];
        p->days = p->event == CORE_NATURE_STORM     ? rng_range(&rng, 1, 3)
                  : p->event == CORE_NATURE_DROUGHT ? rng_range(&rng, 3, 6)
                                                    : rng_range(&rng, 2, 5);
        starts[0] = p->event;
    }
    if (p->plague && --p->plague == 0) {
        p->plague_cd = 12;
        ends[1] = CORE_NATURE_PLAGUE;
    } else if (!p->plague && p->plague_cd > 0)
        p->plague_cd--;
    if (!p->plague && !p->plague_cd && rng_chance_permille(&rng, 8)) {
        p->plague = rng_range(&rng, 3, 6);
        starts[1] = CORE_NATURE_PLAGUE;
    }
    if (p->economic_days && --p->economic_days == 0) {
        ends[2] = p->economic;
        p->economic = 0;
        p->economic_cd = 3;
    } else if (!p->economic_days && p->economic_cd > 0)
        p->economic_cd--;
    if (!p->economic_days && !p->economic_cd && rng_chance_permille(&rng, 10)) {
        p->economic = rng_range(&rng, 0, 1) ? CORE_NATURE_MINE_COLLAPSE : CORE_NATURE_TRADE_BOOM;
        p->economic_days =
            p->economic == CORE_NATURE_MINE_COLLAPSE ? rng_range(&rng, 1, 3) : rng_range(&rng, 3, 6);
        starts[2] = p->economic;
    }
}
void expansion_advance_events(World *w) {
    Plan p = plan_read(w);
    int starts[3], ends[3];
    plan_day(w, exp_day(w), &p, starts, ends);
    w->nature.event = p.event;
    w->nature.event_days = p.days;
    w->nature.calm_days = p.calm;
    w->nature.forecast = w->nature.forecast_days = 0;
    w->expansion->plague_days = p.plague;
    w->expansion->plague_cooldown = p.plague_cd;
    w->expansion->economic_event = p.economic;
    w->expansion->economic_days = p.economic_days;
    w->expansion->economic_cooldown = p.economic_cd;
    for (int i = 0; i < 3; i++) {
        if (ends[i])
            events_push_count(&w->events, CORE_EVT_NATURE_ENDED, ends[i]);
        if (starts[i])
            events_push_pair(&w->events, CORE_EVT_NATURE_STARTED, starts[i],
                             i == 0   ? p.days
                             : i == 1 ? p.plague
                                      : p.economic_days);
    }
    if (starts[1]) {
        int count = w->villager_count / 8 + 1, tries = w->villager_count * 2;
        while (count > 0 && tries-- > 0 && w->villager_count)
            if (nature_make_sick(w, w->villagers[rng_range(&w->rng, 0, w->villager_count - 1)]))
                count--;
    }
    if (p.plague) {
        int sick = 0;
        for (int i = 0; i < w->villager_count; i++)
            sick += w->villagers[i]->sick_ticks > 0;
        for (int i = 0; i < sick && w->villager_count; i++)
            if (rng_chance_permille(&w->rng, 450))
                (void)nature_make_sick(w, w->villagers[rng_range(&w->rng, 0, w->villager_count - 1)]);
    }
    if (p.event && p.event != CORE_NATURE_BUMPER)
        w->expansion->disaster_day = exp_day(w);
}
static int forecast_factor(const World *w, const Building *b) {
    int total = 0;
    if (b->state != CORE_BSTATE_ACTIVE)
        return 0;
    for (int i = 0; i < b->worker_count; i++)
        if (b->workers[i]->stats.intellect >= 5 && expansion_working_present(w, b, b->workers[i]))
            total += expansion_work_percent(w, b->workers[i], b->type) * 10;
    return total / 4;
}
void expansion_forecast_refresh(World *w, Building *b) {
    if (b->type != CORE_BLD_OBSERVATORY)
        return;
    int factor = forecast_factor(w, b);
    if (!factor)
        return;
    int horizon = exp_clamp(12 + (12 * (factor < 3000 ? factor : 3000) + 500) / 1000, 12, 48);
    if (b->forecast_day == exp_day(w) && b->forecast_horizon == horizon)
        return;
    b->forecast_day = exp_day(w);
    b->forecast_horizon = horizon;
    b->forecast_count = 0;
    Plan p = plan_read(w);
    for (int delta = 1; delta <= horizon; delta++) {
        int starts[3], ends[3], day = exp_day(w) + delta;
        plan_day(w, day, &p, starts, ends);
        int chance = 90 - 10 * ((delta - 1) / 12);
        for (int k = 0; k < 3; k++)
            if (starts[k] && expansion_map_hash(w->seed, day, starts[k] + 100) % 100 < (unsigned)chance &&
                b->forecast_count < CORE_EXP_FORECAST_MAX) {
                int *entry = b->forecast_entries[b->forecast_count++];
                entry[0] = starts[k];
                entry[1] = day;
                entry[2] = day + (k == 0 ? p.days : k == 1 ? p.plague : p.economic_days) - 1;
                entry[3] = chance;
            }
    }
}
void expansion_v123_services_daily(World *w) {
    for (int i = 0; i < w->building_count; i++)
        expansion_forecast_refresh(w, w->buildings[i]);
}
CORE_API int32_t core_expansion_forecast(const CoreWorld *w, int32_t id, CoreForecastView *out) {
    if (!w || !out)
        return CORE_ERR_NULL_ARG;
    const Building *b = entity_find_building(w, id);
    if (!w->expansion || !b || b->type != CORE_BLD_OBSERVATORY)
        return CORE_ERR_INVALID_ARG;
    *out = (CoreForecastView){0};
    out->last_day = b->forecast_day;
    out->stale = b->forecast_day < exp_day(w);
    out->reason = b->state != CORE_BSTATE_ACTIVE ? 1 : !forecast_factor(w, b) ? 2 : 0;
    if (out->reason)
        return CORE_OK;
    out->horizon = b->forecast_horizon;
    out->count = b->forecast_count;
    for (int i = 0; i < out->count; i++)
        out->entries[i] = (CoreForecastEntry){b->forecast_entries[i][0], b->forecast_entries[i][1],
                                              b->forecast_entries[i][2], b->forecast_entries[i][3]};
    return CORE_OK;
}
