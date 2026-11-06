/*
 * test_town.c — 마을 테스트 공통 도우미 (주민 도착 흉내, 빨리 짓기, 빈터 만들기, 자원 채우기)
 */
#include <stdio.h>

#include "test_town.h"

#include "placement.h"

/* 주민이 목표로 바로 걸어간 것으로 치고 도착을 보고합니다 (Godot 대신) */
void test_arrive_all(CoreWorld *world)
{
    int32_t i;
    for (i = world->villager_count - 1; i >= 0; i--) {
        Villager *v = world->villagers[i];
        if (!v->at_target && !(v->intent_flags & CORE_INTENT_HOLD)) {
            (void)core_villager_arrived(world, v->id, v->intent_rev);
        }
    }
}

/* ticks 만큼 진행하며 매 틱 모두 도착시킵니다 */
int test_run(CoreWorld *world, int32_t ticks)
{
    int32_t i;
    for (i = 0; i < ticks; i++) {
        if (core_tick(world, 1) != CORE_OK) {
            return 0;
        }
        test_arrive_all(world);
    }
    return 1;
}

void test_keep_alive(CoreWorld *world)
{
    int32_t i;
    world->bank.amount[CORE_RES_FOOD] = world->bank.capacity;
    world->bank.amount[CORE_RES_WOOD] = world->bank.capacity;
    for (i = 0; i < world->villager_count; i++) {
        Villager *villager = world->villagers[i];
        villager->needs.hunger = 100;
        villager->mood = villager->mood < 60 ? 60 : villager->mood;
        villager->low_mood_ticks = 0;
        villager->health = villager->health < 80 ? 80 : villager->health;
    }
}

/* 건물을 짓고 다 지어질 때까지 진행합니다. 성공하면 건물 */
Building *test_build_now(CoreWorld *world, int32_t type, int32_t x, int32_t y)
{
    int32_t id = 0;
    int32_t reason = 0;
    Building *building;
    /* 시험에서는 등급 잠금을 건너뜁니다 (그 건물이 필요한 등급으로 올려 둠) */
    if (world_building_def(world, type)->required_rank > world->rank) {
        world->rank = world_building_def(world, type)->required_rank;
        world->rank_ready_announced = world->rank;
        placement_refresh_capacity(world);
    }
    if (core_place_building(world, type, x, y, &id, &reason) != CORE_OK) {
        fprintf(stderr, "    place %d at (%d,%d) failed: reason %d\n", (int)type, (int)x, (int)y, (int)reason);
        return NULL;
    }
    building = entity_find_building(world, id);
    while (building != NULL && building->state == CORE_BSTATE_CONSTRUCTING) {
        (void)core_tick(world, 1);
    }
    return building;
}

/* 지형을 풀밭으로 바꿔 시험용 빈터를 만듭니다 (x..x+w, y..y+h) */
void test_flatten(CoreWorld *world, int32_t x, int32_t y, int32_t w, int32_t h)
{
    int32_t cx;
    int32_t cy;
    for (cy = y; cy < y + h; cy++) {
        for (cx = x; cx < x + w; cx++) {
            world->grid.terrain[grid_index(&world->grid, cx, cy)] = CORE_TERRAIN_GRASS;
        }
    }
    grid_rebuild_costs(&world->grid);
}

void test_give(CoreWorld *world, int32_t amount)
{
    int32_t i;
    for (i = 0; i < CORE_RES_COUNT; i++) {
        world->bank.amount[i] = eco_is_stored_good(i) && amount > world->bank.capacity ? world->bank.capacity : amount;
    }
}

