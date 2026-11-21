/* render.c — 픽셀 지도·주민·건물·전투와 카메라·건설 입력 */
#include "app.h"
#include "format.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static Vector2 pixel(GamePoint p) { return (Vector2){p.x * 16, p.y * 16}; }
void game_render_connection_preview(GameApp *a) {
    static const char *const walls[4] = {u8"목책", u8"방벽", u8"성벽", u8"철성벽"};
    float scale = fminf(GetScreenWidth() / 1440.0f, GetScreenHeight() / 900.0f);
    Camera2D camera = {{0, 0}, {0, 0}, 0, scale};
    int r, c, x, y;
    BeginMode2D(camera);
    ui_text(&a->ui, 28, 18, UI_GOLD, u8"자동 연결 성벽·성문 · 동물 30종 걷기 4프레임");
    ui_small(&a->ui, 28, 52, UI_MUTED,
             u8"가로·세로·꺾임·교차를 이웃 칸에 맞춰 선택합니다. 동물은 이동 방향에 따라 좌우 반전합니다.");
    for (r = 0; r < 4; r++) {
        uint8_t cost[7 * 5];
        memset(cost, CORE_COST_NORMAL, sizeof(cost));
        for (y = 0; y < 5; y++)
            for (x = 0; x < 7; x++)
                if (x == 0 || x == 6 || y == 0 || y == 4 || x == 3)
                    cost[y * 7 + x] = CORE_COST_WALL;
        cost[4*7+3]=CORE_COST_GATE;cost[2*7]=CORE_COST_GATE;
        ui_small(&a->ui, 28 + r * 350.0f, 86, UI_GOLD, "%s", walls[r]);
        for (y = 0; y < 5; y++)
            for (x = 0; x < 7; x++) {
                float xx = 28 + r * 350.0f + x * 40, yy = 126.0f + y * 32;
                DrawRectangle((int)xx, (int)yy, 40, 32, (Color){48, 75, 46, 255});
                if (cost[y * 7 + x] == CORE_COST_GATE) {
                    DrawTexturePro(a->assets.gates,(Rectangle){(x==0?1:0)*128.0f,r*128.0f,128,128},(Rectangle){xx,yy-8,40,40},(Vector2){0,0},0,WHITE);
                } else if (cost[y * 7 + x] == CORE_COST_WALL) {
                    int mask = game_wall_mask(cost, 7, 5, x, y);
                    DrawTexturePro(a->assets.wall_links, (Rectangle){mask * 128.0f, r * 128.0f, 128, 128},
                                   (Rectangle){xx, yy - 8, 40, 40}, (Vector2){0, 0}, 0, WHITE);
                }
            }
    }
    for (r = 0; r < 8; r++)
        for (c = 0; c < 4; c++) {
            int kind = r * 4 + c, frame;
            if(kind>=30)continue;
            CoreAnimalInfo info;
            core_expansion_animal_info(kind, &info);
            float xx = 28 + c * 350.0f, yy = 317 + r * 70.0f;
            ui_small(&a->ui, xx, yy, UI_TEXT, "%s", info.name);
            for (frame = 0; frame < 4; frame++)
                DrawTexturePro(a->assets.animal_walk, (Rectangle){frame * 96.0f, kind * 96.0f, 96, 96},
                               (Rectangle){xx + frame * 77, yy + 24, 65, 44}, (Vector2){0, 0}, 0, WHITE);
        }
    EndMode2D();
}
void game_render_rotation_preview(GameApp *a) {
    static const int types[6] = {CORE_BLD_GRANARY,  CORE_BLD_HOUSE, CORE_BLD_GREENHOUSE,
                                 CORE_BLD_BARRACKS, CORE_BLD_PORT,  CORE_BLD_MAGE_TOWER};
    static const char *const directions[4] = {u8"정면 · 남쪽 입구", u8"오른쪽 · 동쪽 입구",
                                              u8"후면 · 북쪽 입구", u8"왼쪽 · 서쪽 입구"};
    float column = (GetScreenWidth() - 190.0f) / 4, row = (GetScreenHeight() - 128.0f) / 6;
    int r, c;
    ui_text(&a->ui, 28, 20, UI_GOLD, u8"새 건물 4방향 · 실제 점유와 그림 높이");
    ui_small(&a->ui, 28, 53, UI_MUTED,
             u8"R 키로 회전합니다. 온실은 유리 지붕, 일반 농장은 밭으로 표시합니다. 그림 높이와 점유는 별개입니다.");
    for (c = 0; c < 4; c++)
        ui_small(&a->ui, 200 + c * column, 87, UI_TEXT, "%s", directions[c]);
    for (r = 0; r < 6; r++) {
        CoreExpansionBuildingInfo info;
        float y = 121 + r * row;
        (void)core_expansion_building_info(types[r], &info);
        ui_small(&a->ui, 25, y + row / 2, UI_GOLD, u8"%s\n%d×%d칸", info.name, info.w, info.h);
        for (c = 0; c < 4; c++) {
            Texture2D t = a->assets.turns[types[r]][c];
            Rectangle source={0,0,(float)t.width,(float)t.height};
            if(types[r]==CORE_BLD_GREENHOUSE) {
                t=a->assets.greenhouse_turns;
                source=a->assets.greenhouse_sources[a->session.summary.season*4+c];
            }
            float width, height, scale;
            float x = 190 + c * column, base = y + row - 9;
            int bw = c & 1 ? info.h : info.w, bh = c & 1 ? info.w : info.h, xx, yy;
            for (yy = 0; yy < bh; yy++)
                for (xx = 0; xx < bw; xx++) {
                    Rectangle tile = {x + column / 2 - bw * 8.0f + xx * 16, base - bh * 9.0f + yy * 9, 16, 9};
                    DrawRectangleRec(tile, (Color){54, 79, 60, 255});
                    DrawRectangleLinesEx(tile, 1, (Color){112, 146, 113, 255});
                }
            scale = fminf((column - 32) / source.width, (row - 15) / source.height);
            width = source.width * scale;
            height = source.height * scale;
            DrawTexturePro(t, source,
                           (Rectangle){x + column / 2 - width / 2, base - height, width, height},
                           (Vector2){0, 0}, 0, WHITE);
        }
    }
}
static unsigned tile_noise(int x, int y) {
    unsigned n = (unsigned)x * 0x9e3779b9u ^ (unsigned)y * 0x85ebca6bu;
    n ^= n >> 16;
    n *= 0x7feb352du;
    n ^= n >> 15;
    return n;
}
static Color terrain_tint(const GameSession *s) {
    if (s->summary.season == CORE_SEASON_AUTUMN)
        return (Color){237, 207, 151, 255};
    if (s->summary.season == CORE_SEASON_WINTER)
        return (Color){191, 211, 227, 255};
    if (s->summary.nature_event == CORE_NATURE_DROUGHT)
        return (Color){239, 211, 131, 255};
    return WHITE;
}
static void sprite(Texture2D texture, Rectangle source, Rectangle destination, Color tint) {
    if (texture.id)
        DrawTexturePro(texture, source, destination, (Vector2){0, 0}, 0, tint);
    else
        DrawRectangleRec(destination, tint);
}
static Color effect_tint(GameApp *a, int category, int id) {
    GameEffect *fx = game_effect(&a->session, category, id);
    return fx && fx->hurt_started <= a->session.visual_time && fx->hurt_until > a->session.visual_time
               ? (Color){255, 90, 90, 255}
               : WHITE;
}
static void life_bar(GameApp *a, int category, int id, int hp, int max, Rectangle dest) {
    GameEffect *fx = game_effect(&a->session, category, id);
    Vector2 mouse = GetScreenToWorld2D(GetMousePosition(), a->camera);
    int selected = category == 0 && a->selected_villager == id;
    if (!selected && !CheckCollisionPointRec(mouse, dest) &&
        (!fx || fx->bar_until < a->session.visual_time || fx->hurt_started > a->session.visual_time))
        return;
    float zoom = a->camera.zoom, width = fminf(36, fmaxf(14, dest.width * zoom * .65f)) / zoom;
    Color color = hp * 4 <= max ? RED : hp * 2 <= max ? YELLOW : GREEN;
    ui_bar((Rectangle){dest.x + dest.width / 2 - width / 2, dest.y - 5 / zoom, width, 3 / zoom},
           hp / (float)max, color);
}
static void terrain(GameApp *a) {
    GameSession *s = &a->session;
    int x, y, w = s->summary.map_width, h = s->summary.map_height;
    Vector2 top = GetScreenToWorld2D((Vector2){0, 0}, a->camera),
            bottom =
                GetScreenToWorld2D((Vector2){(float)GetScreenWidth(), (float)GetScreenHeight()}, a->camera);
    int x0 = (int)floorf(top.x / 16) - 1, y0 = (int)floorf(top.y / 16) - 1,
        x1 = (int)ceilf(bottom.x / 16) + 1, y1 = (int)ceilf(bottom.y / 16) + 1;
    Color tint = terrain_tint(s);
    if (x0 < 0)
        x0 = 0;
    if (y0 < 0)
        y0 = 0;
    if (x1 > w)
        x1 = w;
    if (y1 > h)
        y1 = h;
    for (int pass = 0; pass < 2; pass++)
        for (y = y0; y < y1; y++)
            for (x = x0; x < x1; x++) {
                int i = y * w + x, variant = (int)(tile_noise(x, y) % 4);
                if ((pass && !s->roads[i]) || (s->visibility && !s->visibility[i]))
                    continue;
                Rectangle dest = {(float)x * 16, (float)y * 16, 16, 16};
                if (pass) {
                    static const int dx[8]={0,1,0,-1,1,1,-1,-1},dy[8]={-1,0,1,0,-1,1,1,-1};
                    int mask=0;
                    for(int k=0;k<8;k++) {int xx=x+dx[k],yy=y+dy[k];if(xx>=0&&yy>=0&&xx<w&&yy<h&&(s->roads[yy*w+xx]||s->cost[yy*w+xx]==CORE_COST_GATE))mask|=1<<k;}
                    sprite(a->assets.road_tiles,(Rectangle){(mask%16)*64.0f,((s->roads[i]-1)*16+mask/16)*64.0f,64,64},dest,WHITE);
                    if(s->roads[i]==3) for(int k=0;k<4;k++) {
                        int xx=x+dx[k],yy=y+dy[k];
                        if(xx<0||yy<0||xx>=w||yy>=h||s->terrain[yy*w+xx]!=CORE_TERRAIN_WATER||s->roads[yy*w+xx]==3)continue;
                        Vector2 p0={x*16.0f+(k==1?15:1),y*16.0f+(k==2?15:1)},p1=p0;
                        if(k==0||k==2)p1.x+=14;else p1.y+=14;
                        DrawLineEx(p0,p1,1.5f,(Color){101,66,38,255});
                        DrawCircleV(p0,1,(Color){211,166,92,255});DrawCircleV(p1,1,(Color){211,166,92,255});
                    }
                    continue;
                }
                if (s->exp_tiles) {
                    int biome = s->exp_tiles[i].biome;
                    static const int dx[4] = {0, 1, 0, -1}, dy[4] = {-1, 0, 1, 0};
                    int masks[10] = {0};
                    if (biome == CORE_BIOME_LAKE) {
                        CoreWaterView water;
                        if (core_expansion_water(s->world, s->exp_tiles[i].water_id, &water) == CORE_OK)
                            biome = water.cold ? 11 : 10;
                    }
                    sprite(a->assets.biomes, (Rectangle){variant * 64.0f, biome * 64.0f, 64, 64}, dest, tint);
                    for (int k = 0; k < 4; k++) {
                        int xx = x + dx[k], yy = y + dy[k], other;
                        if (xx < 0 || yy < 0 || xx >= w || yy >= h)
                            continue;
                        other = s->exp_tiles[yy * w + xx].biome;
                        if (other != s->exp_tiles[i].biome && other != CORE_BIOME_LAKE)
                            masks[other] |= 1 << k;
                    }
                    for (int b = 0; b < 10; b++)
                        if (masks[b])
                            sprite(s->exp_tiles[i].biome == CORE_BIOME_LAKE ? a->assets.shores
                                                                            : a->assets.biome_edges,
                                   (Rectangle){masks[b] * 64.0f, b * 64.0f, 64, 64}, dest, tint);
                } else
                    sprite(a->assets.terrain, (Rectangle){variant * 64.0f, s->terrain[i] * 64.0f, 64, 64},
                           dest, tint);
                if (s->summary.season == CORE_SEASON_WINTER && s->terrain[i] != CORE_TERRAIN_WATER)
                    sprite(a->assets.snow, (Rectangle){variant * 64.0f, 0, 64, 64}, dest,
                           (Color){255, 255, 255, 225});
                if (s->exp_tiles && s->exp_tiles[i].explored && s->exp_tiles[i].ore_milli > 0 &&
                    !game_building_at(s, x, y)) {
                    int resource = s->exp_tiles[i].ore_kind;
                    if (resource >= 0 && resource < CORE_RES_COUNT) {
                        Texture2D icon = a->assets.resources[resource];
                        DrawCircle(x * 16 + 8, y * 16 + 9, 7, (Color){39, 33, 36, 210});
                        sprite(icon, (Rectangle){0, 0, (float)icon.width, (float)icon.height},
                               (Rectangle){x * 16.0f + 1, y * 16.0f + 2, 14, 14}, WHITE);
                    }
                }
            }
}
static Rectangle box_rect(GameSpriteBox b) { return (Rectangle){b.x, b.y, b.w, b.h}; }
static int farm_row(int type) {
    return type==CORE_BLD_CROPLAND?0:type==CORE_BLD_FARM?1:type==CORE_BLD_IRRIGATED_FARM?2:type==CORE_BLD_ORCHARD?3:type==CORE_BLD_GREENHOUSE?4:-1;
}
static Rectangle building_rect(const GameApp *a,const CoreBuildingView *b, Texture2D t) {
    if(b->type==CORE_BLD_GREENHOUSE) {
        Rectangle source=a->assets.greenhouse_sources[a->session.summary.season*4+b->rotation];
        return box_rect(game_expansion_building_sprite(b,(int)source.width,(int)source.height));
    }
    return box_rect(game_expansion_building_sprite(b, farm_row(b->type)>=0?256:t.width, farm_row(b->type)>=0?256:t.height));
}
static Texture2D building_texture(const GameApp *a, const CoreBuildingView *b) {
    if (b->kind_flags & CORE_EXP_GATE) return a->assets.gates;
    if (b->type==CORE_BLD_GREENHOUSE) return a->assets.greenhouse_turns;
    if (farm_row(b->type)>=0) return a->assets.farms;
    if (b->type == CORE_BLD_KEEP)
        return a->assets.manors[a->session.summary.rank];
    return b->kind_flags & CORE_EXP_WALL ? a->assets.wall_links : a->assets.turns[b->type][b->rotation];
}
static Rectangle building_source(const GameApp *a, const CoreBuildingView *b, Texture2D t) {
    if(b->type==CORE_BLD_GREENHOUSE)
        return a->assets.greenhouse_sources[a->session.summary.season*4+b->rotation];
    if(b->kind_flags&CORE_EXP_GATE) {
        int row=b->type==CORE_BLD_PALISADE_GATE?0:b->type==CORE_BLD_STONE_GATE?1:b->type==CORE_BLD_IRON_GATE?3:2;
        return (Rectangle){b->rotation*128.0f,row*128.0f,128,128};
    }
    int row=farm_row(b->type);
    if(row>=0)return (Rectangle){a->session.summary.season*256.0f,row*256.0f,256,256};
    if (b->kind_flags & CORE_EXP_WALL) {
        int style = b->type == CORE_BLD_PALISADE    ? 0
                    : b->type == CORE_BLD_BARRIER   ? 1
                    : b->type == CORE_BLD_IRON_WALL ? 3
                                                    : 2;
        int mask = game_wall_mask(a->session.cost, a->session.summary.map_width,
                                  a->session.summary.map_height, b->x, b->y);
        return (Rectangle){mask * 128.0f, style * 128.0f, 128, 128};
    }
    return (Rectangle){0, 0, (float)t.width, (float)t.height};
}
static void building(GameApp *a, int index) {
    CoreBuildingView *b = &a->session.buildings[index];
    Texture2D t = building_texture(a, b);
    Rectangle dest = building_rect(a,b, t);
    Color tint = WHITE;
    if(farm_row(b->type)>=0 && a->session.summary.nature_event==CORE_NATURE_DROUGHT)
        tint=b->type==CORE_BLD_GREENHOUSE?(Color){240,225,190,255}:(Color){220,185,125,255};
    float bar_x = b->x * 16.0f, bar_w = b->w * 16.0f;
    if (b->state == CORE_BSTATE_RUINED)
        tint = (Color){105, 95, 103, 255};
    if (b->state == CORE_BSTATE_CONSTRUCTING)
        tint = (Color){185, 168, 126, 170};
    sprite(t, building_source(a, b, t), dest, tint);
    if (b->state == CORE_BSTATE_CONSTRUCTING ||
        (b->type == CORE_BLD_KEEP && a->session.summary.promotion_state)) {
        Texture2D scaffold = a->assets.construction;
        float site_height = b->h * 16.0f;
        sprite(scaffold, (Rectangle){0, 0, (float)scaffold.width, (float)scaffold.height},
               (Rectangle){bar_x, b->y * 16.0f, bar_w, site_height}, WHITE);
        ui_bar((Rectangle){bar_x, dest.y - 4, bar_w, 3},
               b->type == CORE_BLD_KEEP ? (144 - a->session.summary.promotion_remaining) / 144.0f
                                        : b->build_progress / (float)(b->build_total ? b->build_total : 1),
               UI_GOLD);
        if (b->type == CORE_BLD_KEEP) {
            float phase = fmodf(a->session.visual_time + b->id * .13f, 1);
            DrawCircleV((Vector2){dest.x + dest.width * .55f, dest.y + dest.height * .7f - phase * 12}, 2,
                        Fade(BEIGE, .5f * (1 - phase)));
        }
    } else if (b->hp < b->max_hp)
        ui_bar((Rectangle){bar_x, dest.y - 4, bar_w, 3}, b->hp / (float)b->max_hp, UI_BAD);
    if (b->working_now > 0 && b->type != CORE_BLD_FARM) {
        float p = (float)fmod(GetTime() + b->id * 0.17, 1);
        DrawCircleV((Vector2){dest.x + dest.width * 0.6f, dest.y - 3 - p * 9}, 1.5f,
                    Fade(LIGHTGRAY, 0.5f - p * 0.4f));
    }
}
static void villager(GameApp *a, int index) {
    CoreVillagerView *v = &a->session.villagers[index];
    GameActor *unit = game_actor(&a->session, v->id);
    Vector2 p = pixel(unit->position);
    int moving = a->speed > 0 && a->panel == PANEL_NONE && !v->at_target &&
                 !(v->intent_flags & CORE_INTENT_HOLD) && unit->next < unit->path.count;
    GameSpriteFrame frame;
    if (unit->indoor > 0) {
        CoreBuildingView *b = game_building(&a->session, unit->indoor_building);
        if (b) {
            p.x += (b->rotation == 1 ? -1 : b->rotation == 3 ? 1 : 0) * 12 * unit->indoor;
            p.y += (b->rotation == 2 ? 1 : b->rotation == 0 ? -1 : 0) * 12 * unit->indoor;
        }
    }
    frame = game_villager_frame(v->appearance + 1, unit->facing, unit->walk_distance,
                                moving || (unit->indoor > 0 && unit->indoor < 1));
    Rectangle dest = box_rect(game_actor_sprite(p.x, p.y, GAME_ACTOR_HEIGHT));
    sprite(a->assets.villagers,
           (Rectangle){frame.column * 96.0f, frame.row * 96.0f, frame.flip ? -96.0f : 96.0f, 96}, dest,
           Fade(effect_tint(a, 0, v->id), 1 - unit->indoor));
    if (unit->indoor < 0.5f)
        life_bar(a, 0, v->id, v->health, 100, dest);
    if (v->flags & CORE_VF_SICK)
        DrawCircleV((Vector2){p.x, dest.y - 2}, 1.5f, PURPLE);
    else if (v->happiness < 30)
        DrawCircleV((Vector2){p.x, dest.y - 2}, 1.5f, UI_BAD);
    else if (v->state == CORE_VS_SLEEP)
        DrawCircleV((Vector2){p.x, dest.y - 2}, 1.5f, SKYBLUE);
}
static void enemy(GameApp *a, int index) {
    GameEnemy *e = &a->session.enemies[index];
    Vector2 p = pixel(e->actor.position);
    int dir=e->actor.facing==FACE_LEFT?1:e->actor.facing;
    if(dir<0||dir>2)dir=0;
    int attacking=e->attack_serial&&e->attack_elapsed<4.8f;
    int frame=attacking?(int)(e->attack_elapsed/1.2f):game_walk_frame(e->actor.walk_distance,e->actor.next<e->actor.path.count);
    if(frame>3)frame=3;
    float width=e->info.width_milli*16/1000.0f,height=e->info.height_milli*16/1000.0f;
    if(width<=0||height<=0){width=24;height=24;}
    Rectangle dest={p.x-width/2,p.y-height,width,height};
    sprite(attacking?a->assets.raider_attack:a->assets.enemies,
           (Rectangle){((e->def.type%4)*4+frame)*96.0f,((e->def.type/4)*3+dir)*96.0f,e->actor.facing==FACE_LEFT?-96.0f:96.0f,96},dest,effect_tint(a,2,e->actor.id));
    life_bar(a, 2, e->actor.id, e->hp, e->def.hp, dest);
}
static int prop_type(const GameApp *a, int x, int y) {
    const GameSession *s = &a->session;
    int i = y * s->summary.map_width + x, type = s->terrain[i], variant = (int)(tile_noise(x, y) % 12);
    if (s->visibility && !s->visibility[i])
        return -1;
    if (s->roads[i] || a->decor_blocked[i])
        return -1;
    if (s->exp_tiles && s->exp_tiles[i].tree_kind && s->exp_tiles[i].tree_logs_milli > 0)
        return 20;
    if (s->exp_tiles && type == CORE_TERRAIN_FOREST)
        return -1;
    if (type == CORE_TERRAIN_FOREST) {
        if (variant % 3 == 0)
            return 8 + variant % 4;
        if (s->summary.season == CORE_SEASON_WINTER)
            return 3;
        if (s->summary.season == CORE_SEASON_AUTUMN)
            return 2;
        return variant % 2;
    }
    if (type == CORE_TERRAIN_ROCK)
        return variant % 4 == 0 ? -1 : 4 + variant % 2;
    if (type == CORE_TERRAIN_MANA)
        return 6 + variant % 2;
    if (type == CORE_TERRAIN_GRASS && tile_noise(x, y) % 49 == 0)
        return 8;
    return -1;
}
static void prop(GameApp *a, int cell_index) {
    GameSession *s = &a->session;
    int x = cell_index % s->summary.map_width, y = cell_index / s->summary.map_width,
        type = prop_type(a, x, y);
    float size = type < 4 ? 34.0f : (type < 8 ? 22.0f : 13.0f), cx = (x + 0.5f) * 16, feet = (y + 0.8f) * 16;
    if (type == 20) {
        CoreExpansionTileView *t = &s->exp_tiles[cell_index];
        int mature = core_expansion_tree_mature_age(t->tree_kind);
        int stage = t->tree_age < mature * 3 / 10                  ? 0
                    : t->tree_age < mature                    ? 1
                    : s->summary.season == CORE_SEASON_AUTUMN ? 3
                    : s->summary.season == CORE_SEASON_WINTER ? 4
                                                              : 2;
        size = stage == 0 ? 12.0f : stage == 1 ? 25.0f : t->tree_kind == 2 ? 43.0f : 38.0f;
        int species = (int[]){2, 1, 0, 3}[t->tree_kind - 1];
        sprite(a->assets.trees, (Rectangle){stage * 128.0f, species * 128.0f, 128, 128},
               (Rectangle){cx - size / 2, feet - size, size, size}, WHITE);
        return;
    }
    size *= 0.92f + (tile_noise(x, y) % 9) * 0.02f;
    sprite(a->assets.props, (Rectangle){(type % 4) * 256.0f, (type / 4) * 256.0f, 256, 256},
           (Rectangle){cx - size / 2, feet - size, size, size}, WHITE);
}
static int draw_queue(GameApp *a) {
    GameSession *s = &a->session;
    int count = 0, i, x, y, w = s->summary.map_width, h = s->summary.map_height;
    int needed = s->cells + s->building_count + s->villager_count + s->enemy_count + s->animal_count;
    Vector2 top = GetScreenToWorld2D((Vector2){0, 0}, a->camera),
            bottom =
                GetScreenToWorld2D((Vector2){(float)GetScreenWidth(), (float)GetScreenHeight()}, a->camera);
    int x0 = (int)floorf(top.x / 16) - 4, y0 = (int)floorf(top.y / 16) - 2,
        x1 = (int)ceilf(bottom.x / 16) + 4, y1 = (int)ceilf(bottom.y / 16) + 6;
    if (s->cells > a->decor_capacity) {
        unsigned char *blocked = (unsigned char *)realloc(a->decor_blocked, (size_t)s->cells);
        if (!blocked) {
            (void)snprintf(a->message, sizeof(a->message), u8"소품 지도를 위한 메모리가 부족합니다.");
            return 0;
        }
        a->decor_blocked = blocked;
        a->decor_capacity = s->cells;
    }
    memset(a->decor_blocked, 0, (size_t)s->cells);
    /* 칸마다 전체 건물을 탐색하지 않고 한 번 만든 점유 마스크를 재사용합니다. */
    for (i = 0; i < s->building_count; i++) {
        CoreBuildingView *b = &s->buildings[i];
        for (y = b->y; y < b->y + b->h; y++)
            for (x = b->x; x < b->x + b->w; x++)
                if (x >= 0 && x < w && y >= 0 && y < h)
                    a->decor_blocked[y * w + x] = 1;
    }
    if (needed > a->draw_capacity) {
        GameDrawItem *items = (GameDrawItem *)realloc(a->draw_items, (size_t)needed * sizeof(*items));
        if (!items) {
            (void)snprintf(a->message, sizeof(a->message), u8"그림 목록을 위한 메모리가 부족합니다.");
            return 0;
        }
        a->draw_items = items;
        a->draw_capacity = needed;
    }
    if (x0 < 0)
        x0 = 0;
    if (y0 < 0)
        y0 = 0;
    if (x1 > w)
        x1 = w;
    if (y1 > h)
        y1 = h;
    for (y = y0; y < y1; y++)
        for (x = x0; x < x1; x++)
            if (prop_type(a, x, y) >= 0)
                a->draw_items[count++] = (GameDrawItem){(y + 0.8f) * 16, DRAW_PROP, y * w + x, y * w + x};
    for (i = 0; i < s->building_count; i++) {
        CoreBuildingView *b = &s->buildings[i];
        if (s->visibility && !s->visibility[b->y * w + b->x])
            continue;
        if (b->x + b->w < x0 || b->x > x1 || b->y + b->h < y0 || b->y > y1)
            continue;
        if (b->kind_flags & CORE_EXP_PLOT) {
            building(a, i);
            continue;
        }
        a->draw_items[count++] = (GameDrawItem){(b->y + b->h) * 16.0f, DRAW_BUILDING, i, b->id};
        DrawEllipse((int)((b->x + b->w * 0.5f) * 16), (b->y + b->h) * 16 - 2, b->w * 8.0f, 3,
                    (Color){24, 21, 16, 65});
        if (b->id == a->selected_building)
            DrawRectangleLines(b->x * 16, b->y * 16, b->w * 16, b->h * 16, UI_GOLD);
    }
    for (i = 0; i < s->villager_count; i++) {
        CoreVillagerView *v = &s->villagers[i];
        GameActor *unit = game_actor(s, v->id);
        Vector2 p;
        if (!unit || unit->indoor >= 1 || (!s->summary.ruleset && v->state == CORE_VS_FLEE && v->at_target))
            continue;
        p = pixel(unit->position);
        if (unit->position.x < x0 || unit->position.x > x1 || unit->position.y < y0 || unit->position.y > y1)
            continue;
        a->draw_items[count++] = (GameDrawItem){p.y, DRAW_VILLAGER, i, v->id};
        DrawEllipse((int)p.x, (int)p.y, 4, 1.5f, (Color){0, 0, 0, 75});
        if (v->id == a->selected_villager)
            DrawEllipseLines((int)p.x, (int)p.y, 7, 3, UI_GOLD);
    }
    for (i = 0; i < s->enemy_count; i++)
        if (s->enemies[i].hp > 0) {
            Vector2 p = pixel(s->enemies[i].actor.position);
            if (s->visibility && s->visibility[(int)(p.y / 16) * w + (int)(p.x / 16)] < 2)
                continue;
            a->draw_items[count++] = (GameDrawItem){p.y, DRAW_ENEMY, i, s->enemies[i].actor.id};
            DrawEllipse((int)p.x, (int)p.y, 4, 1.5f, (Color){0, 0, 0, 75});
        }
    for (i = 0; i < s->animal_count; i++) {
        CoreAnimalView *animal = &s->animals[i];
        if (animal->owner || !animal->alive || animal->hp <= 0)
            continue;
        int xcell = animal->x_milli / 1000, ycell = animal->y_milli / 1000;
        if (xcell < x0 || xcell > x1 || ycell < y0 || ycell > y1)
            continue;
        if (s->visibility && s->visibility[ycell * w + xcell] < 2)
            continue;
        a->draw_items[count++] =
            (GameDrawItem){s->animal_actors[i].position.y * 16, DRAW_ANIMAL, i, animal->id};
    }
    qsort(a->draw_items, (size_t)count, sizeof(*a->draw_items), game_draw_compare);
    return count;
}
static void objects(GameApp *a) {
    int i, count = draw_queue(a);
    for (i = 0; i < count; i++) {
        GameDrawItem *item = &a->draw_items[i];
        switch (item->kind) {
        case DRAW_PROP:
            prop(a, item->index);
            break;
        case DRAW_BUILDING:
            building(a, item->index);
            break;
        case DRAW_VILLAGER:
            villager(a, item->index);
            break;
        case DRAW_ENEMY:
            enemy(a, item->index);
            break;
        case DRAW_ANIMAL: {
            CoreAnimalView *animal = &a->session.animals[item->index];
            CoreAnimalInfo info;
            core_expansion_animal_info(animal->kind, &info);
            float width = info.width_milli * 16 / 1000.0f, height = info.height_milli * 16 / 1000.0f;
            GameAnimalActor *unit = &a->session.animal_actors[item->index];
            float x = unit->position.x * 16, y = unit->position.y * 16;
            int moving = unit->remaining > 0 && a->speed > 0 && a->panel == PANEL_NONE;
            int frame = game_walk_frame(unit->walk_distance, moving);
            int cell = animal->y_milli / 1000 * a->session.summary.map_width + animal->x_milli / 1000;
            Texture2D texture =
                animal->kind < 24 && a->session.terrain[cell] == CORE_TERRAIN_WATER && !a->session.roads[cell] && info.swim_milli
                    ? a->assets.animal_swim
                    : a->assets.animal_walk;
            GameEffect *fx = game_effect(&a->session, 1, animal->id);
            if (fx && fx->attack_tick && a->session.visual_time - fx->attack_started < .4f &&
                !(fx->hurt_started <= a->session.visual_time && fx->hurt_until > a->session.visual_time)) {
                texture = a->assets.animal_attack;
                frame = (int)((a->session.visual_time - fx->attack_started) * 10);
            }
            Rectangle dest = {x - width / 2, y - height, width, height};
            sprite(texture,
                   (Rectangle){frame * 96.0f, animal->kind * 96.0f, unit->facing < 0 ? -96.0f : 96.0f, 96},
                   dest, effect_tint(a, 1, animal->id));
            life_bar(a, 1, animal->id, animal->hp, info.hp, dest);
            break;
        }
        }
    }
    for (i = 0; i < a->session.projectile_count; i++) {
        GameProjectile *p = &a->session.projectiles[i];
        float t = fminf(1, p->elapsed / p->duration);
        Vector2 from = pixel(p->from), to = pixel(p->to),
                now = {from.x + (to.x - from.x) * t, from.y + (to.y - from.y) * t - 8};
        Texture2D arrow = a->assets.arrow;
        float height = arrow.width ? 8.0f * arrow.height / arrow.width : 2;
        Color shot=p->source_type==6?(Color){65,220,185,255}:p->source_type==12?SKYBLUE:p->source_type==13?PURPLE:p->source_type==16?GREEN:p->source_type==21?SKYBLUE:p->source_type==25?ORANGE:p->source_type==27?GOLD:WHITE;
        Vector2 trail={now.x-(to.x-from.x)*.04f,now.y-(to.y-from.y)*.04f};
        DrawLineEx(trail,now,1,Fade(shot,.65f));
        int type=p->source_type;
        if(type==6||type==12||type==13||type==16||type==21||type==25||type==27||type==100+CORE_BLD_ARCANE_TOWER||type==100+CORE_BLD_CANNON_TOWER) {
            if(type==100+CORE_BLD_ARCANE_TOWER)shot=SKYBLUE;
            if(type==100+CORE_BLD_CANNON_TOWER)shot=DARKGRAY;
            DrawCircleV(now,2.8f,Fade(shot,.45f));
            if(type==21)DrawPoly(now,4,2.3f,45+a->session.visual_time*100,shot);
            else if(type==16)DrawTriangle((Vector2){now.x-3,now.y},(Vector2){now.x+2,now.y-1.8f},(Vector2){now.x+1,now.y+2},shot);
            else DrawCircleV(now,type==12?1.6f:2,shot);
            DrawCircleV(now,.7f,WHITE);
            if(type==27&&t>.8f)DrawCircleLines((int)to.x,(int)to.y,16*t,Fade(GOLD,(t-.8f)*3));
        } else {
            float rotation=atan2f(to.y-from.y,to.x-from.x)*RAD2DEG;
            for(int k=0;k<4;k++)DrawTexturePro(arrow,(Rectangle){0,0,(float)arrow.width,(float)arrow.height},
                (Rectangle){now.x+(k==0?.35f:k==1?-.35f:0),now.y+(k==2?.35f:k==3?-.35f:0),8,height},(Vector2){4,height/2},rotation,(Color){25,20,27,230});
            DrawTexturePro(arrow,(Rectangle){0,0,(float)arrow.width,(float)arrow.height},(Rectangle){now.x,now.y,8,height},(Vector2){4,height/2},rotation,type==10?(Color){190,230,220,255}:shot);
        }
    }
}
static void livestock_proxies(GameApp *a) {
    GameSession *s = &a->session;
    static const int kinds[4] = {7, 9, 8, 10};
    for (int b = 0; b < s->building_count; b++) {
        CoreBuildingView *farm = &s->buildings[b];
        if ((farm->type != CORE_BLD_RANCH && farm->type != CORE_BLD_PASTURE) ||
            farm->state != CORE_BSTATE_ACTIVE)
            continue;
        if (s->visibility && s->visibility[farm->y * s->summary.map_width + farm->x] < 2)
            continue;
        for (int k = 0; k < 4; k++) {
            int count = 0, representative = -1;
            for (int i = 0; i < s->animal_count; i++)
                if (s->animals[i].owner == farm->id && s->animals[i].kind == kinds[k]) {
                    count++;
                    if (representative < 0 || s->animals[i].hp < s->animals[representative].hp)
                        representative = i;
                }
            int visible = count > 0 ? (farm->type == CORE_BLD_PASTURE && count > 1 ? 2 : 1) : 0;
            for (int n = 0; n < visible; n++) {
                unsigned hash = (unsigned)farm->id * 73u + (unsigned)k * 197u + (unsigned)n * 829u;
                float phase = s->visual_time * .8f + (hash % 97) * .17f;
                float x = (farm->x + .5f + (hash % 100) / 100.0f * (farm->w - 1)) * 16 + sinf(phase) * 2;
                float y = (farm->y + .65f + ((hash / 100) % 100) / 100.0f * (farm->h - 1)) * 16 +
                          cosf(phase) * 1.5f;
                CoreAnimalInfo info;
                core_expansion_animal_info(kinds[k], &info);
                float width = info.width_milli * 16 / 1000.0f, height = info.height_milli * 16 / 1000.0f;
                Rectangle dest = {x - width / 2, y - height, width, height};
                int frame = (int)(phase * 3) & 3, id = s->animals[representative].id;
                int age=s->animals[representative].age, young=age<(int[]){24,12,16,8}[k];
                if(young){dest.width*=.65f;dest.height*=.65f;dest.y=y-dest.height;dest.x=x-dest.width/2;}
                sprite(young?a->assets.animal_babies:a->assets.animal_walk,
                       (Rectangle){frame * 96.0f, (young?(int[]){0,2,1,3}[k]:kinds[k]) * 96.0f, cosf(phase) > 0 ? 96.0f : -96.0f, 96},
                       dest, effect_tint(a, 1, id));
                life_bar(a, 1, id, s->animals[representative].hp, info.hp, dest);
            }
        }
    }
}
static void corpses(GameApp *a) {
    GameSession *s = &a->session;
    for (int i = 0; i < 2048; i++) {
        GameEffect *fx = &s->effects[i];
        float elapsed = s->visual_time - fx->death_started;
        if (!fx->id || !fx->dead || elapsed >= 1 || (fx->owner && !game_building(s, fx->owner)))
            continue;
        Texture2D texture;
        Rectangle source;
        float width = 24, height = 24;
        if (fx->category == 0) {
            GameSpriteFrame frame = game_villager_frame(fx->skin, fx->facing, 0, 0);
            texture = a->assets.villagers;
            source = (Rectangle){frame.column * 96.0f, frame.row * 96.0f, frame.flip ? -96.0f : 96.0f, 96};
        } else if (fx->category == 1) {
            CoreAnimalInfo info;
            core_expansion_animal_info(fx->skin, &info);
            width = info.width_milli * 16 / 1000.0f;
            height = info.height_milli * 16 / 1000.0f;
            texture = a->assets.animal_walk;
            source = (Rectangle){96, fx->skin * 96.0f, fx->facing < 0 ? -96.0f : 96.0f, 96};
        } else {
            texture = a->assets.enemies;
            source = (Rectangle){((fx->skin%4)*4+1)*96.0f,(fx->skin/4)*3*96.0f,96,96};
        }
        float angle = fmaxf(0, fminf(1, elapsed / .55f)) * 60 * ((fx->id * 73) & 1 ? 1 : -1);
        Color tint =
            elapsed < 0 ? WHITE
                        : Fade((Color){255, 110, 110, 255}, elapsed < .55f ? 1 : 1 - (elapsed - .55f) / .45f);
        DrawTexturePro(texture, source, (Rectangle){fx->position.x * 16, fx->position.y * 16, width, height},
                       (Vector2){width / 2, height}, angle, tint);
    }
}
void game_render_free(GameApp *a) {
    free(a->draw_items);
    a->draw_items = NULL;
    a->draw_capacity = 0;
    free(a->decor_blocked);
    a->decor_blocked = NULL;
    a->decor_capacity = 0;
}
void game_render_villager_preview(GameApp *a) {
    static const char *const names[12] = {u8"갈색 남성",      u8"갈색 여성",      u8"금발 남성",
                                         u8"금발 여성",      u8"검은 머리 남성", u8"검은 머리 여성",
                                         u8"붉은 머리 남성", u8"붉은 머리 여성",u8"은발 남성",u8"은발 여성",u8"남색 머리 남성",u8"남색 머리 여성"};
    static const char *const directions[4] = {u8"정면", u8"오른쪽", u8"뒷면", u8"왼쪽"};
    float gap = (GetScreenWidth() - 150.0f) / 12, row_height = (GetScreenHeight() - 140.0f) / 4;
    int appearance, direction;
    Texture2D t = a->assets.villagers;
    ClearBackground((Color){23, 32, 33, 255});
    ui_text(&a->ui, 24, 18, UI_GOLD, u8"주민 외형 12종 · 방향별 걷기");
    ui_small(&a->ui, 24, 54, UI_TEXT, u8"직업을 바꿔도 외형 유지 · 주민 1×1칸 / 그림 1.5타일 높이");
    for (appearance = 0; appearance < 12; appearance++)
        ui_small(&a->ui, 140 + appearance * gap, 92, UI_GOLD, "%s", names[appearance]);
    for (direction = 0; direction < 4; direction++) {
        float top = 140 + direction * row_height;
        ui_small(&a->ui, 24, top + 30, UI_TEXT, "%s", directions[direction]);
        for (appearance = 0; appearance < 12; appearance++) {
            float cx = 140 + appearance * gap + gap * 0.35f, feet = top + 100;
            GameSpriteFrame frame =
                game_villager_frame(appearance + 1, direction, (float)GetTime() * 0.5f, 1);
            DrawRectangleLinesEx((Rectangle){cx - 32, feet - 32, 64, 64}, 1, (Color){65, 81, 75, 255});
            DrawEllipse((int)cx, (int)feet, 16, 5, (Color){0, 0, 0, 70});
            sprite(t, (Rectangle){frame.column * 96.0f, frame.row * 96.0f, frame.flip ? -96.0f : 96.0f, 96},
                   (Rectangle){cx - 48, feet - 96, 96, 96}, WHITE);
        }
    }
}
static int world_mouse(GameApp *a, int *x, int *y) {
    Vector2 mouse = GetMousePosition(), world = GetScreenToWorld2D(mouse, a->camera);
    int i;
    float notice_y = 126;
    *x = (int)floorf(world.x / 16);
    *y = (int)floorf(world.y / 16);
    /* 위치로 이동하는 알림을 누를 때 뒤쪽 건물을 동시에 조작하지 않습니다. */
    for (i = a->notice_count - 1; i >= 0 && notice_y < GetScreenHeight() - 185; i--) {
        if (a->notices[i].age > 16)
            continue;
        if (CheckCollisionPointRec(mouse, (Rectangle){12, notice_y, 390, 64}))
            return 0;
        notice_y += 70;
    }
    return a->panel == PANEL_NONE && mouse.y > 135 && mouse.y < GetScreenHeight() - 195 &&
           mouse.x < GetScreenWidth() - 350;
}
static void overlay(GameApp *a) {
    int x, y;
    CorePlacementView preview;
    GameSession *s = &a->session;
    int mouse_on_map=world_mouse(a,&x,&y);
    if(a->demolish && mouse_on_map) {
        Rectangle tile={x*16.0f,y*16.0f,16,16};
        DrawRectangleRec(tile,(Color){230,40,46,100});
        DrawRectangleLinesEx(tile,1.8f,(Color){255,75,75,255});
        DrawLineEx((Vector2){tile.x+3,tile.y+3},(Vector2){tile.x+13,tile.y+13},1.3f,WHITE);
        DrawLineEx((Vector2){tile.x+13,tile.y+3},(Vector2){tile.x+3,tile.y+13},1.3f,WHITE);
    }
    CoreBuildingView *selected=game_building(s,a->selected_building);
    int candidate=a->build_type>=0 && mouse_on_map;
    if(candidate || selected) {
        CoreFacilityView facility;
        int type=candidate?a->build_type:selected->type;
        int bx=candidate?x:selected->x,by=candidate?y:selected->y;
        CoreExpansionBuildingInfo info;core_expansion_building_info(type,&info);
        int rotation=candidate?a->rotation:selected->rotation;
        int bw=(rotation&1)?info.h:info.w,bh=(rotation&1)?info.w:info.h;
        if(core_expansion_facility(s->world,candidate?0:selected->id,type,bx,by,rotation,&facility)==CORE_OK) {
            int radius=facility.range>0?facility.range:facility.radius;
            if(radius>0) {
                Vector2 center={(bx+bw*.5f)*16,(by+bh*.5f)*16};
                Color color=facility.range?SKYBLUE:GREEN;
                DrawCircleLinesV(center,radius*16.0f,Fade(color,.7f));
                for(int yy=by-radius;yy<by+bh+radius;yy++)for(int xx=bx-radius;xx<bx+bw+radius;xx++) {
                    if(xx<0||yy<0||xx>=s->summary.map_width||yy>=s->summary.map_height)continue;
                    int lx=bx+(bw-1)/2,rx=bx+bw/2,ty=by+(bh-1)/2,bot=by+bh/2;
                    int distance=(xx<lx?lx-xx:xx>rx?xx-rx:0)+(yy<ty?ty-yy:yy>bot?yy-bot:0);
                    if(distance<=radius && (!s->visibility||s->visibility[yy*s->summary.map_width+xx]>=2)) {
                        DrawRectangle(xx*16,yy*16,16,16,Fade(color,.10f));
                        DrawRectangleLines(xx*16,yy*16,16,16,Fade(color,.20f));
                    }
                }
            }
        }
    }
    if (a->build_type < 0 || !mouse_on_map)
        return;
    {
        CoreBuildingDefView *d = &s->building_defs[a->build_type];
        int tx, ty, bw = (a->rotation & 1) ? d->h : d->w, bh = (a->rotation & 1) ? d->w : d->h;
        x -= (bw - 1) / 2;
        y -= (bh - 1) / 2;
        if (core_can_place_rotated(s->world, a->build_type, x, y, a->rotation, &preview) != CORE_OK)
            return;
        if (d->work_radius > 0) {
            float cx = x + d->w * 0.5f, cy = y + d->h * 0.5f;
            DrawCircleLines((int)(cx * 16), (int)(cy * 16), (float)d->work_radius * 16, Fade(UI_GOLD, 0.7f));
            for (ty = y - d->work_radius; ty <= y + d->h + d->work_radius; ty++)
                for (tx = x - d->work_radius; tx <= x + d->w + d->work_radius; tx++) {
                    float dx = tx + 0.5f - cx, dy = ty + 0.5f - cy;
                    if (tx >= 0 && ty >= 0 && tx < s->summary.map_width && ty < s->summary.map_height &&
                        dx * dx + dy * dy <= d->work_radius * d->work_radius &&
                        s->terrain[ty * s->summary.map_width + tx] == d->count_terrain)
                        DrawRectangle(tx * 16, ty * 16, 16, 16, Fade(UI_GOLD, 0.3f));
                }
        }
        DrawRectangle(x * 16, y * 16, bw * 16, bh * 16,
                      Fade(preview.result == CORE_PLACE_OK ? UI_GOOD : UI_BAD, 0.4f));
        if (a->build_type != CORE_BLD_ROAD) {
            Texture2D t = a->assets.turns[a->build_type][a->rotation];
            CoreBuildingView temporary = {0};
            Rectangle ghost;
            temporary.type = a->build_type;
            temporary.x = x;
            temporary.y = y;
            temporary.w = bw;
            temporary.h = bh;
            temporary.kind_flags = d->kind_flags;
            temporary.worker_slots = d->worker_slots;
            temporary.resident_slots = d->resident_slots;
            temporary.rotation = a->rotation;
            t = building_texture(a, &temporary);
            ghost = building_rect(a,&temporary, t);
            sprite(t, building_source(a, &temporary, t), ghost,
                   Fade(preview.result == CORE_PLACE_OK ? WHITE : UI_BAD, 0.65f));
        }
        DrawRectangleLines(x * 16, y * 16, bw * 16, bh * 16,
                           preview.result == CORE_PLACE_OK ? UI_GOOD : UI_BAD);
        if (preview.door_x >= 0)
            DrawRectangle(preview.door_x * 16, preview.door_y * 16, 16, 16, Fade(SKYBLUE, 0.6f));
        if (a->dragging)
            DrawLine(a->drag_x * 16 + 8, a->drag_y * 16 + 8, x * 16 + 8, y * 16 + 8, UI_GOLD);
    }
}
static void bubble(GameApp *a, Vector2 anchor, int icon, int warning) {
    Vector2 p = GetWorldToScreen2D(anchor, a->camera);
    Rectangle box = {p.x - 17, p.y - 37, 34, 30};
    if (p.x < 0 || p.y < 0 || p.x > GetScreenWidth() || p.y > GetScreenHeight())
        return;
    DrawRectangleRounded(box, 0.25f, 4, (Color){255, 249, 225, 245});
    DrawRectangleRoundedLinesEx(box, 0.25f, 4, warning ? 3.0f : 1.0f,
                                warning ? RED : (Color){75, 52, 36, 255});
    DrawTriangle((Vector2){p.x - 4, p.y - 8}, (Vector2){p.x, p.y - 1}, (Vector2){p.x + 4, p.y - 8},
                 (Color){255, 249, 225, 245});
    sprite(a->assets.status_icons, (Rectangle){icon % 6 * 64.0f, icon / 6 * 64.0f, 64, 64},
           (Rectangle){box.x + 4, box.y + 2, 26, 26}, WHITE);
}
static void status_bubbles(GameApp *a) {
    GameSession *s = &a->session;
    int i;
    for (i = 0; i < s->building_count; i++) {
        CoreBuildingView *b = &s->buildings[i];
        Rectangle image = building_rect(a,b, building_texture(a, b));
        Vector2 anchor = {image.x + image.width / 2, image.y - 3};
        if (b->state != CORE_BSTATE_ACTIVE ||
            (s->visibility && !s->visibility[b->y * s->summary.map_width + b->x]))
            continue;
        if (b->worker_slots && (!b->workers || b->blocked_reason == CORE_DENY_REQUIREMENTS))
            bubble(a, anchor, 20, 1);
        else if (b->working_now && b->job_field >= 0)
            bubble(a, anchor, b->job_field, 0);
    }
    for (i = 0; i < s->villager_count; i++) {
        CoreVillagerView *v = &s->villagers[i];
        GameActor *actor = game_actor(s, v->id);
        int icon = -1;
        Vector2 p;
        if (!actor)
            continue;
        p = pixel(actor->position);
        p.y -= GAME_ACTOR_HEIGHT;
        if (v->state == CORE_VS_FLEE)
            icon = 19;
        else if (v->hunger < 40)
            icon = 12;
        else if (v->flags & CORE_VF_SICK)
            icon = 14;
        else if (v->water < 30)
            icon = 15;
        else if (v->state == CORE_VS_SLEEP)
            icon = 13;
        else if (v->flags & CORE_VF_UNPAID)
            icon = 18;
        else if (v->happiness < 30)
            icon = 16;
        else if (!v->home_id)
            icon = 17;
        else if (v->rest < 25)
            icon = 13;
        if (icon < 0)
            continue;
        if (v->inside_building) {
            CoreBuildingView *b = game_building(s, v->inside_building);
            if (b) {
                Rectangle image = building_rect(a,b, building_texture(a,b));
                p = (Vector2){image.x + image.width / 2 + (i % 3 - 1) * 12, image.y - 17};
            }
        }
        bubble(a, p, icon, icon == 12 || icon == 14);
    }
}
static void fog(GameApp *a) {
    GameSession *s = &a->session;
    int x, y;
    Vector2 top = GetScreenToWorld2D((Vector2){0, 0}, a->camera),
            bottom =
                GetScreenToWorld2D((Vector2){(float)GetScreenWidth(), (float)GetScreenHeight()}, a->camera);
    int x0 = (int)fmaxf(0, floorf(top.x / 16)), y0 = (int)fmaxf(0, floorf(top.y / 16)),
        x1 = (int)fminf((float)s->summary.map_width, ceilf(bottom.x / 16)),
        y1 = (int)fminf((float)s->summary.map_height, ceilf(bottom.y / 16));
    if (!s->visibility)
        return;
    for (y = y0; y < y1; y++)
        for (x = x0; x < x1; x++) {
            int vis = s->visibility[y * s->summary.map_width + x];
            if (vis < 2)
                DrawRectangle(x * 16, y * 16, 16, 16,
                              vis ? (Color){12, 20, 24, 140} : (Color){12, 20, 24, 255});
        }
}
void game_render_world(GameApp *a) {
    float time = (float)GetTime();
    int i;
    if (!a->session.world)
        return;
    BeginMode2D(a->camera);
    terrain(a);
    objects(a);
    livestock_proxies(a);
    corpses(a);
    fog(a);
    for (i = 0; i < a->session.villager_count; i++) {
        CoreVillagerView *v = &a->session.villagers[i];
        GameActor *unit = game_actor(&a->session, v->id);
        if (!unit || (!v->incoming && v->state != CORE_VS_LEAVING))
            continue;
        int cell = (int)unit->position.y * a->session.summary.map_width + (int)unit->position.x;
        if (cell >= 0 && cell < a->session.cells && a->session.visibility && a->session.visibility[cell] < 2)
            villager(a, i);
    }
    overlay(a);
    EndMode2D();
    if (a->session.summary.is_night)
        DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), (Color){22, 28, 65, 85});
    if (a->session.summary.weather == CORE_WEATHER_RAIN || a->session.summary.weather == CORE_WEATHER_SNOW ||
        a->session.summary.weather == CORE_WEATHER_STORM) {
        int snow = a->session.summary.weather == CORE_WEATHER_SNOW;
        if(!snow)game_sound(&a->assets,SND_RAIN,a->settings.sfx_volume*.12f);
        for (i = 0; i < (a->settings.low_effects?25:100); i++) {
            float x = fmodf(i * 131.7f + time * 17, (float)GetScreenWidth());
            float y = fmodf(i * 83.3f + time * (snow ? 30 : 280), (float)GetScreenHeight());
            if (snow)
                DrawCircleV((Vector2){x, y}, 1.5f, Fade(WHITE, 0.5f));
            else
                DrawLine((int)x, (int)y, (int)x - 3, (int)y + 9, Fade(SKYBLUE, 0.3f));
        }
    }
    CoreEventLayers layers={0};(void)core_expansion_events(a->session.world,&layers);
    int effect=a->session.summary.nature_event;
    if(!a->settings.low_effects && effect==CORE_NATURE_STORM && fmodf(time,7)<.16f) {
        game_sound(&a->assets,SND_THUNDER,a->settings.sfx_volume*.3f);
        float x=GetScreenWidth()*.68f;
        Vector2 last={x,0};
        for(int k=1;k<7;k++){Vector2 next={x+sinf(k*2.7f)*35,k*GetScreenHeight()/9.0f};DrawLineEx(last,next,2,Fade(WHITE,.8f));last=next;}
    }
    if(layers.plague_days) {
        for(int k=0;k<(a->settings.low_effects?4:18);k++) {
            float x=fmodf(k*173.0f+time*9,GetScreenWidth()+80.0f)-40,y=fmodf(k*97.0f+time*3,GetScreenHeight()+80.0f)-40;
            DrawCircleGradient((int)x,(int)y,25,(Color){100,155,80,22},BLANK);
        }
    }
    if(effect==CORE_NATURE_COLD_SNAP)DrawRectangleLinesEx((Rectangle){1,1,GetScreenWidth()-2.0f,GetScreenHeight()-2.0f},a->settings.low_effects?3.0f:10.0f,(Color){175,225,255,65});
    if(!a->settings.low_effects && (effect==CORE_NATURE_BUMPER || effect==CORE_NATURE_DROUGHT || layers.economic_days))
        for(int k=0;k<20;k++) {
            float x=fmodf(k*97.0f+time*11,(float)GetScreenWidth()),y=fmodf(k*61.0f+time*4,(float)GetScreenHeight());
            DrawCircleV((Vector2){x,y},1,Fade(effect==CORE_NATURE_DROUGHT?BROWN:layers.economic_event==CORE_NATURE_MINE_COLLAPSE?GRAY:GOLD,.35f));
        }
    status_bubbles(a);
}
void game_world_input(GameApp *a, float delta) {
    if(a->raid_alert_id)return;
    int x, y, inside = world_mouse(a, &x, &y);
    GameSession *s = &a->session;
    if (a->panel != PANEL_NONE || !a->in_game)
        return;
    if (IsKeyPressed(KEY_SPACE))
        a->speed = a->speed ? 0 : 1;
    if (IsKeyPressed(KEY_ONE))
        a->speed = 1;
    if (IsKeyPressed(KEY_TWO))
        a->speed = 2;
    if (IsKeyPressed(KEY_THREE))
        a->speed = 3;
    if (IsKeyPressed(KEY_V))
        game_app_open(a, PANEL_VILLAGERS);
    if (IsKeyPressed(KEY_R)) {
        if (a->build_type >= 0)
            a->rotation = (a->rotation + 1) % 4;
        else if (a->selected_building) {
            CoreBuildingView *b = game_building(s, a->selected_building);
            int32_t reason = 0;
            if (b)
                game_app_result(a, core_rotate_building(s->world, b->id, (b->rotation + 1) % 4, &reason),
                                reason, 1);
        }
    }
    if (IsKeyPressed(KEY_F1))
        game_app_open(a, PANEL_HELP);
    if (IsKeyPressed(KEY_X)) {
        a->demolish = !a->demolish;
        a->build_type = -1;
    }
    if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
        a->build_type = -1;
        a->demolish = 0;
        a->dragging = 0;
    }
    if (IsKeyPressed(KEY_ESCAPE)) {
        if (a->build_type >= 0 || a->demolish || a->dragging) {
            a->build_type = -1;
            a->demolish = 0;
            a->dragging = 0;
        } else if (a->selection_kind || a->selected_building || a->selected_villager)
            game_select(a,0,0);
        else
            game_app_open(a, PANEL_PAUSE);
    }
    if (a->panel != PANEL_NONE)
        return;
    {
        float amount = 230 * delta / a->camera.zoom;
        if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT))
            a->camera.target.x -= amount;
        if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT))
            a->camera.target.x += amount;
        if (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP))
            a->camera.target.y -= amount;
        if (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN))
            a->camera.target.y += amount;
        if (a->settings.edge_scroll && IsWindowFocused() && inside) {
            if (GetMouseX() < 8)
                a->camera.target.x -= amount;
            if (GetMouseX() > GetScreenWidth() - 358)
                a->camera.target.x += amount;
            if (GetMouseY() < 122)
                a->camera.target.y -= amount;
            if (GetMouseY() > GetScreenHeight() - 153)
                a->camera.target.y += amount;
        }
        if (IsMouseButtonDown(MOUSE_BUTTON_MIDDLE)) {
            Vector2 d = GetMouseDelta();
            a->camera.target.x -= d.x / a->camera.zoom;
            a->camera.target.y -= d.y / a->camera.zoom;
        }
        if (inside) {
            float wheel = GetMouseWheelMove();
            if (wheel != 0) {
                Vector2 before = GetScreenToWorld2D(GetMousePosition(), a->camera), after;
                a->camera.zoom = fmaxf(1, fminf(6, a->camera.zoom + (wheel > 0 ? 1 : -1)));
                after = GetScreenToWorld2D(GetMousePosition(), a->camera);
                a->camera.target.x += before.x - after.x;
                a->camera.target.y += before.y - after.y;
            }
        }
        a->camera.target.x = fmaxf(0, fminf(s->summary.map_width * 16.0f, a->camera.target.x));
        a->camera.target.y = fmaxf(0, fminf(s->summary.map_height * 16.0f, a->camera.target.y));
    }
    if (!inside) {
        if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
            a->dragging = 0;
        return;
    }
    if (a->build_type >= 0) {
        CoreBuildingDefView *d = &s->building_defs[a->build_type];
        int32_t reason = 0, id = 0;
        int rc;
        x -= ((a->rotation & 1 ? d->h : d->w) - 1) / 2;
        y -= ((a->rotation & 1 ? d->w : d->h) - 1) / 2;
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && d->line) {
            a->dragging = 1;
            a->drag_x = x;
            a->drag_y = y;
        }
        if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
            if (d->line && a->dragging)
                rc = core_place_line_rotated(s->world, a->build_type, a->drag_x, a->drag_y, x, y, a->rotation,
                                             &id, &reason);
            else if (!d->line)
                rc = core_place_rotated(s->world, a->build_type, x, y, a->rotation, &id, &reason);
            else
                return;
            a->dragging = 0;
            game_app_result(a, rc, reason, 1);
            if (rc == CORE_OK)
                game_sound(&a->assets, SND_PLACE, a->ui.volume);
            (void)game_session_refresh(s);
            game_session_events(s);
        }
    } else if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        int id = game_building_at(s, x, y);
        if (a->demolish) {
            int rc = id ? core_demolish(s->world, id) : core_remove_road(s->world, x, y);
            game_app_result(a, rc, 0, 0);
            (void)game_session_refresh(s);
            game_session_events(s);
        } else {
            Vector2 mouse=GetScreenToWorld2D(GetMousePosition(),a->camera);
            float best=1000000;game_select(a,id?1:0,id);
            for(int i=0;i<s->actor_count;i++) {
                GameActor *actor=&s->actors[i];CoreVillagerView *v=game_villager(s,actor->id);
                if(!v||v->inside_building)continue;
                GameSpriteBox box=game_actor_sprite(actor->position.x*16,actor->position.y*16,GAME_ACTOR_HEIGHT);
                Rectangle hit={box.x,box.y,box.w,box.h};
                float dx=mouse.x-hit.x-hit.width/2,dy=mouse.y-hit.y-hit.height/2,distance=dx*dx+dy*dy;
                if(CheckCollisionPointRec(mouse,hit)&&distance<best){best=distance;game_select(a,2,v->id);}
            }
            for(int i=0;i<s->animal_count;i++) {
                CoreAnimalView *animal=&s->animals[i];if(!animal->alive)continue;
                int cell=animal->y_milli/1000*s->summary.map_width+animal->x_milli/1000;
                if(s->visibility&&s->visibility[cell]<2)continue;
                CoreAnimalInfo info;core_expansion_animal_info(animal->kind,&info);
                float width=info.width_milli*16/1000.0f,height=info.height_milli*16/1000.0f;
                Vector2 feet={s->animal_actors[i].position.x*16,s->animal_actors[i].position.y*16};
                Rectangle hit={feet.x-width/2,feet.y-height,width,height};
                float dx=mouse.x-hit.x-hit.width/2,dy=mouse.y-hit.y-hit.height/2,distance=dx*dx+dy*dy;
                if(CheckCollisionPointRec(mouse,hit)&&distance<best){best=distance;game_select(a,3,animal->id);}
            }
            for(int i=0;i<s->enemy_count;i++) {
                GameEnemy *enemy=&s->enemies[i];if(enemy->hp<=0)continue;
                int cell=(int)enemy->actor.position.y*s->summary.map_width+(int)enemy->actor.position.x;
                if(s->visibility&&s->visibility[cell]<2)continue;
                float width=enemy->info.width_milli*16/1000.0f,height=enemy->info.height_milli*16/1000.0f;
                Rectangle hit={enemy->actor.position.x*16-width/2,enemy->actor.position.y*16-height,width,height};
                float dx=mouse.x-hit.x-hit.width/2,dy=mouse.y-hit.y-hit.height/2,distance=dx*dx+dy*dy;
                if(CheckCollisionPointRec(mouse,hit)&&distance<best){best=distance;game_select(a,4,enemy->actor.id);}
            }
        }
    }
}
