/*
 * test_town.h — 마을 테스트 공통 도우미 (test_colony.c, test_pressure.c 가 함께 씀)
 */
#ifndef CASTLE_TEST_TOWN_H
#define CASTLE_TEST_TOWN_H

#include "entity.h"
#include "world.h"

/* 걷는 중인 주민을 모두 목표에 도착시킵니다 (Godot 대신) */
void test_arrive_all(CoreWorld *world);
/* ticks 만큼 한 틱씩 진행하며 매 틱 모두 도착시킵니다. 실패하면 0 */
int test_run(CoreWorld *world, int32_t ticks);
/* 건물을 짓고 다 지어질 때까지 진행합니다. 실패하면 NULL */
Building *test_build_now(CoreWorld *world, int32_t type, int32_t x, int32_t y);
/* (x..x+w, y..y+h) 를 풀밭으로 */
void test_flatten(CoreWorld *world, int32_t x, int32_t y, int32_t w, int32_t h);
/* 모든 자원을 amount 로 (물건은 저장 한도까지) */
void test_give(CoreWorld *world, int32_t amount);

#endif
