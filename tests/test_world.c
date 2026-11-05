/*
 * test_world.c — 월드 생성·해제, 틱, 결정성, 메모리 부족, 불변 조건 검사기
 */
#include <string.h>

#include "mem.h"
#include "test_util.h"
#include "validate.h"
#include "world.h"

void test_world_create_and_destroy(void)
{
    CoreWorld *world = test_make_world(99, CORE_DIFF_HARSH);
    CoreSummaryView summary;
    CHECK(world != NULL);
    CHECK_EQ(test_validate_verbose(world), 0);
    CHECK_EQ(core_get_summary(world, &summary), CORE_OK);
    CHECK_EQ(summary.map_width, 96);
    CHECK_EQ(summary.map_height, 96);
    CHECK_EQ(summary.day, 1);
    CHECK_EQ(summary.minute_of_day, 6 * 60);
    CHECK_EQ(summary.difficulty, CORE_DIFF_HARSH);
    CHECK_EQ(summary.keep_x, 48);
    CHECK_EQ(summary.keep_size, 3);
    CHECK(strcmp(summary.castle_name, "시험성") == 0);
    CHECK(summary.cost_grid_rev >= 1);
    core_destroy(world);
    core_destroy(NULL); /* NULL 해제는 아무 일도 하지 않아야 함 */
}

void test_world_rejects_bad_params(void)
{
    CoreNewGameParams params;
    CoreWorld *world = (CoreWorld *)&params; /* 실패하면 NULL 로 바뀌어야 함 */

    memset(&params, 0, sizeof(params));
    strcpy(params.castle_name, "성");
    params.seed = 1;
    params.difficulty = 3;
    CHECK_EQ(core_create(&params, &world), CORE_ERR_INVALID_ARG);
    CHECK(world == NULL);

    params.difficulty = -1;
    CHECK_EQ(core_create(&params, &world), CORE_ERR_INVALID_ARG);
    params.difficulty = CORE_DIFF_NORMAL;
    params.seed = -5;
    CHECK_EQ(core_create(&params, &world), CORE_ERR_INVALID_ARG);
    params.seed = 5;

    params.castle_name[0] = '\0'; /* 빈 이름 */
    CHECK_EQ(core_create(&params, &world), CORE_ERR_INVALID_ARG);
    memset(params.castle_name, 'A', sizeof(params.castle_name)); /* '\0' 이 없음 */
    CHECK_EQ(core_create(&params, &world), CORE_ERR_INVALID_ARG);
    strcpy(params.castle_name, "bad\nname"); /* 제어 문자 */
    CHECK_EQ(core_create(&params, &world), CORE_ERR_INVALID_ARG);
    strcpy(params.castle_name, "\xEC\x84"); /* 잘린 UTF-8 */
    CHECK_EQ(core_create(&params, &world), CORE_ERR_INVALID_ARG);
    strcpy(params.castle_name, "\xC0\xAF"); /* 너무 긴 표현 */
    CHECK_EQ(core_create(&params, &world), CORE_ERR_INVALID_ARG);
    strcpy(params.castle_name, "\xED\xA0\x80"); /* 서로게이트 */
    CHECK_EQ(core_create(&params, &world), CORE_ERR_INVALID_ARG);
    CHECK(world == NULL);

    memset(params.castle_name, 0, sizeof(params.castle_name));
    memset(params.castle_name, 'B', sizeof(params.castle_name) - 1); /* 최대 길이 47바이트는 허용 */
    CHECK_EQ(core_create(&params, &world), CORE_OK);
    CHECK(world != NULL);
    core_destroy(world);
}

void test_world_tick_emits_events(void)
{
    CoreWorld *world = test_make_world(5, CORE_DIFF_NORMAL);
    CoreEventView event;
    CoreSummaryView summary;
    int day_started = 0;
    int night = 0;
    int dawn = 0;
    int season = 0;
    int32_t rc;
    int32_t days;

    CHECK(world != NULL);
    /* 06:00 → 다음 날 06:00 */
    CHECK_EQ(core_tick(world, 144), CORE_OK);
    while ((rc = core_poll_event(world, &event)) == 1) {
        if (event.type == CORE_EVT_DAY_STARTED) {
            day_started++;
            CHECK_EQ(event.a, 2);
        } else if (event.type == CORE_EVT_NIGHT_FELL) {
            night++;
            CHECK_EQ(event.a, 1);
        } else if (event.type == CORE_EVT_DAWN) {
            dawn++;
            CHECK_EQ(event.a, 2);
        }
    }
    CHECK_EQ(rc, 0);
    CHECK_EQ(day_started, 1);
    CHECK_EQ(night, 1);
    CHECK_EQ(dawn, 1);
    CHECK_EQ(core_get_summary(world, &summary), CORE_OK);
    CHECK_EQ(summary.day, 2);
    CHECK_EQ(summary.minute_of_day, 6 * 60);

    /* 7일을 더 가면 9일째 = 여름 */
    for (days = 0; days < 7; days++) {
        CHECK_EQ(core_tick(world, 144), CORE_OK);
    }
    while (core_poll_event(world, &event) == 1) {
        if (event.type == CORE_EVT_SEASON_CHANGED) {
            season++;
            CHECK_EQ(event.a, CORE_SEASON_SUMMER);
            CHECK_EQ(event.b, 1);
        }
    }
    CHECK_EQ(season, 1);
    CHECK_EQ(core_get_summary(world, &summary), CORE_OK);
    CHECK_EQ(summary.season, CORE_SEASON_SUMMER);
    CHECK_EQ(summary.day_of_season, 1);
    CHECK_EQ(test_validate_verbose(world), 0);
    core_destroy(world);
}

