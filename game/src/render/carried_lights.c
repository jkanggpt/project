/* 빛과 불꽃은 원화의 같은 손 위치를 사용하며 탐험·연료 규칙을 바꾸지 않습니다. */
#include "carried_lights.h"
#include "app.h"
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
    const float bob[4] = {0, 1, 0, -1};
    float side = facing == FACE_LEFT || facing == FACE_DOWN ? -1.0f : 1.0f;
    float size = fmaxf(6, floorf(actor.height * .42f + .5f));
    float hand_x = actor.x + actor.width * (.5f + side * .15f);
    float hand_y = actor.y + actor.height * (.57f + bob[pose & 3] * .025f);
    result.destination = (Rectangle){floorf(hand_x - size * .5f + .5f),
                                     floorf(hand_y - size * (torch ? .75f : .15625f) + .5f), size, size};
    int frame = ((int)floorf(fmaxf(0, time) * 9) + (int)(identity % 8u)) % 8;
    result.source = (Rectangle){(float)frame * 16.0f, torch ? 16.0f : 0.0f, 16, 16};
    result.flame =
        (Vector2){result.destination.x + size * .5f, result.destination.y + size * (torch ? .25f : .5625f)};
    result.z = fmaxf(1, (actor.y + actor.height - result.flame.y) / .9f);
    result.foot = foot;
    result.behind = facing == FACE_UP;
    result.strength = clamp_unit(night) * (1 - smooth(indoor));
    return result;
}
void game_carried_light(GameApp *app, uint32_t owner, uint32_t identity, int torch, int facing, int pose,
                        Rectangle actor, float foot, float indoor, int behind) {
    if (!app || !app->assets.carried_lights.id)
        return;
    GameCarriedPose p =
        game_carried_pose(identity, torch, facing, pose, actor, foot, indoor,
                          game_carried_strength(game_gpu_visual_minute(app)), app->session.visual_time);
    if (p.strength <= .001f || p.behind != behind)
        return;
    Color tint = Fade(WHITE, p.strength);
    if (!game_gpu_lamp(app->assets.carried_lights, p.source, p.destination, tint, p.foot, p.z, owner))
        DrawTexturePro(app->assets.carried_lights, p.source, p.destination, (Vector2){0, 0}, 0, tint);
    float phase = app->session.visual_time * 4.1f + (float)(identity % 251u) * .73f;
    float flicker = .96f + .025f * sinf(phase) + .015f * sinf(phase * 1.71f);
    game_gpu_local_light(owner, identity, p.flame.x, p.flame.y, p.z, torch ? 30.0f : 26.0f,
                         torch ? (Color){255, 163, 68, 255} : (Color){255, 193, 108, 255},
                         p.strength * flicker * (torch ? .72f : .62f));
}
void game_carried_flame_image(GameApp *app, uint32_t owner, Vector2 screen, float z, float size,
                              float strength, int lantern, float foot) {
    if (!app || !app->assets.carried_lights.id || strength <= .001f)
        return;
    int frame = ((int)floorf(fmaxf(0, app->session.visual_time) * 9) + (int)(owner % 8u)) % 8;
    int enclosed = lantern == GAME_CARRIED_LANTERN;
    Rectangle source = {(float)frame * 16.0f, enclosed ? 0.0f : 16.0f, 16, 16};
    Rectangle destination = {floorf(screen.x - size * .5f + .5f),
                             floorf(screen.y - size * (enclosed ? .5625f : .25f) + .5f), size, size};
    if (lantern == GAME_CARRIED_FIRE) {
        source.height = 8;
        destination.height = size * .5f;
    }
    Color tint = Fade(WHITE, clamp_unit(strength));
    if (!game_gpu_lamp(app->assets.carried_lights, source, destination, tint, foot, z, owner))
        DrawTexturePro(app->assets.carried_lights, source, destination, (Vector2){0, 0}, 0, tint);
}
void game_carried_flame(GameApp *app, uint32_t owner, Vector2 screen, float z, float size, float strength,
                        int lantern) {
    game_carried_flame_image(app, owner, screen, z, size, strength, lantern, screen.y + z * .9f);
    if (!app || !app->assets.carried_lights.id || strength <= .001f)
        return;
    int enclosed = lantern == GAME_CARRIED_LANTERN;
    game_gpu_local_light(owner, owner, screen.x, screen.y, z, enclosed ? 32.0f : 42.0f,
                         enclosed ? (Color){255, 193, 108, 255} : (Color){255, 163, 68, 255}, strength);
}
