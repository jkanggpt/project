/*
 * test_colony.c — 마일스톤 2: 정의 파일, 주민·건물 연결, 배치 규칙, 걷기 핸드셰이크, 경제, 저장
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fixtures/town.h"
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

void test_defs_builtin_roundtrip(void) {
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
    CHECK(memcmp(defs->defs.traits, reloaded->defs.traits,
                 (size_t)defs->defs.trait_count * sizeof(TraitDef)) == 0);
    CHECK_EQ(defs->defs.name_count, reloaded->defs.name_count);
    CHECK_EQ(defs->defs.research_count, reloaded->defs.research_count);
    CHECK(memcmp(defs->defs.research, reloaded->defs.research,
                 (size_t)defs->defs.research_count * sizeof(ResearchDef)) == 0);
    CHECK(memcmp(defs->defs.ranks, reloaded->defs.ranks, sizeof(defs->defs.ranks)) == 0);
    CHECK_EQ(reloaded->defs.warning_count, 0);
    core_defs_destroy(defs);
    core_defs_destroy(reloaded);
}

void test_defs_errors_keep_defaults(void) {
    static const char *const BAD[] = {
        "[building farm]\nrate = 50\nwidth = 9\n",  /* 범위 밖 → 3번째 줄 */
        "[building farm]\nrate = 50\nflavor = 3\n", /* 모르는 필드 → 3번째 줄 */
        "[building castle]\n",                      /* 모르는 건물 → 1번째 줄 */
        "# ok\n\nrate = 5\n",                       /* 섹션 밖 → 3번째 줄 */
        "[building farm]\nproduces = gems\n",       /* 모르는 자원 → 2번째 줄 */
        "[building wall]\nwidth = 2\n", /* 줄 배치는 1×1 이어야 함 → 끝에서 실패 */
    };
    static const int32_t LINES[] = {3, 3, 1, 3, 2, 2};
    CoreDefs *defs = NULL;
    CoreWorld *world = NULL;
    BuildingDef expected[CORE_BLD_COUNT];
    CoreNewGameParams params;
    CoreEventView event;
    int warnings = 0;
    size_t i;
    CHECK_EQ(core_defs_create(&defs), CORE_OK);
    memcpy(expected, defs->defs.buildings, sizeof(expected));
    for (i = 0; i < sizeof(BAD) / sizeof(BAD[0]); i++) {
        int32_t line = 0;
        CHECK_EQ(core_defs_load_text(defs, CORE_DATA_BUILDINGS, BAD[i], (int32_t)strlen(BAD[i]), &line),
                 CORE_ERR_FORMAT);
        if (i < 5) {
            CHECK_EQ(line, LINES[i]);
        }
    }
    /* 실패한 파일은 하나도 적용되지 않았어야 합니다 (rate = 50 이 들어가면 안 됨) */
    CHECK(memcmp(defs->defs.buildings, expected, sizeof(expected)) == 0);
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

