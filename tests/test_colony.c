/*
 * test_colony.c — 마일스톤 2: 정의 파일, 주민·건물 연결, 배치 규칙, 걷기 핸드셰이크, 경제, 저장
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "defs.h"
#include "entity.h"
#include "jobs.h"
#include "mem.h"
#include "nature.h"
#include "placement.h"
#include "platform.h"
#include "population.h"
#include "test_town.h"
#include "test_util.h"
#include "villager_ai.h"
#include "world.h"

/* ---- 정의 파일 -------------------------------------------------------- */

void test_defs_builtin_roundtrip(void)
{
    CoreDefs *defs = NULL;
    CoreDefs *reloaded = NULL;
    int32_t line = -1;
    int32_t file;
    char path[64];
    CHECK_EQ(core_defs_create(&defs), CORE_OK);
    CHECK_EQ(core_defs_create(&reloaded), CORE_OK);
    /* 내장 기본값을 파일로 쓰고 다시 읽으면 똑같아야 합니다 (game/data 파일이 이렇게 만들어짐) */
    for (file = 0; file < CORE_DATA_COUNT; file++) {
        snprintf(path, sizeof(path), "roundtrip_def_%d.txt", (int)file);
        CHECK_EQ(core_defs_save_file(defs, file, path), CORE_OK);
        CHECK_EQ(core_defs_load_file(reloaded, file, path, &line), CORE_OK);
        CHECK_EQ(line, 0);
        (void)platform_remove_file(path);
    }
    CHECK(memcmp(defs->defs.buildings, reloaded->defs.buildings, sizeof(defs->defs.buildings)) == 0);
    CHECK_EQ(defs->defs.trait_count, reloaded->defs.trait_count);
    CHECK(memcmp(defs->defs.traits, reloaded->defs.traits, (size_t)defs->defs.trait_count * sizeof(TraitDef)) == 0);
    CHECK_EQ(defs->defs.name_count, reloaded->defs.name_count);
    CHECK_EQ(defs->defs.research_count, reloaded->defs.research_count);
    CHECK(memcmp(defs->defs.research, reloaded->defs.research,
                 (size_t)defs->defs.research_count * sizeof(ResearchDef)) == 0);
    CHECK(memcmp(defs->defs.ranks, reloaded->defs.ranks, sizeof(defs->defs.ranks)) == 0);
    CHECK_EQ(reloaded->defs.warning_count, 0);
    core_defs_destroy(defs);
    core_defs_destroy(reloaded);
}

void test_defs_errors_keep_defaults(void)
{
    static const char *const BAD[] = {
        "[building farm]\nrate = 50\nwidth = 9\n",          /* 범위 밖 → 3번째 줄 */
        "[building farm]\nrate = 50\nflavor = 3\n",         /* 모르는 필드 → 3번째 줄 */
        "[building castle]\n",                              /* 모르는 건물 → 1번째 줄 */
        "# ok\n\nrate = 5\n",                               /* 섹션 밖 → 3번째 줄 */
        "[building farm]\nproduces = gems\n",               /* 모르는 자원 → 2번째 줄 */
        "[building wall]\nwidth = 2\n",                     /* 줄 배치는 1×1 이어야 함 → 끝에서 실패 */
    };
    static const int32_t LINES[] = {3, 3, 1, 3, 2, 2};
    CoreDefs *defs = NULL;
    CoreWorld *world = NULL;
    CoreNewGameParams params;
    CoreEventView event;
    int warnings = 0;
    size_t i;
    CHECK_EQ(core_defs_create(&defs), CORE_OK);
    for (i = 0; i < sizeof(BAD) / sizeof(BAD[0]); i++) {
        int32_t line = 0;
        CHECK_EQ(core_defs_load_text(defs, CORE_DATA_BUILDINGS, BAD[i], (int32_t)strlen(BAD[i]), &line),
                 CORE_ERR_FORMAT);
        if (i < 5) {
            CHECK_EQ(line, LINES[i]);
        }
    }
    /* 실패한 파일은 하나도 적용되지 않았어야 합니다 (rate = 50 이 들어가면 안 됨) */
    CHECK(memcmp(defs->defs.buildings, BUILTIN_BUILDINGS, sizeof(BUILTIN_BUILDINGS)) == 0);
    CHECK_EQ(core_defs_load_file(defs, CORE_DATA_BUILDINGS, "no_such_file.def", NULL), CORE_ERR_FILE_OPEN);
    /* 새 월드를 만들면 경고 이벤트로 알립니다 */
    memset(&params, 0, sizeof(params));
    strcpy(params.castle_name, u8"경고성");
    params.difficulty = CORE_DIFF_NORMAL;
    CHECK_EQ(core_create(&params, defs, &world), CORE_OK);
    while (core_poll_event(world, &event) == 1) {
        warnings += event.type == CORE_EVT_DEF_WARNING;
    }
    CHECK_EQ(warnings, 6);
    core_destroy(world);
    core_defs_destroy(defs); /* 월드가 복사본을 가지므로 순서와 상관없이 안전 */
}

