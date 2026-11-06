/*
 * tutorial.h — 튜토리얼 목표 (처음 10분쯤의 체크리스트)
 *
 * 농장 짓기, 주택 짓기, 주민을 직접 배치하기, 성벽 짓기, 첫 도적 습격 버티기.
 * 목표마다 "이루었는가"를 확인하는 함수가 표(TUTORIAL_CHECKS, 함수 포인터)에 있고,
 * 한 시간마다 아직 못 이룬 목표만 확인합니다. 순서와 상관없이 이룬 것부터 체크됩니다.
 * 모두 이루면 작은 보상(금화·마정석)을 줍니다. 진행은 저장됩니다.
 */
#ifndef CASTLE_TUTORIAL_H
#define CASTLE_TUTORIAL_H

#include <stdint.h>

#include "castle_core.h"

typedef struct CoreWorld World;

#define TUTORIAL_REWARD_GOLD 50
#define TUTORIAL_REWARD_MANA 5
#define TUTORIAL_ALL_DONE ((1 << CORE_TUTORIAL_COUNT) - 1)

/* 플레이어가 주민을 일터에 직접 배치했음 (core_assign_worker 가 부름) */
void tutorial_note_assign(World *world);
/* 한 시간마다: 새로 이룬 목표를 체크하고 알림 */
void tutorial_hourly(World *world);

#endif
