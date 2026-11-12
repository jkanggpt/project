/*
 * test_grid.c — 지형 생성: 결정성, 공정한 시작, 연결성, 비율
 */
#include <stdlib.h>
#include <string.h>

#include "grid.h"
#include "mem.h"
#include "test_util.h"
#include "world.h"

#define MANY_SEEDS 60

void test_grid_generation_is_deterministic(void)
{
    CoreWorld *a = test_make_world(1234, CORE_DIFF_NORMAL);
    CoreWorld *b = test_make_world(1234, CORE_DIFF_NORMAL);
    CoreWorld *c = test_make_world(1235, CORE_DIFF_NORMAL);
    size_t cells;
    CHECK(a != NULL && b != NULL && c != NULL);
    cells = (size_t)a->grid.width * (size_t)a->grid.height;
    CHECK(memcmp(a->grid.terrain, b->grid.terrain, cells) == 0);
    CHECK(memcmp(a->grid.terrain, c->grid.terrain, cells) != 0);
    core_destroy(a);
    core_destroy(b);
    core_destroy(c);
}

void test_grid_fair_start_many_seeds(void)
{
    int32_t seed;
    for (seed = 0; seed < MANY_SEEDS; seed++) {
        CoreWorld *world = test_make_world(seed * 7919, seed % CORE_DIFF_COUNT);
        const Grid *grid;
        CHECK(world != NULL);
        grid = &world->grid;
        CHECK(grid_count_in_radius(grid, world->keep_x, world->keep_y, GRID_FAIR_RADIUS, CORE_TERRAIN_FOREST) >= 30);
        CHECK(grid_count_in_radius(grid, world->keep_x, world->keep_y, GRID_FAIR_RADIUS, CORE_TERRAIN_ROCK) >= 10);
        CHECK(grid_count_in_radius(grid, world->keep_x, world->keep_y, GRID_FAIR_RADIUS, CORE_TERRAIN_MANA) >= 3);
        CHECK_EQ(test_validate_verbose(world), 0);
        core_destroy(world);
    }
}

void test_grid_connectivity_many_seeds(void)
{
    int32_t seed;
    for (seed = 0; seed < MANY_SEEDS; seed++) {
        CoreWorld *world = test_make_world(seed * 104729 + 3, CORE_DIFF_NORMAL);
        const Grid *grid;
        int32_t count;
        int32_t land = 0;
        int32_t reached;
        uint8_t *mark;
        int32_t i;
        int north = 0;
        int south = 0;
        int east = 0;
        int west = 0;
        CHECK(world != NULL);
        grid = &world->grid;
        count = grid->width * grid->height;
        mark = (uint8_t *)calloc((size_t)count, 1);
        CHECK(mark != NULL);
        reached = grid_flood_reachable(grid, world->keep_x, world->keep_y, mark);
        for (i = 0; i < count; i++) {
            if (grid->terrain[i] != CORE_TERRAIN_WATER) {
                land++;
            }
        }
        /* 물이 아닌 칸은 모두 성채에서 걸어서 닿아야 합니다 */
        CHECK_EQ(reached, land);
        for (i = 0; i < grid->width; i++) {
            north |= mark[grid_index(grid, i, 0)];
            south |= mark[grid_index(grid, i, grid->height - 1)];
        }
        for (i = 0; i < grid->height; i++) {
            west |= mark[grid_index(grid, 0, i)];
            east |= mark[grid_index(grid, grid->width - 1, i)];
        }
        free(mark);
        CHECK(north && south && east && west);
        core_destroy(world);
    }
}

void test_grid_terrain_mix_is_reasonable(void)
{
    int32_t seed;
    for (seed = 1; seed <= 20; seed++) {
        CoreWorld *world = test_make_world(seed, CORE_DIFF_NORMAL);
        int32_t counts[CORE_TERRAIN_COUNT];
        int32_t total;
        int32_t i;
        CHECK(world != NULL);
        memset(counts, 0, sizeof(counts));
        total = world->grid.width * world->grid.height;
        for (i = 0; i < total; i++) {
            counts[world->grid.terrain[i]]++;
        }
        /* 비율이 크게 어긋나지 않는지 (생성 상수를 바꿨을 때 실수를 잡기 위함) */
        CHECK(counts[CORE_TERRAIN_GRASS] * 100 / total >= 40);
        CHECK(counts[CORE_TERRAIN_FOREST] * 100 / total >= 15);
        CHECK(counts[CORE_TERRAIN_ROCK] * 100 / total >= 4);
        CHECK(counts[CORE_TERRAIN_WATER] * 100 / total <= 15);
        CHECK(counts[CORE_TERRAIN_MANA] >= 10);
        core_destroy(world);
    }
}
