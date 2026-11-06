/*
 * rank.h — 등급과 승패: 촌락 → 마을 → 요새 → 성 → 왕성
 *
 * 다음 등급으로 오르려면 조건(인구, 평균 기분, 방어 점수, 끝낸 연구 수)을 모두 채우고 공물(금화, 마정석)을 냅니다.
 * 조건과 공물은 data/ranks.def 에 있고, 공물은 난이도 %(difficulty.c)를 곱합니다.
 * 오르면 저장 한도가 늘고, 그 등급이 필요한 건물(정의의 required_rank)과 연구가 풀립니다.
 * 왕성이 되면 승리입니다 (계속할 수 있음). 성채가 무너지거나 주민이 한 명도 없으면 패배입니다 (시간이 멈춤).
 */
#ifndef CASTLE_RANK_H
#define CASTLE_RANK_H

#include <stdint.h>

#include "castle_core.h"

typedef struct CoreWorld World;

/* 방어 점수: 다 지은 건물의 defense 합 + 경비병 수 */
int32_t rank_defense(const World *world);
/* 다음 등급의 조건과 지금 값 */
void rank_fill_view(const World *world, CoreRankView *out);
/* 지금 다음 등급으로 오를 수 있는가 (조건과 공물 모두) */
int rank_is_ready(const World *world);
/* 다음 등급으로 오릅니다. CORE_OK 또는 CORE_ERR_REJECTED + *out_reason */
int rank_up(World *world, int32_t *out_reason);
/* 지금 등급까지 승급으로 늘어난 저장 한도 */
int32_t rank_storage_bonus(const World *world);
/* 한 시간마다: 승급 조건을 처음 채우면 알림 (등급마다 한 번), 패배 확인 */
void rank_hourly(World *world);
/* 패배: 시간이 멈추고 DEFEAT 이벤트 (이미 졌으면 아무것도 안 함) */
void rank_lose(World *world, int32_t reason);

#endif
