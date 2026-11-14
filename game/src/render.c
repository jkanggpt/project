/* render.c — 픽셀 지도·주민·건물·전투와 카메라·건설 입력 */
#include "app.h"
#include "format.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static Vector2 pixel(GamePoint p) { return (Vector2){p.x*16,p.y*16}; }
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
        int i=y*w+x,variant=(x*7+y*11)%4;
        Rectangle dest={(float)x*16,(float)y*16,16,16};
        sprite(a->assets.terrain,(Rectangle){variant*16.0f,s->terrain[i]*16.0f,16,16},dest,tint);
        if (s->roads[i]) sprite(a->assets.roads,(Rectangle){((x+y)%2)*16.0f,0,16,16},dest,WHITE);
        else if (s->summary.season==CORE_SEASON_WINTER && s->terrain[i]!=CORE_TERRAIN_WATER)
            sprite(a->assets.snow,(Rectangle){variant*16.0f,0,16,16},dest,(Color){255,255,255,205});
    }
}
static void buildings(GameApp *a)
{
    GameSession *s=&a->session; int i;
    for (i=0;i<s->building_count;i++) {
        CoreBuildingView *b=&s->buildings[i]; Texture2D t=a->assets.buildings[b->type];
        Rectangle dest={b->x*16.0f,b->y*16.0f,b->w*16.0f,b->h*16.0f}; Color tint=WHITE;
        if (b->state==CORE_BSTATE_RUINED) tint=(Color){105,95,103,255};
        if (b->state==CORE_BSTATE_CONSTRUCTING) tint=(Color){185,168,126,180};
        sprite(t,(Rectangle){0,0,(float)t.width,(float)t.height},dest,tint);
        if (b->state==CORE_BSTATE_CONSTRUCTING) ui_bar((Rectangle){dest.x,dest.y-4,dest.width,3},b->build_progress/(float)(b->build_total?b->build_total:1),UI_GOLD);
        else if (b->hp<b->max_hp) ui_bar((Rectangle){dest.x,dest.y-4,dest.width,3},b->hp/(float)b->max_hp,UI_BAD);
        if (b->state==CORE_BSTATE_ACTIVE && b->worker_slots && !b->workers) {
            DrawCircle((int)(dest.x+dest.width-3),(int)(dest.y+3),3,UI_BAD);
        }
        if (b->working_now>0 && b->type!=CORE_BLD_FARM) {
            float p=(float)fmod(GetTime()+b->id*0.17,1);
            DrawCircleV((Vector2){dest.x+dest.width*0.6f,dest.y-3-p*9},1.5f,Fade(LIGHTGRAY,0.5f-p*0.4f));
        }
        if (b->id==a->selected_building) DrawRectangleLinesEx(dest,1,UI_GOLD);
    }
}
static void units(GameApp *a)
{
    GameSession *s=&a->session; int i;
    for (i=0;i<s->villager_count;i++) {
        CoreVillagerView *v=&s->villagers[i]; GameActor *unit=game_actor(s,v->id); Vector2 p; float bob;
        if (!unit || (v->state==CORE_VS_FLEE && v->at_target)) continue;
        p=pixel(unit->position); bob=!v->at_target?sinf((float)GetTime()*12+v->id)*0.6f:0;
        DrawEllipse((int)p.x,(int)(p.y+4),4,1.5f,(Color){0,0,0,70});
        sprite(a->assets.villagers,(Rectangle){(v->id%8)*16.0f,0,16,16},(Rectangle){p.x-8,p.y-10+bob,16,16},WHITE);
        if (v->flags&CORE_VF_SICK) DrawCircleV((Vector2){p.x,p.y-11},1.5f,PURPLE);
        else if (v->mood<30) DrawCircleV((Vector2){p.x,p.y-11},1.5f,UI_BAD);
        else if (v->state==CORE_VS_SLEEP) DrawCircleV((Vector2){p.x,p.y-11},1.5f,SKYBLUE);
        if (v->id==a->selected_villager) DrawCircleLines((int)p.x,(int)p.y,8,UI_GOLD);
    }
    for (i=0;i<s->enemy_count;i++) {
        GameEnemy *e=&s->enemies[i]; Vector2 p;
        if (e->hp<=0) continue;
        p=pixel(e->actor.position);
        sprite(a->assets.enemies,(Rectangle){e->def.type*16.0f,0,16,16},(Rectangle){p.x-8,p.y-10,16,16},WHITE);
        ui_bar((Rectangle){p.x-7,p.y-13,14,2},e->hp/(float)e->def.hp,UI_BAD);
    }
    for (i=0;i<s->projectile_count;i++) {
        GameProjectile *p=&s->projectiles[i]; float t=fminf(1,p->elapsed/p->duration);
        Vector2 from=pixel(p->from),to=pixel(p->to),now={from.x+(to.x-from.x)*t,from.y+(to.y-from.y)*t};
        float dx=to.x-from.x,dy=to.y-from.y,length=sqrtf(dx*dx+dy*dy);
        if (length>0) DrawLineEx((Vector2){now.x-dx/length*5,now.y-dy/length*5},now,1,UI_GOLD);
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
        DrawRectangleLines(x*16,y*16,d->w*16,d->h*16,preview.result==CORE_PLACE_OK?UI_GOOD:UI_BAD);
        if (preview.door_x>=0) DrawRectangle(preview.door_x*16,preview.door_y*16,16,16,Fade(SKYBLUE,0.6f));
        if (a->dragging) DrawLine(a->drag_x*16+8,a->drag_y*16+8,x*16+8,y*16+8,UI_GOLD);
    }
}
void game_render_world(GameApp *a)
{
    float time=(float)GetTime(); int i;
    if (!a->session.world) return;
    BeginMode2D(a->camera); terrain(a); buildings(a); units(a); overlay(a); EndMode2D();
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
