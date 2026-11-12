/*
 * walker.h — CLI 용 "걷기 흉내"
 *
 * 게임에서는 Godot 이 주민을 실제로 걷게 하고 도착하면 core_villager_arrived 를 부릅니다.
 * CLI 에는 화면이 없으므로, 이 모듈이 그 역할을 대신합니다:
 *   - 의도가 바뀐 주민마다 이동 비용 격자 위에서 최단 경로(다익스트라)를 구해 걸리는 틱 수를 계산하고,
 *   - 그만큼 틱이 지나면 core_villager_arrived, 길이 없으면 core_villager_unreachable 을 부릅니다.
 * 그래서 CLI 에서도 도로가 걷는 시간을 줄이고, 걷는 시간이 일하는 시간을 줄입니다.
 */
#ifndef CASTLE_CLI_WALKER_H
#define CASTLE_CLI_WALKER_H

#include <stdint.h>

#include "castle_core.h"

/* 걷는 중인 주민 하나 */
typedef struct WalkEntry {
    int32_t villager_id;
    int32_t intent_rev;
    int32_t ticks_left;
} WalkEntry;

typedef struct Walker {
    WalkEntry *entries;          /* 동적 배열 */
    int32_t count;
    int32_t capacity;
    CoreVillagerView *villagers; /* 뷰 복사 버퍼 (주민 수에 맞춰 realloc) */
    int32_t villager_capacity;
    uint8_t *cost;               /* 이동 비용 격자 복사본 */
    int32_t cost_rev;
    int32_t width;
    int32_t height;
    int32_t *distance;           /* 다익스트라 작업 공간 (칸 수만큼) */
    int32_t *heap;
    int32_t instant;             /* 1 이면 걷지 않고 바로 도착 (테스트용) */
} Walker;

void walker_init(Walker *walker);
void walker_free(Walker *walker);
/* 한 틱이 지난 뒤 부릅니다: 도착/길 없음 보고. 성공 0, 메모리 부족 -1 */
int walker_step(Walker *walker, CoreWorld *world);
/* 한 틱 진행 + 걷기. core_tick 결과를 돌려줌 */
int32_t walker_tick(Walker *walker, CoreWorld *world);
/* (sx,sy) → (tx,ty) 의 경로 비용 (비용 10 = 보통 칸 하나). 길이 없으면 -1, 메모리 부족 -2 */
int32_t walker_path_cost(Walker *walker, int32_t sx, int32_t sy, int32_t tx, int32_t ty);

#endif
