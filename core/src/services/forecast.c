/* expansion_services_services.c — 실제 사건과 같은 날짜별 계획을 복제해 예측합니다.
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
static void forecast_resolve(World *w, const int starts[3]);
void expansion_advance_events(World *w) {
    Plan p = plan_read(w);
    int starts[3], ends[3];
    plan_day(w, exp_day(w), &p, starts, ends);
    forecast_resolve(w, starts);
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
int expansion_forecast_probability(int band, int which) {
    static const int rates[4][3] = {{92, 2, 5}, {82, 6, 10}, {72, 10, 15}, {62, 15, 20}};
    return band >= 0 && band < 4 && which >= 0 && which < 3 ? rates[band][which] : 0;
}
int expansion_forecast_jitter(Rng *r, int band) {
    static const int weights[4][7] = {{0, 0, 15, 70, 15, 0, 0},
                                      {0, 5, 20, 50, 20, 5, 0},
                                      {0, 15, 20, 30, 20, 15, 0},
                                      {10, 15, 15, 20, 15, 15, 10}};
    int roll = rng_range(r, 0, 99);
    for (int i = 0; i < 7; i++)
        if ((roll -= weights[band][i]) < 0)
            return i - 3;
    return 0;
}
static int forecast_choice(Rng *rng, int stream, int day, int exclude) {
    int choices[4], n = 0, season = ((day - 1) / 12) % 4;
    if (stream == 0) {
        choices[n++] = CORE_NATURE_STORM;
        if (season < 2) {
            choices[n++] = CORE_NATURE_DROUGHT;
            choices[n++] = CORE_NATURE_BUMPER;
        }
        if (season == 3)
            choices[n++] = CORE_NATURE_COLD_SNAP;
    } else if (stream == 1)
        choices[n++] = CORE_NATURE_PLAGUE;
    else {
        choices[n++] = CORE_NATURE_MINE_COLLAPSE;
        choices[n++] = CORE_NATURE_TRADE_BOOM;
    }
    for (int i = 0; i < n; i++)
        if (choices[i] == exclude) {
            choices[i] = choices[--n];
            i--;
        }
    return n ? choices[rng_range(rng, 0, n - 1)] : exclude;
}
static int forecast_key(const World *w, int day, int type) {
    return 1 + (int)(expansion_map_hash(w->seed, day, type + 724) % 2147483646u);
}
static void forecast_add(Building *b, int type, int start, int end, int chance, int key, int source_type,
                         int source_day, int status) {
    if (b->forecast_count >= CORE_EXP_FORECAST_MAX)
        return;
    for (int i = 0; i < b->forecast_count; i++)
        if (b->forecast_meta[i][0] == key ||
            (b->forecast_entries[i][0] == type && b->forecast_entries[i][1] == start))
            return;
    int i = b->forecast_count++;
    memcpy(b->forecast_entries[i], (int[]){type, start, end, chance}, 4 * sizeof(int));
    memcpy(b->forecast_meta[i], (int[]){key, source_type, source_day, status}, 4 * sizeof(int));
}
static void forecast_resolve(World *w, const int starts[3]) {
    int today = exp_day(w);
    for (int j = 0; j < w->building_count; j++) {
        Building *b = w->buildings[j];
        if (b->type != CORE_BLD_OBSERVATORY)
            continue;
        for (int i = 0; i < b->forecast_count; i++) {
            int *meta = b->forecast_meta[i], *entry = b->forecast_entries[i];
            if (!(meta[3] & 7)) {
                for (int k = 0; k < 3; k++)
                    if (starts[k] && meta[2] == today && meta[1] == starts[k])
                        meta[3] = (meta[3] & 8) | (entry[0] == starts[k] ? 1 : 3);
                if (!(meta[3] & 7) && today > entry[2])
                    meta[3] = (meta[3] & 8) | 2;
            }
        }
    }
}
void expansion_forecast_refresh(World *w, Building *b) {
    if (b->type != CORE_BLD_OBSERVATORY)
        return;
    int factor = forecast_factor(w, b), today = exp_day(w);
    if (!factor || b->forecast_day == today)
        return;
    int horizon = exp_clamp(12 + (12 * (factor < 3000 ? factor : 3000) + 500) / 1000, 12, 48);
    int old[24][4], old_meta[24][4], old_count = b->forecast_count;
    memcpy(old, b->forecast_entries, sizeof(old));
    memcpy(old_meta, b->forecast_meta, sizeof(old_meta));
    b->forecast_day = today;
    b->forecast_horizon = horizon;
    b->forecast_count = 0;
    memset(b->forecast_entries, 0, sizeof(b->forecast_entries));
    memset(b->forecast_meta, 0, sizeof(b->forecast_meta));
    for (int i = 0; i < old_count; i++)
        if (old[i][1] <= today && old[i][2] >= today - 2)
            forecast_add(b, old[i][0], old[i][1], old[i][2], old[i][3], old_meta[i][0], old_meta[i][1],
                         old_meta[i][2], old_meta[i][3]);
    int actual[49][3] = {{0}}, ends[3];
    Plan p = plan_read(w);
    for (int d = 1; d <= horizon; d++)
        plan_day(w, today + d, &p, actual[d], ends);
    Rng rng;
    expansion_event_rng(w, today, b->id + 1240, &rng);
    for (int d = 1; d <= horizon; d++)
        for (int stream = 0; stream < 3; stream++)
            if (actual[d][stream]) {
                int band = (d - 1) / 12, source = actual[d][stream];
                if (rng_range(&rng, 0, 99) >= expansion_forecast_probability(band, 0))
                    continue;
                int type = source;
                if (rng_range(&rng, 0, 99) < expansion_forecast_probability(band, 1) && stream != 1)
                    type = forecast_choice(&rng, stream, today + d, source);
                int center =
                    exp_clamp(today + d + expansion_forecast_jitter(&rng, band), today + 1, today + horizon);
                int season = ((center - 1) / 12) % 4;
                if (((type == CORE_NATURE_DROUGHT || type == CORE_NATURE_BUMPER) && season >= 2) ||
                    (type == CORE_NATURE_COLD_SNAP && season != 3))
                    center = today + d;
                int spread = band == 0 ? 1 : band == 3 ? 3 : 2;
                int start = exp_clamp(center - spread, today + 1, today + horizon),
                    end = exp_clamp(center + spread, today + 1, today + horizon);
                int key = forecast_key(w, today + d, source), revised = 0;
                for (int i = 0; i < old_count; i++)
                    if (old_meta[i][0] == key &&
                        (old[i][0] != type || old[i][1] != start || old[i][2] != end))
                        revised = 8;
                forecast_add(b, type, start, end, expansion_forecast_probability(band, 0), key, source,
                             today + d, revised);
            }
    for (int band = 0; band < 4 && band * 12 < horizon; band++)
        for (int stream = 0; stream < 3; stream++) {
            if (rng_range(&rng, 0, 99) >= expansion_forecast_probability(band, 2))
                continue;
            for (int attempt = 0; attempt < 64; attempt++) {
                int d = rng_range(&rng, band * 12 + 1, exp_clamp(band * 12 + 12, 1, horizon));
                int type = forecast_choice(&rng, stream, today + d, 0), spread = band == 0   ? 1
                                                                                 : band == 3 ? 3
                                                                                             : 2;
                int start = exp_clamp(d - spread, 1, horizon), end = exp_clamp(d + spread, 1, horizon),
                    valid = 1;
                for (int dd = start; dd <= end; dd++)
                    if (actual[dd][stream] == type)
                        valid = 0;
                int season = ((today + d - 1) / 12) % 4;
                if ((type == CORE_NATURE_DROUGHT || type == CORE_NATURE_BUMPER) && season >= 2)
                    valid = 0;
                if (type == CORE_NATURE_COLD_SNAP && season != 3)
                    valid = 0;
                if (!valid)
                    continue;
                int key = forecast_key(w, today * 128 + d, stream + 1000);
                forecast_add(b, type, today + start, today + end, expansion_forecast_probability(band, 0),
                             key, 0, 0, 0);
                break;
            }
        }
    for (int i = 1; i < b->forecast_count; i++)
        for (int j = i; j > 0 && b->forecast_entries[j][1] < b->forecast_entries[j - 1][1]; j--) {
            int temp[4];
            memcpy(temp, b->forecast_entries[j], sizeof(temp));
            memcpy(b->forecast_entries[j], b->forecast_entries[j - 1], sizeof(temp));
            memcpy(b->forecast_entries[j - 1], temp, sizeof(temp));
            memcpy(temp, b->forecast_meta[j], sizeof(temp));
            memcpy(b->forecast_meta[j], b->forecast_meta[j - 1], sizeof(temp));
            memcpy(b->forecast_meta[j - 1], temp, sizeof(temp));
        }
}
void expansion_forecast_daily(World *w) {
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
    out->horizon = b->forecast_horizon;
    out->count = b->forecast_count;
    for (int i = 0; i < out->count; i++)
        out->entries[i] = (CoreForecastEntry){b->forecast_entries[i][0],        b->forecast_entries[i][1],
                                              b->forecast_entries[i][2],        b->forecast_entries[i][3],
                                              b->forecast_meta[i][0],           b->forecast_meta[i][3] & 7,
                                              (b->forecast_meta[i][3] & 8) != 0};
    return CORE_OK;
}
