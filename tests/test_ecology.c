/* ecology — 현행 기능별 회귀 사례 */
#include "fixtures/settlement.h"

void test_settlement_ecology_water_and_roads(void) {
    World *w = fx_settlement_make_world();
    if (!w)
        return;
    int wild = 0, attack = 0, swim = 0, livestock = 0;
    for (int k = 0; k < 24; k++) {
        CoreAnimalInfo info;
        core_expansion_animal_info(k, &info);
        attack += info.attack > 0;
        swim += info.swim_milli > 0;
        livestock += info.livestock;
    }
    CHECK_EQ(attack, 12);
    CHECK_EQ(swim, 5);
    CHECK_EQ(livestock, 4);
    CHECK(!EXP_ANIMALS[14].swim_milli);
    CHECK(!EXP_ANIMALS[15].swim_milli);
    CHECK_EQ(EXP_ANIMALS[17].behavior, 1);
    for (int i = 0; i < w->expansion->animal_count; i++) {
        CoreAnimalView *a = &w->expansion->animals[i];
        CHECK(!EXP_ANIMALS[a->kind].livestock);
        wild += a->alive;
    }
    /* 현재 지도 시드42의 변경 전 공개 API 골든 개체 수입니다. */
    CHECK_EQ(wild, 209);
    CHECK_EQ(w->expansion->region_count, 30);
    for (int i = 0; i < 30; i++) {
        CHECK(w->expansion->regions[i].count <= w->expansion->regions[i].count_cap);
        CHECK(w->expansion->regions[i].biomass <= w->expansion->regions[i].capacity);
        CHECK(w->expansion->regions[i].aggressive <= (w->expansion->regions[i].count_cap + 3) / 4);
    }
    int p = grid_index(&w->grid, 20, 20);
    w->grid.terrain[p] = CORE_TERRAIN_GRASS;
    w->grid.block[p] = 0;
    w->grid.cost[p] = 10;
    w->expansion->tiles[p].biome = CORE_BIOME_SWAMP;
    expansion_terrain_costs(w);
    CHECK_EQ(w->grid.cost[p], 135);
    w->grid.road[p] = 1;
    expansion_terrain_costs(w);
    CHECK_EQ(w->grid.cost[p], 67);
    w->grid.road[p] = 2;
    expansion_terrain_costs(w);
    CHECK_EQ(w->grid.cost[p], 50);
    w->grid.terrain[p] = CORE_TERRAIN_WATER;
    w->grid.road[p] = 3;
    expansion_terrain_costs(w);
    CHECK_EQ(w->grid.cost[p], 80);
    core_destroy(w);
}
