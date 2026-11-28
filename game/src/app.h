/* app.h — C 그래픽 프런트엔드의 한 창 상태 */
#ifndef CASTLE_GAME_APP_H
#define CASTLE_GAME_APP_H
#include "assets.h"
#include "session.h"
#include "sprite_layout.h"
#include "storage.h"
#include "ui.h"
#include "ui/main_menu.h"
#include "ui/tutorial.h"
#include "ui/test_tools.h"
typedef enum GamePanel {
    PANEL_NONE,
    PANEL_TITLE,
    PANEL_NEW,
    PANEL_PAUSE,
    PANEL_SAVE,
    PANEL_LOAD,
    PANEL_SETTINGS,
    PANEL_HELP,
    PANEL_RESEARCH,
    PANEL_RANK,
    PANEL_MARKET,
    PANEL_MERCHANT,
    PANEL_VILLAGERS,
    PANEL_LEDGER,
    PANEL_ASSIGN,
    PANEL_OUTCOME,
    PANEL_CONFIRM,
    PANEL_TEST_TOOLS
} GamePanel;
typedef struct GameNotice {
    char text[256];
    float age;
    int x, y, has_position;
} GameNotice;
typedef struct GameApp {
    GameSession session;
    GameAssets assets;
    GameSettings settings;
    GameUi ui;
    Camera2D camera;
    GamePanel panel, return_panel;
    GamePanel panel_stack[8];
    int panel_depth;
    int in_game, speed, saved_speed, build_type, category, demolish, selected_building, selected_villager;
    int drag_x, drag_y, dragging, scroll, sort_column, sort_reverse, difficulty, quit, automated, stress,
        rotation;
    int inspector_scroll, build_scroll, profile_home, confirm_entity;
    int assign_shift, raid_scroll, status_height, notice_bottom, notice_top;
    int build_preview_x, build_preview_y;
    int selection_kind, selection_id, skill_tab, raid_alert_id, raid_alert_speed;
    int autosave_day, autosave_pending, shown_outcome, save_count, backup_slot, confirm_action;
    char castle_name[CORE_NAME_BYTES], seed_text[32], message[512], confirm_text[512];
    char user_dir[GAME_PATH_BYTES], save_dir[GAME_PATH_BYTES], settings_path[GAME_PATH_BYTES];
    GameNotice notices[8];
    int notice_count;
    Rectangle toast_areas[8];
    int toast_area_count;
    GameDrawItem *draw_items;
    int draw_capacity;
    unsigned char *decor_blocked;
    int decor_capacity;
    double accumulator;
    /* 기능별 상태는 각자의 헤더에서 늘립니다 (이 구조체에 필드를 더하지 않아도 되게) */
    GameMenuState menu;         /* 메인 화면: ui/main_menu.h */
    GameTutorialState tutorial; /* 튜토리얼: ui/tutorial.h */
    GameTestTools test_tools;   /* 실행 중 검사 도구: ui/test_tools.h */
} GameApp;
int game_hover_building(GameApp *app, Vector2 screen);
float game_ui_facility_details(GameApp *app, const CoreBuildingView *building, float x, float y, float width);
void game_ui_building_hover(GameApp *app);
void game_app_notice(GameApp *app, const char *text);
void game_app_result(GameApp *app, int result, int reason, int placement);
void game_app_open(GameApp *app, GamePanel panel);
void game_app_close(GameApp *app);
int game_app_start(GameApp *app);
int game_app_start_test(GameApp *app);
int game_app_load_test(GameApp *app);
int game_app_copy_test(GameApp *app);
int game_app_save(GameApp *app, int slot);
int game_app_load(GameApp *app, int slot, int backup);
int game_app_latest(GameApp *app);
void game_app_slot_path(GameApp *app, int slot, int backup, char path[GAME_PATH_BYTES]);
void game_render_world(GameApp *app);
Texture2D game_building_preview(const GameApp *app, int type, Rectangle *source);
void game_render_villager_preview(GameApp *app);
void game_render_uniform_preview(GameApp *app);
void game_render_farm_preview(GameApp *app);
void game_render_effect_preview(GameApp *app);
void game_render_rotation_preview(GameApp *app);
void game_render_connection_preview(GameApp *app);
void game_render_free(GameApp *app);
void game_render_interface(GameApp *app);
void game_world_input(GameApp *app, float delta);
void game_select(GameApp *app, int kind, int id);
void game_building_label(const GameSession *session, int id, char *out, size_t capacity);
Color game_stat_color(int value);
float game_person_details(GameApp *app, const CoreVillagerView *villager, float x, float y, float width);
float game_facility_details(GameApp *app, const CoreBuildingView *building, float x, float y, float width);
int game_unit_details(GameApp *app, float x, float *y, float width);
void game_raid_alert(GameApp *app);
int game_ui_merchant_details(GameApp *app, float x, float *y, float width);
float game_ui_services_details(GameApp *app, const CoreBuildingView *building, float x, float y, float width);
void game_portrait(GameApp *app, int appearance, Rectangle box);
void game_stat_numbers(GameApp *app, const int *values, float x, float y, float gap);
const char *game_faction_name(int faction);
#endif
