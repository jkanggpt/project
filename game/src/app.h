/* app.h — C 그래픽 프런트엔드의 한 창 상태 */
#ifndef CASTLE_GAME_APP_H
#define CASTLE_GAME_APP_H
#include "session.h"
#include "storage.h"
#include "assets.h"
#include "ui.h"
#include "sprite_layout.h"
typedef enum GamePanel {
    PANEL_NONE,PANEL_TITLE,PANEL_NEW,PANEL_PAUSE,PANEL_SAVE,PANEL_LOAD,PANEL_SETTINGS,
    PANEL_HELP,PANEL_RESEARCH,PANEL_RANK,PANEL_MARKET,PANEL_MERCHANT,PANEL_VILLAGERS,
    PANEL_LEDGER,PANEL_ASSIGN,PANEL_OUTCOME,PANEL_CONFIRM
} GamePanel;
typedef struct GameNotice { char text[256]; float age; int x,y,has_position; } GameNotice;
typedef struct GameApp {
    GameSession session;
    GameAssets assets;
    GameSettings settings;
    GameUi ui;
    Camera2D camera;
    GamePanel panel,return_panel;
    GamePanel panel_stack[8]; int panel_depth;
    int in_game,speed,saved_speed,build_type,category,demolish,selected_building,selected_villager;
    int drag_x,drag_y,dragging,scroll,sort_column,sort_reverse,difficulty,quit,automated,stress,rotation;
    int inspector_scroll;
    int autosave_day,autosave_pending,shown_outcome,save_count,backup_slot,confirm_action;
    char castle_name[CORE_NAME_BYTES],seed_text[32],message[512],confirm_text[512];
    char user_dir[GAME_PATH_BYTES],save_dir[GAME_PATH_BYTES],settings_path[GAME_PATH_BYTES];
    GameNotice notices[8]; int notice_count;
    GameDrawItem *draw_items; int draw_capacity;
    unsigned char *decor_blocked; int decor_capacity;
    double accumulator;
} GameApp;
void game_app_notice(GameApp *app,const char *text);
void game_app_result(GameApp *app,int result,int reason,int placement);
void game_app_open(GameApp *app,GamePanel panel);
void game_app_close(GameApp *app);
int game_app_start(GameApp *app);
int game_app_save(GameApp *app,int slot);
int game_app_load(GameApp *app,int slot,int backup);
int game_app_latest(GameApp *app);
void game_app_slot_path(GameApp *app,int slot,int backup,char path[GAME_PATH_BYTES]);
void game_render_world(GameApp *app);
void game_render_villager_preview(GameApp *app);
void game_render_free(GameApp *app);
void game_render_interface(GameApp *app);
void game_world_input(GameApp *app,float delta);
#endif
