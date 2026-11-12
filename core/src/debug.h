/*
 * debug.h — 성능 측정용 큰 마을 (디버그 전용)
 *
 * SPEC §5 의 성능 목표(주민 150명에서 C 틱 1 ms 미만, 주민 150명 + 적 40명에서 60 fps)를 재려면
 * 그만큼 큰 마을이 필요합니다. 봇이 100일 동안 키워도 50~60명이라, 규칙을 건너뛰고 한 번에 만듭니다.
 * 일반 플레이에서는 부르지 않습니다 (CLI --bench, Godot --stress 만). 장부·배치·불변 조건은 그대로 지킵니다.
 */
#ifndef CASTLE_DEBUG_H
#define CASTLE_DEBUG_H

#include <stdint.h>

#include "world.h"

#define DEBUG_MAX_VILLAGERS 300
#define DEBUG_MAX_ENEMIES 64

/* 성채 둘레에 집(주민 넷에 하나), 농장(넷에 하나), 창고(스물다섯에 하나)를 공짜로 바로 짓고, 주민을 villagers 명까지 늘리고,
 * 식량·목재를 채웁니다. enemies > 0 이면 그만큼의 마물 습격을 북쪽에서 바로 시작합니다.
 * CORE_OK, 자리가 모자라 다 짓지 못했으면 그래도 CORE_OK (지은 만큼), 범위 밖이면 CORE_ERR_INVALID_ARG */
int debug_stress(World *world, int32_t villagers, int32_t enemies);

#endif
