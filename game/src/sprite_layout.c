/* sprite_layout.c — 세이브·길찾기는 점유 칸, 그림은 지면 기준점으로 배치합니다. */
#include "sprite_layout.h"
#include <castle_expansion.h>
#include <math.h>

GameSpriteBox game_building_sprite(int type, int x, int y, int w, int h, int image_w, int image_h) {
    static const float rise[CORE_BLD_COUNT] = {24, 16, 8, 16, 12, 12, 16, 16, 16, 24, 32, 8, 16, 16, 36, 0};
    float width = w * GAME_TILE_SIZE, height = h * GAME_TILE_SIZE;
    if (type >= 0 && type < CORE_BLD_COUNT && image_w > 0 && image_h > 0) {
        float scale = fmaxf(width / image_w, (height + rise[type]) / image_h);
        width = image_w * scale;
        height = image_h * scale;
    }
    return (GameSpriteBox){(x + w * 0.5f) * GAME_TILE_SIZE - width * 0.5f, (y + h) * GAME_TILE_SIZE - height,
                           width, height};
}
GameSpriteBox game_expansion_building_sprite(const CoreBuildingView *b, int image_w, int image_h) {
    float width = b->w * 16.0f, height;
    if (b->kind_flags & CORE_EXP_PLOT)
        return (GameSpriteBox){b->x * 16.0f, b->y * 16.0f, width, b->h * 16.0f};
    if (b->kind_flags & (CORE_EXP_WALL | CORE_EXP_GATE))
        return (GameSpriteBox){b->x * 16.0f, b->y * 16.0f - 4, width, b->h * 16.0f + 4};
    height = b->h * 16.0f + (b->worker_slots || b->resident_slots || b->type == CORE_BLD_KEEP ? 24 : 10);
    if (image_w > 0 && image_h > 0) {
        float scale = fmaxf(width / image_w, height / image_h);
        width = image_w * scale;
        height = image_h * scale;
    }
    width = fminf(width, b->w * 16.0f * 1.10f + 4.0f);
    if (image_w > 0 && image_h > 0)
        height = fmaxf(b->h * 16.0f, fminf(height, width * image_h / (float)image_w));
    return (GameSpriteBox){(b->x + b->w * 0.5f) * 16 - width / 2, (b->y + b->h) * 16 - height, width, height};
}
int game_wall_mask(const uint8_t *cost, int width, int height, int x, int y) {
    static const int dx[4] = {0, 1, 0, -1}, dy[4] = {-1, 0, 1, 0};
    int i, mask = 0;
    if (!cost)
        return 0;
    for (i = 0; i < 4; i++) {
        int xx = x + dx[i], yy = y + dy[i];
        if (xx >= 0 && yy >= 0 && xx < width && yy < height) {
            int value = cost[yy * width + xx];
            if (value == CORE_COST_WALL || value == CORE_COST_GATE)
                mask |= 1 << i;
        }
    }
    return mask;
}
GameSpriteBox game_actor_sprite(float feet_x, float feet_y, float height) {
    return (GameSpriteBox){feet_x - height * 0.5f, feet_y - height, height, height};
}
int game_draw_compare(const void *left, const void *right) {
    const GameDrawItem *a = (const GameDrawItem *)left, *b = (const GameDrawItem *)right;
    if (a->depth < b->depth)
        return -1;
    if (a->depth > b->depth)
        return 1;
    if (a->kind != b->kind)
        return a->kind < b->kind ? -1 : 1;
    return (a->key > b->key) - (a->key < b->key);
}
int game_villager_art(int id) {
    /* 직업과 분리해 저장·로드·일터 변경에도 같은 주민의 외형을 유지합니다. */
    return id > 0 ? (id - 1) % GAME_VILLAGER_DESIGNS : 0;
}
int game_walk_facing(float dx, float dy, int previous) {
    if (fabsf(dx) + fabsf(dy) < 0.0001f)
        return previous;
    if (fabsf(dx) >= fabsf(dy))
        return dx > 0 ? FACE_RIGHT : FACE_LEFT;
    return dy > 0 ? FACE_DOWN : FACE_UP;
}
int game_walk_frame(float distance, int moving) {
    if (!moving || !isfinite(distance) || distance < 0)
        return 1;
    return (int)(fmodf(distance, 0.8f) * 5.0f) % 4;
}
GameSpriteFrame game_villager_frame(int id, int facing, float distance, int moving) {
    int direction = facing == FACE_LEFT ? FACE_RIGHT : facing;
    if (direction < FACE_DOWN || direction > FACE_UP)
        direction = FACE_DOWN;
    return (GameSpriteFrame){direction * 4 + game_walk_frame(distance, moving), game_villager_art(id),
                             facing == FACE_LEFT};
}
