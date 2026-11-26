/* harvest — 현재 기능의 단일 구현 */
#include "expansion.h"
#include "placement.h"
#include "rank.h"
#include "villager_ai.h"
#include <stdlib.h>
#include <string.h>

int64_t expansion_harvest_deferred(const Building *b) {
    return (int64_t)(((uint64_t)(uint32_t)b->harvest_deferred_hi << 32) | (uint32_t)b->harvest_deferred_lo);
}

void expansion_harvest_defer(Building *b, int64_t amount) {
    int64_t old = expansion_harvest_deferred(b);
    int64_t value = amount > 0 && old > INT64_MAX - amount ? INT64_MAX
                    : amount < 0 && amount < -old          ? 0
                                                           : old + amount;
    b->harvest_deferred_lo = (int32_t)(uint32_t)value;
    b->harvest_deferred_hi = (int32_t)((uint64_t)value >> 32);
}

void expansion_harvest_release(World *w) {
    static const int cumulative[6] = {10, 25, 45, 70, 90, 100};
    int day = exp_day(w), year_day = (day - 1) % 48 + 1;
    if (year_day < 25 || year_day > 30 || exp_minute(w) < 480)
        return;
    for (int i = 0; i < w->building_count; i++) {
        Building *b = w->buildings[i];
        if (b->annual_harvest_year != exp_year(w) || b->annual_harvest_day >= day)
            continue;
        int target = (int)((int64_t)b->annual_harvest_total * cumulative[year_day - 25] / 100);
        expansion_harvest_defer(b, target - b->annual_harvest_released);
        b->annual_harvest_released = target;
        b->annual_harvest_day = day;
    }
}

CORE_API int32_t core_expansion_harvest(const CoreWorld *w, int32_t id, CoreHarvestView *out) {
    if (!w || !out)
        return CORE_ERR_NULL_ARG;
    const Building *b = entity_find_building(w, id);
    if (!w->expansion || !b || !expansion_spec(b->type)->annual_yield)
        return CORE_ERR_INVALID_ARG;
    *out = (CoreHarvestView){b->annual_harvest_total, b->annual_harvest_released, b->annual_harvest_year,
                             b->annual_harvest_day, expansion_harvest_deferred(b) + b->pending_harvest_milli};
    return CORE_OK;
}
