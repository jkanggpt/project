/* lighthouse — 현재 기능의 단일 구현 */
#include "expansion.h"
#include "placement.h"
#include "rank.h"
#include "villager_ai.h"
#include <stdlib.h>
#include <string.h>

int expansion_lighthouse_bonus(const World *w, const Building *b) {
    int water = expansion_adjacent_water_id(w, b);
    if (!water)
        return 100;
    for (int i = 0; i < w->building_count; i++) {
        const Building *light = w->buildings[i];
        if (light->type == CORE_BLD_LIGHTHOUSE && light->state == CORE_BSTATE_ACTIVE && light->operating &&
            expansion_centers_distance(light, b) <= 10 && expansion_adjacent_water_id(w, light) == water)
            return 115;
    }
    return 100;
}
