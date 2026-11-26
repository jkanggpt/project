/* runtime_state — 현재 기능의 단일 구현 */
#include "expansion.h"
#include "placement.h"
#include "rank.h"
#include "villager_ai.h"
#include <stdlib.h>
#include <string.h>

static int32_t *runtime_slot(const World *w, int kind, int id, int *count) {
    *count = 0;
    if (!w->expansion)
        return NULL;
    if (kind == CORE_RUNTIME_ACTOR) {
        Villager *v = entity_find_villager(w, id);
        *count = 20;
        return v ? v->frontend_actor : NULL;
    }
    if (kind == CORE_RUNTIME_TOWER) {
        Building *b = entity_find_building(w, id);
        *count = 2;
        return b ? b->frontend_tower : NULL;
    }
    if (kind == CORE_RUNTIME_CLOCK && !id) {
        *count = 2;
        return w->expansion->frontend_clock;
    }
    if (kind == CORE_RUNTIME_PROJECTILE && id >= 0 && id < 512) {
        *count = 13;
        return w->expansion->frontend_projectiles[id];
    }
    return NULL;
}

CORE_API int32_t core_expansion_copy_runtime(const CoreWorld *w, int32_t kind, int32_t id, int32_t *values,
                                             int32_t count) {
    if (!w || !values)
        return CORE_ERR_NULL_ARG;
    int expected = 0;
    int32_t *slot = runtime_slot(w, kind, id, &expected);
    if (!slot)
        return CORE_ERR_NOT_FOUND;
    if (count != expected)
        return CORE_ERR_BUFFER_TOO_SMALL;
    memcpy(values, slot, (size_t)count * sizeof(*values));
    return CORE_OK;
}

CORE_API int32_t core_expansion_report_runtime(CoreWorld *w, int32_t kind, int32_t id, const int32_t *values,
                                               int32_t count) {
    if (!w || !values)
        return CORE_ERR_NULL_ARG;
    int expected = 0;
    int32_t *slot = runtime_slot(w, kind, id, &expected);
    if (!slot)
        return CORE_ERR_NOT_FOUND;
    if (count != expected)
        return CORE_ERR_INVALID_ARG;
    if (kind == CORE_RUNTIME_CLOCK &&
        (values[0] < 0 || values[0] > 10000 || values[1] < 0 || values[1] > 512))
        return CORE_ERR_INVALID_ARG;
    if (kind == CORE_RUNTIME_ACTOR &&
        (values[0] < 0 || values[0] > 1 || values[1] < 0 || values[1] >= w->grid.width * 1000 ||
         values[2] < 0 || values[2] >= w->grid.height * 1000))
        return CORE_ERR_INVALID_ARG;
    memcpy(slot, values, (size_t)count * sizeof(*values));
    return CORE_OK;
}

CORE_API int32_t core_expansion_build_locked(const CoreWorld *w) {
    return w && w->expansion && threat_raid_active(w);
}

CORE_API int32_t core_expansion_enemy_runtime(CoreWorld *w, int32_t id, const CoreEnemyRuntime *state) {
    if (!w || !state)
        return CORE_ERR_NULL_ARG;
    Raid *r = &w->threat.raid;
    if (!w->expansion || !r->id || id < 1 || id > r->enemy_count)
        return CORE_ERR_NOT_FOUND;
    if (state->valid < 0 || state->valid > 2 || state->inbound_count < 0 || state->inbound_count > 64 ||
        state->hp < 0 || state->hp > 100000 || state->x_milli < 0 || state->y_milli < 0 ||
        state->x_milli >= w->grid.width * 1000 || state->y_milli >= w->grid.height * 1000)
        return CORE_ERR_INVALID_ARG;
    r->enemies[id - 1].runtime = *state;
    return CORE_OK;
}
