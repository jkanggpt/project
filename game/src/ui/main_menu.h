/* main_menu.h — 메인 화면(제목·새 게임) 상태와 연결점
 *
 * 담당: 메인 화면 작업 (docs/PLAN_MAIN_SCREEN_TUTORIAL.md). 튜토리얼 작업은 이 파일과 main_menu.c 를 고치지
 * 않습니다. 상태가 더 필요하면 GameMenuState 에 필드를 더합니다 (app.h 는 고치지 않아도 됩니다).
 */
#ifndef CASTLE_GAME_MAIN_MENU_H
#define CASTLE_GAME_MAIN_MENU_H
#include <castle_core.h>

struct GameApp;

typedef struct GameMenuState {
    float time; /* 게임 밖(메인 화면)에 머문 시간(초). 배경 연출에 씁니다 */
    float simulation_time, save_age, preview_delay;
    int background_ready, selected, credits, saves_checked, latest_slot, latest_backup;
    int pending_action, pending_panel;
    CoreSaveInfoView latest;
    float mouse_x, mouse_y;
    char preview_key[32];
    int preview_ready, preview_seed, map_width, map_height;
    unsigned char preview[64 * 64]; /* 임시 세계를 해제한 뒤 남기는 작은 지형 사본 */
} GameMenuState;

/* 게임 밖일 때(in_game == 0) 매 프레임 한 번, 입력 처리 뒤·그리기 전에 부릅니다 (main.c). */
void game_main_menu_update(struct GameApp *app, float delta);
void game_main_menu_title(struct GameApp *app);
void game_main_menu_new_game(struct GameApp *app);
void game_main_menu_enter(struct GameApp *app);

#endif
