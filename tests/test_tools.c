/* test_tools.c — 검사 도구의 일반 세계 격리, 경계값, 저장 왕복과 실제 실행 계층 */
#include "castle_test_tools.h"
#include "castle_expansion.h"
#include "session.h"
#include "world.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define REQUIRE(test)                                                                                        \
    do {                                                                                                     \
        if (!(test)) {                                                                                       \
            fprintf(stderr, u8"검사 실패: %s (%d줄)\n", #test, __LINE__);                                    \
            return 1;                                                                                        \
        }                                                                                                    \
    } while (0)

static CoreWorld *make_world(void) {
    CoreNewGameParams p = {0};
    CoreWorld *w = NULL;
    strcpy(p.castle_name, u8"도구 검사");
    p.seed = 42;
    p.difficulty = CORE_DIFF_NORMAL;
    p.ruleset = CORE_RULES_EXPANSION;
    if (core_create(&p, NULL, &w) != CORE_OK)
        return NULL;
    return w;
}

static uint64_t state(const CoreWorld *w) {
    uint64_t hash = 0;
    (void)core_state_hash(w, &hash);
    return hash;
}

static int normal_world(void) {
    CoreWorld *a = make_world(), *b = make_world();
    int32_t flags, weather;
    REQUIRE(a && b);
    REQUIRE(core_test_read(a, &flags, &weather) == CORE_OK && flags == 0 && weather == -1);
    REQUIRE(core_test_resource(a, CORE_RES_GOLD, 1000) == CORE_ERR_REJECTED);
    REQUIRE(core_test_clock(a, 2, 720) == CORE_ERR_REJECTED);
    REQUIRE(core_test_rank(a, 5) == CORE_ERR_REJECTED);
    REQUIRE(core_test_research(a, -1) == CORE_ERR_REJECTED);
    REQUIRE(core_test_population(a, 100) == CORE_ERR_REJECTED);
    REQUIRE(core_test_finish_buildings(a) == CORE_ERR_REJECTED);
    REQUIRE(core_test_restore(a) == CORE_ERR_REJECTED);
    REQUIRE(core_test_raid(a, CORE_THREAT_MONSTERS, 0, 8) == CORE_ERR_REJECTED);
    REQUIRE(core_test_configure(NULL, 0, -1) == CORE_ERR_NULL_ARG);
    REQUIRE(core_test_configure(a, CORE_TEST_FREE_BUILD, -1) == CORE_ERR_INVALID_ARG);
    REQUIRE(core_test_configure(a, CORE_TEST_ALL_FLAGS + 1, -1) == CORE_ERR_INVALID_ARG);
    REQUIRE(core_test_configure(a, CORE_TEST_ENABLED, CORE_WEATHER_COUNT) == CORE_ERR_INVALID_ARG);
    REQUIRE(state(a) == state(b));
    REQUIRE(core_test_configure(a, CORE_TEST_ENABLED, -1) == CORE_OK);
    REQUIRE(state(a) == state(b));
    REQUIRE(core_tick(a, 144) == CORE_OK && core_tick(b, 144) == CORE_OK);
    REQUIRE(state(a) == state(b));
    REQUIRE(core_test_configure(a, 0, -1) == CORE_OK);
    REQUIRE(state(a) == state(b));
    REQUIRE(core_validate(a) == 0);
    core_destroy(a);
    core_destroy(b);
    return 0;
}

static int place(CoreWorld *w, int type, int *id) {
    CoreSummaryView s;
    (void)core_get_summary(w, &s);
    for (int radius = 5; radius < 24; radius++)
        for (int dy = -radius; dy <= radius; dy++)
            for (int dx = -radius; dx <= radius; dx++) {
                CorePlacementView p;
                if (core_can_place(w, type, s.keep_x + dx, s.keep_y + dy, &p) == CORE_OK && !p.result) {
                    int32_t reason;
                    return core_place_building(w, type, s.keep_x + dx, s.keep_y + dy, id, &reason);
                }
            }
    return CORE_ERR_REJECTED;
}

static int economy_building(void) {
    CoreWorld *w = make_world();
    CoreSummaryView s;
    int id = 0;
    REQUIRE(w);
    REQUIRE(core_test_configure(w,
                                CORE_TEST_ENABLED | CORE_TEST_FREE_BUILD | CORE_TEST_UNLOCK_BUILD |
                                    CORE_TEST_REVEAL | CORE_TEST_INSTANT_BUILD,
                                -1) == CORE_OK);
    REQUIRE(core_test_resource(w, CORE_RES_GOLD, 12345) == CORE_OK);
    REQUIRE(core_get_summary(w, &s) == CORE_OK && s.resources[CORE_RES_GOLD] == 12345);
    uint64_t before = state(w);
    REQUIRE(core_test_resource(w, CORE_RES_GOLD, -1) == CORE_ERR_INVALID_ARG);
    REQUIRE(core_test_resource(w, CORE_RES_COUNT, 0) == CORE_ERR_INVALID_ARG);
    REQUIRE(core_test_resource(w, CORE_RES_FOOD, s.resource_capacity[CORE_RES_FOOD] + 1) ==
            CORE_ERR_INVALID_ARG);
    REQUIRE(state(w) == before);
    for (int r = 0; r < CORE_RES_COUNT; r++)
        REQUIRE(core_test_resource(w, r, 0) == CORE_OK);
    REQUIRE(place(w, CORE_BLD_MANSION, &id) == CORE_OK && id > 0);
    REQUIRE(core_get_summary(w, &s) == CORE_OK);
    REQUIRE(s.rank == 0 && s.resources[CORE_RES_GOLD] == 0);
    REQUIRE(w->buildings[w->building_count - 1]->state == CORE_BSTATE_ACTIVE);
    CorePlacementView overlap;
    Building *b = w->buildings[w->building_count - 1];
    REQUIRE(core_can_place(w, CORE_BLD_MANSION, b->x, b->y, &overlap) == CORE_OK);
    REQUIRE(overlap.result == CORE_PLACE_OCCUPIED);
    REQUIRE(core_test_configure(
                w, CORE_TEST_ENABLED | CORE_TEST_FREE_BUILD | CORE_TEST_UNLOCK_BUILD | CORE_TEST_REVEAL,
                -1) == CORE_OK);
    REQUIRE(place(w, CORE_BLD_WAREHOUSE, &id) == CORE_OK);
    REQUIRE(w->buildings[w->building_count - 1]->state == CORE_BSTATE_CONSTRUCTING);
    REQUIRE(core_test_finish_buildings(w) == CORE_OK);
    REQUIRE(w->buildings[w->building_count - 1]->state == CORE_BSTATE_ACTIVE);
    REQUIRE(core_test_research(w, 7) == CORE_OK);
    REQUIRE(core_test_research(w, -1) == CORE_OK && w->research.done_count == 24);
    REQUIRE(core_test_research(w, 24) == CORE_ERR_INVALID_ARG);
    for (int rank = 0; rank < CORE_RANK_COUNT; rank++)
        REQUIRE(core_test_rank(w, rank) == CORE_OK && core_validate(w) == 0);
    REQUIRE(core_test_configure(w, 0, -1) == CORE_OK);
    REQUIRE(place(w, CORE_BLD_MANSION, &id) != CORE_OK);
    REQUIRE(core_validate(w) == 0);
    core_destroy(w);
    return 0;
}

static int clock_people(void) {
    CoreWorld *w = make_world();
    CoreSummaryView s;
    REQUIRE(w);
    REQUIRE(core_test_configure(
                w, CORE_TEST_ENABLED | CORE_TEST_NO_RAIDS | CORE_TEST_SUSTAIN | CORE_TEST_RESOURCES,
                CORE_WEATHER_SNOW) == CORE_OK);
    REQUIRE(core_test_clock(w, 1, 720) == CORE_OK);
    REQUIRE(core_test_clock(w, 1, 360) == CORE_ERR_REJECTED);
    REQUIRE(core_test_clock(w, 0, 720) == CORE_ERR_INVALID_ARG);
    REQUIRE(core_test_clock(w, 1, 721) == CORE_ERR_INVALID_ARG);
    REQUIRE(core_test_clock(w, 1000001, 720) == CORE_ERR_INVALID_ARG);
    REQUIRE(core_test_clock(w, 37, 1380) == CORE_OK);
    REQUIRE(core_get_summary(w, &s) == CORE_OK && s.season == CORE_SEASON_WINTER && s.minute_of_day == 1380);
    REQUIRE(core_tick(w, 12) == CORE_OK);
    REQUIRE(core_get_summary(w, &s) == CORE_OK && s.weather == CORE_WEATHER_SNOW && !s.raid_id);
    REQUIRE(core_test_population(w, 30) == CORE_OK && w->villager_count == 30);
    REQUIRE(core_validate(w) == 0);
    REQUIRE(core_test_population(w, 4) == CORE_OK && w->villager_count == 4);
    REQUIRE(core_test_population(w, 0) == CORE_ERR_INVALID_ARG);
    REQUIRE(core_test_raid(w, CORE_THREAT_MONSTERS, CORE_DIR_EAST, 12) == CORE_OK);
    REQUIRE(core_test_raid(w, CORE_THREAT_ARMY, CORE_DIR_EAST, 2) == CORE_ERR_REJECTED);
    REQUIRE(core_test_raid(w, CORE_THREAT_MONSTERS, 0, 65) == CORE_ERR_INVALID_ARG);
    REQUIRE(core_test_raid(w, CORE_THREAT_MONSTERS, 0, 0) == CORE_OK);
    REQUIRE(core_get_summary(w, &s) == CORE_OK && !s.raid_id);
    w->villagers[0]->health = 10;
    w->villagers[0]->water = 5;
    w->villagers[0]->needs.hunger = 5;
    REQUIRE(core_test_restore(w) == CORE_OK);
    REQUIRE(w->villagers[0]->health == 100 && w->villagers[0]->water == 100);
    REQUIRE(core_validate(w) == 0);
    core_destroy(w);
    return 0;
}

static int round_trip(const char *data) {
    GameSession s = {0};
    int32_t flags, weather;
    REQUIRE(game_session_init(&s, data) == CORE_OK);
    REQUIRE(game_session_new(&s, u8"도구 왕복", 42, CORE_DIFF_NORMAL) == CORE_OK);
    for (int cycle = 0; cycle < 5; cycle++) {
        REQUIRE(core_test_configure(s.world, CORE_TEST_ALL_FLAGS, CORE_WEATHER_RAIN) == CORE_OK);
        REQUIRE(core_test_population(s.world, 30) == CORE_OK);
        REQUIRE(game_session_refresh(&s) == CORE_OK && s.actor_count == 30);
        REQUIRE(game_session_step(&s, 60) == CORE_OK);
        REQUIRE(core_test_population(s.world, 4) == CORE_OK);
        REQUIRE(game_session_refresh(&s) == CORE_OK && s.actor_count == 4);
        REQUIRE(core_test_finish_buildings(s.world) == CORE_OK);
        REQUIRE(core_test_research(s.world, -1) == CORE_OK);
        REQUIRE(game_session_checkpoint(&s, 0) == CORE_OK);
        REQUIRE(core_validate(s.world) == 0);
        REQUIRE(core_save(s.world, "test-tools-roundtrip.sav") == CORE_OK);
        uint64_t before = state(s.world);
        REQUIRE(game_session_load(&s, "test-tools-roundtrip.sav", NULL) == CORE_OK);
        REQUIRE(core_test_read(s.world, &flags, &weather) == CORE_OK && !flags && weather == -1);
        REQUIRE(state(s.world) == before);
        REQUIRE(game_session_step(&s, 10) == CORE_OK && core_validate(s.world) == 0);
        REQUIRE(game_session_new(&s, u8"일반 검사", 42, CORE_DIFF_NORMAL) == CORE_OK);
        REQUIRE(core_test_read(s.world, &flags, &weather) == CORE_OK && !flags);
    }
    game_session_free(&s);
    (void)remove("test-tools-roundtrip.sav");
    (void)remove("test-tools-roundtrip.sav.bak");
    return 0;
}

int main(int argc, char **argv) {
    if (argc != 3)
        return 2;
    if (!strcmp(argv[1], "normal"))
        return normal_world();
    if (!strcmp(argv[1], "economy"))
        return economy_building();
    if (!strcmp(argv[1], "clock"))
        return clock_people();
    if (!strcmp(argv[1], "roundtrip"))
        return round_trip(argv[2]);
    return 2;
}
