/* assignment.c — 현재 주민의 수동/자동 일터 연결 */
#include "expansion.h"
#include "jobs.h"

int32_t jobs_work_percent(const World *world, const Villager *v, int32_t type) {
    return expansion_work_percent(world, v, type);
}
void jobs_auto_assign(World *world) {
    expansion_assign(world);
    world->expansion->dirty = 1;
}
void jobs_assign_housing(World *world) { expansion_assign(world); }
void jobs_on_buildings_changed(World *world) {
    expansion_assign(world);
    world->expansion->dirty = 1;
}

int jobs_assign_manual(World *world, Villager *villager, Building *building) {
    const BuildingDef *def = world_building_def(world, building->type);
    int rc;
    if (building->state != CORE_BSTATE_ACTIVE || def->worker_slots <= 0 ||
        villager->state == CORE_VS_LEAVING) {
        return CORE_ERR_REJECTED;
    }
    if (villager->workplace == building) {
        villager->flags |= CORE_VF_MANUAL;
        return CORE_OK;
    }
    rc = entity_link_worker(building, villager, def->worker_slots);
    if (rc == CORE_OK) {
        villager->flags |= CORE_VF_MANUAL;
        if (world->expansion) {
            world->expansion->dirty = 1;
            expansion_initialize_shift(world, building, villager);
        }
        events_push_villager(&world->events, CORE_EVT_STORY, villager->id, CORE_STORY_NEW_JOB, building->type,
                             villager->name);
    }
    return rc;
}

void jobs_unassign_manual(World *world, Villager *villager) {
    if (world->expansion)
        world->expansion->dirty = 1;
    (void)world;
    entity_unlink_worker(villager);
    villager->flags |= CORE_VF_MANUAL; /* 자동 배치가 다시 넣지 않도록 */
}

void jobs_release_manual(World *world, Villager *villager) {
    villager->flags &= ~CORE_VF_MANUAL;
    jobs_auto_assign(world);
}
