/*
 * clock.c — 게임 속 시간 계산 (설명은 clock.h)
 */
#include "clock.h"

int32_t clock_day(const Clock *clock) { return clock->tick / CLOCK_TICKS_PER_DAY + 1; }

int32_t clock_minute_of_day(const Clock *clock) {
    return (clock->tick % CLOCK_TICKS_PER_DAY) * CLOCK_MINUTES_PER_TICK;
}

/* 1년 안에서 몇 번째 날인지 (0부터) */
static int32_t day_index_in_year(const Clock *clock) { return (clock_day(clock) - 1) % CLOCK_DAYS_PER_YEAR; }

int32_t clock_season(const Clock *clock) { return day_index_in_year(clock) / CLOCK_DAYS_PER_SEASON; }

int32_t clock_day_of_season(const Clock *clock) {
    return day_index_in_year(clock) % CLOCK_DAYS_PER_SEASON + 1;
}

int32_t clock_year(const Clock *clock) { return (clock_day(clock) - 1) / CLOCK_DAYS_PER_YEAR + 1; }

int clock_is_night_minute(int32_t minute_of_day) {
    /* 밤은 자정을 넘어가므로 두 구간으로 나눠서 봅니다: 21:00~24:00, 00:00~05:00 */
    return minute_of_day >= CLOCK_NIGHT_START_MIN || minute_of_day < CLOCK_NIGHT_END_MIN;
}

int clock_is_night(const Clock *clock) { return clock_is_night_minute(clock_minute_of_day(clock)); }