void test_defs_traits_and_names_grow(void) {
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

void test_entity_links_both_directions(void) {
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

void test_placement_rules(void) {
    World *w = fx_town_world(21);
    CHECK(w);
    CorePlacementView p;
    int x = w->keep_x + 8, y = w->keep_y + 6, id = 0, reason = 0;
    w->rank = 0;
    for (int yy = y; yy <= y + 5; yy++)
        for (int xx = x; xx <= x + 6; xx++)
            fx_town_ground(w, xx, yy);
    fx_town_connect(w, x + 1, y + 3);
    CHECK_EQ(core_can_place(w, CORE_BLD_KEEP, x, y, &p), CORE_OK);
    CHECK_EQ(p.result, CORE_PLACE_BAD_TYPE);
    core_can_place(w, CORE_BLD_SHACK, -1, 10, &p);
    CHECK_EQ(p.result, CORE_PLACE_OUT_OF_BOUNDS);
    core_can_place(w, CORE_BLD_SHACK, w->keep_x, w->keep_y, &p);
    CHECK_EQ(p.result, CORE_PLACE_OCCUPIED);
    core_can_place(w, CORE_BLD_HOUSE, x, y, &p);
    CHECK_EQ(p.result, CORE_PLACE_LOCKED);
    core_can_place(w, CORE_BLD_SHACK, x, y, &p);
    CHECK_EQ(p.result, CORE_PLACE_OK);
    int logs = w->bank.amount[CORE_RES_LOGS], cost = p.cost[CORE_RES_LOGS];
    CHECK_EQ(core_place_building(w, CORE_BLD_SHACK, x, y, &id, &reason), CORE_OK);
    CHECK_EQ(w->bank.amount[CORE_RES_LOGS], logs - cost);
    Building *b = entity_find_building(w, id);
    CHECK(b);
    CHECK_EQ(b->state, CORE_BSTATE_CONSTRUCTING);
    core_can_place(w, CORE_BLD_SHACK, x, y, &p);
    CHECK_EQ(p.result, CORE_PLACE_OCCUPIED);
    CHECK(test_run(w, w->defs.buildings[b->type].build_ticks));
    CHECK_EQ(b->state, CORE_BSTATE_ACTIVE);
    CHECK_EQ(core_validate(w), 0);
    core_destroy(w);
}

void test_placement_roads_and_paths(void) {
    World *w = fx_town_world(33);
    CHECK(w);
    int x = w->keep_x, y = world_keep(w)->door_y, placed = 0, reason = 0;
    for (int yy = y; yy < y + 8; yy++)
        fx_town_ground(w, x, yy);
    int rev = w->grid.cost_rev;
    CHECK_EQ(core_place_line(w, CORE_BLD_ROAD, x, y, x, y + 7, &placed, &reason), CORE_OK);
    CHECK(placed >= 5);
    CHECK(w->grid.cost_rev > rev);
    CHECK_EQ(w->grid.cost[grid_index(&w->grid, x, y + 5)], 67);
    CHECK_EQ(core_place_line(w, CORE_BLD_ROAD, x, y, x, y + 7, &placed, &reason), CORE_OK);
    CHECK_EQ(placed, 0);
    CHECK_EQ(core_remove_road(w, x, y + 7), CORE_OK);
    CHECK_EQ(core_remove_road(w, x, y + 7), CORE_ERR_NOT_FOUND);
    Building *b = fx_town_building(w, CORE_BLD_HOUSE, 0);
    CHECK(b);
    CHECK(w->expansion->connected[grid_index(&w->grid, b->door_x, b->door_y)]);
    CHECK_EQ(core_remove_road(w, b->door_x, b->door_y), CORE_ERR_REJECTED);
    core_destroy(w);
}

void test_demolish_refunds_and_unlinks(void) {
    World *w = fx_town_world(42);
    CHECK(w);
    Building *b = fx_town_building(w, CORE_BLD_SHACK, 0);
    CHECK(b);
    Villager *v = w->villagers[0];
    CHECK_EQ(entity_link_resident(b, v, 6), CORE_OK);
    int id = b->id, before = w->bank.amount[CORE_RES_LOGS];
    CHECK_EQ(core_demolish(w, id), CORE_OK);
    CHECK(!entity_find_building(w, id));
    CHECK(!v->home);
    CHECK(w->bank.amount[CORE_RES_LOGS] >= before);
    CHECK_EQ(core_demolish(w, w->keep_id), CORE_ERR_REJECTED);
    core_destroy(w);
}

/* ---- 핸드셰이크와 경제 ------------------------------------------------- */

void test_villager_handshake_stale_rev(void) {
    World *w = fx_town_world(43);
    CHECK(w);
    Villager *v = w->villagers[0];
    vil_set_intent(v, CORE_VS_IDLE, NULL, w->keep_x + 5, w->keep_y + 5, 0);
    int rev = v->intent_rev;
    CHECK_EQ(core_villager_arrived(w, v->id, rev - 1), CORE_IGNORED);
    CHECK(!v->at_target);
    CHECK_EQ(core_villager_arrived(w, v->id, rev), CORE_OK);
    CHECK(v->at_target);
    vil_set_intent(v, CORE_VS_IDLE, NULL, w->keep_x + 7, w->keep_y + 5, 0);
    CHECK_EQ(core_villager_arrived(w, v->id, rev), CORE_IGNORED);
    CHECK_EQ(core_villager_unreachable(w, v->id, v->intent_rev), CORE_OK);
    CHECK(v->intent_rev > rev);
    core_destroy(w);
}

void test_production_only_while_working(void) {
    World *w = fx_town_world(44);
    CHECK(w);
    Building *b = fx_town_building(w, CORE_BLD_GARDEN, 0);
    CHECK(b);
    Villager *v = w->villagers[0];
    fx_town_worker(w, b, v);
    CHECK(expansion_working_present(w, b, v));
    int before = b->agriculture_work_milli;
    CHECK_EQ(core_tick(w, 1), CORE_OK);
    CHECK(b->agriculture_work_milli > before);
    before = b->agriculture_work_milli;
    v->at_target = 0;
    v->inside_building = 0;
    v->current_x_milli = 0;
    v->current_y_milli = 0;
    CHECK(!expansion_working_present(w, b, v));
    CHECK_EQ(core_tick(w, 1), CORE_OK);
    CHECK_EQ(b->agriculture_work_milli, before);
    core_destroy(w);
}

void test_consumption_and_starvation(void) {
    World *w = fx_town_world(45);
    CHECK(w);
    int req[CORE_RES_COUNT];
    CHECK_EQ(core_expansion_daily_requirements(w, req), CORE_OK);
    CHECK_EQ(req[CORE_RES_FOOD], w->villager_count * 1000);
    Villager *v = w->villagers[0];
    for (int i = 0; i < w->villager_count; i++) {
        w->villagers[i]->needs.hunger = 0;
        w->villagers[i]->health = 100;
    }
    w->bank.amount[CORE_RES_FOOD] = 0;
    w->expansion->fractional[CORE_RES_FOOD] = 0;
    expansion_people_daily(w);
    CHECK(v->health < 100);
    CHECK(v->health > 0);
    CHECK_EQ(req[CORE_RES_FOOD], 4000);
    core_destroy(w);
}

void test_wages_and_taxes(void) {
    World *w = fx_town_world(46);
    CHECK(w);
    Building *b = fx_town_building(w, CORE_BLD_GATHERER, 0), *h = fx_town_building(w, CORE_BLD_SHACK, 1);
    CHECK(b && h);
    Villager *v = w->villagers[0];
    CHECK_EQ(core_assign_worker(w, v->id, b->id), CORE_OK);
    CHECK_EQ(entity_link_resident(h, v, 6), CORE_OK);
    w->expansion->tax = 2;
    int gold = w->bank.amount[CORE_RES_GOLD];
    expansion_people_daily(w);
    CHECK(w->ledger.today[CORE_RES_GOLD][CORE_LEDGER_TAXES] > 0);
    CHECK(w->ledger.today[CORE_RES_GOLD][CORE_LEDGER_WAGES] < 0);
    CHECK_EQ(w->bank.amount[CORE_RES_GOLD] - gold, w->ledger.today[CORE_RES_GOLD][CORE_LEDGER_TAXES] +
                                                       w->ledger.today[CORE_RES_GOLD][CORE_LEDGER_WAGES]);
    CHECK(!(v->flags & CORE_VF_UNPAID));
    core_destroy(w);
}

void test_departure_after_low_mood(void) {
    World *w = fx_town_world(47);
    CHECK(w);
    while (w->villager_count < 10)
        CHECK(population_create(w, w->keep_x, w->keep_y + 5));
    Villager *v = w->villagers[0];
    v->happiness = 0;
    v->mood = -200;
    v->low_mood_ticks = 5 * 144;
    v->needs.hunger = 0;
    v->long_needs[0] = v->long_needs[1] = v->long_needs[2] = 0;
    w->bank.amount[CORE_RES_FOOD] = 0;
    expansion_people_daily(w);
    CHECK_EQ(v->state, CORE_VS_LEAVING);
    CHECK(!v->workplace && !v->home);
    core_destroy(w);
}

void test_immigration_needs_housing_and_food(void) {
    World *w = fx_town_world(48);
    CHECK(w);
    CoreImmigrationView s;
    w->rank = 1;
    w->bank.amount[CORE_RES_FOOD] = 0;
    CHECK_EQ(core_expansion_immigration(w, &s), CORE_OK);
    CHECK(s.reasons & CORE_IMM_FOOD);
    CHECK(s.reasons & CORE_IMM_HOUSING);
    w->rank = 0;
    CHECK_EQ(core_expansion_immigration(w, &s), CORE_OK);
    CHECK_EQ(s.reasons, 0);
    CHECK_EQ(s.minimum, 2);
    CHECK_EQ(s.maximum, 2);
    core_destroy(w);
}

void test_auto_and_manual_assignment(void) {
    World *w = fx_town_world(49);
    CHECK(w);
    Building *b = fx_town_building(w, CORE_BLD_GATHERER, 0);
    CHECK(b);
    Villager *v = w->villagers[0];
    CHECK_EQ(core_assign_worker(w, v->id, b->id), CORE_OK);
    CHECK(v->workplace == b);
    CHECK(v->flags & CORE_VF_MANUAL);
    CHECK_EQ(core_unassign_worker(w, v->id), CORE_OK);
    expansion_assign(w);
    CHECK(!v->workplace);
    CHECK_EQ(core_release_worker(w, v->id), CORE_OK);
    b->auto_assign = 1;
    expansion_assign(w);
    CHECK(!(v->flags & CORE_VF_MANUAL));
    CHECK_EQ(core_set_auto_assign(w, b->id, 0), CORE_OK);
    CHECK(!b->auto_assign);
    core_destroy(w);
}

void test_save_roundtrip_colony(void) {
    World *w = fx_town_world(52);
    CHECK(w);
    Building *b = fx_town_building(w, CORE_BLD_GARDEN, 0), *h = fx_town_building(w, CORE_BLD_SHACK, 1);
    CHECK(b && h);
    CHECK_EQ(core_assign_worker(w, w->villagers[0]->id, b->id), CORE_OK);
    CHECK_EQ(entity_link_resident(h, w->villagers[0], 6), CORE_OK);
    expansion_rebuild(w);
    CHECK_EQ(core_validate(w), 0);
    fx_content_current_roundtrip(w);
    core_destroy(w);
}

void test_auto_assign_unstaffed_type_first(void) {
    World *w = fx_town_world(50);
    CHECK(w);
    Building *a = fx_town_building(w, CORE_BLD_GATHERER, 0), *b = fx_town_building(w, CORE_BLD_GARDEN, 1);
    CHECK(a && b);
    a->auto_assign = b->auto_assign = 1;
    expansion_assign(w);
    CHECK(a->worker_count > 0);
    CHECK(b->worker_count > 0);
    CHECK(a->worker_count <= w->defs.buildings[a->type].worker_slots);
    CHECK(b->worker_count <= w->defs.buildings[b->type].worker_slots);
    core_destroy(w);
}

/* 가을에는 겨울을 날 식량이 있어야 이주민이 옴 (마일스톤 3 의 규칙, 밸런스를 맞추며 다시 확인) */
void test_autumn_immigration_needs_winter_food(void) {
    World *w = fx_town_world(51);
    CHECK(w);
    CoreImmigrationView a, b;
    w->rank = 1;
    w->clock.tick = 24 * 144 + 360 / 10;
    w->bank.amount[CORE_RES_FOOD] = 0;
    CHECK_EQ(core_expansion_immigration(w, &a), CORE_OK);
    CHECK(a.reasons & CORE_IMM_FOOD);
    w->clock.tick = 36 * 144 + 360 / 10;
    CHECK_EQ(core_expansion_immigration(w, &b), CORE_OK);
    CHECK_EQ(b.minimum, (a.minimum + 1) / 2);
    CHECK_EQ(b.maximum, (a.maximum + 1) / 2);
    CHECK_EQ(b.food_needed, a.food_needed);
    core_destroy(w);
}

void test_debug_stress_town(void) {
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
    CHECK(raid.enemy_count >= 40 && raid.enemy_count <= 96);
    CHECK_EQ(core_debug_stress(world, 150, 40), CORE_ERR_REJECTED); /* 이미 습격 중 */
    test_keep_alive(world);
    CHECK(test_run(world, 144));
    CHECK_EQ(test_validate_verbose(world), 0);
    core_destroy(world);
}
