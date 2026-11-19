/* expansion_wellbeing.c — 장기 행복도와 사건 기분, 영구 인구 안전망 */
#include "expansion.h"
#include "villager_ai.h"
#include <stdlib.h>

int expansion_permanent(const World *w) {
    int n = 0, i;
    for (i = 0; i < w->villager_count; i++)
        n += !w->villagers[i]->contract_end_tick;
    return n;
}
int expansion_departure_allowed(World *w, const Villager *v, int reserve) {
    int i, pending = 0, pop = expansion_permanent(w);
    if (v->contract_end_tick)
        return 1;
    for (i = 0; i < w->villager_count; i++)
        pending += w->villagers[i] != v && !w->villagers[i]->contract_end_tick &&
                   w->villagers[i]->state == CORE_VS_LEAVING;
    if (pop - (reserve ? pending : 0) <= 8) {
        w->expansion->blocked_floor8++;
        return 0;
    }
    return 1;
}
void expansion_wellbeing_sample(World *w, Villager *v) {
    int i, needs[3] = {v->needs.hunger, v->needs.rest, v->needs.safety};
    for (i = 0; i < 3; i++)
        v->need_sums[i] += needs[i];
    v->need_samples++;
    if (v->needs.hunger < 20)
        v->hunger_ticks++;
    else
        v->hunger_ticks = 0;
    if (v->hunger_ticks >= 72 && v->hunger_event_day != exp_day(w)) {
        v->mood = exp_clamp(v->mood - 8, -200, 200);
        v->hunger_event_day = exp_day(w);
        v->mood_cause = 1;
    }
    v->happiness = expansion_mood(w, v);
}
void expansion_wellbeing_daily(World *w, Villager *v) {
    int i;
    (void)w;
    if (v->need_samples)
        for (i = 0; i < 3; i++) {
            int actual = v->need_sums[i] * 1000 / v->need_samples;
            v->long_needs[i] = (v->long_needs[i] * 3 + actual + 2) / 4;
            v->need_sums[i] = 0;
        }
    v->need_samples = 0;
    v->mood += v->mood > 0 ? -exp_clamp(v->mood, 0, 2) : exp_clamp(-v->mood, 0, 2);
    if (v->flags & CORE_VF_UNPAID) {
        v->mood = exp_clamp(v->mood - 6, -200, 200);
        v->mood_cause = 2;
    }
    v->happiness = expansion_mood(w, v);
}
void expansion_hurt(World *w, Villager *v, int damage, int raid) {
    if (damage <= 0)
        return;
    if (!v->injury_tick || w->clock.tick - v->injury_tick >= 12) {
        v->mood -= 8;
        v->injury_tick = w->clock.tick ? w->clock.tick : 1;
        v->mood_cause = 3;
    }
    if (v->health < 30 && !v->critical) {
        v->mood -= 25;
        v->critical = 1;
        v->mood_cause = 4;
    } else if (v->health >= 30)
        v->critical = 0;
    if (raid && v->threat_raid != raid) {
        v->mood -= 15;
        v->threat_raid = raid;
        v->mood_cause = 5;
    }
    v->mood = exp_clamp(v->mood, -200, 200);
    v->happiness = expansion_mood(w, v);
}
CORE_API int32_t core_expansion_immigration(const CoreWorld *w, CoreImmigrationView *out) {
    const ExpansionState *e;
    int i, disease = 0, sum = 0, day, rank;
    if (!w || !out)
        return CORE_ERR_NULL_ARG;
    e = w->expansion;
    if (!e)
        return CORE_ERR_INVALID_ARG;
    *out = (CoreImmigrationView){0};
    day = exp_day(w);
    rank = w->rank;
    out->permanent = expansion_permanent(w);
    out->last_day = e->last_arrival_day;
    out->next_day = e->last_arrival_day + 4;
    out->special_waves = e->starter_waves - (e->pending_arrivals > 0 && e->pending_type == 1);
    out->refill_day = e->refill_day;
    out->vacant = expansion_available_housing((World *)w);
    out->required_happiness = (int[]){0, 38, 40, 42, 44, 46}[rank];
    out->food_needed = out->permanent * 1200 * (int[]){0, 8, 10, 12, 12, 12}[rank];
    out->food_available = expansion_bank_have(w, CORE_RES_FOOD);
    out->road_ok = 1;
    for (i = 0; i < w->villager_count; i++) {
        const Villager *v = w->villagers[i];
        if (v->contract_end_tick)
            continue;
        sum += v->happiness;
        out->water_served += v->water_building != 0;
        disease += v->sick_ticks > 0;
        if (v->incoming)
            out->status = CORE_IMM_ARRIVING;
        if (v->home && !e->connected[grid_index(&w->grid, v->home->door_x, v->home->door_y)])
            out->road_ok = 0;
    }
    out->happiness = out->permanent ? sum / out->permanent : 0;
    if (!rank) {
        uint32_t schedule = (uint32_t)w->seed * UINT32_C(2654435761);
        out->status = e->pending_arrivals    ? CORE_IMM_ARRIVING
                      : e->starter_waves < 2 ? CORE_IMM_SPECIAL
                                             : CORE_IMM_REFILL;
        out->next_day = e->starter_waves == 0   ? 5 + (int)((schedule >> 8) & 3u)
                        : e->starter_waves == 1 ? 10 + (int)((schedule >> 14) & 3u)
                                                : e->refill_day;
        out->minimum = out->maximum = e->starter_waves < 2 ? 2 : out->permanent < 8;
        return CORE_OK;
    }
    out->minimum = rank == 1 ? 2 : rank < 4 ? 3 : 4;
    out->maximum = rank + 2;
    if (exp_season(w) == 3) {
        out->minimum = (out->minimum + 1) / 2;
        out->maximum = (out->maximum + 1) / 2;
    }
    if (out->minimum > out->vacant)
        out->minimum = out->vacant;
    if (out->maximum > out->vacant)
        out->maximum = out->vacant;
    if (day < out->next_day)
        out->reasons |= CORE_IMM_COOLDOWN;
    if (out->vacant <= 0)
        out->reasons |= CORE_IMM_HOUSING;
    if (out->water_served < out->permanent)
        out->reasons |= CORE_IMM_WATER;
    if (!out->road_ok)
        out->reasons |= CORE_IMM_ROAD;
    if (out->food_available < out->food_needed)
        out->reasons |= CORE_IMM_FOOD;
    if ((e->disaster_day && day < e->disaster_day + 5) || disease * 2 > out->permanent)
        out->reasons |= CORE_IMM_DISASTER;
    if (out->happiness < out->required_happiness)
        out->reasons |= CORE_IMM_HAPPINESS;
    if (out->status != CORE_IMM_ARRIVING)
        out->status = out->reasons & CORE_IMM_COOLDOWN ? CORE_IMM_WAIT
                      : out->reasons                   ? CORE_IMM_BLOCKED
                                                       : CORE_IMM_READY;
    return CORE_OK;
}
