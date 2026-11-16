/* expansion_commands.c — 회전 배치, 성채 도로망, 폐쇄 성벽과 시야 */
#include "expansion.h"
#include "placement.h"
#include "mem.h"
#include <string.h>
#include <stdlib.h>

static const int DX[4]={1,-1,0,0},DY[4]={0,0,1,-1};
static void door(int x,int y,int w,int h,int rot,int *dx,int *dy)
{
    *dx=x+w/2;*dy=y+h;
    if(rot==1){*dx=x+w;*dy=y+h/2;}
    if(rot==2){*dx=x+w/2;*dy=y-1;}
    if(rot==3){*dx=x-1;*dy=y+h/2;}
}
static int road_cell(const World *w,int i)
{
    return w->grid.road[i] || w->grid.block[i]==GRID_BLOCK_GATE;
}
static void reveal(World *w,int cx,int cy,int radius,uint8_t *mask)
{
    int x,y;
    for(y=cy-radius;y<=cy+radius;y++)for(x=cx-radius;x<=cx+radius;x++)
        if(grid_in_bounds(&w->grid,x,y) && (x-cx)*(x-cx)+(y-cy)*(y-cy)<=radius*radius)
            mask[grid_index(&w->grid,x,y)]=1;
}
void expansion_rebuild(World *w)
{
    ExpansionState *e=w->expansion;Grid *g=&w->grid;Building *keep=world_keep(w);
    int i,x,y,head=0,tail=0;
    memset(g->occupant,0,(size_t)e->cells*sizeof(int32_t));
    memset(g->block,0,(size_t)e->cells);memset(g->door_count,0,(size_t)e->cells);
    for(i=0;i<w->building_count;i++){
        Building *b=w->buildings[i];int flags=expansion_spec(b->type)->flags;
        for(y=b->y;y<b->y+b->h;y++)for(x=b->x;x<b->x+b->w;x++){
            int p=grid_index(g,x,y);g->occupant[p]=b->id;
            if(b->state!=CORE_BSTATE_RUINED)g->block[p]=(uint8_t)((flags&CORE_EXP_GATE)?GRID_BLOCK_GATE:(flags&CORE_EXP_WALL)?GRID_BLOCK_WALL:(flags&CORE_EXP_OUTDOOR)?0:GRID_BLOCK_SOLID);
        }
        if(grid_in_bounds(g,b->door_x,b->door_y))g->door_count[grid_index(g,b->door_x,b->door_y)]++;
    }
    grid_rebuild_costs(g);memset(e->connected,0,(size_t)e->cells);
    for(i=0;i<e->cells;i++)e->distance[i]=-1;
    if(keep){
        for(y=keep->y-1;y<=keep->y+keep->h;y++)for(x=keep->x-1;x<=keep->x+keep->w;x++)
            if(grid_in_bounds(g,x,y) && !entity_building_covers(keep,x,y)){
                i=grid_index(g,x,y);if(road_cell(w,i) && !e->connected[i]){e->connected[i]=1;e->distance[i]=0;e->queue[tail++]=i;}
            }
    }
    while(head<tail){int p=e->queue[head++],k;
        for(k=0;k<4;k++){x=p%g->width+DX[k];y=p/g->width+DY[k];if(grid_in_bounds(g,x,y)){
            int n=grid_index(g,x,y);if(!e->connected[n] && road_cell(w,n)){e->connected[n]=1;e->distance[n]=e->distance[p]+1;e->queue[tail++]=n;}
        }}
    }
    /* 외부에서 들어올 수 없는 칸만 보호 구역입니다. 성문도 외부 홍수 채우기에는 벽입니다. */
    memset(e->protected_cells,0,(size_t)e->cells);head=tail=0;
    for(i=0;i<e->cells;i++)if((i<g->width || i>=e->cells-g->width || i%g->width==0 || i%g->width==g->width-1) && g->block[i]!=GRID_BLOCK_WALL && g->block[i]!=GRID_BLOCK_GATE){e->protected_cells[i]=1;e->queue[tail++]=i;}
    while(head<tail){int p=e->queue[head++],k;
        for(k=0;k<4;k++){x=p%g->width+DX[k];y=p/g->width+DY[k];if(grid_in_bounds(g,x,y)){
            int n=grid_index(g,x,y);if(!e->protected_cells[n] && g->block[n]!=GRID_BLOCK_WALL && g->block[n]!=GRID_BLOCK_GATE){e->protected_cells[n]=1;e->queue[tail++]=n;}
        }}
    }
    e->protected_area=0;
    for(i=0;i<e->cells;i++){e->protected_cells[i]=(uint8_t)(!e->protected_cells[i] && g->block[i]!=GRID_BLOCK_WALL && g->block[i]!=GRID_BLOCK_GATE);e->protected_area+=e->protected_cells[i]!=0;}
    /* 승급 도로는 실제 시설의 입구에 도달하는 최단 서비스 경로만 셉니다. */
    memset(g->scratch_mark,0,(size_t)e->cells);e->effective_roads=0;
    for(i=0;i<w->building_count;i++){
        Building *b=w->buildings[i];int p,k,d;
        if(b->state!=CORE_BSTATE_ACTIVE || !grid_in_bounds(g,b->door_x,b->door_y))continue;
        p=grid_index(g,b->door_x,b->door_y);d=e->distance[p];
        while(d>=0){g->scratch_mark[p]=1;if(!d)break;
            for(k=0;k<4;k++){x=p%g->width+DX[k];y=p/g->width+DY[k];if(grid_in_bounds(g,x,y)){
                int n=grid_index(g,x,y);if(e->distance[n]==d-1){p=n;break;}
            }}if(k==4)break;d--;
        }
    }
    for(i=0;i<e->cells;i++)if(g->scratch_mark[i] && g->road[i])e->effective_roads++;
    e->protected_population=0;
    for(i=0;i<w->villager_count;i++){Villager *v=w->villagers[i];if(!v->contract_end_tick && v->home && e->protected_cells[grid_index(g,v->home->x,v->home->y)])e->protected_population++;}
    memset(e->static_visible,0,(size_t)e->cells);
    for(i=0;i<w->building_count;i++){
        Building *b=w->buildings[i];const ExpansionBuildingSpec *s=expansion_spec(b->type);int sight=s->sight;
        if(b->state!=CORE_BSTATE_ACTIVE)continue;
        if(s->category==CORE_CAT_TOWERS && !b->worker_count)sight=2;
        reveal(w,b->x+b->w/2,b->y+b->h/2,sight,e->static_visible);
    }
    for(i=0;i<e->cells;i++)if(g->road[i])reveal(w,i%g->width,i/g->width,1,e->static_visible);
    e->dirty=0;
}
void expansion_visibility(World *w)
{
    ExpansionState *e=w->expansion;int i;
    memcpy(e->visible,e->static_visible,(size_t)e->cells);
    for(i=0;i<w->villager_count;i++){
        Villager *v=w->villagers[i];if(!v->inside_building)reveal(w,v->current_x_milli/1000,v->current_y_milli/1000,4,e->visible);
    }
    for(i=0;i<e->cells;i++)if(e->visible[i])e->tiles[i].explored=1;
}
void expansion_place_check(const World *w,int type,int x,int y,int rotation,int ignore,CorePlacementView *out)
{
    const ExpansionBuildingSpec *s=expansion_spec(type);int xx,yy,dx,dy;
    memset(out,0,sizeof(*out));out->result=CORE_PLACE_BAD_TYPE;
    if(!s || type==CORE_BLD_KEEP || rotation<0 || rotation>3)return;
    out->w=(rotation&1)?s->h:s->w;out->h=(rotation&1)?s->w:s->h;
    memcpy(out->cost,w->defs.buildings[type].cost,sizeof(out->cost));out->productivity=100;
    if(s->rank>w->rank){out->result=CORE_PLACE_LOCKED;return;}
    if(x<1 || y<1 || x+out->w>=w->grid.width || y+out->h>=w->grid.height){out->result=CORE_PLACE_OUT_OF_BOUNDS;return;}
    for(yy=y;yy<y+out->h;yy++)for(xx=x;xx<x+out->w;xx++){
        int p=grid_index(&w->grid,xx,yy);
        if(!w->expansion->tiles[p].explored){out->result=CORE_PLACE_UNREACHABLE;return;}
        if(w->grid.occupant[p] && w->grid.occupant[p]!=ignore){out->result=CORE_PLACE_OCCUPIED;return;}
        if(type==CORE_BLD_BRIDGE ? w->grid.terrain[p]!=CORE_TERRAIN_WATER : w->grid.terrain[p]==CORE_TERRAIN_WATER){out->result=CORE_PLACE_BAD_TERRAIN;return;}
        if(w->grid.road[p] && !(s->flags&CORE_EXP_ROAD)){out->result=CORE_PLACE_ROAD_IN_WAY;return;}
        if(w->grid.door_count[p] && !(s->flags&CORE_EXP_ROAD) && !(s->flags&CORE_EXP_GATE) && !ignore){out->result=CORE_PLACE_BLOCKS_DOOR;return;}
        if((s->flags&CORE_EXP_ROAD) && w->grid.road[p]==(type==CORE_BLD_STONE_ROAD?2:type==CORE_BLD_BRIDGE?3:1)){out->result=CORE_PLACE_ALREADY_THERE;return;}
    }
    door(x,y,out->w,out->h,rotation,&dx,&dy);out->door_x=dx;out->door_y=dy;
    if(!(s->flags&(CORE_EXP_ROAD|CORE_EXP_WALL|CORE_EXP_GATE))){
        int p=grid_index(&w->grid,dx,dy);
        if(!w->grid.cost[p] || (w->grid.occupant[p] && w->grid.occupant[p]!=ignore)){out->result=CORE_PLACE_NO_DOOR;return;}
        if(w->rank>=1 && s->rank>=1 && !w->expansion->connected[p]){out->result=CORE_PLACE_UNREACHABLE;return;}
    }
    if(!ignore && !eco_bank_can_afford(&w->bank,out->cost)){out->result=CORE_PLACE_CANNOT_AFFORD;return;}
    out->result=CORE_PLACE_OK;
}
int expansion_place(World *w,int type,int x,int y,int rotation,int *id,int *reason)
{
    CorePlacementView p;const ExpansionBuildingSpec *s=expansion_spec(type);Building *b;int xx,yy;
    expansion_place_check(w,type,x,y,rotation,0,&p);*id=0;*reason=p.result;
    if(p.result)return CORE_ERR_REJECTED;
    if(s->flags&CORE_EXP_ROAD){w->grid.road[grid_index(&w->grid,x,y)]=(uint8_t)(type==CORE_BLD_STONE_ROAD?2:type==CORE_BLD_BRIDGE?3:1);}
    else{
        b=entity_add_building(w,type);if(!b)return CORE_ERR_OUT_OF_MEMORY;
        *id=b->id;b->x=x;b->y=y;b->w=p.w;b->h=p.h;b->rotation=rotation;b->door_x=p.door_x;b->door_y=p.door_y;
        b->hp=w->defs.buildings[type].max_hp;b->auto_assign=1;b->state=CORE_BSTATE_CONSTRUCTING;b->mode_day=-1;b->crop_year=exp_year(w);
    }
    (void)eco_pay(&w->bank,&w->ledger,p.cost,CORE_LEDGER_CONSTRUCTION);
    for(yy=y;yy<y+p.h;yy++)for(xx=x;xx<x+p.w;xx++){
        int n=grid_index(&w->grid,xx,yy);CoreExpansionTileView *t=&w->expansion->tiles[n];
        t->tree_kind=0;t->tree_logs_milli=0;t->regrow_day=0;
        if(w->grid.terrain[n]==CORE_TERRAIN_FOREST)w->grid.terrain[n]=CORE_TERRAIN_GRASS;
    }
    w->expansion->dirty=1;expansion_rebuild(w);
    events_push_building(&w->events,CORE_EVT_BUILDING_PLACED,*id,type,x,y);return CORE_OK;
}
int expansion_remove_road(World *w,int x,int y)
{
    ExpansionState *e=w->expansion;int i,p,old,reject=0;uint8_t *before;
    if(!grid_in_bounds(&w->grid,x,y))return CORE_ERR_INVALID_ARG;
    p=grid_index(&w->grid,x,y);old=w->grid.road[p];if(!old)return CORE_ERR_NOT_FOUND;
    before=(uint8_t *)mem_calloc((size_t)w->building_count,1);if(!before)return CORE_ERR_OUT_OF_MEMORY;
    for(i=0;i<w->building_count;i++){Building *b=w->buildings[i];if(b->state==CORE_BSTATE_ACTIVE && grid_in_bounds(&w->grid,b->door_x,b->door_y))before[i]=e->connected[grid_index(&w->grid,b->door_x,b->door_y)];}
    w->grid.road[p]=0;expansion_rebuild(w);
    for(i=0;i<w->building_count;i++){Building *b=w->buildings[i];if(before[i] && !e->connected[grid_index(&w->grid,b->door_x,b->door_y)])reject++;}
    mem_free(before);
    if(reject){w->grid.road[p]=(uint8_t)old;expansion_rebuild(w);return CORE_ERR_REJECTED;}
    return CORE_OK;
}
int expansion_demolish(World *w,Building *b)
{
    int r;if(b->type==CORE_BLD_KEEP)return CORE_ERR_REJECTED;
    for(r=0;r<CORE_RES_COUNT;r++)(void)expansion_bank_add(w,r,w->defs.buildings[b->type].cost[r]*500,CORE_LEDGER_CONSTRUCTION);
    entity_remove_building(w,b);w->expansion->dirty=1;expansion_rebuild(w);expansion_capacity(w);expansion_assign(w);return CORE_OK;
}
CORE_API int32_t core_can_place_rotated(const CoreWorld *w,int32_t type,int32_t x,int32_t y,int32_t rot,CorePlacementView *out)
{
    if(!w || !out)return CORE_ERR_NULL_ARG;if(!w->expansion)return CORE_ERR_INVALID_ARG;
    expansion_place_check(w,type,x,y,rot,0,out);return CORE_OK;
}
CORE_API int32_t core_place_rotated(CoreWorld *w,int32_t type,int32_t x,int32_t y,int32_t rot,int32_t *id,int32_t *reason)
{
    int a,b;if(!w)return CORE_ERR_NULL_ARG;if(!w->expansion)return CORE_ERR_INVALID_ARG;
    return expansion_place(w,type,x,y,rot,id?id:&a,reason?reason:&b);
}
CORE_API int32_t core_rotate_building(CoreWorld *w,int32_t id,int32_t rot,int32_t *reason)
{
    Building *b;CorePlacementView p;if(!w)return CORE_ERR_NULL_ARG;if(!w->expansion)return CORE_ERR_INVALID_ARG;
    b=entity_find_building(w,id);if(!b)return CORE_ERR_NOT_FOUND;
    expansion_place_check(w,b->type,b->x,b->y,rot,b->id,&p);if(reason)*reason=p.result;if(p.result)return CORE_ERR_REJECTED;
    b->rotation=rot;b->w=p.w;b->h=p.h;b->door_x=p.door_x;b->door_y=p.door_y;w->expansion->dirty=1;
    expansion_rebuild(w);return CORE_OK;
}
CORE_API int32_t core_expansion_copy_tiles(const CoreWorld *w,CoreExpansionTileView *out,int32_t cap)
{
    if(!w || !out)return CORE_ERR_NULL_ARG;if(!w->expansion)return CORE_ERR_INVALID_ARG;
    if(cap<w->expansion->cells)return CORE_ERR_BUFFER_TOO_SMALL;memcpy(out,w->expansion->tiles,(size_t)w->expansion->cells*sizeof(*out));return CORE_OK;
}
CORE_API int32_t core_expansion_copy_visibility(const CoreWorld *w,uint8_t *out,int32_t cap)
{
    int i;if(!w || !out)return CORE_ERR_NULL_ARG;if(!w->expansion)return CORE_ERR_INVALID_ARG;
    if(cap<w->expansion->cells)return CORE_ERR_BUFFER_TOO_SMALL;
    for(i=0;i<w->expansion->cells;i++)out[i]=(uint8_t)(w->expansion->visible[i]?2:w->expansion->tiles[i].explored?1:0);return CORE_OK;
}
CORE_API int32_t core_expansion_report_position(CoreWorld *w,int32_t id,int32_t x,int32_t y)
{
    Villager *v;if(!w)return CORE_ERR_NULL_ARG;if(!w->expansion)return CORE_ERR_INVALID_ARG;
    v=entity_find_villager(w,id);if(!v)return CORE_ERR_NOT_FOUND;
    if(x<0 || y<0 || x>=w->grid.width*1000 || y>=w->grid.height*1000)return CORE_ERR_INVALID_ARG;
    v->current_x_milli=x;v->current_y_milli=y;return CORE_OK;
}
CORE_API int32_t core_expansion_spotted(CoreWorld *w,int32_t x,int32_t y)
{
    if(!w)return CORE_ERR_NULL_ARG;if(!w->expansion || !grid_in_bounds(&w->grid,x,y))return CORE_ERR_INVALID_ARG;
    if(!w->expansion->visible[grid_index(&w->grid,x,y)])return CORE_IGNORED;
    w->expansion->alarm=1;return CORE_OK;
}
CORE_API int32_t core_expansion_fire(CoreWorld *w,int32_t id)
{
    Building *b;if(!w)return CORE_ERR_NULL_ARG;b=entity_find_building(w,id);if(!b)return CORE_ERR_NOT_FOUND;
    if(!w->expansion || !b->worker_count || !expansion_spec(b->type)->attack)return CORE_ERR_REJECTED;
    if(b->type==CORE_BLD_ARCANE_TOWER && b->shots%3==0){if(w->bank.amount[CORE_RES_MANA]<1)return CORE_ERR_REJECTED;(void)expansion_bank_add(w,CORE_RES_MANA,-1000,CORE_LEDGER_PRODUCTION);}
    b->shots++;return CORE_OK;
}
