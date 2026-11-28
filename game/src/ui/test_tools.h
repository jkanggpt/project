/* test_tools.h — 메인 메뉴에서 여는 세계 검사 도구 */
#ifndef CASTLE_GAME_TEST_TOOLS_H
#define CASTLE_GAME_TEST_TOOLS_H
#include <castle_test_tools.h>
struct GameApp;
enum GameTestLoad { GAME_TEST_LOAD_NONE, GAME_TEST_LOAD_SAVED, GAME_TEST_COPY_NORMAL };
typedef struct GameTestTools {
    int active, loading, flags, weather, tab, multiplier, raid_type, direction;
    char resources[CORE_RES_COUNT][24];
    char day[16], hour[16], minute[16], population[16], enemies[8];
    char result[256];
} GameTestTools;
void game_test_tools_enter(struct GameApp *app, int active);
void game_test_tools_open(struct GameApp *app);
void game_test_tools_draw(struct GameApp *app);
void game_test_tools_badge(struct GameApp *app);
#endif
