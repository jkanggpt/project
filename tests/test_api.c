/*
 * test_api.c — 공개 API 가 잘못된 인자에 죽지 않고 오류 코드를 돌려주는지
 */
#include <string.h>

#include "test_util.h"

void test_api_rejects_null_and_bad_args(void)
{
    CoreWorld *world = test_make_world(1, CORE_DIFF_NORMAL);
    CoreSummaryView summary;
    CoreEventView event;
    uint8_t small[16];
    int32_t width = 0;
    int32_t height = 0;
    int32_t line = 7;
    uint64_t hash;
    CoreWorld *loaded = world;

    CHECK(world != NULL);
    CHECK_EQ(core_create(NULL, NULL, &loaded), CORE_ERR_NULL_ARG);
    CHECK(loaded == NULL);
    CHECK_EQ(core_create(NULL, NULL, NULL), CORE_ERR_NULL_ARG);
    CHECK_EQ(core_tick(NULL, 1), CORE_ERR_NULL_ARG);
    CHECK_EQ(core_tick(world, 0), CORE_ERR_INVALID_ARG);
    CHECK_EQ(core_tick(world, -5), CORE_ERR_INVALID_ARG);
    CHECK_EQ(core_tick(world, 144 * 10 + 1), CORE_ERR_INVALID_ARG);
    CHECK_EQ(core_tick(world, 144 * 10), CORE_OK);
    CHECK_EQ(core_get_summary(NULL, &summary), CORE_ERR_NULL_ARG);
    CHECK_EQ(core_get_summary(world, NULL), CORE_ERR_NULL_ARG);
    CHECK_EQ(core_get_map_size(world, NULL, &height), CORE_ERR_NULL_ARG);
    CHECK_EQ(core_get_map_size(world, &width, &height), CORE_OK);
    CHECK_EQ(width * height, 96 * 96);
    CHECK_EQ(core_copy_terrain(world, small, (int32_t)sizeof(small)), CORE_ERR_BUFFER_TOO_SMALL);
    CHECK_EQ(core_copy_terrain(world, NULL, 100000), CORE_ERR_NULL_ARG);
    CHECK_EQ(core_copy_cost_grid(world, small, (int32_t)sizeof(small), NULL), CORE_ERR_BUFFER_TOO_SMALL);
    CHECK_EQ(core_copy_cost_grid(NULL, small, (int32_t)sizeof(small), NULL), CORE_ERR_NULL_ARG);
    CHECK_EQ(core_poll_event(NULL, &event), CORE_ERR_NULL_ARG);
    CHECK_EQ(core_poll_event(world, NULL), CORE_ERR_NULL_ARG);
    CHECK_EQ(core_save(NULL, "x.sav"), CORE_ERR_NULL_ARG);
    CHECK_EQ(core_save(world, NULL), CORE_ERR_NULL_ARG);
    CHECK_EQ(core_save(world, ""), CORE_ERR_INVALID_ARG);
    CHECK_EQ(core_load(NULL, NULL, &loaded, &line), CORE_ERR_NULL_ARG);
    CHECK_EQ(line, 0);
    CHECK_EQ(core_load("x.sav", NULL, NULL, NULL), CORE_ERR_NULL_ARG);
    CHECK_EQ(core_load("", NULL, &loaded, NULL), CORE_ERR_INVALID_ARG);
    CHECK_EQ(core_validate(NULL), CORE_ERR_NULL_ARG);
    CHECK_EQ(core_state_hash(NULL, &hash), CORE_ERR_NULL_ARG);
    CHECK_EQ(core_state_hash(world, NULL), CORE_ERR_NULL_ARG);
    CHECK_EQ(core_get_config(NULL), CORE_ERR_NULL_ARG);
    CHECK(strcmp(core_result_name(CORE_ERR_FORMAT), "CORE_ERR_FORMAT") == 0);
    CHECK(strcmp(core_result_name(12345), "CORE_ERR_UNKNOWN") == 0);
    CHECK_EQ(core_validate(world), 0);
    core_destroy(world);
}

