/* buildings — 현행 기능별 회귀 사례 */
#include "fixtures/content.h"

void test_content_rank_groups(void) {
    World *w = fx_content_content_world(14);
    CHECK(w);
    CoreRankRequirementView rows[24];
    for (int rank = 0; rank < 5; rank++) {
        w->rank = rank;
        int n = core_expansion_rank_requirements(w, rows, 24);
        CHECK(n > 0 && n <= 24);
        for (int i = 0; i < n; i++)
            if (rows[i].kind == 2) {
                CHECK(rows[i].type_count > 0);
                for (int j = 0; j < rows[i].type_count; j++) {
                    CoreExpansionBuildingInfo info;
                    core_expansion_building_info(rows[i].types[j], &info);
                    CHECK(info.rank <= rank);
                }
                for (int j = 0; j < w->building_count; j++)
                    for (int k = 0; k < rows[i].type_count; k++)
                        if (w->buildings[j]->type == rows[i].types[k])
                            w->buildings[j]->state = CORE_BSTATE_RUINED;
                Building *b = fx_content_content_facility(w, rows[i].types[0], 10 + rank * 10, 20 + i * 3);
                CHECK(b);
                b->state = CORE_BSTATE_CONSTRUCTING;
                CoreRankRequirementView copy[24];
                CHECK_EQ(core_expansion_rank_requirements(w, copy, 24), n);
                CHECK(!copy[i].satisfied);
                b->state = CORE_BSTATE_ACTIVE;
                CHECK_EQ(core_expansion_rank_requirements(w, copy, 24), n);
                CHECK(copy[i].satisfied);
                CHECK_EQ(copy[i].building_id, b->id);
                b->state = CORE_BSTATE_RUINED;
            }
    }
    w->rank = 5;
    CHECK_EQ(core_expansion_rank_requirements(w, rows, 24), 0);
    core_destroy(w);
}

void test_content_catalog(void) {
    CHECK_EQ(CORE_ABI_VERSION, 15);
    CHECK_EQ(CORE_SAVE_VERSION, 14);
    CHECK_EQ(CORE_BLD_COUNT, 73);
    CHECK_EQ(CORE_EXP_ANIMALS, 35);
    CHECK_EQ(core_expansion_barter_count(), 85);
    CHECK(!strcmp(core_expansion_building_name(CORE_BLD_STALL), u8"가판대"));
    CHECK(!strcmp(core_expansion_building_name(CORE_BLD_RANCH), u8"축사"));
    CHECK(!strcmp(core_expansion_building_name(CORE_BLD_STONE_ROAD), u8"자갈길"));
    int seen[73] = {0};
    for (int i = 0; i < 72; i++) {
        int type = core_expansion_display_building(i);
        CHECK(type > 0 && type < 73);
        CHECK(!seen[type]);
        seen[type] = 1;
    }
    for (int i = 1; i < 73; i++)
        CHECK(seen[i]);
    CoreExpansionBuildingInfo clinic, light, road;
    core_expansion_building_info(CORE_BLD_CLINIC, &clinic);
    core_expansion_building_info(CORE_BLD_LIGHTHOUSE, &light);
    core_expansion_building_info(CORE_BLD_PAVED_ROAD, &road);
    CHECK_EQ(clinic.workers, 2);
    CHECK_EQ(clinic.rank, 2);
    CHECK_EQ(light.sight, 12);
    CHECK_EQ(light.workers, 0);
    CHECK_EQ(road.cost[CORE_RES_GOLD], 3);
    CHECK_EQ(road.cost[CORE_RES_STONE], 3);
    CHECK_EQ(road.cost[CORE_RES_BRICKS], 2);
}
