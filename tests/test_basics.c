/*
 * test_basics.c — 난수, 시계, 이벤트 큐, 자원 은행, 플랫폼 파일 함수
 */
#include <stdlib.h>
#include <string.h>

#include "clock.h"
#include "economy.h"
#include "events.h"
#include "mem.h"
#include "platform.h"
#include "rng.h"
#include "test_util.h"

void test_rng_is_deterministic(void)
{
    Rng a;
    Rng b;
    Rng c;
    int i;
    int differs = 0;
    rng_seed(&a, 42u, 7u);
    rng_seed(&b, 42u, 7u);
    rng_seed(&c, 43u, 7u);
    for (i = 0; i < 1000; i++) {
        uint32_t va = rng_next(&a);
        CHECK(va == rng_next(&b));
        if (va != rng_next(&c)) {
            differs++;
        }
    }
    CHECK(differs > 990);
    CHECK((a.inc & 1u) == 1u);
}

void test_rng_range_stays_in_bounds(void)
{
    Rng rng;
    int seen[7];
    int i;
    memset(seen, 0, sizeof(seen));
    rng_seed(&rng, 1u, 1u);
    for (i = 0; i < 7000; i++) {
        int32_t v = rng_range(&rng, -3, 3);
        CHECK(v >= -3 && v <= 3);
        seen[v + 3]++;
    }
    for (i = 0; i < 7; i++) {
        CHECK(seen[i] > 800); /* 고르게 나오는지 대충 확인 (기댓값 1000) */
    }
    CHECK_EQ(rng_range(&rng, 5, 5), 5);
    CHECK_EQ(rng_range(&rng, 9, 2), 9);
    CHECK(rng_chance_permille(&rng, 0) == 0);
    CHECK(rng_chance_permille(&rng, 1000) == 1);
}

void test_clock_math(void)
{
    Clock clock;
    clock.tick = CLOCK_START_TICK;
    CHECK_EQ(clock_day(&clock), 1);
    CHECK_EQ(clock_minute_of_day(&clock), 6 * 60);
    CHECK_EQ(clock_season(&clock), CORE_SEASON_SPRING);
    CHECK_EQ(clock_day_of_season(&clock), 1);
    CHECK_EQ(clock_year(&clock), 1);
    CHECK(!clock_is_night(&clock));

    clock.tick = CLOCK_TICKS_PER_DAY * 8; /* 9일 0시: 여름 1일 */
    CHECK_EQ(clock_day(&clock), 9);
    CHECK_EQ(clock_season(&clock), CORE_SEASON_SUMMER);
    CHECK_EQ(clock_day_of_season(&clock), 1);
    CHECK(clock_is_night(&clock));

    clock.tick = CLOCK_TICKS_PER_DAY * 32 - 1; /* 32일 23:50: 겨울 마지막 날 */
    CHECK_EQ(clock_day(&clock), 32);
    CHECK_EQ(clock_season(&clock), CORE_SEASON_WINTER);
    CHECK_EQ(clock_day_of_season(&clock), 8);
    CHECK_EQ(clock_year(&clock), 1);
    CHECK_EQ(clock_minute_of_day(&clock), 23 * 60 + 50);

    clock.tick++; /* 2년 봄 */
    CHECK_EQ(clock_year(&clock), 2);
    CHECK_EQ(clock_season(&clock), CORE_SEASON_SPRING);

    CHECK(clock_is_night_minute(CLOCK_NIGHT_START_MIN));
    CHECK(!clock_is_night_minute(CLOCK_NIGHT_START_MIN - 10));
    CHECK(clock_is_night_minute(CLOCK_NIGHT_END_MIN - 10));
    CHECK(!clock_is_night_minute(CLOCK_NIGHT_END_MIN));
}

void test_events_grow_and_drop_oldest(void)
{
    EventQueue queue;
    Event event;
    int32_t i;
    CoreEventView view;

    CHECK_EQ(events_init(&queue, 4), CORE_OK);
    /* 감긴 상태에서 커지는 경우: 3개 넣고 2개 빼고 5개 더 넣기 */
    for (i = 1; i <= 3; i++) {
        events_push_day(&queue, CORE_EVT_DAY_STARTED, i);
    }
    CHECK(events_pop(&queue, &event) && event.as.day.day == 1);
    CHECK(events_pop(&queue, &event) && event.as.day.day == 2);
    for (i = 4; i <= 8; i++) {
        events_push_day(&queue, CORE_EVT_DAY_STARTED, i);
    }
    CHECK(queue.capacity >= 6);
    for (i = 3; i <= 8; i++) {
        CHECK(events_pop(&queue, &event));
        CHECK_EQ(event.as.day.day, i);
    }
    CHECK(!events_pop(&queue, &event));

    /* 최대 크기를 넘기면 가장 오래된 것을 버리고, 꺼낼 때 경고를 먼저 줍니다 */
    for (i = 0; i < EVENTS_MAX_CAPACITY + 10; i++) {
        events_push_day(&queue, CORE_EVT_DAY_STARTED, i);
    }
    CHECK_EQ(queue.capacity, EVENTS_MAX_CAPACITY);
    CHECK(events_pop(&queue, &event));
    CHECK_EQ(event.type, CORE_EVT_WARNING);
    CHECK_EQ(event.as.warning.code, CORE_WARN_EVENTS_DROPPED);
    CHECK_EQ(event.as.warning.detail, 10);
    CHECK(events_pop(&queue, &event));
    CHECK_EQ(event.as.day.day, 10); /* 0..9 는 버려짐 */

    events_push_season(&queue, CORE_SEASON_AUTUMN, 3);
    while (events_pop(&queue, &event) && event.type != CORE_EVT_SEASON_CHANGED) {
    }
    events_to_view(&event, &view);
    CHECK_EQ(view.type, CORE_EVT_SEASON_CHANGED);
    CHECK_EQ(view.a, CORE_SEASON_AUTUMN);
    CHECK_EQ(view.b, 3);
    events_free(&queue);
}

