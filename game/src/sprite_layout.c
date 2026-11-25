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

/* 광맥 재고는 그대로 두고 표시만 성기게 합니다. 작은 광맥도 4×4 구역마다 대표 한 곳을 남깁니다. */
static uint32_t rock_noise(int x, int y) {
    uint32_t n = (uint32_t)x * 0x9e3779b9u ^ (uint32_t)y * 0x85ebca6bu;
    n ^= n >> 16;
    n *= 0x7feb352du;
    n ^= n >> 15;
    return n;
}
static int rock_kind(const CoreExpansionTileView *tile) {
    return tile->ore_milli > 0 ? tile->ore_kind : CORE_RES_STONE;
}
int game_rock_marker(const CoreExpansionTileView *tiles, const uint8_t *roads, const uint8_t *blocked,
                     int width, int height, int x, int y) {
    if (!tiles || x < 0 || y < 0 || x >= width || y >= height)
        return 0;
    int p = y * width + x;
    const CoreExpansionTileView *tile = &tiles[p];
    if (!tile->rock_variant || (!tile->stone_milli && !tile->ore_milli) || (roads && roads[p]) ||
        (blocked && blocked[p]))
        return 0;
    uint32_t score = rock_noise(x, y);
    if (score % 5 == 0)
        return 1;
    int kind = rock_kind(tile);
    for (int yy = y / 4 * 4; yy < y / 4 * 4 + 4 && yy < height; yy++)
        for (int xx = x / 4 * 4; xx < x / 4 * 4 + 4 && xx < width; xx++) {
            int q = yy * width + xx;
            const CoreExpansionTileView *other = &tiles[q];
            if (!other->rock_variant || (!other->stone_milli && !other->ore_milli) || (roads && roads[q]) ||
                (blocked && blocked[q]) || rock_kind(other) != kind)
                continue;
            uint32_t candidate = rock_noise(xx, yy);
            if (candidate < score || (candidate == score && q < p))
                return 0;
        }
    return 1;
}

/* 같은 부착 좌표로 게임과 카드의 정지 합성 그림을 만듭니다. */
GameSpriteBox game_building_attachment(int type,int rotation) {
    int r=rotation&3;
    if(type==CORE_BLD_WINDMILL) {
        static const GameSpriteBox boxes[4]={{.05f,.01f,.9f,.6f},{.31f,.03f,.64f,.56f},{.05f,.01f,.9f,.6f},{.05f,.03f,.64f,.56f}};
        return boxes[r];
    }
    if(type==CORE_BLD_IRRIGATED_FARM) {
        static const GameSpriteBox boxes[4]={{.80f,.34f,.20f,.42f},{.35f,.65f,.30f,.34f},{0,.34f,.20f,.42f},{.35f,0,.30f,.34f}};
        return boxes[r];
    }
    return (GameSpriteBox){0,0,0,0};
}
GameSpriteBox game_boat_seat(int kind,int direction,int seat) {
    static const float seats[2][4][2][2]={
        {{{.5f,.43f},{.5f,.61f}},{{.43f,.5f},{.61f,.5f}},{{.5f,.41f},{.5f,.59f}},{{.4f,.5f},{.58f,.5f}}},
        {{{.45f,.49f},{.57f,.62f}},{{.48f,.44f},{.62f,.55f}},{{.43f,.42f},{.57f,.54f}},{{.4f,.45f},{.54f,.56f}}}
    };
    const float *p=seats[kind!=0][direction&3][seat!=0];
    return (GameSpriteBox){p[0],p[1],0,0};
}
