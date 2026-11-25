/* sprite_layout.h — 점유 격자와 분리된 그림 영역·발밑 정렬 */
#ifndef CASTLE_SPRITE_LAYOUT_H
#define CASTLE_SPRITE_LAYOUT_H
#include <castle_core.h>
#include <castle_expansion.h>

#define GAME_TILE_SIZE 16.0f
#define GAME_ACTOR_HEIGHT 24.0f
#define GAME_VILLAGER_DESIGNS 48
#define GAME_VILLAGER_DIRECTIONS 3
typedef enum GameFacing { FACE_DOWN, FACE_RIGHT, FACE_UP, FACE_LEFT } GameFacing;
typedef struct GameSpriteFrame {
    int column, row, flip;
} GameSpriteFrame;
typedef struct GameSpriteBox {
    float x, y, w, h;
} GameSpriteBox;
typedef enum GameDrawKind {
    DRAW_PROP,
    DRAW_BUILDING,
    DRAW_VILLAGER,
    DRAW_ENEMY,
    DRAW_ANIMAL,
    DRAW_HIT_FX,
    DRAW_BOAT,
    DRAW_MERCHANT,
    DRAW_BRIDGE_RAIL
} GameDrawKind;
typedef struct GameDrawItem {
    float depth;
    int kind, index, key;
} GameDrawItem;

GameSpriteBox game_building_sprite(int type, int x, int y, int w, int h, int image_w, int image_h);
GameSpriteBox game_actor_sprite(float feet_x, float feet_y, float height);
GameSpriteBox game_expansion_building_sprite(const CoreBuildingView *building, int image_w, int image_h);
int game_rock_marker(const CoreExpansionTileView *tiles,const uint8_t *roads,const uint8_t *blocked,int width,int height,int x,int y);
int game_wall_mask(const uint8_t *cost, int width, int height, int x, int y);
int game_draw_compare(const void *left, const void *right);
int game_villager_art(int id);
int game_walk_facing(float dx, float dy, int previous);
int game_walk_frame(float distance, int moving);
GameSpriteFrame game_villager_frame(int id, int facing, float distance, int moving);
GameSpriteBox game_building_attachment(int type,int rotation);
GameSpriteBox game_boat_seat(int boat_kind,int direction,int seat);
#endif