void test_defs_traits_and_names_grow(void)
{
    const char *traits = "[trait diligent]\nwork = 40\n\n[trait night_owl]\nwork = 5\nmood = 2\n";
    char names[2048];
    CoreDefs *defs = NULL;
    int32_t line = 0;
    int32_t i;
    size_t used = 0;
    CHECK_EQ(core_defs_create(&defs), CORE_OK);
    CHECK_EQ(core_defs_load_text(defs, CORE_DATA_TRAITS, traits, (int32_t)strlen(traits), &line), CORE_OK);
    CHECK_EQ(defs->defs.trait_count, BUILTIN_TRAIT_COUNT + 1); /* 새 특성은 동적 배열 끝에 붙음 */
    CHECK_EQ(defs->defs.traits[defs_find_trait(&defs->defs, "diligent")].work_pct, 40);
    CHECK_EQ(defs->defs.traits[defs_find_trait(&defs->defs, "night_owl")].mood, 2);
    /* 이름 파일: 30개 → 목록이 바뀜 */
    names[0] = '\0';
    for (i = 0; i < 30; i++) {
        used += (size_t)snprintf(names + used, sizeof(names) - used, u8"이름%d\n", (int)i);
    }
    CHECK_EQ(core_defs_load_text(defs, CORE_DATA_NAMES, names, (int32_t)used, &line), CORE_OK);
    CHECK_EQ(defs->defs.name_count, 30);
    /* 너무 적으면 거부하고 그대로 */
    CHECK_EQ(core_defs_load_text(defs, CORE_DATA_NAMES, u8"하나\n둘\n", 10, &line), CORE_ERR_FORMAT);
    CHECK_EQ(defs->defs.name_count, 30);
    CHECK_EQ(core_defs_load_text(defs, 99, "x", 1, &line), CORE_ERR_INVALID_ARG);
    CHECK_EQ(core_defs_load_text(NULL, CORE_DATA_NAMES, "x", 1, &line), CORE_ERR_NULL_ARG);
    core_defs_destroy(defs);
}

/* ---- 주민과 건물 ------------------------------------------------------ */

void test_entity_links_both_directions(void)
{
    CoreWorld *world = test_make_world(12, CORE_DIFF_NORMAL);
    Building *house;
    Villager *a;
    Villager *b;
    int32_t house_id;
    CHECK(world != NULL);
    house = entity_add_building(world, CORE_BLD_HOUSE);
    CHECK(house != NULL);
    house_id = house->id;
    a = world->villagers[0];
    b = world->villagers[1];
    CHECK_EQ(entity_link_resident(house, b, 4), CORE_OK);
    CHECK_EQ(entity_link_resident(house, a, 4), CORE_OK);
    /* 양쪽 방향 모두 연결되고, 건물 쪽 배열은 id 순서 */
    CHECK(a->home == house && b->home == house);
    CHECK_EQ(house->resident_count, 2);
    CHECK(house->residents[0]->id < house->residents[1]->id);
    CHECK_EQ(entity_link_resident(house, a, 1), CORE_ERR_REJECTED); /* 자리 없음 */
    entity_unlink_resident(a);
    CHECK(a->home == NULL);
    CHECK_EQ(house->resident_count, 1);
    CHECK(house->residents[0] == b);
    /* 주민을 지우면 건물 쪽 배열에서도 빠짐 */
    entity_remove_villager(world, b);
    CHECK_EQ(house->resident_count, 0);
    /* 건물을 지우면 주민 쪽 포인터가 NULL 이 됨 */
    CHECK_EQ(entity_link_resident(house, a, 4), CORE_OK);
    CHECK_EQ(entity_link_worker(house, a, 2), CORE_OK);
    a->target = house;
    entity_remove_building(world, house);
    CHECK(a->home == NULL && a->workplace == NULL && a->target == NULL);
    CHECK(entity_find_building(world, house_id) == NULL);
    placement_rebuild_layers(world);
    CHECK_EQ(test_validate_verbose(world), 0);
    core_destroy(world);
}

