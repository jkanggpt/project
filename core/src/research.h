/*
 * research.h — 연구: 마탑의 마법사가 점수를 모아 연구를 끝내고, 효과를 적용합니다
 *
 * 1) 플레이어가 연구를 고르면(core_start_research) 금화·마정석을 내고 "연구 중"이 됩니다. 한 번에 하나만.
 * 2) 마탑에서 일하는 마법사마다 한 틱에 점수를 모읍니다 (jobs.c 의 JOB_TABLE[마탑] → research_work).
 *    모으는 양 = 마탑의 rate × 일 속도(기분·지능·특성) × (100 + 특성의 연구 속도 %) ÷ 100.
 * 3) 점수가 다 차면 끝: 효과 함수 표(RESEARCH_EFFECTS, 함수 포인터)에서 효과를 골라 적용합니다.
 *
 * 효과는 두 가지 방법으로 적용됩니다.
 * - 건물 정의에 자리가 있는 것(생산률, 저장 한도, 공격, 최대 체력)은 이 월드의 건물 정의를 고칩니다.
 *   그래서 C 그래픽 프런트엔드 의 감시탑, CLI 의 숫자 전투, 생산 계산이 모두 같은 값을 봅니다.
 * - 그 밖의 규칙(시장 차이, 난방, 병, 습격 피해)은 ResearchState 의 값으로 두고 그 규칙을 계산하는 곳이
 * 묻습니다. 효과를 다시 계산할 때는 언제나 "원래 건물 정의(world->base_buildings)에서 시작해 끝낸 연구를 번호
 * 순서로" 적용합니다. 그래서 끝낸 순서와 상관없이, 그리고 불러온 뒤에도 똑같은 값이 나옵니다.
 */
#ifndef CASTLE_RESEARCH_H
#define CASTLE_RESEARCH_H

#include <stdint.h>

#include "castle_core.h"

typedef struct CoreWorld World;
typedef struct Villager Villager;
typedef struct Building Building;

typedef struct ResearchState {
    uint32_t done; /* 끝낸 연구 비트 (1 << 번호). 연구는 최대 32개 */
    int32_t done_count;
    int32_t active;         /* 연구 중인 번호 (-1 = 없음) */
    int32_t progress_milli; /* 모은 점수 × 1000 */
    /* 건물 정의에 자리가 없는 효과 (저장하지 않고 끝낸 연구에서 다시 계산) */
    int32_t market_spread; /* 시장 차이를 이만큼 %p 줄임 */
    int32_t heating_minus; /* 집 한 채의 하룻밤 난방 목재를 이만큼 줄임 */
    int32_t healing_pct;   /* 병을 앓는 시간을 이만큼 % 줄임 */
    int32_t ward_pct;      /* 습격 때 건물이 받는 피해를 이만큼 % 줄임 */
} ResearchState;

void research_init(ResearchState *state);

/* 연구 하나의 상태 (CoreResearchState) */
int32_t research_state_of(const World *world, int32_t index);
/* 연구를 시작합니다 (비용을 냄). CORE_OK 또는 CORE_ERR_REJECTED + *out_reason */
/* 마탑의 마법사 한 명이 일한 한 틱 (jobs.c 의 작업 함수 표가 부름) */
/* 지금 마탑에서 일하는 마법사 수 */
int32_t research_mages(const World *world);
/* 원래 건물 정의에서 시작해 끝낸 연구의 효과를 번호 순서로 다시 적용합니다 (불러올 때, 연구가 끝날 때) */
void research_reapply(World *world);

#endif
