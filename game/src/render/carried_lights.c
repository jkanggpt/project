/* 등불과 횃불은 인계 원화를 굽은 아틀라스(fire_atlas.h)를 세계 화소 1:1로 그립니다.
   손잡이(grip)는 손 위치에, 광원은 같은 프레임의 불꽃 중심에 둡니다. */
#include "carried_lights.h"
#include "app.h"
#include "fire_atlas.h"
#include "gpu_bridge.h"
#include "sprite_layout.h"
#include <math.h>

static float clamp_unit(float n) { return fminf(1, fmaxf(0, n)); }
static float smooth(float n) {
    n = clamp_unit(n);
    return n * n * (3 - 2 * n);
}
float game_carried_strength(int minute) {
    minute %= 1440;
    if (minute < 0)
        minute += 1440;
    if (minute >= 1080)
        return smooth((float)(minute - 1080) / 120.0f);
    if (minute <= 420)
        return smooth((float)(420 - minute) / 120.0f);
    return 0;
}
GameCarriedPose game_carried_pose(uint32_t identity, int torch, int facing, int pose, Rectangle actor,
                                  float foot, float indoor, float night, float time) {
    GameCarriedPose result = {0};
    if (facing < FACE_DOWN || facing > FACE_LEFT)
        facing = FACE_DOWN;
    int row = torch                ? GAME_CARRIED_ROW_TORCH
              : facing == FACE_UP   ? GAME_CARRIED_ROW_BACK
              : facing == FACE_DOWN ? GAME_CARRIED_ROW_FRONT
                                    : GAME_CARRIED_ROW_SIDE;
    const GameFireRow *spec = &GAME_CARRIED_ROWS[row];
    /* 아래·왼쪽을 볼 때는 화면 왼쪽 손, 위·오른쪽은 오른쪽 손입니다. 걸음마다 한 화소 흔들립니다. */
    static const int bob[4] = {0, 1, 0, -1};
    float side = facing == FACE_LEFT || facing == FACE_DOWN ? -1.0f : 1.0f;
    float scale = actor.height >= 20 ? 1.0f : .75f;
    float hand_x = floorf(actor.x + actor.width * (.5f + side * .22f) + .5f);
    float hand_y = floorf(actor.y + actor.height * .62f + (float)bob[pose & 3] * (torch ? 1.0f : .5f) + .5f);
    int mirror = facing == FACE_LEFT;
    float cell = GAME_CARRIED_CELL * scale;
    float anchor_x = (mirror ? GAME_CARRIED_CELL - spec->base_x : spec->base_x) * scale;
    result.destination = (Rectangle){hand_x - anchor_x, hand_y - spec->grip_y * scale, cell, cell};
    int frame = ((int)floorf(fmaxf(0, time) * 9) + (int)(identity % 8u)) % GAME_FIRE_FRAMES;
    result.source = (Rectangle){(float)(frame * GAME_CARRIED_CELL), (float)(row * GAME_CARRIED_CELL),
                                (float)(mirror ? -GAME_CARRIED_CELL : GAME_CARRIED_CELL), (float)GAME_CARRIED_CELL};
    float flame_x = mirror ? GAME_CARRIED_CELL - spec->flame_x : spec->flame_x;
    result.flame = (Vector2){result.destination.x + flame_x * scale, result.destination.y + spec->flame_y * scale};
    result.z = fmaxf(1, (foot - result.flame.y) / .9f);
    result.foot = foot;
    result.row = row;
    result.behind = facing == FACE_UP;
    result.strength = clamp_unit(night) * (1 - smooth(indoor));
    return result;
}
int game_carried_light(GameApp *app, uint32_t owner, uint32_t identity, int torch, int facing, int pose,
                       Rectangle actor, float foot, float indoor, int behind) {
    if (!app || !app->assets.carried_lights.id)
        return 0;
    GameCarriedPose p =
        game_carried_pose(identity, torch, facing, pose, actor, foot, indoor,
                          game_carried_strength(game_gpu_visual_minute(app)), app->session.visual_time);
    if (p.strength <= .001f || p.behind != behind)
        return 0;
    /* 해질녘에는 손에 든 등불이 함께 나타나고, 실내에 들어가면 인물과 같이 사라집니다. */
    Color tint = Fade(WHITE, p.strength);
    if (!game_gpu_lamp(app->assets.carried_lights, p.source, p.destination, tint, p.foot, p.z, owner))
        DrawTexturePro(app->assets.carried_lights, p.source, p.destination, (Vector2){0, 0}, 0, tint);
    float phase = app->session.visual_time * 4.1f + (float)(identity % 251u) * .73f;
    float flicker = .96f + .025f * sinf(phase) + .015f * sinf(phase * 1.71f);
    game_gpu_local_light(owner, identity, p.flame.x, p.flame.y, p.z, torch ? 34.0f : 28.0f,
                         torch ? (Color){255, 163, 68, 255} : (Color){255, 196, 116, 255},
                         p.strength * flicker * (torch ? .70f : .58f));
    return 1;
}
void game_stationary_fire(GameApp *app, uint32_t owner, int row, Vector2 flame, float foot, float strength,
                          int mirror) {
    if (!app || !app->assets.stationary_fires.id || strength <= .001f || row < 0 || row > 3)
        return;
    const GameFireRow *spec = &GAME_STATIONARY_ROWS[row];
    int frame = ((int)floorf(fmaxf(0, app->session.visual_time) * 9) + (int)(owner % 8u)) % GAME_FIRE_FRAMES;
    float flame_x = mirror ? GAME_STATIONARY_CELL - spec->flame_x : spec->flame_x;
    Rectangle source = {(float)(frame * GAME_STATIONARY_CELL), (float)(row * GAME_STATIONARY_CELL),
                        (float)(mirror ? -GAME_STATIONARY_CELL : GAME_STATIONARY_CELL), (float)GAME_STATIONARY_CELL};
    Rectangle destination = {floorf(flame.x - flame_x + .5f), floorf(flame.y - spec->flame_y + .5f),
                             (float)GAME_STATIONARY_CELL, (float)GAME_STATIONARY_CELL};
    float height = fmaxf(1, (foot - flame.y) / .9f);
    Color tint = Fade(WHITE, clamp_unit(strength));
    if (!game_gpu_lamp(app->assets.stationary_fires, source, destination, tint, foot, height, owner))
        DrawTexturePro(app->assets.stationary_fires, source, destination, (Vector2){0, 0}, 0, tint);
}