void test_api_struct_sizes_and_config(void)
{
    CoreStructSizes sizes;
    CoreConfigView config;
    CHECK_EQ(core_abi_version(), CORE_ABI_VERSION);
    CHECK_EQ(core_get_struct_sizes(NULL, (int32_t)sizeof(sizes)), CORE_ERR_NULL_ARG);
    CHECK_EQ(core_get_struct_sizes(&sizes, 3), CORE_ERR_STRUCT_SIZE);
    CHECK_EQ(core_get_struct_sizes(&sizes, (int32_t)sizeof(sizes)), CORE_OK);
    CHECK_EQ(sizes.new_game_params, (int32_t)sizeof(CoreNewGameParams));
    CHECK_EQ(sizes.summary_view, (int32_t)sizeof(CoreSummaryView));
    CHECK_EQ(sizes.event_view, 20 + 48);
    /* 외부 호출자 에서 같은 크기가 나와야 하는 값: int 55개 + 자원·한도·변화 6개씩 + 이름 48바이트 */
    CHECK_EQ(sizes.summary_view, (55+3*CORE_RES_COUNT+12)*4+48);
    CHECK_EQ(sizes.config_view, 17 * 4);
    CHECK_EQ(sizes.villager_view, 57*4+48);
    CHECK_EQ(sizes.building_view, 31*4);
    CHECK_EQ(sizes.building_def_view, 44*4+72);
    CHECK_EQ(sizes.trait_def_view, 6 * 4 + 24);
    CHECK_EQ(sizes.placement_view, 22*4);
    CHECK_EQ(sizes.mood_term_view, 8);
    CHECK_EQ(sizes.ledger_view, 2 * CORE_RES_COUNT * CORE_LEDGER_COUNT * 4);
    CHECK_EQ(sizes.market_view, (2 + 4 * CORE_RES_COUNT) * 4);
    CHECK_EQ(sizes.merchant_view, (2 + 5 * CORE_MERCHANT_MAX_DEALS) * 4);
    CHECK_EQ(sizes.enemy_def_view, 10 * 4 + 24);
    CHECK_EQ(sizes.threat_view, 5 * 4);
    CHECK_EQ(sizes.raid_view, 9 * 4);
    CHECK_EQ(sizes.enemy_view, 3 * 4);
    CHECK_EQ(sizes.research_view, 11 * 4 + 24);
    CHECK_EQ(sizes.rank_view, 80);
    CHECK_EQ(sizes.run_stats_view, 12 * 4);
    CHECK_EQ(sizes.difficulty_view, (2 + CORE_RES_COUNT + 5) * 4);
    CHECK_EQ(sizes.save_info_view, 13 * 4 + CORE_NAME_BYTES);

    CHECK_EQ(core_get_config(&config), CORE_OK);
    CHECK_EQ(config.abi_version, CORE_ABI_VERSION);
    CHECK_EQ(config.ticks_per_day * config.minutes_per_tick, 24 * 60);
    CHECK(config.real_ms_per_tick > 0);
    CHECK(config.max_speed >= 3);
    CHECK_EQ(config.resource_count, CORE_RES_COUNT);
    CHECK_EQ(config.terrain_count, CORE_TERRAIN_COUNT);
}

void test_api_events_drain(void)
{
    CoreWorld *world = test_make_world(8, CORE_DIFF_NORMAL);
    CoreEventView event;
    int count = 0;
    int warnings = 0;
    int i;
    CHECK(world != NULL);
    /* 아무도 이벤트를 꺼내지 않아도 큐가 끝없이 커지지 않고, 꺼낼 때 버린 개수를 알려 줍니다.
     * (아무것도 짓지 않으므로 주민이 굶다가 모두 떠나 패배하면, 그 뒤에는 시간이 흐르지 않음) */
    for (i = 0; i < 400; i++) {
        int32_t rc = core_tick(world, 1440);
        if (rc == CORE_ERR_REJECTED) {
            CoreSummaryView summary;
            CHECK_EQ(core_get_summary(world, &summary), CORE_OK);
            CHECK_EQ(summary.outcome, CORE_OUTCOME_LOST);
            CHECK_EQ(summary.lose_reason, CORE_LOSE_POPULATION);
            break;
        }
        CHECK_EQ(rc, CORE_OK);
    }
    CHECK(i > 0);
    CHECK_EQ(core_validate(world), 0);
    while (core_poll_event(world, &event) == 1) {
        count++;
        if (event.type == CORE_EVT_WARNING) {
            warnings++;
        }
    }
    /* 큐는 4096개를 넘지 않음. 넘쳐서 버렸다면 "버림" 경고가 꼭 한 번 (버리는 규칙 자체는 test_basics 가 확인) */
    CHECK(count > 0 && count <= 4097);
    CHECK(warnings <= 1);
    CHECK(count < 4097 || warnings == 1);
    CHECK_EQ(core_poll_event(world, &event), 0);
    CHECK_EQ(event.type, CORE_EVT_NONE);
    core_destroy(world);
}