void test_placement_rules(void)
{
    CoreWorld *world = test_make_world(21, CORE_DIFF_NORMAL);
    CorePlacementView view;
    int32_t kx;
    int32_t ky;
    int32_t id;
    int32_t reason;
    CHECK(world != NULL);
    kx = world->keep_x;
    ky = world->keep_y;
    test_give(world, 5000);
    test_flatten(world, kx + 4, ky - 6, 12, 12);

    CHECK_EQ(core_can_place(world, CORE_BLD_KEEP, kx + 5, ky, &view), CORE_OK);
    CHECK_EQ(view.result, CORE_PLACE_BAD_TYPE);
    CHECK_EQ(core_can_place(world, 999, 0, 0, &view), CORE_OK);
    CHECK_EQ(view.result, CORE_PLACE_BAD_TYPE);
    (void)core_can_place(world, CORE_BLD_HOUSE, -1, 10, &view);
    CHECK_EQ(view.result, CORE_PLACE_OUT_OF_BOUNDS);
    (void)core_can_place(world, CORE_BLD_HOUSE, 0, 10, &view);
    CHECK_EQ(view.result, CORE_PLACE_TOO_CLOSE_EDGE);
    (void)core_can_place(world, CORE_BLD_HOUSE, kx, ky, &view);
    CHECK_EQ(view.result, CORE_PLACE_OCCUPIED); /* 성채 위 */
    /* 물 위 */
    world->grid.terrain[grid_index(&world->grid, kx + 6, ky)] = CORE_TERRAIN_WATER;
    grid_rebuild_costs(&world->grid);
    (void)core_can_place(world, CORE_BLD_HOUSE, kx + 5, ky - 1, &view);
    CHECK_EQ(view.result, CORE_PLACE_BAD_TERRAIN);
    test_flatten(world, kx + 6, ky, 1, 1);
    /* 성채 문 앞을 막는 자리 */
    (void)core_can_place(world, CORE_BLD_HOUSE, kx - 1, ky + 2, &view);
    CHECK_EQ(view.result, CORE_PLACE_BLOCKS_DOOR);
    /* 숲이 없으면 벌목장 불가, 바위가 없으면 채석장 불가, 광맥이 아니면 광산 불가 */
    (void)core_can_place(world, CORE_BLD_LUMBER, kx + 8, ky - 2, &view);
    CHECK_EQ(view.result, CORE_PLACE_NEEDS_FOREST);
    (void)core_can_place(world, CORE_BLD_QUARRY, kx + 8, ky - 2, &view);
    CHECK_EQ(view.result, CORE_PLACE_NEEDS_ROCK);
    world->rank = 1; /* 광산은 등급 1 */
    (void)core_can_place(world, CORE_BLD_MANA_MINE, kx + 8, ky - 2, &view);
    CHECK_EQ(view.result, CORE_PLACE_NEEDS_MANA);
    world->rank = 0;
    (void)core_can_place(world, CORE_BLD_MANA_MINE, kx + 8, ky - 2, &view);
    CHECK_EQ(view.result, CORE_PLACE_LOCKED);
    /* 숲을 심으면 벌목장 가능, 생산성 미리보기가 숲 칸 수를 셈 */
    {
        int32_t cx;
        for (cx = kx + 4; cx < kx + 16; cx++) {
            world->grid.terrain[grid_index(&world->grid, cx, ky - 6)] = CORE_TERRAIN_FOREST;
        }
        grid_rebuild_costs(&world->grid);
    }
    (void)core_can_place(world, CORE_BLD_LUMBER, kx + 8, ky - 4, &view);
    CHECK_EQ(view.result, CORE_PLACE_OK);
    CHECK(view.terrain_count >= 8);
    CHECK(view.productivity > 0);
    /* 자원 부족 */
    world->bank.amount[CORE_RES_WOOD] = 0;
    (void)core_can_place(world, CORE_BLD_HOUSE, kx + 8, ky + 1, &view);
    CHECK_EQ(view.result, CORE_PLACE_CANNOT_AFFORD);
    CHECK_EQ(core_place_building(world, CORE_BLD_HOUSE, kx + 8, ky + 1, &id, &reason), CORE_ERR_REJECTED);
    CHECK_EQ(reason, CORE_PLACE_CANNOT_AFFORD);
    /* 지으면 비용을 내고, 짓는 시간이 지나야 완성 */
    world->bank.amount[CORE_RES_WOOD] = 100;
    world->bank.amount[CORE_RES_GOLD] = 100;
    CHECK_EQ(core_place_building(world, CORE_BLD_HOUSE, kx + 8, ky + 1, &id, &reason), CORE_OK);
    CHECK_EQ(world->bank.amount[CORE_RES_WOOD], 75);
    CHECK_EQ(world->bank.amount[CORE_RES_GOLD], 80);
    CHECK_EQ(entity_find_building(world, id)->state, CORE_BSTATE_CONSTRUCTING);
    (void)core_can_place(world, CORE_BLD_HOUSE, kx + 8, ky + 1, &view);
    CHECK_EQ(view.result, CORE_PLACE_OCCUPIED);
    CHECK_EQ(core_tick(world, 35), CORE_OK);
    CHECK_EQ(entity_find_building(world, id)->state, CORE_BSTATE_CONSTRUCTING);
    CHECK_EQ(core_tick(world, 1), CORE_OK);
    CHECK_EQ(entity_find_building(world, id)->state, CORE_BSTATE_ACTIVE);
    /* 창고를 다 지으면 저장 한도가 늘어남 */
    test_give(world, 5000);
    {
        int32_t before = world->bank.capacity;
        CHECK(test_build_now(world, CORE_BLD_WAREHOUSE, kx + 11, ky + 1) != NULL);
        CHECK_EQ(world->bank.capacity, before + BUILTIN_BUILDINGS[CORE_BLD_WAREHOUSE].storage);
    }
    CHECK_EQ(test_validate_verbose(world), 0);
    core_destroy(world);
}

