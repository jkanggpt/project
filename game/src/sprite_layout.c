/* sprite_layout.c — 세이브·길찾기는 점유 칸, 그림은 지면 기준점으로 배치합니다. */
#include "sprite_layout.h"
#include <math.h>

GameSpriteBox game_building_sprite(int type,int x,int y,int w,int h,int image_w,int image_h)
{
    static const float rise[CORE_BLD_COUNT]={24,16,8,16,12,12,16,16,16,24,32,8,16,16,36,0};
    float width=w*GAME_TILE_SIZE,height=h*GAME_TILE_SIZE;
    if (type>=0 && type<CORE_BLD_COUNT && image_w>0 && image_h>0) {
        float scale=fmaxf(width/image_w,(height+rise[type])/image_h);
        width=image_w*scale; height=image_h*scale;
    }
    return (GameSpriteBox){(x+w*0.5f)*GAME_TILE_SIZE-width*0.5f,(y+h)*GAME_TILE_SIZE-height,width,height};
}
GameSpriteBox game_actor_sprite(float feet_x,float feet_y,float height)
{
    return (GameSpriteBox){feet_x-height*0.5f,feet_y-height,height,height};
}
int game_draw_compare(const void *left,const void *right)
{
    const GameDrawItem *a=(const GameDrawItem *)left,*b=(const GameDrawItem *)right;
    if (a->depth<b->depth) return -1;
    if (a->depth>b->depth) return 1;
    if (a->kind!=b->kind) return a->kind<b->kind?-1:1;
    return (a->key>b->key)-(a->key<b->key);
}
int game_villager_art(int job)
{
    switch (job) {
        case CORE_BLD_FARM: return 1;
        case CORE_BLD_GATHERER: case CORE_BLD_HEALER: return 2;
        case CORE_BLD_LUMBER: return 3;
        case CORE_BLD_QUARRY: case CORE_BLD_MANA_MINE: return 4;
        case CORE_BLD_MARKET: return 5;
        case CORE_BLD_BARRACKS: return 6;
        case CORE_BLD_MAGE_TOWER: return 7;
        default: return 0;
    }
}
