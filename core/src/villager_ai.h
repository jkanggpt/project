/*
 * villager_ai.h — 주민의 하루: 욕구, 할 일 고르기, 의도, 도착/길 없음 처리, 기분
 *
 * 핸드셰이크 (SPEC §3.2)
 *   1. C 가 매 틱 주민마다 할 일을 고르고, 상태 번호로 고르는 함수 표 STATE_TABLE[state] 를 부릅니다.
 *   2. 주민은 "의도"(상태, 목표 건물/칸, intent_rev)를 내보냅니다. 의도가 바뀌면 intent_rev 가 1 오릅니다.
 *   3. C 그래픽 프런트엔드 이 길을 찾아 걷게 하고, 도착하면 core_villager_arrived(id, intent_rev) 를 부릅니다.
 *      C 는 rev 가 지금 것과 다르면(그새 의도가 바뀌었으면) 무시합니다.
 *   4. 길이 없으면 core_villager_unreachable(...). C 는 잠시 그 자리에서 기다리게 하고 알림을 냅니다.
 *   5. 일은 WORKING 상태로 일터에 도착해 있을 때만 됩니다.
 */
#ifndef CASTLE_VILLAGER_AI_H
#define CASTLE_VILLAGER_AI_H

#include <stdint.h>

#include "world.h"

#define VIL_HUNGRY_BELOW 35        /* 이보다 배고프면 먹으러 감 */
#define VIL_MEAL_HUNGER 70         /* 한 끼에 채워지는 배고픔 */
#define VIL_UNHAPPY_BELOW 25       /* 이보다 기분이 낮으면 떠날 위기 */
#define VIL_LEAVE_AFTER_TICKS 288  /* 기분이 낮은 채로 이틀이면 떠남 */
#define VIL_STARVING_LEAVE_HEALTH 50 /* 굶어서 건강이 이만큼 떨어지면 죽기 전에 마을을 떠남 */
#define VIL_HOLD_TICKS 18          /* 길이 막히면 3시간 기다렸다가 다시 시도 */
#define VIL_WALK_TIMEOUT_TICKS 144 /* 하루가 지나도 도착 보고가 없으면 길이 없는 것으로 봄 */
#define VIL_BASE_WALK_MILLI 2000   /* 보통 풀밭에서 한 틱에 2칸 */

/* 한 틱: 욕구 → 할 일 고르기 → 상태별 처리. 굶어 죽으면 이 안에서 주민이 지워질 수 있습니다 */
void vil_tick(World *world, Villager *villager);

/* C 그래픽 프런트엔드 의 보고. CORE_OK, CORE_IGNORED(오래된 rev) */
int vil_arrived(World *world, Villager *villager, int32_t intent_rev);
int vil_unreachable(World *world, Villager *villager, int32_t intent_rev);

/* 기분 항목별 값을 terms 에 채우고, 목표 기분(0..100)을 돌려줍니다 */
int32_t vil_mood_terms(const World *world, const Villager *villager, int32_t terms[CORE_MOOD_TERM_COUNT]);
/* 걷는 속도 (보통 풀밭에서 한 틱에 걷는 칸 × 1000) */
int32_t vil_walk_speed(const World *world, const Villager *villager);
/* 주민의 의도를 정합니다. 바뀐 것이 있을 때만 intent_rev 가 오릅니다 */
void vil_set_intent(Villager *villager, int32_t state, Building *target, int32_t x, int32_t y, int32_t flags);
/* (near_x, near_y) 에서 가장 가까운, 걸어서 나갈 수 있는 지도 가장자리 칸 */
void vil_edge_tile(const World *world, int32_t near_x, int32_t near_y, int32_t *out_x, int32_t *out_y);

#endif
