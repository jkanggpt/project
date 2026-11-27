/* tutorial.h — 튜토리얼 안내의 화면 쪽 상태와 연결점
 *
 * 담당: 튜토리얼 작업 (docs/PLAN_MAIN_SCREEN_TUTORIAL.md). 메인 화면 작업은 이 파일과 tutorial.c 를 고치지
 * 않고, "튜토리얼" 단추에서 game_tutorial_start 만 부릅니다. 이 함수의 이름·인자·반환값은 바꾸지 않는
 * 약속입니다. 진행 단계와 완료 판정은 C 코어가 가지며 (core/src/common/tutorial.c, core_guide_*), 여기서는
 * 그 상태를 그리고, 정보 단계의 "다음"과 "건너뛰기"를 코어에 요청하기만 합니다.
 */
#ifndef CASTLE_GAME_TUTORIAL_H
#define CASTLE_GAME_TUTORIAL_H

#include "raylib.h"
#include <castle_core.h>

struct GameApp;

/* 강조할 수 있는 화면 요소의 번호 (game_tutorial_anchor) */
enum {
    GAME_ANCHOR_UI = 0,    /* + CoreGuideUi */
    GAME_ANCHOR_TAB = 8,   /* + 건설 분류 (CoreBuildingDefView.category) */
    GAME_ANCHOR_CARD = 32, /* + 건물 종류 */
    GAME_ANCHOR_COUNT = GAME_ANCHOR_CARD + CORE_BLD_COUNT
};

typedef struct GameTutorialState {
    int active;          /* 이번 프레임의 안내가 켜져 있음 (튜토리얼 게임) */
    int last_step;       /* 단계가 바뀐 것을 알아보는 값 (-1 = 아직 없음) */
    int paused_by_guide; /* 정보 단계가 시간을 멈췄으면 1 (resume_speed 로 돌려 놓음) */
    int resume_speed;
    int confirm_skip; /* "건너뛰기"를 한 번 눌러 확인을 묻는 중 */
    int done_notice;  /* 마친 알림을 보였음 */
    Vector2 camera_start;
    float zoom_start, pulse;
    Rectangle panel; /* 안내 창 (지도 클릭을 막음) */
    Rectangle anchors[GAME_ANCHOR_COUNT];
    unsigned char anchor_set[GAME_ANCHOR_COUNT];
    CoreGuideView view; /* 이번 프레임의 안내 (core_guide_view) */
} GameTutorialState;

/* 메인 화면의 "튜토리얼" 단추가 부릅니다. 튜토리얼 게임을 새로 시작해 게임 화면으로 들어가면 1, 실패하면 0
 * (실패 이유는 알림으로 보여 줌). */
int game_tutorial_start(struct GameApp *app);

/* 게임 중(in_game == 1) 매 프레임 한 번, 입력 처리 뒤·시간 진행 전에 부릅니다 (main.c). */
void game_tutorial_update(struct GameApp *app, float delta);

/* HUD·건설 막대·알림 위, 창(패널) 아래에 안내를 그립니다 (ui/panels.c 의 game_render_interface). */
void game_tutorial_draw(struct GameApp *app);

/* 강조할 수 있는 화면 요소가 이번 프레임에 그려진 자리를 알려 줍니다 (HUD·건설 막대가 부름). */
void game_tutorial_anchor(struct GameApp *app, int anchor, Rectangle area);

/* 그 화면 점이 안내 창 위이면 1 (지도 클릭을 막음) */
int game_tutorial_covers(const struct GameApp *app, Vector2 point);

#endif
