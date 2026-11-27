/*
 * tutorial.h — 튜토리얼 안내 (메인 화면의 "튜토리얼"로 시작하는 게임)
 *
 * 단계 표(GUIDE_STEPS, 함수 포인터)를 차례로 따라갑니다. 정보 단계는 플레이어가 "다음"을 눌러 넘기고,
 * 행동 단계(텃밭 짓기 등)는 해낸 것을 확인 함수가 알아보면 저절로 넘어갑니다. 지금 단계는 세이브에 남습니다.
 * 처음 목표 5개(모든 게임의 체크리스트)는 world/content.c 의 expansion_goals 가 따로 맡습니다.
 */
#ifndef CASTLE_TUTORIAL_H
#define CASTLE_TUTORIAL_H

#include <stdint.h>

#include "castle_core.h"

typedef struct CoreWorld World;

/* 단계 번호 (저장되는 값이므로 순서를 바꾸면 세이브 형식도 바꿉니다) */
enum {
    GUIDE_WELCOME,
    GUIDE_CAMERA,
    GUIDE_TIME,
    GUIDE_GARDEN,
    GUIDE_SHACK,
    GUIDE_WELL,
    GUIDE_LOGGING,
    GUIDE_ASSIGN,
    GUIDE_ROADS,
    GUIDE_RESOURCES,
    GUIDE_DEFENSE,
    GUIDE_RANK_PANEL,
    GUIDE_PROMOTE,
    GUIDE_DONE,
    GUIDE_STEP_COUNT
};

typedef struct GuideState {
    int32_t active; /* 1: 튜토리얼 게임 (저장) */
    int32_t step;   /* 지금 단계, GUIDE_STEP_COUNT 이면 끝 (저장) */
    int32_t flags;  /* 예약 (저장, 지금은 0) */
    /* 저장하지 않는 캐시: 권하는 자리는 단계나 지도(비용 격자)가 바뀔 때만 다시 찾습니다 */
    int32_t hint_valid, hint_step, hint_type, hint_revision, hint_x, hint_y;
} GuideState;

/* 플레이어가 주민을 일터에 직접 배치했음 (core_assign_worker 가 부름, 처음 목표의 기록) */
void tutorial_note_assign(World *world);
/* 매 틱: 해낸 행동 단계를 넘깁니다 (튜토리얼이 아니면 아무것도 안 함) */
void guide_tick(World *world);
/* 튜토리얼 방어 단계를 마치기 전이면 1 (습격을 미룸) */
int guide_holds_raids(const World *world);

#endif