void test_world_same_seed_same_state(void)
{
    CoreWorld *a = test_make_world(777, CORE_DIFF_NORMAL);
    CoreWorld *b = test_make_world(777, CORE_DIFF_NORMAL);
    uint64_t hash_a = 0;
    uint64_t hash_b = 1;
    int i;
    CHECK(a != NULL && b != NULL);
    /* 같은 양의 시간을 다른 크기로 나눠 진행해도 결과가 같아야 합니다 */
    CHECK_EQ(core_tick(a, 300), CORE_OK);
    for (i = 0; i < 300; i++) {
        CHECK_EQ(core_tick(b, 1), CORE_OK);
    }
    CHECK_EQ(core_state_hash(a, &hash_a), CORE_OK);
    CHECK_EQ(core_state_hash(b, &hash_b), CORE_OK);
    CHECK(hash_a == hash_b);
    core_destroy(a);
    core_destroy(b);
}

void test_world_out_of_memory_is_clean(void)
{
    CoreNewGameParams params;
    long baseline = mem_test_live_blocks();
    long fail_after;
    int succeeded = 0;

    memset(&params, 0, sizeof(params));
    strcpy(params.castle_name, "메모리성");
    params.seed = 11;
    params.difficulty = CORE_DIFF_NORMAL;

    /* k 번째 할당에서 실패시키며 모든 실패 지점을 지나 봅니다 */
    for (fail_after = 0; fail_after < 100 && !succeeded; fail_after++) {
        CoreWorld *world = (CoreWorld *)&params;
        int32_t rc;
        mem_test_fail_after(fail_after);
        rc = core_create(&params, &world);
        mem_test_fail_after(-1);
        if (rc == CORE_OK) {
            CHECK(world != NULL);
            CHECK_EQ(test_validate_verbose(world), 0);
            core_destroy(world);
            succeeded = 1;
        } else {
            CHECK_EQ(rc, CORE_ERR_OUT_OF_MEMORY);
            CHECK(world == NULL);
        }
        /* 실패하든 성공하든 새는 블록이 없어야 합니다 */
        CHECK_EQ(mem_test_live_blocks(), baseline);
    }
    CHECK(succeeded);
    CHECK(fail_after > 3); /* 실패 지점이 여러 개 있었는지 */
}

static void count_report(void *context, const char *what)
{
    (void)what;
    (*(int *)context)++;
}

void test_validate_detects_corruption(void)
{
    CoreWorld *world = test_make_world(3, CORE_DIFF_NORMAL);
    int reports = 0;
    uint8_t saved_terrain;
    int32_t saved_cost;
    CHECK(world != NULL);
    CHECK_EQ(core_validate(world), 0);

    world->bank.amount[CORE_RES_FOOD] = -1;
    CHECK(core_validate(world) > 0);
    world->bank.amount[CORE_RES_FOOD] = world->bank.capacity + 1;
    CHECK(core_validate(world) > 0);
    world->bank.amount[CORE_RES_FOOD] = 0;
    CHECK_EQ(core_validate(world), 0);

    saved_terrain = world->grid.terrain[0];
    world->grid.terrain[0] = 200;
    CHECK(core_validate(world) > 0);
    world->grid.terrain[0] = saved_terrain;

    saved_cost = world->grid.cost[5];
    world->grid.cost[5] = (uint8_t)(saved_cost + 1);
    CHECK(validate_world(world, count_report, &reports) > 0);
    CHECK(reports > 0);
    world->grid.cost[5] = (uint8_t)saved_cost;

    world->grid.terrain[grid_index(&world->grid, world->keep_x, world->keep_y)] = CORE_TERRAIN_WATER;
    grid_rebuild_costs(&world->grid);
    CHECK(core_validate(world) > 0);
    world->grid.terrain[grid_index(&world->grid, world->keep_x, world->keep_y)] = CORE_TERRAIN_GRASS;
    grid_rebuild_costs(&world->grid);

    world->rng.inc = 2;
    CHECK(core_validate(world) > 0);
    world->rng.inc = 3;
    world->clock.tick = -1;
    CHECK(core_validate(world) > 0);
    world->clock.tick = 0;
    CHECK_EQ(core_validate(world), 0);
    CHECK(validate_world(NULL, NULL, NULL) > 0);
    core_destroy(world);
}
