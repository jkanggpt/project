/* research.c — 현재 연구 상태의 공용 조회 */
#include "research.h"
#include "expansion.h"
#include <string.h>

void research_init(ResearchState *state) {
    memset(state, 0, sizeof(*state));
    state->active = -1;
}

static int is_done(const World *world, int32_t index) {
    return index >= 0 && index < 32 && (world->research.done & (1u << index)) != 0;
}

int32_t research_state_of(const World *world, int32_t index) {
    const ResearchDef *def = defs_research(&world->defs, index);
    if (def == NULL) {
        return CORE_RSTATE_LOCKED;
    }
    if (is_done(world, index)) {
        return CORE_RSTATE_DONE;
    }
    if (world->research.active == index) {
        return CORE_RSTATE_ACTIVE;
    }
    if (world->rank < def->required_rank || (def->requires >= 0 && !is_done(world, def->requires))) {
        return CORE_RSTATE_LOCKED;
    }
    return CORE_RSTATE_AVAILABLE;
}

int32_t research_mages(const World *world) {
    int32_t count = 0;
    int32_t i;
    for (i = 0; i < world->villager_count; i++) {
        const Villager *villager = world->villagers[i];
        count += villager->state == CORE_VS_WORKING && villager->at_target && villager->workplace != NULL &&
                 villager->workplace->type == CORE_BLD_MAGE_TOWER &&
                 villager->workplace->state == CORE_BSTATE_ACTIVE;
    }
    return count;
}
void research_reapply(World *world) { expansion_apply_research(world); }
