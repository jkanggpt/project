/* 친선 인물의 야간 표시·반전·실내 전환을 GPU 없이 검사합니다. */
#include "app.h"
#include "carried_lights.h"
#include "gpu_bridge.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

static int sprites, lights, errors;
static float last_strength;
int game_gpu_visual_minute(const GameApp *app) { return app->session.summary.minute_of_day; }
int game_gpu_lamp(Texture2D atlas, Rectangle source, Rectangle destination, Color tint, float foot,
                  float height, uint32_t owner) {
    errors += !atlas.id || source.x < 0 || source.x >= 128 || destination.width < 6 || tint.a == 0 ||
              !isfinite(foot) || height < 1 || !owner;
    sprites++;
    return 1;
}
void game_gpu_local_light(uint32_t owner, uint32_t identity, float x, float y, float z, float radius,
                          Color color, float strength) {
    errors += !owner || !identity || !isfinite(x) || !isfinite(y) || z < 1 || radius < 20 ||
              color.r < color.b || strength <= 0 || strength > 1;
    last_strength = strength;
    lights++;
}
static void pair(GameApp *a, uint32_t id, int torch, int facing, float indoor) {
    Rectangle actor = {100, 100, 24, 24};
    game_carried_light(a, id, id, torch, facing, 1, actor, 124, indoor, 1);
    game_carried_light(a, id, id, torch, facing, 1, actor, 124, indoor, 0);
}
int main(void) {
    GameApp *a = calloc(1, sizeof(*a));
    if (!a)
        return 1;
    a->assets.carried_lights = (Texture2D){1, 128, 32, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
    a->session.summary.minute_of_day = 0;
    for (int id = 1; id <= 500; id++)
        pair(a, (uint32_t)id, id % 3 == 0, id % 4, 0);
    errors += sprites != 500 || lights != 500;
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
    for (int torch = 0; torch <= 1; torch++)
        for (int pose = 0; pose < 4; pose++) {
            GameCarriedPose left = game_carried_pose(12, torch, FACE_LEFT, pose, actor, 124, 0, 1, 1);
            GameCarriedPose right = game_carried_pose(12, torch, FACE_RIGHT, pose, actor, 124, 0, 1, 1);
            GameCarriedPose back = game_carried_pose(12, torch, FACE_UP, pose, actor, 124, 0, 1, 1);
            errors += fabsf(left.flame.x + right.flame.x - 224) > .01f || left.flame.y != right.flame.y ||
                      !back.behind || left.behind || left.source.y != (torch ? 16.0f : 0.0f) || left.z <= 0;
        }
    printf("CARRIED LIGHTS: crowd=%d sprites=%d lights=%d clock=1 mirror=1 indoor=1 errors=%d\n",
           stable ? 500 : 0, sprites, lights, errors);
    free(a);
    return errors ? 1 : 0;
}
