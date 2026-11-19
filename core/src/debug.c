/*
 * debug.c — 성능 측정용 큰 마을 (설명은 debug.h)
 */
#include "debug.h"
#include "expansion.h"

#include "economy.h"
#include "entity.h"
#include "jobs.h"
#include "placement.h"
#include "population.h"
#include "threat.h"

#define DEBUG_SEARCH_RADIUS 40

/* 건물 하나를 공짜로 바로 짓습니다: 비용만큼 먼저 주고(장부 "기타") 보통처럼 놓은 뒤 공사를 끝냄 */
static int build_free(World *world, int32_t type, int32_t x, int32_t y)
{
    const BuildingDef *def = world_building_def(world, type);
    CorePlacementView view;
    int32_t id = 0;
    int32_t reason = 0;
    int32_t r;
    placement_check(world, type, x, y, &view);
    if (view.result != CORE_PLACE_OK && view.result != CORE_PLACE_CANNOT_AFFORD) {
        return 0;
    }
    for (r = 0; r < CORE_RES_COUNT; r++) {
        if (def->cost[r] > 0) {
            (void)eco_change(&world->bank, &world->ledger, r, def->cost[r], CORE_LEDGER_OTHER);
        }
    }
    if (placement_place(world, type, x, y, &id, &reason) != CORE_OK) {
        return 0;
    }
    placement_finish_now(world, entity_find_building(world, id));
    return 1;
}

/* 성채에서 가까운 칸부터 나선형으로 돌며 count 채를 짓습니다 (한 칸씩 띄워 길이 막히지 않게). 지은 수 */
static int32_t build_ring(World *world, int32_t type, int32_t count)
{
    int32_t cx = world->keep_x;
    int32_t cy = world->keep_y;
    int32_t built = 0;
    int32_t radius;
    for (radius = 3; radius <= DEBUG_SEARCH_RADIUS && built < count; radius++) {
        int32_t dx;
        int32_t dy;
        for (dy = -radius; dy <= radius && built < count; dy += 3) {
            for (dx = -radius; dx <= radius && built < count; dx += 3) {
                if (dx > -radius && dx < radius && dy > -radius && dy < radius) {
                    continue; /* 고리의 테두리만 */
                }
                built += build_free(world, type, cx + dx, cy + dy);
            }
        }
    }
    return built;
}

int debug_stress(World *world, int32_t villagers, int32_t enemies)
{
    if (world->expansion)
        return expansion_debug_town(world, villagers, enemies);
    int32_t houses;
    int32_t farms;
    Building *keep = world_keep(world);
    if (villagers < 0 || villagers > DEBUG_MAX_VILLAGERS || enemies < 0 || enemies > DEBUG_MAX_ENEMIES || keep == NULL) {
        return CORE_ERR_INVALID_ARG;
    }
    houses = (villagers + 3) / 4;
    farms = villagers / 4;
    (void)build_ring(world, CORE_BLD_HOUSE, houses);
    (void)build_ring(world, CORE_BLD_FARM, farms);
    /* 큰 마을이 하루 먹을 식량을 담을 창고 (없으면 200명쯤부터 하루 만에 굶어 떠남) */
    (void)build_ring(world, CORE_BLD_WAREHOUSE, villagers / 25);
    while (world->villager_count < villagers) {
        Villager *villager = population_create(world, keep->door_x, keep->door_y);
        if (villager == NULL) {
            return CORE_ERR_OUT_OF_MEMORY;
        }
        villager->mood = 70;
    }
    jobs_on_buildings_changed(world);
    /* 재는 동안 굶거나 얼지 않게 식량·목재를 한도까지 */
    (void)eco_change(&world->bank, &world->ledger, CORE_RES_FOOD, world->bank.capacity, CORE_LEDGER_OTHER);
    (void)eco_change(&world->bank, &world->ledger, CORE_RES_WOOD, world->bank.capacity, CORE_LEDGER_OTHER);
    if (enemies > 0) {
        return threat_debug_raid(world, CORE_THREAT_MONSTERS, CORE_DIR_NORTH, enemies);
    }
    return CORE_OK;
}
