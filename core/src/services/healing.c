/* healing — 현재 기능의 단일 구현 */
#include "expansion.h"
#include "placement.h"
#include "rank.h"
#include "villager_ai.h"
#include <stdlib.h>
#include <string.h>

int expansion_heal_herbs_required(const World *w, const Villager *v, const Building *b) {
    return (b->type == CORE_BLD_HOSPITAL ? 3 : 2) + (v->health <= 35 || v->critical) +
           (v->sick_ticks > 0 && expansion_event_active(w, CORE_NATURE_PLAGUE));
}

CORE_API int32_t core_expansion_healing_herbs(const CoreWorld *w, int32_t id, int32_t clinic) {
    if (!w || !w->expansion)
        return -1;
    const Villager *v = entity_find_villager(w, id);
    const Building *b = entity_find_building(w, clinic);
    if (!v || !b || !expansion_is_medical(b->type))
        return -1;
    return expansion_heal_herbs_required(w, v, b);
}
