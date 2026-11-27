/* main_menu.c — 메인 화면 (설명은 main_menu.h)
 *
 * 지금은 연결점만 있습니다. 제목·새 게임 화면(title, new_game)은 아직 ui/panels.c 에 있고,
 * 메인 화면 작업이 이 파일로 옮겨 새로 꾸밉니다.
 */
#include "app.h"

void game_main_menu_update(GameApp *a, float delta) { a->menu.time += delta; }