void test_placement_roads_and_paths(void)
{
    CoreWorld *world = test_make_world(33, CORE_DIFF_NORMAL);
    CorePlacementView view;
    int32_t placed = 0;
    int32_t reason = 0;
    int32_t kx;
    int32_t ky;
    int32_t rev;
    CHECK(world != NULL);
    kx = world->keep_x;
    ky = world->keep_y;
    test_give(world, 5000);
    test_flatten(world, kx - 12, ky + 3, 25, 10);
    rev = world->grid.cost_rev;
    /* 도로 줄: 비용 격자가 바뀌고 걷기 비용이 낮아짐 */
    CHECK_EQ(core_place_line(world, CORE_BLD_ROAD, kx - 10, ky + 6, kx + 10, ky + 6, &placed, &reason), CORE_OK);
    CHECK_EQ(placed, 21);
    CHECK(world->grid.cost_rev > rev);
    CHECK_EQ(world->grid.cost[grid_index(&world->grid, kx, ky + 6)], 5);
    /* 같은 줄을 다시 놓으면 이미 있는 칸은 건너뜀 (오류는 아님) */
    CHECK_EQ(core_place_line(world, CORE_BLD_ROAD, kx - 10, ky + 6, kx + 10, ky + 6, &placed, &reason), CORE_OK);
    CHECK_EQ(placed, 0);
    CHECK_EQ(reason, CORE_PLACE_OK);
    /* 도로 위에는 건물 불가 */
    (void)core_can_place(world, CORE_BLD_HOUSE, kx + 3, ky + 5, &view);
    CHECK_EQ(view.result, CORE_PLACE_ROAD_IN_WAY);
    CHECK_EQ(core_remove_road(world, kx + 3, ky + 6), CORE_OK);
    CHECK_EQ(core_remove_road(world, kx + 3, ky + 6), CORE_ERR_REJECTED);
    /* 성채를 성벽 고리로 둘러쌉니다 (x: kx-2..kx+2, y: ky-3..ky+3).
     * 성채 문 앞 줄과 이어지는 오른쪽 아래 (kx+2, ky+2) 한 칸만 비워 둠 */
    CHECK_EQ(core_place_line(world, CORE_BLD_WALL, kx - 2, ky - 3, kx + 2, ky - 3, &placed, &reason), CORE_OK);
    CHECK_EQ(core_place_line(world, CORE_BLD_WALL, kx - 2, ky + 3, kx + 2, ky + 3, &placed, &reason), CORE_OK);
    CHECK_EQ(core_place_line(world, CORE_BLD_WALL, kx - 2, ky - 2, kx - 2, ky + 2, &placed, &reason), CORE_OK);
    CHECK_EQ(core_place_line(world, CORE_BLD_WALL, kx + 2, ky - 2, kx + 2, ky + 1, &placed, &reason), CORE_OK);
    CHECK_EQ(placed, 4);
    /* 마지막 틈을 성벽으로 막으면 성채에서 지도 밖으로 갈 수 없으므로 거부 */
    (void)core_can_place(world, CORE_BLD_WALL, kx + 2, ky + 2, &view);
    CHECK_EQ(view.result, CORE_PLACE_BLOCKS_PATH);
    CHECK_EQ(core_place_line(world, CORE_BLD_WALL, kx + 2, ky + 2, kx + 2, ky + 2, &placed, &reason),
             CORE_ERR_REJECTED);
    CHECK_EQ(reason, CORE_PLACE_BLOCKS_PATH);
    /* 성문은 주민이 지나가므로 그 틈을 성문으로 막는 것은 괜찮음 */
    (void)core_can_place(world, CORE_BLD_GATE, kx + 2, ky + 2, &view);
    CHECK_EQ(view.result, CORE_PLACE_OK);
    /* 고리 안의 빈터에는 집을 지을 수 있지만, 문이 성채 문과 이어지지 않는 바깥 고립 지대는 안 됨 */
    CHECK_EQ(world->grid.cost[grid_index(&world->grid, kx + 2, ky - 3)], CORE_COST_WALL);
    CHECK_EQ(test_validate_verbose(world), 0);
    core_destroy(world);
}

void test_demolish_refunds_and_unlinks(void)
{
    CoreWorld *world = test_make_world(44, CORE_DIFF_NORMAL);
    Building *house;
    int32_t residents;
    int32_t wood_before;
    int32_t i;
    CHECK(world != NULL);
    test_flatten(world, world->keep_x + 4, world->keep_y - 2, 4, 4);
    house = test_build_now(world, CORE_BLD_HOUSE, world->keep_x + 5, world->keep_y - 1);
    CHECK(house != NULL);
    residents = house->resident_count;
    CHECK(residents > 0); /* 다 지으면 집 없는 주민이 들어감 */
    wood_before = world->bank.amount[CORE_RES_WOOD];
    CHECK_EQ(core_demolish(world, house->id), CORE_OK);
    CHECK_EQ(world->bank.amount[CORE_RES_WOOD], wood_before + BUILTIN_BUILDINGS[CORE_BLD_HOUSE].cost[CORE_RES_WOOD] / 2);
    for (i = 0; i < world->villager_count; i++) {
        CHECK(world->villagers[i]->home == NULL);
    }
    CHECK_EQ(core_demolish(world, world->keep_id), CORE_ERR_REJECTED);
    CHECK_EQ(core_demolish(world, 123456), CORE_ERR_NOT_FOUND);
    CHECK_EQ(test_validate_verbose(world), 0);
    core_destroy(world);
}

/* ---- 핸드셰이크와 경제 ------------------------------------------------- */

