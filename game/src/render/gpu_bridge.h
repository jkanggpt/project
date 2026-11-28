/* 게임 스냅숏을 공용 GPU 렌더러에 연결합니다. */
#ifndef CASTLE_GAME_GPU_BRIDGE_H
#define CASTLE_GAME_GPU_BRIDGE_H
#include "raylib.h"
#include <stdint.h>
struct GameApp;
struct CoreBuildingView;
void game_gpu_review_enable(int enabled);
void game_gpu_review_input(void);
int game_gpu_review_view(void);
int game_gpu_visual_minute(struct GameApp *app);
int game_gpu_initialize(struct GameApp *app, const char *assets_directory);
void game_gpu_shutdown(void);
void game_gpu_new_world(void);
int game_gpu_failed(void);
int game_gpu_begin(struct GameApp *app);
int game_gpu_finish(void);
int game_gpu_active(void);
Camera2D game_gpu_camera(struct GameApp *app);
Camera2D game_gpu_screen_camera(struct GameApp *app);
void game_gpu_overlay_begin(void);
void game_gpu_overlay_end(void);
Vector2 game_gpu_canvas_size(void);
void game_gpu_object(int material, uint32_t owner, float foot);
int game_gpu_sprite(Texture2D texture, Rectangle source, Rectangle destination, Color tint);
int game_gpu_floor_sprite(Texture2D texture, Rectangle source, Rectangle destination, Color tint,
                          float angle);
Texture2D game_gpu_clouds(struct GameApp *app);
int game_gpu_lamp(Texture2D atlas, Rectangle source, Rectangle destination, Color tint, float foot,
                  float height, uint32_t owner);
void game_gpu_local_light(uint32_t owner, uint32_t identity, float x, float y, float z, float radius,
                          Color color, float strength);
void game_gpu_building(struct GameApp *app, const struct CoreBuildingView *building, Rectangle picture);
void game_gpu_prop(struct GameApp *app, int cell, int type);
void game_gpu_actor(uint32_t owner, float x, float foot, float width, float height);
void game_gpu_report(void);
int game_gpu_transition_checkpoint(int first);
int game_gpu_validate(void);
unsigned int game_gpu_live_resources(void);
#endif
