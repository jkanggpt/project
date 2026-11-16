/* render.c — 픽셀 지도·주민·건물·전투와 카메라·건설 입력 */
#include "app.h"
#include "format.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static Vector2 pixel(GamePoint p) { return (Vector2){p.x*16,p.y*16}; }
static unsigned tile_noise(int x,int y)
{
    unsigned n=(unsigned)x*0x9e3779b9u ^ (unsigned)y*0x85ebca6bu;
    n^=n>>16; n*=0x7feb352du; n^=n>>15; return n;
}
static Color terrain_tint(const GameSession *s)
{
    if (s->summary.season==CORE_SEASON_AUTUMN) return (Color){237,207,151,255};
    if (s->summary.season==CORE_SEASON_WINTER) return (Color){191,211,227,255};
    if (s->summary.nature_event==CORE_NATURE_DROUGHT) return (Color){239,211,131,255};
    return WHITE;
}
static void sprite(Texture2D texture,Rectangle source,Rectangle destination,Color tint)
{
    if (texture.id) DrawTexturePro(texture,source,destination,(Vector2){0,0},0,tint);
    else DrawRectangleRec(destination,tint);
}
static void terrain(GameApp *a)
{
    GameSession *s=&a->session; int x,y,w=s->summary.map_width,h=s->summary.map_height;
    Vector2 top=GetScreenToWorld2D((Vector2){0,0},a->camera),bottom=GetScreenToWorld2D((Vector2){(float)GetScreenWidth(),(float)GetScreenHeight()},a->camera);
    int x0=(int)floorf(top.x/16)-1,y0=(int)floorf(top.y/16)-1,x1=(int)ceilf(bottom.x/16)+1,y1=(int)ceilf(bottom.y/16)+1;
    Color tint=terrain_tint(s);
    if (x0<0) x0=0;
    if (y0<0) y0=0;
    if (x1>w) x1=w;
    if (y1>h) y1=h;
    for (y=y0;y<y1;y++) for (x=x0;x<x1;x++) {
        int i=y*w+x,variant=(int)(tile_noise(x,y)%4);
        Rectangle dest={(float)x*16,(float)y*16,16,16};
        sprite(a->assets.terrain,(Rectangle){variant*64.0f,s->terrain[i]*64.0f,64,64},dest,tint);
        if (s->roads[i]) sprite(a->assets.roads,(Rectangle){((x+y)%2)*64.0f,0,64,64},dest,WHITE);
        else if (s->summary.season==CORE_SEASON_WINTER && s->terrain[i]!=CORE_TERRAIN_WATER)
            sprite(a->assets.snow,(Rectangle){variant*64.0f,0,64,64},dest,(Color){255,255,255,225});
    }
}
static Rectangle box_rect(GameSpriteBox b) { return (Rectangle){b.x,b.y,b.w,b.h}; }
static void building(GameApp *a,int index)
{
    CoreBuildingView *b=&a->session.buildings[index]; Texture2D t=a->assets.buildings[b->type];
    Rectangle dest=box_rect(game_building_sprite(b->type,b->x,b->y,b->w,b->h,t.width,t.height));
    Color tint=WHITE; float bar_x=b->x*16.0f,bar_w=b->w*16.0f;
    if (b->state==CORE_BSTATE_RUINED) tint=(Color){105,95,103,255};
    if (b->state==CORE_BSTATE_CONSTRUCTING) tint=(Color){185,168,126,170};
    sprite(t,(Rectangle){0,0,(float)t.width,(float)t.height},dest,tint);
    if (b->state==CORE_BSTATE_CONSTRUCTING) {
        Texture2D scaffold=a->assets.construction; float size=fminf(bar_w,24);
        sprite(scaffold,(Rectangle){0,0,(float)scaffold.width,(float)scaffold.height},(Rectangle){bar_x,(b->y+b->h)*16-size,size,size},WHITE);
        ui_bar((Rectangle){bar_x,dest.y-4,bar_w,3},b->build_progress/(float)(b->build_total?b->build_total:1),UI_GOLD);
    } else if (b->hp<b->max_hp) ui_bar((Rectangle){bar_x,dest.y-4,bar_w,3},b->hp/(float)b->max_hp,UI_BAD);
    if (b->state==CORE_BSTATE_ACTIVE && b->worker_slots && !b->workers)
        DrawCircle((int)(bar_x+bar_w-3),(int)(dest.y+3),2,UI_BAD);
    if (b->working_now>0 && b->type!=CORE_BLD_FARM) {
        float p=(float)fmod(GetTime()+b->id*0.17,1);
        DrawCircleV((Vector2){dest.x+dest.width*0.6f,dest.y-3-p*9},1.5f,Fade(LIGHTGRAY,0.5f-p*0.4f));
    }
}
static void villager(GameApp *a,int index)
{
    CoreVillagerView *v=&a->session.villagers[index]; GameActor *unit=game_actor(&a->session,v->id);
    Vector2 p=pixel(unit->position);
    int moving=a->speed>0 && a->panel==PANEL_NONE && !v->at_target && !(v->intent_flags&CORE_INTENT_HOLD) && unit->next<unit->path.count;
    GameSpriteFrame frame=game_villager_frame(v->id,unit->facing,unit->walk_distance,moving);
    Rectangle dest=box_rect(game_actor_sprite(p.x,p.y,GAME_ACTOR_HEIGHT));
    sprite(a->assets.villagers,(Rectangle){frame.column*96.0f,frame.row*96.0f,frame.flip?-96.0f:96.0f,96},dest,WHITE);
    if (v->flags&CORE_VF_SICK) DrawCircleV((Vector2){p.x,dest.y-2},1.5f,PURPLE);
    else if (v->mood<30) DrawCircleV((Vector2){p.x,dest.y-2},1.5f,UI_BAD);
    else if (v->state==CORE_VS_SLEEP) DrawCircleV((Vector2){p.x,dest.y-2},1.5f,SKYBLUE);
}
static void enemy(GameApp *a,int index)
{
    GameEnemy *e=&a->session.enemies[index]; Vector2 p=pixel(e->actor.position);
    /* 적 그림의 네 번째 행은 큰 오우거입니다. 정의 순서와 아틀라스를 맞춥니다. */
    int frame=((int)(GetTime()*7)+e->actor.id)%4; float height=e->def.type==3?30.0f:24.0f;
    Rectangle dest=box_rect(game_actor_sprite(p.x,p.y,height));
    sprite(a->assets.enemies,(Rectangle){frame*96.0f,e->def.type*96.0f,96,96},dest,WHITE);
    ui_bar((Rectangle){p.x-7,dest.y-4,14,2},e->hp/(float)e->def.hp,UI_BAD);
}
static int prop_type(const GameApp *a,int x,int y)
{
    const GameSession *s=&a->session;
    int i=y*s->summary.map_width+x,type=s->terrain[i],variant=(int)(tile_noise(x,y)%12);
    if (s->roads[i] || a->decor_blocked[i]) return -1;
    if (type==CORE_TERRAIN_FOREST) {
        if (variant%3==0) return 8+variant%4;
        if (s->summary.season==CORE_SEASON_WINTER) return 3;
        if (s->summary.season==CORE_SEASON_AUTUMN) return 2;
        return variant%2;
    }
    if (type==CORE_TERRAIN_ROCK) return variant%4==0?-1:4+variant%2;
    if (type==CORE_TERRAIN_MANA) return 6+variant%2;
    if (type==CORE_TERRAIN_GRASS && tile_noise(x,y)%49==0) return 8;
    return -1;
}
static void prop(GameApp *a,int cell_index)
{
    GameSession *s=&a->session; int x=cell_index%s->summary.map_width,y=cell_index/s->summary.map_width,type=prop_type(a,x,y);
    float size=type<4?34.0f:(type<8?22.0f:13.0f),cx=(x+0.5f)*16,feet=(y+0.8f)*16;
    size*=0.92f+(tile_noise(x,y)%9)*0.02f;
    sprite(a->assets.props,(Rectangle){(type%4)*256.0f,(type/4)*256.0f,256,256},(Rectangle){cx-size/2,feet-size,size,size},WHITE);
}
static int draw_queue(GameApp *a)
{
    GameSession *s=&a->session; int count=0,i,x,y,w=s->summary.map_width,h=s->summary.map_height;
    int needed=s->cells+s->building_count+s->villager_count+s->enemy_count;
    Vector2 top=GetScreenToWorld2D((Vector2){0,0},a->camera),bottom=GetScreenToWorld2D((Vector2){(float)GetScreenWidth(),(float)GetScreenHeight()},a->camera);
    int x0=(int)floorf(top.x/16)-4,y0=(int)floorf(top.y/16)-2,x1=(int)ceilf(bottom.x/16)+4,y1=(int)ceilf(bottom.y/16)+6;
    if (s->cells>a->decor_capacity) {
        unsigned char *blocked=(unsigned char *)realloc(a->decor_blocked,(size_t)s->cells);
        if (!blocked) { (void)snprintf(a->message,sizeof(a->message),u8"소품 지도를 위한 메모리가 부족합니다."); return 0; }
        a->decor_blocked=blocked; a->decor_capacity=s->cells;
    }
    memset(a->decor_blocked,0,(size_t)s->cells);
    /* 칸마다 전체 건물을 탐색하지 않고 한 번 만든 점유 마스크를 재사용합니다. */
    for (i=0;i<s->building_count;i++) {
        CoreBuildingView *b=&s->buildings[i];
        for (y=b->y;y<b->y+b->h;y++) for (x=b->x;x<b->x+b->w;x++)
            if (x>=0 && x<w && y>=0 && y<h) a->decor_blocked[y*w+x]=1;
    }
    if (needed>a->draw_capacity) {
        GameDrawItem *items=(GameDrawItem *)realloc(a->draw_items,(size_t)needed*sizeof(*items));
        if (!items) { (void)snprintf(a->message,sizeof(a->message),u8"그림 목록을 위한 메모리가 부족합니다."); return 0; }
        a->draw_items=items; a->draw_capacity=needed;
    }
    if (x0<0) x0=0;
    if (y0<0) y0=0;
    if (x1>w) x1=w;
    if (y1>h) y1=h;
    for (y=y0;y<y1;y++) for (x=x0;x<x1;x++) if (prop_type(a,x,y)>=0)
        a->draw_items[count++]=(GameDrawItem){(y+0.8f)*16,DRAW_PROP,y*w+x,y*w+x};
    for (i=0;i<s->building_count;i++) {
        CoreBuildingView *b=&s->buildings[i];
        a->draw_items[count++]=(GameDrawItem){(b->y+b->h)*16.0f,DRAW_BUILDING,i,b->id};
        DrawEllipse((int)((b->x+b->w*0.5f)*16),(b->y+b->h)*16-2,b->w*8.0f,3,(Color){24,21,16,65});
        if (b->id==a->selected_building) DrawRectangleLines(b->x*16,b->y*16,b->w*16,b->h*16,UI_GOLD);
    }
    for (i=0;i<s->villager_count;i++) {
        CoreVillagerView *v=&s->villagers[i]; GameActor *unit=game_actor(s,v->id); Vector2 p;
        if (!unit || (v->state==CORE_VS_FLEE && v->at_target)) continue;
        p=pixel(unit->position); a->draw_items[count++]=(GameDrawItem){p.y,DRAW_VILLAGER,i,v->id};
        DrawEllipse((int)p.x,(int)p.y,4,1.5f,(Color){0,0,0,75});
        if (v->id==a->selected_villager) DrawEllipseLines((int)p.x,(int)p.y,7,3,UI_GOLD);
    }
    for (i=0;i<s->enemy_count;i++) if (s->enemies[i].hp>0) {
        Vector2 p=pixel(s->enemies[i].actor.position);
        a->draw_items[count++]=(GameDrawItem){p.y,DRAW_ENEMY,i,s->enemies[i].actor.id};
        DrawEllipse((int)p.x,(int)p.y,4,1.5f,(Color){0,0,0,75});
    }
    qsort(a->draw_items,(size_t)count,sizeof(*a->draw_items),game_draw_compare);
    return count;
}
static void objects(GameApp *a)
{
    int i,count=draw_queue(a);
    for (i=0;i<count;i++) {
        GameDrawItem *item=&a->draw_items[i];
        switch (item->kind) {
            case DRAW_PROP: prop(a,item->index); break;
            case DRAW_BUILDING: building(a,item->index); break;
            case DRAW_VILLAGER: villager(a,item->index); break;
            case DRAW_ENEMY: enemy(a,item->index); break;
        }
    }
    for (i=0;i<a->session.projectile_count;i++) {
        GameProjectile *p=&a->session.projectiles[i]; float t=fminf(1,p->elapsed/p->duration);
        Vector2 from=pixel(p->from),to=pixel(p->to),now={from.x+(to.x-from.x)*t,from.y+(to.y-from.y)*t-8};
        Texture2D arrow=a->assets.arrow; float height=arrow.width?8.0f*arrow.height/arrow.width:2;
        DrawTexturePro(arrow,(Rectangle){0,0,(float)arrow.width,(float)arrow.height},(Rectangle){now.x,now.y,8,height},(Vector2){4,height/2},atan2f(to.y-from.y,to.x-from.x)*RAD2DEG,WHITE);
    }
}
void game_render_free(GameApp *a)
{
    free(a->draw_items); a->draw_items=NULL; a->draw_capacity=0;
    free(a->decor_blocked); a->decor_blocked=NULL; a->decor_capacity=0;
}
void game_render_villager_preview(GameApp *a)
{
    static const char *const names[8]={u8"갈색 남성",u8"갈색 여성",u8"금발 남성",u8"금발 여성",u8"검은 머리 남성",u8"검은 머리 여성",u8"붉은 머리 남성",u8"붉은 머리 여성"};
    static const char *const directions[4]={u8"정면",u8"오른쪽",u8"뒷면",u8"왼쪽"};
    float gap=(GetScreenWidth()-150.0f)/8,row_height=(GetScreenHeight()-140.0f)/4;
    int appearance,direction; Texture2D t=a->assets.villagers;
    ClearBackground((Color){23,32,33,255});
    ui_text(&a->ui,24,18,UI_GOLD,u8"주민 외형 8종 · 방향별 걷기");
    ui_small(&a->ui,24,54,UI_TEXT,u8"직업을 바꿔도 외형 유지 · 주민 1×1칸 / 그림 1.5타일 높이");
    for (appearance=0;appearance<8;appearance++) ui_small(&a->ui,140+appearance*gap,92,UI_GOLD,"%s",names[appearance]);
    for (direction=0;direction<4;direction++) {
        float top=140+direction*row_height;
        ui_small(&a->ui,24,top+30,UI_TEXT,"%s",directions[direction]);
        for (appearance=0;appearance<8;appearance++) {
            float cx=140+appearance*gap+gap*0.35f,feet=top+100;
            GameSpriteFrame frame=game_villager_frame(appearance+1,direction,(float)GetTime()*0.5f,1);
            DrawRectangleLinesEx((Rectangle){cx-32,feet-32,64,64},1,(Color){65,81,75,255});
            DrawEllipse((int)cx,(int)feet,16,5,(Color){0,0,0,70});
            sprite(t,(Rectangle){frame.column*96.0f,frame.row*96.0f,frame.flip?-96.0f:96.0f,96},(Rectangle){cx-48,feet-96,96,96},WHITE);
        }
    }
}
static int world_mouse(GameApp *a,int *x,int *y)
{
    Vector2 mouse=GetMousePosition(),world=GetScreenToWorld2D(mouse,a->camera);
    int i; float notice_y=126;
    *x=(int)floorf(world.x/16); *y=(int)floorf(world.y/16);
    /* 위치로 이동하는 알림을 누를 때 뒤쪽 건물을 동시에 조작하지 않습니다. */
    for (i=a->notice_count-1;i>=0 && notice_y<GetScreenHeight()-185;i--) {
        if (a->notices[i].age>16) continue;
        if (CheckCollisionPointRec(mouse,(Rectangle){12,notice_y,390,64})) return 0;
        notice_y+=70;
    }
    return a->panel==PANEL_NONE && mouse.y>114 && mouse.y<GetScreenHeight()-145 && mouse.x<GetScreenWidth()-350;
}
static void overlay(GameApp *a)
{
    int x,y; CorePlacementView preview; GameSession *s=&a->session;
    if (a->build_type<0 || !world_mouse(a,&x,&y)) return;
    {
        CoreBuildingDefView *d=&s->building_defs[a->build_type]; int tx,ty;
        x-=(d->w-1)/2; y-=(d->h-1)/2;
        if (core_can_place(s->world,a->build_type,x,y,&preview)!=CORE_OK) return;
        if (d->work_radius>0) {
            float cx=x+d->w*0.5f,cy=y+d->h*0.5f;
            DrawCircleLines((int)(cx*16),(int)(cy*16),(float)d->work_radius*16,Fade(UI_GOLD,0.7f));
            for (ty=y-d->work_radius;ty<=y+d->h+d->work_radius;ty++) for (tx=x-d->work_radius;tx<=x+d->w+d->work_radius;tx++) {
                float dx=tx+0.5f-cx,dy=ty+0.5f-cy;
                if (tx>=0 && ty>=0 && tx<s->summary.map_width && ty<s->summary.map_height &&
                    dx*dx+dy*dy<=d->work_radius*d->work_radius && s->terrain[ty*s->summary.map_width+tx]==d->count_terrain)
                    DrawRectangle(tx*16,ty*16,16,16,Fade(UI_GOLD,0.3f));
            }
        }
        DrawRectangle(x*16,y*16,d->w*16,d->h*16,Fade(preview.result==CORE_PLACE_OK?UI_GOOD:UI_BAD,0.4f));
        if (a->build_type!=CORE_BLD_ROAD) {
            Texture2D t=a->assets.buildings[a->build_type];
            Rectangle ghost=box_rect(game_building_sprite(a->build_type,x,y,d->w,d->h,t.width,t.height));
            sprite(t,(Rectangle){0,0,(float)t.width,(float)t.height},ghost,Fade(preview.result==CORE_PLACE_OK?WHITE:UI_BAD,0.65f));
        }
        DrawRectangleLines(x*16,y*16,d->w*16,d->h*16,preview.result==CORE_PLACE_OK?UI_GOOD:UI_BAD);
        if (preview.door_x>=0) DrawRectangle(preview.door_x*16,preview.door_y*16,16,16,Fade(SKYBLUE,0.6f));
        if (a->dragging) DrawLine(a->drag_x*16+8,a->drag_y*16+8,x*16+8,y*16+8,UI_GOLD);
    }
}
void game_render_world(GameApp *a)
{
    float time=(float)GetTime(); int i;
    if (!a->session.world) return;
    BeginMode2D(a->camera); terrain(a); objects(a); overlay(a); EndMode2D();
    if (a->session.summary.is_night) DrawRectangle(0,0,GetScreenWidth(),GetScreenHeight(),(Color){22,28,65,85});
    if (a->session.summary.weather==CORE_WEATHER_RAIN || a->session.summary.weather==CORE_WEATHER_SNOW || a->session.summary.weather==CORE_WEATHER_STORM) {
        int snow=a->session.summary.weather==CORE_WEATHER_SNOW;
        for (i=0;i<100;i++) {
            float x=fmodf(i*131.7f+time*17, (float)GetScreenWidth());
            float y=fmodf(i*83.3f+time*(snow?30:280),(float)GetScreenHeight());
            if (snow) DrawCircleV((Vector2){x,y},1.5f,Fade(WHITE,0.5f));
            else DrawLine((int)x,(int)y,(int)x-3,(int)y+9,Fade(SKYBLUE,0.3f));
        }
    }
}
void game_world_input(GameApp *a,float delta)
{
    int x,y,inside=world_mouse(a,&x,&y); GameSession *s=&a->session;
    if (a->panel!=PANEL_NONE || !a->in_game) return;
    if (IsKeyPressed(KEY_SPACE)) a->speed=a->speed?0:1;
    if (IsKeyPressed(KEY_ONE)) a->speed=1;
    if (IsKeyPressed(KEY_TWO)) a->speed=2;
    if (IsKeyPressed(KEY_THREE)) a->speed=3;
    if (IsKeyPressed(KEY_V)) game_app_open(a,PANEL_VILLAGERS);
    if (IsKeyPressed(KEY_F1)) game_app_open(a,PANEL_HELP);
    if (IsKeyPressed(KEY_X)) { a->demolish=!a->demolish; a->build_type=-1; }
    if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) { a->build_type=-1; a->demolish=0; a->dragging=0; }
    if (IsKeyPressed(KEY_ESCAPE)) {
        if (a->build_type>=0 || a->demolish || a->dragging) { a->build_type=-1; a->demolish=0; a->dragging=0; }
        else if (a->selected_building || a->selected_villager) a->selected_building=a->selected_villager=0;
        else game_app_open(a,PANEL_PAUSE);
    }
    if (a->panel!=PANEL_NONE) return;
    {
        float amount=230*delta/a->camera.zoom;
        if (IsKeyDown(KEY_A)||IsKeyDown(KEY_LEFT)) a->camera.target.x-=amount;
        if (IsKeyDown(KEY_D)||IsKeyDown(KEY_RIGHT)) a->camera.target.x+=amount;
        if (IsKeyDown(KEY_W)||IsKeyDown(KEY_UP)) a->camera.target.y-=amount;
        if (IsKeyDown(KEY_S)||IsKeyDown(KEY_DOWN)) a->camera.target.y+=amount;
        if (a->settings.edge_scroll && IsWindowFocused() && inside) {
            if (GetMouseX()<8) a->camera.target.x-=amount;
            if (GetMouseX()>GetScreenWidth()-358) a->camera.target.x+=amount;
            if (GetMouseY()<122) a->camera.target.y-=amount;
            if (GetMouseY()>GetScreenHeight()-153) a->camera.target.y+=amount;
        }
        if (IsMouseButtonDown(MOUSE_BUTTON_MIDDLE)) {
            Vector2 d=GetMouseDelta(); a->camera.target.x-=d.x/a->camera.zoom; a->camera.target.y-=d.y/a->camera.zoom;
        }
        if (inside) {
            float wheel=GetMouseWheelMove();
            if (wheel!=0) {
                Vector2 before=GetScreenToWorld2D(GetMousePosition(),a->camera),after;
                a->camera.zoom=fmaxf(1,fminf(6,a->camera.zoom+(wheel>0?1:-1)));
                after=GetScreenToWorld2D(GetMousePosition(),a->camera);
                a->camera.target.x+=before.x-after.x; a->camera.target.y+=before.y-after.y;
            }
        }
        a->camera.target.x=fmaxf(0,fminf(s->summary.map_width*16.0f,a->camera.target.x));
        a->camera.target.y=fmaxf(0,fminf(s->summary.map_height*16.0f,a->camera.target.y));
    }
    if (!inside) { if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) a->dragging=0; return; }
    if (a->build_type>=0) {
        CoreBuildingDefView *d=&s->building_defs[a->build_type]; int32_t reason=0,id=0; int rc;
        x-=(d->w-1)/2; y-=(d->h-1)/2;
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && d->line) { a->dragging=1; a->drag_x=x; a->drag_y=y; }
        if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
            if (d->line && a->dragging) rc=core_place_line(s->world,a->build_type,a->drag_x,a->drag_y,x,y,&id,&reason);
            else if (!d->line) rc=core_place_building(s->world,a->build_type,x,y,&id,&reason);
            else return;
            a->dragging=0; game_app_result(a,rc,reason,1);
            if (rc==CORE_OK) game_sound(&a->assets,SND_PLACE,a->ui.volume);
            (void)game_session_refresh(s); game_session_events(s);
        }
    } else if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        int id=game_building_at(s,x,y);
        if (a->demolish) {
            int rc=id?core_demolish(s->world,id):core_remove_road(s->world,x,y);
            game_app_result(a,rc,0,0); (void)game_session_refresh(s); game_session_events(s);
        } else {
            int i; Vector2 mouse=GetScreenToWorld2D(GetMousePosition(),a->camera); float best=8;
            a->selected_villager=0; a->selected_building=id;
            for (i=0;i<s->actor_count;i++) {
                CoreVillagerView *v=game_villager(s,s->actors[i].id); float distance=game_distance(s->actors[i].position,(GamePoint){mouse.x/16,mouse.y/16})*16;
                if (v && !(v->state==CORE_VS_FLEE && v->at_target) && distance<best) {
                    best=distance; a->selected_villager=v->id; a->selected_building=0;
                }
            }
        }
    }
}