void test_villager_handshake_stale_rev(void)
{
    CoreWorld *world = test_make_world(55, CORE_DIFF_NORMAL);
    CoreEventView event;
    Villager *v;
    int32_t old_rev;
    int blocked = 0;
    CHECK(world != NULL);
    v = world->villagers[0];
    /* 1시간 진행: 쉬러 광장으로 가는 의도가 생김 (아직 도착 안 함) */
    CHECK_EQ(core_tick(world, 6), CORE_OK);
    old_rev = v->intent_rev;
    /* 의도를 바꿔 rev 를 올림: 이전 rev 의 도착 보고는 무시돼야 함 */
    vil_set_intent(v, CORE_VS_IDLE, NULL, v->loc_x, v->loc_y + 1, 0);
    CHECK(v->intent_rev == old_rev + 1);
    CHECK(!v->at_target);
    CHECK_EQ(core_villager_arrived(world, v->id, old_rev), CORE_IGNORED);
    CHECK(!v->at_target);
    CHECK_EQ(core_villager_arrived(world, v->id, v->intent_rev), CORE_OK);
    CHECK(v->at_target);
    CHECK_EQ(v->loc_x, v->target_x);
    CHECK_EQ(v->loc_y, v->target_y);
    /* 같은 의도를 다시 정하면 rev 가 오르지 않음 */
    old_rev = v->intent_rev;
    vil_set_intent(v, v->state, v->target, v->target_x, v->target_y, v->intent_flags);
    CHECK_EQ(v->intent_rev, old_rev);
    /* 길 없음: 제자리 대기(HOLD) + 알림 */
    vil_set_intent(v, CORE_VS_IDLE, NULL, v->loc_x + 1, v->loc_y, 0);
    CHECK_EQ(core_villager_unreachable(world, v->id, v->intent_rev - 1), CORE_IGNORED);
    CHECK_EQ(core_villager_unreachable(world, v->id, v->intent_rev), CORE_OK);
    CHECK(v->intent_flags & CORE_INTENT_HOLD);
    CHECK(v->hold_ticks > 0);
    while (core_poll_event(world, &event) == 1) {
        blocked += event.type == CORE_EVT_PATH_BLOCKED && event.a == v->id && strcmp(event.text, v->name) == 0;
    }
    CHECK_EQ(blocked, 1);
    /* 하루가 지나도 도착 보고가 없으면 C 가 스스로 길 없음으로 처리 (멈춰 버리지 않게) */
    v->hold_ticks = 0;
    vil_set_intent(v, CORE_VS_IDLE, NULL, v->loc_x, v->loc_y + 2, 0);
    v->state_ticks = VIL_WALK_TIMEOUT_TICKS;
    CHECK_EQ(core_tick(world, 1), CORE_OK);
    CHECK(v->intent_flags & CORE_INTENT_HOLD);
    CHECK_EQ(core_villager_arrived(world, 999999, 1), CORE_ERR_NOT_FOUND);
    CHECK_EQ(test_validate_verbose(world), 0);
    core_destroy(world);
}

void test_production_only_while_working(void)
{
    CoreWorld *world = test_make_world(66, CORE_DIFF_NORMAL);
    Building *farm;
    int32_t food_before;
    int32_t i;
    CHECK(world != NULL);
    test_flatten(world, world->keep_x + 4, world->keep_y - 2, 5, 5);
    farm = test_build_now(world, CORE_BLD_FARM, world->keep_x + 5, world->keep_y - 1);
    CHECK(farm != NULL);
    CHECK_EQ(farm->worker_count, 2); /* 자동 배치 */
    /* 낮 근무 시간까지 진행하되 아무도 도착시키지 않으면 생산 없음 */
    while (clock_minute_of_day(&world->clock) != 8 * 60) {
        CHECK_EQ(core_tick(world, 1), CORE_OK);
    }
    for (i = 0; i < world->villager_count; i++) {
        world->villagers[i]->needs.hunger = 100; /* 먹으러 가지 않게 */
    }
    food_before = world->bank.amount[CORE_RES_FOOD];
    CHECK_EQ(core_tick(world, 30), CORE_OK);
    CHECK_EQ(farm->work_milli[0], 0);
    CHECK(world->bank.amount[CORE_RES_FOOD] <= food_before);
    /* 도착시키면 WORKING 이 되고 식량이 쌓임 (13:00 → 16:20, 아직 근무 시간) */
    CHECK(test_run(world, 20));
    CHECK(farm->work_milli[0] > 0 || world->ledger.today[CORE_RES_FOOD][CORE_LEDGER_PRODUCTION] > 0);
    CHECK(world->ledger.today[CORE_RES_FOOD][CORE_LEDGER_PRODUCTION] > 0);
    CHECK(farm->workers[0]->state == CORE_VS_WORKING);
    CHECK_EQ(test_validate_verbose(world), 0);
    core_destroy(world);
}

