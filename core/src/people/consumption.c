/* consumption — 현재 기능의 단일 구현 */
#include "expansion.h"
#include "placement.h"
#include "rank.h"
#include "villager_ai.h"
#include <stdlib.h>
#include <string.h>

CORE_API int32_t core_expansion_daily_requirements(const CoreWorld *w, int32_t out[CORE_RES_COUNT]) {
    if (!w || !out)
        return CORE_ERR_NULL_ARG;
    if (!w->expansion)
        return CORE_ERR_INVALID_ARG;
    memset(out, 0, CORE_RES_COUNT * sizeof(*out));
    int fuel = 0;
    for (int i = 0; i < w->villager_count; i++) {
        const Villager *v = w->villagers[i];
        if (v->incoming || v->state == CORE_VS_LEAVING)
            continue;
        out[CORE_RES_FOOD] += 1000;
        if (v->workplace && !v->contract_end_tick)
            out[CORE_RES_GOLD] += w->defs.buildings[v->workplace->type].wage * 1000;
    }
    for (int i = 0; i < w->expansion->animal_count; i++) {
        const CoreAnimalView *a = &w->expansion->animals[i];
        if (!a->alive || !a->owner)
            continue;
        out[CORE_RES_FOOD] += expansion_animal_feed(w, a, entity_find_building(w, a->owner));
    }
    for (int i = 0; i < w->building_count; i++) {
        const Building *b = w->buildings[i];
        if (b->state != CORE_BSTATE_ACTIVE)
            continue;
        const ExpansionBuildingSpec *spec = expansion_spec(b->type);
        if (!(expansion_is_medical(b->type) && !b->operating))
            out[CORE_RES_GOLD] += spec->upkeep * 1000;
        if (exp_season(w) == 3 && spec->heating_milli && (!spec->residents || b->resident_count)) {
            int need = spec->heating_milli * (expansion_has_research(w, 19) ? 80 : 100) / 100;
            if (expansion_event_active(w, CORE_NATURE_COLD_SNAP))
                need = need * 130 / 100;
            fuel += need;
        }
    }
    fuel = exp_clamp(fuel - w->expansion->fuel_milli, 0, INT32_MAX);
    int coal = exp_clamp((fuel + 1999) / 2000, 0, w->bank.amount[CORE_RES_COAL]);
    out[CORE_RES_COAL] = coal * 1000;
    fuel = exp_clamp(fuel - coal * 2000, 0, INT32_MAX);
    int logs = exp_clamp((fuel + 999) / 1000, 0, w->bank.amount[CORE_RES_LOGS]);
    out[CORE_RES_LOGS] = logs * 1000;
    fuel = exp_clamp(fuel - logs * 1000, 0, INT32_MAX);
    out[CORE_RES_WOOD] = ((fuel + 999) / 1000) * 1000;
    return CORE_OK;
}
