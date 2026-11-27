/* tutorial.h — 튜토리얼 안내의 화면 쪽 상태와 연결점
 *
 * 담당: 튜토리얼 작업 (docs/PLAN_MAIN_SCREEN_TUTORIAL.md). 메인 화면 작업은 이 파일과 tutorial.c 를 고치지
 * 않고, "튜토리얼" 단추에서 game_tutorial_start 만 부릅니다. 이 함수의 이름·인자·반환값은 바꾸지 않는
 * 약속입니다. 진행 단계와 완료 판정은 C 코어가 가지며 (core/src/common/tutorial.c), 여기서는 그 상태를
 * 그리기만 합니다.
 */
#ifndef CASTLE_GAME_TUTORIAL_H
#define CASTLE_GAME_TUTORIAL_H

struct GameApp;

typedef struct GameTutorialState {
    int active; /* 1 이면 튜토리얼 게임 중 (자리표시: 튜토리얼 작업이 코어 상태로 바꿈) */
} GameTutorialState;

/* 메인 화면의 "튜토리얼" 단추가 부릅니다. 튜토리얼 게임을 새로 시작해 게임 화면으로 들어가면 1, 실패하면 0
 * (실패 이유는 알림으로 보여 줌). */
int game_tutorial_start(struct GameApp *app);

/* 게임 중(in_game == 1) 매 프레임 한 번, 입력 처리 뒤·시간 진행 전에 부릅니다 (main.c). */
void game_tutorial_update(struct GameApp *app, float delta);

/* HUD·건설 막대·알림 위, 창(패널) 아래에 안내를 그립니다 (ui/panels.c 의 game_render_interface). */
void game_tutorial_draw(struct GameApp *app);

#endif