void test_consumption_and_starvation(void)
{
    CoreWorld *world = test_make_world(77, CORE_DIFF_NORMAL);
    CoreEventView event;
    int32_t population;
    int died = 0;
    int left = 0;
    int ate = 0;
    CHECK(world != NULL);
    population = world->villager_count;
    world->villagers[0]->needs.hunger = 10;
    CHECK(test_run(world, 3));
    ate = world->ledger.today[CORE_RES_FOOD][CORE_LEDGER_CONSUMPTION] < 0;
    CHECK(ate);
    CHECK(world->villagers[0]->needs.hunger >= 70 || world->villager_count != population);
    /* 식량이 없으면 굶다가, 건강이 절반이 되면 죽기 전에 마을을 떠남 (굶어 죽는 주민은 없음) */
    world->bank.amount[CORE_RES_FOOD] = 0;
    /* 모두 떠나면 패배하고 시간이 멈춤 (그 뒤 core_tick 은 거절) */
    CHECK(test_run(world, 144 * 6) || (world->villager_count == 0 && world->outcome == CORE_OUTCOME_LOST &&
                                       world->lose_reason == CORE_LOSE_POPULATION));
    while (core_poll_event(world, &event) == 1) {
        died += event.type == CORE_EVT_VILLAGER_DIED;
        left += event.type == CORE_EVT_VILLAGER_LEFT;
    }
    CHECK_EQ(died, 0);
    CHECK(left > 0);
    CHECK(world->villager_count < population);
    CHECK_EQ(test_validate_verbose(world), 0);
    core_destroy(world);
}

void test_wages_and_taxes(void)
{
    CoreWorld *world = test_make_world(88, CORE_DIFF_NORMAL);
    Building *farm;
    Building *house;
    int32_t gold_before;
    int32_t expected;
    int32_t i;
    CHECK(world != NULL);
    test_give(world, 2000);
    test_flatten(world, world->keep_x + 4, world->keep_y - 3, 8, 8);
    farm = test_build_now(world, CORE_BLD_FARM, world->keep_x + 5, world->keep_y - 2);
    house = test_build_now(world, CORE_BLD_HOUSE, world->keep_x + 9, world->keep_y - 2);
    CHECK(farm != NULL && house != NULL);
    while (clock_minute_of_day(&world->clock) != WORLD_DAILY_MIN - CLOCK_MINUTES_PER_TICK) {
        CHECK_EQ(core_tick(world, 1), CORE_OK);
    }
    gold_before = world->bank.amount[CORE_RES_GOLD];
    expected = house->resident_count * JOBS_TAX_PER_HOUSED - farm->worker_count * BUILTIN_BUILDINGS[CORE_BLD_FARM].wage;
    CHECK_EQ(core_tick(world, 1), CORE_OK); /* 06:00 결산 */
    CHECK_EQ(world->bank.amount[CORE_RES_GOLD], gold_before + expected);
    CHECK_EQ(world->ledger.yesterday[CORE_RES_GOLD][CORE_LEDGER_WAGES], 0); /* 장부가 막 넘어감 */
    CHECK_EQ(world->ledger.today[CORE_RES_GOLD][CORE_LEDGER_WAGES], -farm->worker_count);
    /* 금화가 없으면 임금을 못 받고 기분이 떨어짐 */
    for (i = 0; i < world->villager_count; i++) {
        world->villagers[i]->home = NULL;
    }
    house->resident_count = 0; /* 세금도 없게 (시험용으로 연결을 직접 끊음) */
    world->bank.amount[CORE_RES_GOLD] = 0;
    CHECK_EQ(core_tick(world, 144), CORE_OK);
    CHECK(farm->workers[0]->flags & CORE_VF_UNPAID);
    {
        int32_t terms[CORE_MOOD_TERM_COUNT];
        (void)vil_mood_terms(world, farm->workers[0], terms);
        CHECK(terms[CORE_MOOD_WAGES] < 0);
    }
    core_destroy(world);
}

void test_departure_after_low_mood(void)
{
    CoreWorld *world = test_make_world(99, CORE_DIFF_HARSH);
    CoreEventView event;
    Villager *v;
    int32_t id;
    int leaving = 0;
    int left = 0;
    CHECK(world != NULL);
    v = world->villagers[0];
    id = v->id;
    v->mood = 5;
    v->traits[0] = defs_find_trait(&world->defs, "grumbler");
    v->traits[1] = -1;
    /* 이틀 넘게 우울하면 떠나기 시작하고, 가장자리에 도착하면 사라짐.
     * 목표 기분이 낮게 유지되도록 슬픔을 계속 최대로 둡니다 */
    {
        int32_t hour;
        for (hour = 0; hour < 72; hour++) {
            world->mourning = 30;
            CHECK(test_run(world, 6));
        }
    }
    while (core_poll_event(world, &event) == 1) {
        leaving += event.type == CORE_EVT_VILLAGER_LEAVING && event.a == id;
        left += event.type == CORE_EVT_VILLAGER_LEFT && event.a == id;
    }
    CHECK_EQ(leaving, 1);
    CHECK_EQ(left, 1);
    CHECK(entity_find_villager(world, id) == NULL);
    CHECK_EQ(test_validate_verbose(world), 0);
    core_destroy(world);
}

void test_immigration_needs_housing_and_food(void)
{
    CoreWorld *world = test_make_world(111, CORE_DIFF_PEACEFUL);
    int32_t population;
    int32_t i;
    CHECK(world != NULL);
    population = world->villager_count;
    /* 집이 없으면 이주민도 없음 */
    CHECK(test_run(world, 144));
    CHECK_EQ(world->villager_count, population);
    test_give(world, 3000);
    test_flatten(world, world->keep_x + 4, world->keep_y - 4, 10, 10);
    for (i = 0; i < 3; i++) {
        CHECK(test_build_now(world, CORE_BLD_HOUSE, world->keep_x + 5 + (i % 2) * 3, world->keep_y - 3 + (i / 2) * 3) !=
              NULL);
    }
    for (i = 0; i < world->villager_count; i++) {
        world->villagers[i]->mood = 90;
    }
    world->bank.amount[CORE_RES_FOOD] = world->bank.capacity;
    CHECK(test_run(world, 144 * 2));
    CHECK(world->villager_count > population);
    CHECK_EQ(test_validate_verbose(world), 0);
    core_destroy(world);
}

