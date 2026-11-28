/* 친선 인물의 야간 등불: 500명 부착·광원 수, 방향별 행, 좌우 반전, 뒷모습 순서, 해질녘·실내를
   GPU 없이 검사합니다. 광원은 같은 프레임의 불꽃 중심에 있어야 합니다. */
#include "app.h"
#include "carried_lights.h"
#include "fire_atlas.h"
#include "gpu_bridge.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

static int sprites, lights, errors;
static float last_strength;
static Rectangle last_destination;
static Rectangle last_source;
static Vector2 last_light;
int game_gpu_visual_minute(GameApp *app) { return app->session.summary.minute_of_day; }
int game_gpu_lamp(Texture2D atlas, Rectangle source, Rectangle destination, Color tint, float foot,
                  float height, uint32_t owner) {
    errors += !atlas.id || source.x < 0 || source.x >= GAME_CARRIED_CELL * 8 || source.y < 0 ||
              source.y >= GAME_CARRIED_CELL * 4 || destination.width < 11 || tint.a == 0 || !isfinite(foot) ||
              height < 1 || !owner;
    last_destination = destination;
    last_source = source;
    sprites++;
    return 1;
}
void game_gpu_local_light(uint32_t owner, uint32_t identity, float x, float y, float z, float radius,
                          Color color, float strength) {
    errors += !owner || !identity || !isfinite(x) || !isfinite(y) || z < 1 || radius < 20 ||
              color.r < color.b || strength <= 0 || strength > 1;
    last_strength = strength;
    last_light = (Vector2){x, y};
    lights++;
}
static void pair(GameApp *a, uint32_t id, int torch, int facing, float indoor) {
    Rectangle actor = {100, 100, 24, 24};
    game_carried_light(a, id, id, torch, facing, (int)(id % 4), actor, 124, indoor, 1);
    game_carried_light(a, id, id, torch, facing, (int)(id % 4), actor, 124, indoor, 0);
}
int main(void) {
    GameApp *a = calloc(1, sizeof(*a));
    if (!a)
        return 1;
    a->assets.carried_lights =
        (Texture2D){1, GAME_CARRIED_CELL * 8, GAME_CARRIED_CELL * 4, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
    a->session.summary.minute_of_day = 0;
    int placed = 0;
    for (int id = 1; id <= 500; id++) {
        pair(a, (uint32_t)id, id % 3 == 0, id % 4, 0);
        /* 그려진 불꽃 화소 위치와 광원 위치가 일치해야 합니다. */
        int row = (int)(last_source.y / GAME_CARRIED_CELL);
        float flame_x = last_source.width < 0 ? GAME_CARRIED_CELL - GAME_CARRIED_ROWS[row].flame_x
                                              : GAME_CARRIED_ROWS[row].flame_x;
        placed += fabsf(last_destination.x + flame_x - last_light.x) < .01f &&
                  fabsf(last_destination.y + GAME_CARRIED_ROWS[row].flame_y - last_light.y) < .01f;
    }
    errors += sprites != 500 || lights != 500 || placed != 500;
    int stable = errors == 0;
    pair(a, 501, 0, FACE_DOWN, 1);
    errors += sprites != 500 || lights != 500;
    a->session.summary.minute_of_day = 720;
    pair(a, 502, 1, FACE_RIGHT, 0);
    errors += sprites != 500 || lights != 500;
    a->session.summary.minute_of_day = 1140;
    pair(a, 503, 1, FACE_RIGHT, 0);
    errors += sprites != 501 || lights != 501 || last_strength < .30f || last_strength > .39f;
    errors += game_carried_strength(0) != 1 || game_carried_strength(300) != 1 ||
              game_carried_strength(420) != 0 || game_carried_strength(1080) != 0 ||
              game_carried_strength(1200) != 1 || game_carried_strength(-60) != 1;
    Rectangle actor = {100, 100, 24, 24};
    int rows = 0;
    for (int torch = 0; torch <= 1; torch++)
        for (int pose = 0; pose < 4; pose++) {
            GameCarriedPose left = game_carried_pose(12, torch, FACE_LEFT, pose, actor, 124, 0, 1, 1);
            GameCarriedPose right = game_carried_pose(12, torch, FACE_RIGHT, pose, actor, 124, 0, 1, 1);
            GameCarriedPose back = game_carried_pose(12, torch, FACE_UP, pose, actor, 124, 0, 1, 1);
            GameCarriedPose front = game_carried_pose(12, torch, FACE_DOWN, pose, actor, 124, 0, 1, 1);
            /* 좌우 반전: 인물 가운데(112)를 기준으로 불꽃이 대칭이고 높이는 같습니다. */
            errors += fabsf(left.flame.x + right.flame.x - 224) > 1.01f || left.flame.y != right.flame.y ||
                      left.source.width >= 0 || right.source.width <= 0;
            errors += !back.behind || left.behind || front.behind || left.z <= 0;
            rows += torch ? left.row == GAME_CARRIED_ROW_TORCH && back.row == GAME_CARRIED_ROW_TORCH
                          : left.row == GAME_CARRIED_ROW_SIDE && back.row == GAME_CARRIED_ROW_BACK &&
                                front.row == GAME_CARRIED_ROW_FRONT;
            /* 손잡이는 인물 그림 안, 불꽃은 발보다 위에 있습니다. */
            errors += left.flame.y >= 124 || left.destination.y + GAME_CARRIED_ROWS[left.row].grip_y < actor.y ||
                      left.destination.y + GAME_CARRIED_ROWS[left.row].grip_y > actor.y + actor.height;
        }
    errors += rows != 8;
    printf("CARRIED LIGHTS: crowd=%d sprites=%d lights=%d flame_aligned=%d rows=%d clock=1 mirror=1 indoor=1 "
           "errors=%d\n",
           stable ? 500 : 0, sprites, lights, placed, rows, errors);
    free(a);
    return errors ? 1 : 0;
}