void test_economy_bank_rules(void)
{
    ResourceBank bank;
    int32_t cost[CORE_RES_COUNT];

    eco_bank_init(&bank, CORE_DIFF_NORMAL);
    CHECK(bank.capacity > 0);
    CHECK(bank.amount[CORE_RES_GOLD] > 0);

    /* 물건은 저장 한도에서 잘림 */
    bank.amount[CORE_RES_WOOD] = bank.capacity - 5;
    CHECK_EQ(eco_bank_add(&bank, CORE_RES_WOOD, 20), 5);
    CHECK_EQ(bank.amount[CORE_RES_WOOD], bank.capacity);
    /* 금화는 한도가 없음 */
    CHECK_EQ(eco_bank_add(&bank, CORE_RES_GOLD, 5000), 5000);
    /* 0 아래로 내려가지 않음 */
    CHECK_EQ(eco_bank_add(&bank, CORE_RES_FOOD, -100000), -60);
    CHECK_EQ(bank.amount[CORE_RES_FOOD], 0);
    /* 잘못된 자원 번호는 무시 */
    CHECK_EQ(eco_bank_add(&bank, 99, 10), 0);

    memset(cost, 0, sizeof(cost));
    cost[CORE_RES_GOLD] = 100;
    cost[CORE_RES_FOOD] = 1;
    CHECK(!eco_bank_can_afford(&bank, cost));
    CHECK(!eco_bank_spend(&bank, cost)); /* 식량이 0이라 아무것도 빠지지 않음 */
    CHECK_EQ(bank.amount[CORE_RES_GOLD], 5200);
    cost[CORE_RES_FOOD] = 0;
    CHECK(eco_bank_spend(&bank, cost));
    CHECK_EQ(bank.amount[CORE_RES_GOLD], 5100);

    eco_bank_init(&bank, 99); /* 잘못된 난이도 → 보통 */
    CHECK_EQ(bank.amount[CORE_RES_GOLD], 200);
}

void test_platform_replace_and_copy(void)
{
    const char *a = "platform_a.txt";
    const char *b = "platform_b.txt";
    const char *c = u8"플랫폼_복사본.txt";
    FILE *file;
    size_t size = 0;
    char *data;

    (void)platform_remove_file(b);
    file = platform_fopen_utf8(a, "wb");
    CHECK(file != NULL);
    fputs("first", file);
    fclose(file);
    file = platform_fopen_utf8(b, "wb");
    CHECK(file != NULL);
    fputs("second", file);
    fclose(file);

    /* 대상이 있어도 바꿔치기가 되어야 합니다 (Windows rename 의 함정) */
    CHECK_EQ(platform_replace_file(a, b), 0);
    CHECK(!platform_file_exists(a));
    data = test_read_file(b, &size);
    CHECK(data != NULL);
    CHECK(size == 5 && memcmp(data, "first", 5) == 0);
    free(data);

    CHECK_EQ(platform_copy_file(b, c), 0);
    CHECK(platform_file_exists(c));
    CHECK(platform_file_exists(b));
    CHECK_EQ(platform_copy_file("does_not_exist.txt", c), -1);
    CHECK(platform_fopen_utf8("no_such_dir/file.txt", "rb") == NULL);
    CHECK_EQ(platform_make_dir(u8"플랫폼_폴더"), 0);
    CHECK_EQ(platform_make_dir(u8"플랫폼_폴더"), 0); /* 이미 있어도 성공 */
    CHECK(!platform_file_exists(u8"플랫폼_폴더"));   /* 폴더는 "파일"이 아님 */
    CHECK_EQ(platform_remove_file(b), 0);
    CHECK_EQ(platform_remove_file(c), 0);
    CHECK_EQ(platform_remove_file(c), -1);
}
