/*
 * clock.h — 게임 속 시간 계산
 *
 * 저장하는 값은 총 틱 수(tick) 하나뿐이고, 날짜·시각·계절은 모두 여기서 계산합니다.
 * 값이 하나뿐이라 서로 어긋날 일이 없습니다.
 */
#ifndef CASTLE_CLOCK_H
#define CASTLE_CLOCK_H

#include <stdint.h>

#define CLOCK_MINUTES_PER_TICK 10
#define CLOCK_TICKS_PER_DAY 144 /* 24시간 × 6틱 */
#define CLOCK_DAYS_PER_SEASON 12
#define CLOCK_SEASONS_PER_YEAR 4
#define CLOCK_DAYS_PER_YEAR (CLOCK_DAYS_PER_SEASON * CLOCK_SEASONS_PER_YEAR)
#define CLOCK_START_TICK (6 * 6)        /* 새 게임은 1일 06:00 에 시작 */
#define CLOCK_NIGHT_START_MIN (21 * 60) /* 21:00 부터 밤 */
#define CLOCK_NIGHT_END_MIN (5 * 60)    /* 05:00 에 새벽 */
#define CLOCK_MAX_DAYS 1000000 /* 이보다 오래 진행하면 int32 를 넘을 수 있어 막습니다 */
#define CLOCK_MAX_TICK (CLOCK_MAX_DAYS * CLOCK_TICKS_PER_DAY)

/* 1배속에서 한 틱(게임 속 10분)이 걸리는 실제 시간. 하루 = 72초 */
#define CLOCK_REAL_MS_PER_TICK 500
#define CLOCK_MAX_SPEED 3

typedef struct Clock {
    int32_t tick; /* 1일 00:00 부터 센 총 틱 수 */
} Clock;

int32_t clock_day(const Clock *clock);           /* 1부터 */
int32_t clock_minute_of_day(const Clock *clock); /* 0..1439 */
int32_t clock_season(const Clock *clock);        /* CoreSeason */
int32_t clock_day_of_season(const Clock *clock); /* 1..8 */
int32_t clock_year(const Clock *clock);          /* 1부터 */
int clock_is_night_minute(int32_t minute_of_day);
int clock_is_night(const Clock *clock);

#endif
