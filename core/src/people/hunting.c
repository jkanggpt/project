/* hunting — 현재 기능의 단일 구현 */
#include "expansion.h"
#include "placement.h"
#include "rank.h"
#include "villager_ai.h"
#include <stdlib.h>
#include <string.h>

int expansion_hunter_threat(World *w, Villager *v, int animal_id) {
    Building *b = v->workplace;
    if (!b || (b->type != CORE_BLD_HUNTER_CAMP && b->type != CORE_BLD_HUNTING_LODGE))
        return 0;
    CoreWorkSchedule schedule;
    if (!expansion_schedule(w, v, &schedule) || core_expansion_outfit(w, v->id) == 0)
        return 0;
    for (int i = 0; i < w->expansion->animal_count; i++)
        if (w->expansion->animals[i].id == animal_id) {
            const CoreAnimalView *a = &w->expansion->animals[i];
            const CoreAnimalInfo *info = &EXP_ANIMALS[a->kind];
            if (v->health <= 35 || (b->type == CORE_BLD_HUNTER_CAMP && info->biomass >= 5)) {
                v->hunt_retreat_reason = v->health <= 35 ? 1 : 2;
                v->hold_ticks = 0;
                v->mission = CORE_MISSION_RETURN;

                vil_set_intent(v, CORE_VS_GUARD_POST, b, b->door_x, b->door_y, 0);
                return 1;
            }
            int radius = b->type == CORE_BLD_HUNTER_CAMP ? 8 : 15;
            if (!a->owner && expansion_center_distance(b, a->x_milli / 1000, a->y_milli / 1000) <= radius) {
                v->hold_ticks = 0;
                v->mission = CORE_MISSION_HUNT;

                return 1;
            }
        }
    return 0;
}
