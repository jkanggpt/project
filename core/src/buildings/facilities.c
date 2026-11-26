/* facilities — 현재 기능의 단일 구현 */
#include "expansion.h"
#include "placement.h"
#include "rank.h"
#include "villager_ai.h"
#include <stdlib.h>
#include <string.h>

CORE_API int32_t core_expansion_tree_mature_age(int32_t kind) {
    static const int ages[4] = {16000, 28000, 48000, 72000};
    return kind >= 1 && kind <= 4 ? ages[kind - 1] : 0;
}

CORE_API int32_t core_expansion_facility(const CoreWorld *w, int32_t id, int32_t type, int32_t x, int32_t y,
                                         int32_t rotation, CoreFacilityView *out) {
    if (!w || !out)
        return CORE_ERR_NULL_ARG;
    if (!w->expansion)
        return CORE_ERR_INVALID_ARG;
    const Building *b = id ? entity_find_building(w, id) : NULL;
    Building candidate = {0};
    if (id && !b)
        return CORE_ERR_NOT_FOUND;
    if (!b) {
        const ExpansionBuildingSpec *s = expansion_spec(type);
        if (!s)
            return CORE_ERR_INVALID_ARG;
        candidate.type = type;
        candidate.x = x;
        candidate.y = y;
        candidate.w = (rotation & 1) ? s->h : s->w;
        candidate.h = (rotation & 1) ? s->w : s->h;
        b = &candidate;
    }
    const ExpansionBuildingSpec *s = expansion_spec(b->type);
    *out = (CoreFacilityView){0};
    out->id = b->id;
    out->type_serial = b->type_serial;
    out->radius = s->radius;
    out->sight = s->sight;
    out->attack = w->defs.buildings[b->type].attack;
    out->range = w->defs.buildings[b->type].attack_range;
    out->cooldown = s->cooldown;
    if (b->type == CORE_BLD_KEEP) {
        out->attack = (int[]){6, 9, 13, 18, 26, 35}[w->rank];
        out->range = 4 + w->rank;
        out->cooldown = 25;
    }
    out->assigned = b->worker_count;
    out->present = expansion_working_count(w, b);
    out->labor_milli = id ? expansion_labor_milli(w, b) : 1000;
    out->combat_guard_id = b->combat_guard_id;
    out->stopped_reason = id ? b->blocked_reason : 0;
    out->nominal_milli = b->type == CORE_BLD_CLAY_PIT    ? 8000
                         : b->type == CORE_BLD_QUARRY    ? 8000
                         : b->type == CORE_BLD_MINE      ? 24000
                         : b->type == CORE_BLD_MANA_MINE ? 50000
                                                         : s->output[CORE_RES_LOGS];
    out->annual_yield = s->annual_yield;
    int nominal_fuel = 0;
    (void)expansion_recipe_plan(w, b, 1000, 1000, 0, out->nominal_input, out->nominal_output, &nominal_fuel);
    int factor = id ? expansion_production_factor(w, b) : 0;
    int attendance = s->workers ? exp_clamp(b->actual_work_milli / (s->workers * 540), 0, 1000) : 0;
    int ratio = expansion_recipe_plan(w, b, factor, attendance, 1, out->actual_input, out->actual_output,
                                      &out->fuel_milli);
    for (int res = 0; res < CORE_RES_COUNT; res++) {
        out->actual_input[res] = out->actual_input[res] * ratio / 1000;
        out->actual_output[res] = out->actual_output[res] * ratio / 1000;
    }
    out->fuel_milli = out->fuel_milli * ratio / 1000;
    int level = b->type == CORE_BLD_STALL          ? 1
                : b->type == CORE_BLD_MARKET       ? 2
                : b->type == CORE_BLD_TRADING_POST ? 3
                                                   : 0;
    if (level) {
        out->trade_base = (int[]){0, 20, 80, 260}[level];
        out->trade_limit = expansion_trade_limit(w);
        out->trade_used = w->expansion->trade_used;
        out->trade_efficiency = expansion_trade_efficiency(w);
        out->trade_next_day = w->expansion->market_next_restock_day;
    }
    int r = s->radius;
    for (int yy = b->y - r; yy < b->y + b->h + r; yy++)
        for (int xx = b->x - r; xx < b->x + b->w + r; xx++) {
            if (!grid_in_bounds(&w->grid, xx, yy) || expansion_center_distance(b, xx, yy) > r)
                continue;
            int p = grid_index(&w->grid, xx, yy);
            const CoreExpansionTileView *t = &w->expansion->tiles[p];
            if (!t->explored)
                continue;
            if (t->tree_kind && t->tree_age >= core_expansion_tree_mature_age(t->tree_kind) &&
                t->tree_logs_milli > 0) {
                out->mature_trees++;
                if (t->tree_kind == 4)
                    out->magic_logs_milli += t->tree_logs_milli;
                else
                    out->logs_milli += t->tree_logs_milli;
            }
            out->deposits[CORE_RES_STONE] += t->stone_milli;
            out->deposits[CORE_RES_CLAY] += t->clay_milli;
            if (t->ore_kind >= 0 && t->ore_kind < CORE_RES_COUNT)
                out->deposits[t->ore_kind] += t->ore_milli;
        }
    return CORE_OK;
}
