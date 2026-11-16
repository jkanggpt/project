/* sprite_layout.h — 점유 격자와 분리된 그림 영역·발밑 정렬 */
#ifndef CASTLE_SPRITE_LAYOUT_H
#define CASTLE_SPRITE_LAYOUT_H
#include "castle_core.h"

#define GAME_TILE_SIZE 16.0f
#define GAME_ACTOR_HEIGHT 24.0f
typedef struct GameSpriteBox { float x,y,w,h; } GameSpriteBox;
typedef enum GameDrawKind { DRAW_PROP,DRAW_BUILDING,DRAW_VILLAGER,DRAW_ENEMY } GameDrawKind;
typedef struct GameDrawItem { float depth; int kind,index,key; } GameDrawItem;

GameSpriteBox game_building_sprite(int type,int x,int y,int w,int h,int image_w,int image_h);
GameSpriteBox game_actor_sprite(float feet_x,float feet_y,float height);
int game_draw_compare(const void *left,const void *right);
int game_villager_art(int job);
#endif