void test_auto_and_manual_assignment(void)
{
    CoreWorld *world = test_make_world(122, CORE_DIFF_NORMAL);
    Building *farm;
    Villager *outsider = NULL;
    int32_t i;
    CHECK(world != NULL);
    test_flatten(world, world->keep_x + 4, world->keep_y - 2, 5, 5);
    farm = test_build_now(world, CORE_BLD_FARM, world->keep_x + 5, world->keep_y - 1);
    CHECK(farm != NULL);
    CHECK_EQ(farm->worker_count, 2);
    for (i = 0; i < world->villager_count; i++) {
        if (world->villagers[i]->workplace == NULL) {
            outsider = world->villagers[i];
            break;
        }
    }
    CHECK(outsider != NULL);
    /* 가득 찬 건물에는 배치 불가 */
    CHECK_EQ(core_assign_worker(world, outsider->id, farm->id), CORE_ERR_REJECTED);
    /* 한 명 빼면 그 사람은 직접 배치 표시가 되어 자동 배치가 다시 넣지 않음 */
    {
        Villager *removed = farm->workers[0];
        CHECK_EQ(core_unassign_worker(world, removed->id), CORE_OK);
        CHECK(removed->workplace == NULL && (removed->flags & CORE_VF_MANUAL));
        CHECK_EQ(farm->worker_count, 1);
        CHECK_EQ(core_assign_worker(world, outsider->id, farm->id), CORE_OK);
        CHECK(outsider->workplace == farm && (outsider->flags & CORE_VF_MANUAL));
        /* 자동 배치에 다시 맡기기 */
        CHECK_EQ(core_unassign_worker(world, outsider->id), CORE_OK);
        CHECK_EQ(core_release_worker(world, removed->id), CORE_OK);
        CHECK_EQ(farm->worker_count, 2);
    }
    CHECK_EQ(core_set_auto_assign(world, farm->id, 2), CORE_ERR_INVALID_ARG);
    CHECK_EQ(core_set_auto_assign(world, farm->id, 0), CORE_OK);
    CHECK_EQ(test_validate_verbose(world), 0);
    core_destroy(world);
}

void test_save_roundtrip_colony(void)
{
    const char *path = "colony.sav";
    CoreWorld *world = test_make_world(133, CORE_DIFF_NORMAL);
    CoreWorld *loaded = NULL;
    uint64_t a = 0;
    uint64_t b = 1;
    int32_t placed;
    int32_t reason;
    int32_t i;
    CHECK(world != NULL);
    test_give(world, 5000);
    test_flatten(world, world->keep_x + 4, world->keep_y - 4, 10, 10);
    CHECK(test_build_now(world, CORE_BLD_FARM, world->keep_x + 5, world->keep_y - 3) != NULL);
    CHECK(test_build_now(world, CORE_BLD_HOUSE, world->keep_x + 9, world->keep_y - 3) != NULL);
    CHECK_EQ(core_place_line(world, CORE_BLD_ROAD, world->keep_x, world->keep_y + 3, world->keep_x + 10,
                             world->keep_y + 3, &placed, &reason),
             CORE_OK);
    CHECK(test_run(world, 200));
    /* 일부는 걷는 도중(도착 안 함)인 상태로 저장 */
    CHECK_EQ(core_tick(world, 7), CORE_OK);
    CHECK_EQ(core_save(world, path), CORE_OK);
    CHECK_EQ(core_load(path, NULL, &loaded, NULL), CORE_OK);
    (void)core_state_hash(world, &a);
    (void)core_state_hash(loaded, &b);
    CHECK(a == b);
    CHECK_EQ(loaded->villager_count, world->villager_count);
    CHECK_EQ(loaded->building_count, world->building_count);
    for (i = 0; i < loaded->villager_count; i++) {
        const Villager *v = loaded->villagers[i];
        CHECK((v->home == NULL) == (world->villagers[i]->home == NULL));
        CHECK(v->home == NULL || v->home->id == world->villagers[i]->home->id);
    }
    /* 같은 보고를 하면 같은 미래 */
    CHECK(test_run(world, 300));
    CHECK(test_run(loaded, 300));
    (void)core_state_hash(world, &a);
    (void)core_state_hash(loaded, &b);
    CHECK(a == b);
    CHECK_EQ(test_validate_verbose(loaded), 0);
    core_destroy(world);
    core_destroy(loaded);
    (void)platform_remove_file(path);
    (void)platform_remove_file("colony.sav.bak");
}

