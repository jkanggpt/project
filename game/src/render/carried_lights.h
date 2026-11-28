/* 친선 인물의 야간 랜턴·횃불 위치와 지역 광원을 함께 계산합니다. */
#ifndef CASTLE_GAME_CARRIED_LIGHTS_H
#define CASTLE_GAME_CARRIED_LIGHTS_H
#include "raylib.h"
#include <stdint.h>
struct GameApp;
enum GameCarriedArt { GAME_CARRIED_TORCH, GAME_CARRIED_LANTERN, GAME_CARRIED_FIRE };
typedef struct GameCarriedPose {
    Rectangle source, destination;
    Vector2 flame;
    float z, foot, strength;
    int behind;
} GameCarriedPose;
float game_carried_strength(int minute);
GameCarriedPose game_carried_pose(uint32_t identity, int torch, int facing, int pose, Rectangle actor,
                                  float foot, float indoor, float night, float time);
void game_carried_light(struct GameApp *app, uint32_t owner, uint32_t identity, int torch, int facing,
                        int pose, Rectangle actor, float foot, float indoor, int behind);
void game_carried_flame(struct GameApp *app, uint32_t owner, Vector2 screen, float z, float size,
                        float strength, int lantern);
void game_carried_flame_image(struct GameApp *app, uint32_t owner, Vector2 screen, float z, float size,
                              float strength, int lantern, float foot);
#endif
