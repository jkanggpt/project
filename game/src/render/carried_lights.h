/* 친선 인물의 야간 랜턴·횃불과 건물의 고정 불을 그리고 같은 위치에 지역 광원을 둡니다.
   그림은 fire_atlas.h의 칸 규약을 따르며 탐험·연료 등 게임 규칙을 바꾸지 않습니다. */
#ifndef CASTLE_GAME_CARRIED_LIGHTS_H
#define CASTLE_GAME_CARRIED_LIGHTS_H
#include "raylib.h"
#include <stdint.h>
struct GameApp;
typedef struct GameCarriedPose {
    Rectangle source, destination;
    Vector2 flame;
    float z, foot, strength;
    int behind, row;
} GameCarriedPose;
/* 18:00부터 20:00까지 켜지고 05:00부터 07:00까지 꺼지는 0~1 세기입니다. */
float game_carried_strength(int minute);
/* actor는 인물 그림의 세계 사각형, foot은 발 위치(깊이 정렬)입니다. */
GameCarriedPose game_carried_pose(uint32_t identity, int torch, int facing, int pose, Rectangle actor,
                                  float foot, float indoor, float night, float time);
/* behind=1은 인물 앞에서(뒷모습일 때), 0은 인물 뒤에서 호출합니다. 실제로 그렸으면 1입니다. */
int game_carried_light(struct GameApp *app, uint32_t owner, uint32_t identity, int torch, int facing,
                       int pose, Rectangle actor, float foot, float indoor, int behind);
/* row는 GameStationaryRow입니다. flame 위치에 불꽃 중심을 맞추고 강도는 0~1입니다. */
void game_stationary_fire(struct GameApp *app, uint32_t owner, int row, Vector2 flame, float foot,
                          float strength, int mirror);
#endif