void test_auto_assign_unstaffed_type_first(void)
{
    /* 놀고 있는 주민이 한 명뿐일 때: 먼저 지은 농장의 두 번째 자리보다, 아무도 일하지 않는 종류(치료소)가 먼저 */
    CoreWorld *world = test_make_world(141, CORE_DIFF_NORMAL);
    Building *farm;
    Building *healer;
    Villager *free_one = NULL;
    int32_t i;
    CHECK(world != NULL);
    test_flatten(world, world->keep_x + 4, world->keep_y - 4, 10, 10);
    farm = test_build_now(world, CORE_BLD_FARM, world->keep_x + 5, world->keep_y - 3);
    CHECK(farm != NULL);
    /* 모두 일터에서 빼고 직접 배치 표시(자동 배치가 다시 넣지 않음). 한 명은 농장에 직접, 한 명은 자동에 맡김 */
    for (i = 0; i < world->villager_count; i++) {
        Villager *villager = world->villagers[i];
        entity_unlink_worker(villager);
        villager->flags |= CORE_VF_MANUAL;
    }
    CHECK_EQ(core_assign_worker(world, world->villagers[0]->id, farm->id), CORE_OK);
    free_one = world->villagers[1];
    free_one->flags &= ~CORE_VF_MANUAL;
    healer = test_build_now(world, CORE_BLD_HEALER, world->keep_x + 9, world->keep_y - 3);
    CHECK(healer != NULL);
    jobs_auto_assign(world);
    CHECK(free_one->workplace == healer);
    CHECK_EQ(farm->worker_count, 1);
    CHECK_EQ(test_validate_verbose(world), 0);
    core_destroy(world);
}

/* 가을에는 겨울을 날 식량이 있어야 이주민이 옴 (마일스톤 3 의 규칙, 밸런스를 맞추며 다시 확인) */
void test_autumn_immigration_needs_winter_food(void)
{
    CoreWorld *world = test_make_world(142, CORE_DIFF_NORMAL);
    int32_t food = 0;
    int32_t wood = 0;
    int32_t before;
    int32_t i;
    CHECK(world != NULL);
    test_flatten(world, world->keep_x + 4, world->keep_y - 4, 10, 10);
    CHECK(test_build_now(world, CORE_BLD_HOUSE, world->keep_x + 5, world->keep_y - 3) != NULL);
    CHECK(test_build_now(world, CORE_BLD_HOUSE, world->keep_x + 8, world->keep_y - 3) != NULL);
    for (i = 0; i < world->villager_count; i++) {
        world->villagers[i]->mood = 90;
    }
    /* 가을 첫날 */
    world->clock.tick = CORE_SEASON_AUTUMN * CLOCK_DAYS_PER_SEASON * CLOCK_TICKS_PER_DAY + 6 * 6;
    nature_winter_needs(world, &food, &wood);
    CHECK(food > 0);
    world->bank.capacity = food * 4;
    world->bank.amount[CORE_RES_FOOD] = food - 1;
    before = world->villager_count;
    population_daily(world);
    CHECK_EQ(world->villager_count, before); /* 겨울 식량이 모자라면 오지 않음 */
    world->bank.amount[CORE_RES_FOOD] = food * 2;
    population_daily(world);
    CHECK(world->villager_count > before);
    /* 여름에는 겨울 식량을 따지지 않음 (집을 하나 더 지어 빈자리를 만들고) */
    test_give(world, 500);
    CHECK(test_build_now(world, CORE_BLD_HOUSE, world->keep_x + 11, world->keep_y - 3) != NULL);
    world->clock.tick = CORE_SEASON_SUMMER * CLOCK_DAYS_PER_SEASON * CLOCK_TICKS_PER_DAY + 6 * 6;
    world->bank.amount[CORE_RES_FOOD] = world->villager_count * 6;
    before = world->villager_count;
    population_daily(world);
    CHECK(world->villager_count > before);
    core_destroy(world);
}

void test_debug_stress_town(void)
{
    /* 성능 측정용 큰 마을(core_debug_stress)도 불변 조건을 지켜야 함 */
    CoreWorld *world = test_make_world(143, CORE_DIFF_NORMAL);
    CoreSummaryView summary;
    CoreRaidView raid;
    CHECK(world != NULL);
    CHECK_EQ(core_debug_stress(NULL, 10, 0), CORE_ERR_NULL_ARG);
    CHECK_EQ(core_debug_stress(world, -1, 0), CORE_ERR_INVALID_ARG);
    CHECK_EQ(core_debug_stress(world, 301, 0), CORE_ERR_INVALID_ARG);
    CHECK_EQ(core_debug_stress(world, 10, 65), CORE_ERR_INVALID_ARG);
    CHECK_EQ(core_debug_stress(world, 150, 40), CORE_OK);
    CHECK_EQ(test_validate_verbose(world), 0);
    CHECK_EQ(core_get_summary(world, &summary), CORE_OK);
    CHECK_EQ(summary.population, 150);
    CHECK(summary.housing_capacity >= 150);
    CHECK_EQ(core_get_raid(world, &raid), CORE_OK);
    CHECK(raid.id != 0);
    CHECK_EQ(raid.enemy_count, 40);
    CHECK_EQ(core_debug_stress(world, 150, 40), CORE_ERR_REJECTED); /* 이미 습격 중 */
    test_keep_alive(world);
    CHECK(test_run(world, 144));
    CHECK_EQ(test_validate_verbose(world), 0);
    core_destroy(world);
}
