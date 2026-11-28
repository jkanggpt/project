/* render.c — 픽셀 지도·주민·건물·전투와 카메라·건설 입력 */
#include "app.h"
#include "format.h"
#include "gpu_bridge.h"
#include "carried_lights.h"
#include "scene_profiles.h"
#include "review.h"
#include "gpu.h"
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
    ui_text(&a->ui, 28, 18, UI_GOLD, u8"자동 연결 성벽·성문 · 동물 35종 걷기 4프레임");
    ui_small(&a->ui, 28, 52, UI_MUTED,
             u8"가로·세로·꺾임·교차를 이웃 칸에 맞춰 선택합니다. 동물은 이동 방향에 따라 좌우 반전합니다.");
    for (r = 0; r < 4; r++) {
        uint8_t cost[7 * 5];
        memset(cost, CORE_COST_NORMAL, sizeof(cost));
        for (y = 0; y < 5; y++)
            for (x = 0; x < 7; x++)
                if (x == 0 || x == 6 || y == 0 || y == 4 || x == 3)
                    cost[y * 7 + x] = CORE_COST_WALL;
        cost[4 * 7 + 3] = CORE_COST_GATE;
        cost[2 * 7] = CORE_COST_GATE;
        ui_small(&a->ui, 28 + r * 350.0f, 86, UI_GOLD, "%s", walls[r]);
        for (y = 0; y < 5; y++)
            for (x = 0; x < 7; x++) {
                float xx = 28 + r * 350.0f + x * 40, yy = 126.0f + y * 32;
                DrawRectangle((int)xx, (int)yy, 40, 32, (Color){48, 75, 46, 255});
                if (cost[y * 7 + x] == CORE_COST_GATE) {
                    DrawTexturePro(a->assets.gates,
                                   (Rectangle){(x == 0 ? 1 : 0) * 128.0f, r * 128.0f, 128, 128},
                                   (Rectangle){xx, yy - 8, 40, 40}, (Vector2){0, 0}, 0, WHITE);
                } else if (cost[y * 7 + x] == CORE_COST_WALL) {
                    int mask = game_wall_mask(cost, 7, 5, x, y);
                    DrawTexturePro(a->assets.wall_links, (Rectangle){mask * 128.0f, r * 128.0f, 128, 128},
                                   (Rectangle){xx, yy - 8, 40, 40}, (Vector2){0, 0}, 0, WHITE);
                }
            }
    }
    for (r = 0; r < 9; r++)
        for (c = 0; c < 4; c++) {
            int kind = r * 4 + c, frame;
            if (kind >= 35)
                continue;
            CoreAnimalInfo info;
            core_expansion_animal_info(kind, &info);
            float xx = 28 + c * 350.0f, yy = 292 + r * 67.0f;
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
             u8"R 키로 회전합니다. 온실은 유리 지붕, 일반 농장은 밭으로 표시합니다. 그림 높이와 점유는 "
             u8"별개입니다.");
    for (c = 0; c < 4; c++)
        ui_small(&a->ui, 200 + c * column, 87, UI_TEXT, "%s", directions[c]);
    for (r = 0; r < 6; r++) {
        CoreExpansionBuildingInfo info;
        float y = 121 + r * row;
        (void)core_expansion_building_info(types[r], &info);
        ui_small(&a->ui, 25, y + row / 2, UI_GOLD, u8"%s\n%d×%d칸", info.name, info.w, info.h);
        for (c = 0; c < 4; c++) {
            Texture2D t = a->assets.turns[types[r]][c];
            Rectangle source = {0, 0, (float)t.width, (float)t.height};
            if (types[r] == CORE_BLD_GREENHOUSE) {
                t = a->assets.greenhouse_turns;
                source = a->assets.greenhouse_sources[a->session.summary.season * 4 + c];
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
            DrawTexturePro(t, source, (Rectangle){x + column / 2 - width / 2, base - height, width, height},
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
    if (game_gpu_sprite(texture, source, destination, tint))
        return;
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
    if (max <= 0 || hp <= 0)
        return;
    GameEffect *fx = game_effect(&a->session, category, id);
    Vector2 mouse = GetScreenToWorld2D(GetMousePosition(), game_gpu_screen_camera(a));
    int selected = category == 0   ? a->selected_villager == id
                   : category == 1 ? a->selection_kind == 3 && a->selection_id == id
                                   : a->selection_kind == 4 && a->selection_id == id;
    if (!selected && !CheckCollisionPointRec(mouse, dest) &&
        (!fx || fx->bar_until < a->session.visual_time || fx->hurt_started > a->session.visual_time))
        return;
    Vector2 screen =
        GetWorldToScreen2D((Vector2){dest.x + dest.width * .5f, dest.y}, game_gpu_screen_camera(a));
    float zoom = game_gpu_screen_camera(a).zoom;
    float width = fminf(72, fmaxf(32, dest.width * zoom * .65f)),
          height = selected || hp * 4 <= max ? 8.0f : 6.0f;
    if (screen.x + width / 2 < 0 || screen.x - width / 2 > GetScreenWidth() || screen.y > GetScreenHeight())
        return;
    float top = screen.y - height - 10;
    if (top < 0)
        top = screen.y + 2;
    Vector2 origin =
        GetScreenToWorld2D((Vector2){floorf(screen.x - width / 2), floorf(top)}, game_gpu_screen_camera(a));
    float z = zoom;
    Rectangle bar = {origin.x, origin.y, width / z, height / z};
    DrawRectangleRec(bar, Fade(BLACK, .75f));
    DrawRectangleLinesEx(bar, 1 / z, Fade(WHITE, .75f));
    Color color = hp * 4 <= max ? RED : hp * 2 <= max ? YELLOW : GREEN;
    DrawRectangleRec((Rectangle){bar.x + 1 / z, bar.y + 1 / z, (width - 2) / z * hp / max, (height - 2) / z},
                     color);
}

/* 시뮬레이션 시야는 그대로 두고 구름 밑에 지면 한 칸을 여유로 그립니다. */
static int terrain_fringe(const GameSession *s, int x, int y) {
    if (!s->visibility || s->visibility[y * s->summary.map_width + x] >= 2)
        return 1;
    for (int dy = -1; dy <= 1; dy++)
        for (int dx = -1; dx <= 1; dx++) {
            int xx = x + dx, yy = y + dy;
            if (xx >= 0 && yy >= 0 && xx < s->summary.map_width && yy < s->summary.map_height &&
                s->visibility[yy * s->summary.map_width + xx] >= 2)
                return 1;
        }
    return 0;
}
static void terrain(GameApp *a) {
    GameSession *s = &a->session;
    int x, y, w = s->summary.map_width, h = s->summary.map_height;
    Vector2 top = GetScreenToWorld2D((Vector2){0, 0}, game_gpu_screen_camera(a)),
            bottom = GetScreenToWorld2D((Vector2){(float)GetScreenWidth(), (float)GetScreenHeight()},
                                        game_gpu_screen_camera(a));
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
                if ((pass && !s->roads[i]) || !terrain_fringe(s, x, y))
                    continue;
                Rectangle dest = {(float)x * 16, (float)y * 16, 16, 16};
                if (pass) {
                    static const int dx[8] = {0, 1, 0, -1, 1, 1, -1, -1}, dy[8] = {-1, 0, 1, 0, -1, 1, 1, -1};
                    int mask = 0;
                    for (int k = 0; k < 8; k++) {
                        int xx = x + dx[k], yy = y + dy[k];
                        if (xx >= 0 && yy >= 0 && xx < w && yy < h &&
                            (s->roads[yy * w + xx] || s->cost[yy * w + xx] == CORE_COST_GATE))
                            mask |= 1 << k;
                    }
                    if (s->roads[i] == 3) {
                        int row = s->terrain[i] == CORE_TERRAIN_WATER ? 0 : 1;
                        sprite(a->assets.bridge_parts, (Rectangle){variant * 128.0f, row * 128.0f, 128, 128},
                               dest, WHITE);
                        int rail = core_expansion_bridge_mask(s->world, x, y);
                        for (int d = 0; d < 4; d++)
                            if (rail & (1 << d))
                                if (d == 0 || d == 3)
                                    sprite(a->assets.bridge_parts, (Rectangle){d * 128.0f, 384, 128, 128},
                                           (Rectangle){dest.x, dest.y - 5, 16, 21}, WHITE);
                        if (s->terrain[i] == CORE_TERRAIN_WATER)
                            for (int d = 0; d < 4; d++) {
                                int xx = x + dx[d], yy = y + dy[d];
                                if (xx >= 0 && yy >= 0 && xx < w && yy < h &&
                                    s->terrain[yy * w + xx] != CORE_TERRAIN_WATER && s->cost[yy * w + xx] &&
                                    !(rail & (1 << d)))
                                    sprite(a->assets.bridge_parts, (Rectangle){d * 128.0f, 256, 128, 128},
                                           dest, WHITE);
                            }
                    } else
                        sprite(
                            a->assets.road_tiles,
                            (Rectangle){(mask % 16) * 64.0f,
                                        ((s->roads[i] == 4 ? 2 : s->roads[i] - 1) * 16 + mask / 16) * 64.0f,
                                        64, 64},
                            dest, WHITE);
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
                if (s->exp_tiles && s->terrain[i] != CORE_TERRAIN_WATER && tile_noise(x, y) % 43 == 0) {
                    CoreExpansionTileView *soil = &s->exp_tiles[i];
                    if (soil->clay_initial_milli > 0 && soil->clay_milli > 0) {
                        int col = soil->clay_shore_percent > 100 || soil->biome == CORE_BIOME_SWAMP ? 3
                                  : soil->biome == CORE_BIOME_WETLAND                               ? 2
                                  : soil->biome == CORE_BIOME_FOREST                                ? 1
                                                                                                    : 0;
                        float alpha = .42f * soil->clay_milli / soil->clay_initial_milli;
                        sprite(a->assets.soil_markers, (Rectangle){col * 128.0f, 0, 128, 128}, dest,
                               Fade(WHITE, alpha));
                    }
                }
                if (s->summary.season == CORE_SEASON_WINTER && s->terrain[i] != CORE_TERRAIN_WATER)
                    sprite(a->assets.snow, (Rectangle){variant * 64.0f, 0, 64, 64}, dest,
                           (Color){255, 255, 255, 225});
            }
}
static Rectangle box_rect(GameSpriteBox b) { return (Rectangle){b.x, b.y, b.w, b.h}; }
static int farm_row(int type) {
    return type == CORE_BLD_CROPLAND         ? 0
           : type == CORE_BLD_FARM           ? 1
           : type == CORE_BLD_IRRIGATED_FARM ? 2
           : type == CORE_BLD_ORCHARD        ? 3
           : type == CORE_BLD_GARDEN         ? 4
                                             : -1;
}
static int observation_frame(const GameApp *a, const CoreBuildingView *b) {
    if (b->type != CORE_BLD_OBSERVATORY || b->state != CORE_BSTATE_ACTIVE || !b->working_now)
        return -1;
    CoreForecastView f;
    if (core_expansion_forecast(a->session.world, b->id, &f) != CORE_OK || f.reason)
        return -1;
    return b->rotation * 16 + (a->session.summary.is_night ? 8 : 0) + (int)(a->session.visual_time * 2) % 8;
}
static Rectangle building_rect(const GameApp *a, const CoreBuildingView *b, Texture2D t) {
    int frame = observation_frame(a, b);
    if (frame >= 0) {
        Rectangle source = a->assets.observatory_sources[(frame / 16) * 16 + (frame % 16)];
        return box_rect(game_expansion_building_sprite(b, (int)source.width, (int)source.height));
    }
    if (farm_row(b->type) >= 0)
        return (Rectangle){b->x * 16.0f, b->y * 16.0f, b->w * 16.0f, b->h * 16.0f};
    if (b->type == CORE_BLD_GREENHOUSE) {
        Rectangle source = a->assets.greenhouse_sources[a->session.summary.season * 4 + b->rotation];
        return box_rect(game_expansion_building_sprite(b, (int)source.width, (int)source.height));
    }
    return box_rect(game_expansion_building_sprite(b, farm_row(b->type) >= 0 ? 256 : t.width,
                                                   farm_row(b->type) >= 0 ? 256 : t.height));
}
static Texture2D building_texture(const GameApp *a, const CoreBuildingView *b) {
    if (observation_frame(a, b) >= 0)
        return a->assets.observatory_motion;
    if (b->kind_flags & CORE_EXP_GATE)
        return a->assets.gates;
    if (b->type == CORE_BLD_GREENHOUSE)
        return a->assets.greenhouse_turns;
    if (farm_row(b->type) >= 0)
        return a->assets.farms;
    if (b->type == CORE_BLD_KEEP)
        return a->assets.manors[a->session.summary.rank];
    return b->kind_flags & CORE_EXP_WALL ? a->assets.wall_links : a->assets.turns[b->type][b->rotation];
}
static Rectangle building_source(const GameApp *a, const CoreBuildingView *b, Texture2D t) {
    int frame = observation_frame(a, b);
    if (frame >= 0)
        return a->assets.observatory_sources[frame];
    if (b->type == CORE_BLD_GREENHOUSE)
        return a->assets.greenhouse_sources[a->session.summary.season * 4 + b->rotation];
    if (b->kind_flags & CORE_EXP_GATE) {
        int row = b->type == CORE_BLD_PALISADE_GATE ? 0
                  : b->type == CORE_BLD_STONE_GATE  ? 1
                  : b->type == CORE_BLD_IRON_GATE   ? 3
                                                    : 2;
        return (Rectangle){b->rotation * 128.0f, row * 128.0f, 128, 128};
    }
    int row = farm_row(b->type);
    if (row >= 0) {
        int atlas_row = row * 4 + a->session.summary.season;
        if (b->type == CORE_BLD_ORCHARD && a->session.summary.day - b->completed_day < 24)
            atlas_row = 20;
        return a->assets.farm_sources[atlas_row * 4 + b->rotation];
    }
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
static int building_attachment(const GameApp *a, const CoreBuildingView *b, Rectangle base,
                               Texture2D *texture, Rectangle *source, Rectangle *dest) {
    if (b->state != CORE_BSTATE_ACTIVE ||
        (b->type != CORE_BLD_WINDMILL && b->type != CORE_BLD_IRRIGATED_FARM))
        return 0;
    int running = b->operating && (b->type == CORE_BLD_WINDMILL || b->working_now > 0);
    float period = b->type == CORE_BLD_WINDMILL ? .375f : .3f;
    int frame = running ? (int)(a->session.visual_time / period * 8) % 8 : 0;
    *texture = b->type == CORE_BLD_WINDMILL ? a->assets.windmill_blades[b->rotation]
                                            : a->assets.irrigated_wheel[b->rotation];
    GameSpriteBox attach = game_building_attachment(b->type, b->rotation);
    *source =
        (Rectangle){frame * (texture->height * 1.0f), 0, (float)texture->height, (float)texture->height};
    *dest = (Rectangle){base.x + base.width * attach.x, base.y + base.height * attach.y,
                        base.width * attach.w, base.height * attach.h};
    return 1;
}
int game_hover_building(GameApp *a, Vector2 screen) {
    Vector2 p = GetScreenToWorld2D(screen, game_gpu_screen_camera(a));
    int found = 0;
    float depth = -1;
    for (int i = 0; i < a->session.building_count; i++) {
        CoreBuildingView *b = &a->session.buildings[i];
        int cell = b->y * a->session.summary.map_width + b->x;
        if (cell < 0 || cell >= a->session.cells || !a->session.visibility[cell])
            continue;
        Texture2D texture = building_texture(a, b);
        Rectangle source = building_source(a, b, texture), dest = building_rect(a, b, texture);
        if (b->state == CORE_BSTATE_CONSTRUCTING) {
            texture = a->assets.construction;
            source = (Rectangle){0, 0, (float)texture.width, (float)texture.height};
            dest = (Rectangle){b->x * 16.0f, b->y * 16.0f, b->w * 16.0f, b->h * 16.0f};
        }
        int hit = dest.width > 0 && dest.height > 0 && CheckCollisionPointRec(p, dest) &&
                  game_texture_hit(&a->assets, texture, source, (p.x - dest.x) / dest.width,
                                   (p.y - dest.y) / dest.height);
        Texture2D attachment;
        Rectangle attachment_source, attachment_dest;
        if (!hit && building_attachment(a, b, dest, &attachment, &attachment_source, &attachment_dest) &&
            CheckCollisionPointRec(p, attachment_dest))
            hit = game_texture_hit(&a->assets, attachment, attachment_source,
                                   (p.x - attachment_dest.x) / attachment_dest.width,
                                   (p.y - attachment_dest.y) / attachment_dest.height);
        if (hit) {
            float d = (b->y + b->h) * 16.0f;
            if (d >= depth) {
                depth = d;
                found = b->id;
            }
        }
    }
    return found;
}
Texture2D game_building_preview(const GameApp *a, int type, Rectangle *source) {
    CoreExpansionBuildingInfo info;
    if (core_expansion_building_info(type, &info) != CORE_OK) {
        *source = (Rectangle){0};
        return (Texture2D){0};
    }
    CoreBuildingView b = {0};
    b.type = type;
    b.w = info.w;
    b.h = info.h;
    b.kind_flags = info.flags;
    b.state = CORE_BSTATE_ACTIVE;
    b.completed_day = a->session.summary.day - 24;
    if (info.flags & CORE_EXP_ROAD) {
        if (type == CORE_BLD_BRIDGE) {
            *source = (Rectangle){0, 0, 128, 128};
            return a->assets.bridge_parts;
        }
        *source = (Rectangle){0,
                              type == CORE_BLD_PAVED_ROAD   ? 2048.0f
                              : type == CORE_BLD_STONE_ROAD ? 1024.0f
                                                            : 0,
                              64, 64};
        return a->assets.road_tiles;
    }
    if (info.flags & CORE_EXP_WALL) {
        int style = type == CORE_BLD_PALISADE    ? 0
                    : type == CORE_BLD_BARRIER   ? 1
                    : type == CORE_BLD_IRON_WALL ? 3
                                                 : 2;
        *source = (Rectangle){10 * 128.0f, style * 128.0f, 128, 128};
        return a->assets.wall_links;
    }
    if (type == CORE_BLD_WINDMILL) {
        Texture2D t = a->assets.windmill_previews[0];
        *source = (Rectangle){0, 0, (float)t.width, (float)t.height};
        return t;
    }
    if (type == CORE_BLD_IRRIGATED_FARM) {
        *source = (Rectangle){0, a->session.summary.season * 256.0f, 256, 256};
        return a->assets.irrigated_previews;
    }
    Texture2D texture = building_texture(a, &b);
    *source = building_source(a, &b, texture);
    return texture;
}
static void building(GameApp *a, int index) {
    CoreBuildingView *b = &a->session.buildings[index];
    Texture2D t = building_texture(a, b);
    Rectangle dest = building_rect(a, b, t);
    Color tint = WHITE;
    if (farm_row(b->type) >= 0 && a->session.summary.nature_event == CORE_NATURE_DROUGHT)
        tint = b->type == CORE_BLD_GREENHOUSE ? (Color){240, 225, 190, 255} : (Color){220, 185, 125, 255};
    float bar_x = b->x * 16.0f, bar_w = b->w * 16.0f;
    if (b->state == CORE_BSTATE_RUINED)
        tint = (Color){105, 95, 103, 255};
    if (b->state == CORE_BSTATE_CONSTRUCTING)
        tint = (Color){185, 168, 126, 170};
    /* 빈 집·멈춘 일터의 창과 화덕은 원화에 그려져 있어도 밤에 빛나지 않습니다. */
    game_gpu_emission(game_scene_lit(b) ? 1.0f : 0.0f);
    sprite(t, building_source(a, b, t), dest, tint);
    Texture2D attachment;
    Rectangle attachment_source, attachment_dest;
    if (building_attachment(a, b, dest, &attachment, &attachment_source, &attachment_dest))
        sprite(attachment, attachment_source, attachment_dest, WHITE);
    GameSceneProfile illumination;
    if (game_scene_profile(b, dest, a->session.visual_time, &illumination))
        for (int light = 0; light < illumination.light_count; light++) {
            GameSceneLight *source = &illumination.lights[light];
            int fire = source->kind == GAME_SCENE_FURNACE;
            if (!fire && source->kind != GAME_SCENE_TORCH)
                continue;
            float night = game_carried_strength(game_gpu_visual_minute(a));
            float strength = source->strength * (fire ? 1.0f : night);
            uint32_t owner = 0x10000000u | (uint32_t)b->id;
            float size = fmaxf(6, fminf(fire ? 18.0f : 10.0f, dest.width * (fire ? .30f : .12f)));
            game_carried_flame_image(a, owner, source->screen, source->position.z, size, strength,
                                     fire ? GAME_CARRIED_FIRE : GAME_CARRIED_TORCH,
                                     (b->y + b->h) * 16.0f);
        }
    if (b->type == CORE_BLD_LIGHTHOUSE && b->state == CORE_BSTATE_ACTIVE && b->operating &&
        a->session.summary.is_night) {
        int frame = (int)(a->session.visual_time * 8) % 8;
        Rectangle src = {frame * 128.0f, 0, 128, 128};
        sprite(a->assets.lighthouse_glow[b->rotation], src,
               (Rectangle){dest.x - dest.width * .5f, dest.y + dest.height * .6f, dest.width * 2,
                           dest.height * .6f},
               Fade(WHITE, .35f));
        sprite(a->assets.lighthouse_lamp[b->rotation], src,
               (Rectangle){dest.x - dest.width * .5f, dest.y - dest.height * .15f, dest.width * 2,
                           dest.height * .8f},
               Fade(WHITE, .45f));
    }
    if (farm_row(b->type) >= 0 && b->type != CORE_BLD_GARDEN &&
        a->session.summary.season == CORE_SEASON_AUTUMN) {
        CoreHarvestView h;
        if (core_expansion_harvest(a->session.world, b->id, &h) == CORE_OK &&
            h.year == a->session.summary.year && (h.release_day - 1) % 48 >= 24 &&
            (h.release_day - 1) % 48 <= 29 && h.total_milli > 0) {
            int frame = (h.release_day - 1) % 48 - 24;
            sprite(a->assets.harvest_phases, (Rectangle){frame * 128.0f, 0, 128, 128},
                   (Rectangle){dest.x + dest.width * .35f, dest.y + dest.height * .3f, dest.width * .4f,
                               dest.height * .4f},
                   WHITE);
        }
    }
    if (b->type == CORE_BLD_CLAY_PIT && b->working_now && b->state == CORE_BSTATE_ACTIVE) {
        int frame = (int)(a->session.visual_time * 8) % 4;
        sprite(a->assets.ore_soil, (Rectangle){frame * 128.0f, 256, 128, 128},
               (Rectangle){dest.x + dest.width * .25f, dest.y + dest.height * .5f, 20, 16}, WHITE);
    }
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
    int carry_pose = game_walk_frame(unit->walk_distance, moving || unit->chasing);
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
    Texture2D clothes = a->assets.villagers;
    if (unit->outfit > 0 && unit->outfit <= 4) {
        int dir = unit->facing == FACE_LEFT ? 1 : unit->facing;
        if (dir < 0 || dir > 2)
            dir = 0;
        int attacking = unit->attack_serial && unit->attack_elapsed < 4.8f;
        int pose = attacking ? (int)(unit->attack_elapsed / 1.2f)
                             : game_walk_frame(unit->walk_distance, moving || unit->chasing);
        if (pose > 3)
            pose = 3;
        carry_pose = pose;
        frame.column = dir * 8 + attacking * 4 + pose;
        clothes = a->assets.uniforms[unit->outfit - 1];
    }
    uint32_t carry_owner = 0x30000000u | ((uint32_t)DRAW_VILLAGER << 24) | (uint32_t)v->id;
    int carry_torch = unit->outfit >= 3;
    game_carried_light(a, carry_owner, (uint32_t)v->id, carry_torch, unit->facing, carry_pose, dest,
                       unit->position.y * 16, unit->indoor, 1);
    sprite(clothes, (Rectangle){frame.column * 96.0f, frame.row * 96.0f, frame.flip ? -96.0f : 96.0f, 96},
           dest, Fade(effect_tint(a, 0, v->id), 1 - unit->indoor));
    game_carried_light(a, carry_owner, (uint32_t)v->id, carry_torch, unit->facing, carry_pose, dest,
                       unit->position.y * 16, unit->indoor, 0);
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
    int dir = e->actor.facing == FACE_LEFT ? 1 : e->actor.facing;
    if (dir < 0 || dir > 2)
        dir = 0;
    int attacking = e->attack_serial && e->attack_elapsed < 4.8f;
    int frame = attacking ? (int)(e->attack_elapsed / 1.2f)
                          : game_walk_frame(e->actor.walk_distance, e->actor.next < e->actor.path.count);
    if (frame > 3)
        frame = 3;
    float width = e->info.width_milli * 16 / 1000.0f, height = e->info.height_milli * 16 / 1000.0f;
    if (width <= 0 || height <= 0) {
        width = 24;
        height = 24;
    }
    Rectangle dest = {p.x - width / 2, p.y - height, width, height};
    sprite(attacking ? a->assets.raider_attack : a->assets.enemies,
           (Rectangle){((e->def.type % 4) * 4 + frame) * 96.0f, ((e->def.type / 4) * 3 + dir) * 96.0f,
                       e->actor.facing == FACE_LEFT ? -96.0f : 96.0f, 96},
           dest, effect_tint(a, 2, e->actor.id));
    life_bar(a, 2, e->actor.id, e->hp, e->def.hp, dest);
}
static int prop_type(const GameApp *a, int x, int y) {
    const GameSession *s = &a->session;
    int i = y * s->summary.map_width + x, type = s->terrain[i], variant = (int)(tile_noise(x, y) % 12);
    if (s->visibility && !s->visibility[i])
        return -1;
    if (s->roads[i] || a->decor_blocked[i])
        return -1;
    if (s->exp_tiles && type != CORE_TERRAIN_WATER &&
        game_rock_marker(s->exp_tiles, s->roads, a->decor_blocked, s->summary.map_width,
                         s->summary.map_height, x, y))
        return 21;
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
        return s->exp_tiles       ? (variant % 4 == 0 ? 4 + variant % 2 : -1)
               : variant % 4 == 0 ? -1
                                  : 4 + variant % 2;
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
    if (type == 21) {
        CoreExpansionTileView *t = &s->exp_tiles[cell_index];
        int row = 0;
        if (t->ore_milli > 0)
            row = t->ore_kind == CORE_RES_COAL       ? 1
                  : t->ore_kind == CORE_RES_IRON_ORE ? 2
                  : t->ore_kind == CORE_RES_GOLD_ORE ? 3
                  : t->ore_kind == CORE_RES_MANA     ? 4
                                                     : 0;
        size = 20.0f + (float)(tile_noise(x, y) % 4);
        Texture2D texture = row >= 3 ? a->assets.ore_soil : a->assets.rocks;
        Rectangle source = row >= 3
                               ? (Rectangle){(tile_noise(x, y) % 4) * 128.0f, (row - 3) * 128.0f, 128, 128}
                               : (Rectangle){(t->rock_variant - 1) * 128.0f, row * 128.0f, 128, 128};
        sprite(texture, source, (Rectangle){cx - size / 2, feet - size, size, size}, WHITE);
        return;
    }
    if (type == 20) {
        CoreExpansionTileView *t = &s->exp_tiles[cell_index];
        int mature = core_expansion_tree_mature_age(t->tree_kind);
        int stage = t->tree_age < mature * 3 / 10             ? 0
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
    int needed = s->cells + s->building_count + s->villager_count + s->enemy_count + s->animal_count + 160 +
                 s->boat_count + s->cells + 1;
    Vector2 top = GetScreenToWorld2D((Vector2){0, 0}, game_gpu_screen_camera(a)),
            bottom = GetScreenToWorld2D((Vector2){(float)GetScreenWidth(), (float)GetScreenHeight()},
                                        game_gpu_screen_camera(a));
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
        game_gpu_building(a, b, building_rect(a, b, building_texture(a, b)));
        for (y = b->y; y < b->y + b->h; y++)
            for (x = b->x; x < b->x + b->w; x++)
                if (x >= 0 && x < w && y >= 0 && y < h)
                    a->decor_blocked[y * w + x] = 1;
    }
    if (game_gpu_active())
        for (y = 0; y < h; y++)
            for (x = 0; x < w; x++) {
                int type = prop_type(a, x, y);
                if (type >= 0)
                    game_gpu_prop(a, y * w + x, type);
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
        if (!game_gpu_active())
            DrawEllipse((int)((b->x + b->w * 0.5f) * 16), (b->y + b->h) * 16 - 2, b->w * 8.0f, 3,
                        (Color){24, 21, 16, 65});
    }
    for (i = 0; i < s->villager_count; i++) {
        CoreVillagerView *v = &s->villagers[i];
        GameActor *unit = game_actor(s, v->id);
        Vector2 p;
        CoreResidentStatus status = {0};
        core_expansion_resident_status(s->world, v->id, &status);
        if (status.boat_port_id)
            continue;
        if (!unit || unit->indoor >= 1)
            continue;
        p = pixel(unit->position);
        if (unit->position.x < x0 || unit->position.x > x1 || unit->position.y < y0 || unit->position.y > y1)
            continue;
        a->draw_items[count++] = (GameDrawItem){p.y, DRAW_VILLAGER, i, v->id};
        if (!game_gpu_active())
            DrawEllipse((int)p.x, (int)p.y, 4, 1.5f, (Color){0, 0, 0, 75});
        if (v->id == a->selected_villager)
            DrawEllipseLines((int)p.x, (int)p.y, 7, 3, UI_GOLD);
    }
    for (i = 0; i < s->enemy_count; i++)
        if (s->enemies[i].hp > 0 && !s->enemies[i].pending_spawn) {
            Vector2 p = pixel(s->enemies[i].actor.position);
            if (s->visibility && s->visibility[(int)(p.y / 16) * w + (int)(p.x / 16)] < 2)
                continue;
            a->draw_items[count++] = (GameDrawItem){p.y, DRAW_ENEMY, i, s->enemies[i].actor.id};
            if (!game_gpu_active())
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
    for (i = 0; i < s->boat_count; i++) {
        CoreBoatView *boat = &s->boats[i];
        int cell = boat->y_milli / 1000 * w + boat->x_milli / 1000;
        if (cell >= 0 && cell < s->cells && (!s->visibility || s->visibility[cell] >= 2))
            a->draw_items[count++] = (GameDrawItem){boat->y_milli * .016f, DRAW_BOAT, i, i};
    }
    if (s->wandering.state != CORE_MERCHANT_DORMANT) {
        int cell = s->wandering.y_milli / 1000 * w + s->wandering.x_milli / 1000;
        if (cell >= 0 && cell < s->cells && (!s->visibility || s->visibility[cell] >= 2))
            a->draw_items[count++] = (GameDrawItem){s->wandering.y_milli * .016f, DRAW_MERCHANT, 0, 0};
    }
    for (y = y0; y < y1; y++)
        for (x = x0; x < x1; x++)
            if (s->roads[y * w + x] == 3 && (!s->visibility || s->visibility[y * w + x]))
                a->draw_items[count++] =
                    (GameDrawItem){(y + 1) * 16.0f + .5f, DRAW_BRIDGE_RAIL, y * w + x, y * w + x};
    for (i = 0; i < 160; i++) {
        GameHitFx *fx = &s->hit_fx[i];
        float elapsed = s->visual_time - fx->started;
        if (fx->active && elapsed >= 0 && elapsed < .5f)
            a->draw_items[count++] = (GameDrawItem){fx->position.y * 16 + .1f, DRAW_HIT_FX, i, i};
    }
    qsort(a->draw_items, (size_t)count, sizeof(*a->draw_items), game_draw_compare);
    return count;
}
static void merchant_sprite(GameApp *a) {
    GameSession *s = &a->session;
    CoreWanderingMerchantView *m = &s->wandering, *old = &s->wandering_previous;
    float fraction = fminf(1, s->tick_minutes / 10), x = m->x_milli / 1000.0f, y = m->y_milli / 1000.0f;
    int moving = m->state == CORE_MERCHANT_ENTERING || m->state == CORE_MERCHANT_LEAVING,
        dx = m->x_milli - old->x_milli, dy = m->y_milli - old->y_milli;
    if (old->visit_id == m->visit_id && old->state != CORE_MERCHANT_DORMANT) {
        x = (old->x_milli + (m->x_milli - old->x_milli) * fraction) / 1000.0f;
        y = (old->y_milli + (m->y_milli - old->y_milli) * fraction) / 1000.0f;
    }
    int row = abs(dx) > abs(dy) ? (dx > 0 ? 3 : 1)
              : dy < 0          ? 2
                                : 0,
        frame = moving ? (int)(s->visual_time * 7) % 4 : 4 + (int)(s->visual_time * 2) % 2;
    Rectangle actor = {x * 16 - 12, y * 16 - 24, 24, 24};
    int facing = (int[]){FACE_DOWN, FACE_LEFT, FACE_UP, FACE_RIGHT}[row];
    uint32_t owner = 0x30000000u | ((uint32_t)DRAW_MERCHANT << 24);
    uint32_t identity = 0x70000000u | (uint32_t)m->visit_id;
    game_carried_light(a, owner, identity, 0, facing, frame, actor, y * 16, 0, 1);
    sprite(a->assets.merchant, (Rectangle){frame * 96.0f, row * 96.0f, 96, 96}, actor, WHITE);
    game_carried_light(a, owner, identity, 0, facing, frame, actor, y * 16, 0, 0);
}
static void boat_sprite(GameApp *a, int index) {
    GameSession *s = &a->session;
    CoreBoatView *b = &s->boats[index];
    float f = fminf(1, s->tick_minutes / 10),
          x = (b->previous_x_milli + (b->x_milli - b->previous_x_milli) * f) * .016f,
          y = (b->previous_y_milli + (b->y_milli - b->previous_y_milli) * f) * .016f;
    int row = (int[]){2, 1, 0, 3}[b->facing], kind = b->boat_kind != 0,
        moving = b->x_milli != b->previous_x_milli || b->y_milli != b->previous_y_milli;
    int frame = moving ? (int)(s->visual_time * 6) % 4 : 0;
    float size = kind ? 42.0f : 34.0f;
    Rectangle dest = {x - size * .5f, y - size * .68f, size, size};
    if (moving && !b->fishing_phase) {
        static const int dx[4] = {0, 1, 0, -1}, dy[4] = {-1, 0, 1, 0};
        sprite(a->assets.boat_wake[row], (Rectangle){frame * 96.0f, 0, 96, 96},
               (Rectangle){x - size * .5f - dx[b->facing] * size * .45f,
                           y - size * .5f - dy[b->facing] * size * .45f, size, size},
               Fade(WHITE, .55f));
    }
    Texture2D hull = a->assets.boats[kind][row];
    Rectangle source = {frame * 128.0f, 0, 128, 128};
    sprite(hull, source, dest, WHITE);
    for (int i = 0; i < b->crew_count; i++) {
        CoreVillagerView *v = game_villager(s, b->crew[i]);
        if (!v)
            continue;
        int facing = (int[]){FACE_UP, FACE_RIGHT, FACE_DOWN, FACE_LEFT}[b->facing];
        GameSpriteFrame pose = game_villager_frame(v->appearance + 1, facing, 0, 0);
        GameSpriteBox seat = game_boat_seat(kind, row, i);
        float sx = dest.x + dest.width * seat.x, sy = dest.y + dest.height * seat.y;
        Rectangle carried_actor = {sx - 7, sy - 13, 14, 19};
        /* 배에는 별도 번호가 없으므로 항구와 정박 칸으로 안정된 소유 번호를 만듭니다. */
        uint32_t boat_identity = ((uint32_t)b->port_id << 3) | ((uint32_t)b->slot & 7u);
        uint32_t owner = 0x30000000u | ((uint32_t)DRAW_BOAT << 24) | (boat_identity & 0xffffffu);
        game_carried_light(a, owner, (uint32_t)v->id, 0, facing, 1, carried_actor, y, 0, 1);
        sprite(a->assets.villagers,
               (Rectangle){pose.column * 96.0f, pose.row * 96.0f, pose.flip ? -96.0f : 96.0f, 61},
               (Rectangle){sx - 7, sy - 13, 14, 12}, WHITE);
        game_carried_light(a, owner, (uint32_t)v->id, 0, facing, 1, carried_actor, y, 0, 0);
    }
    /* 선수 가장자리를 다시 그려 주민의 하체를 선체 안에 가립니다. */
    sprite(hull, (Rectangle){source.x, 88, 128, 40},
           (Rectangle){dest.x, dest.y + dest.height * 88 / 128, dest.width, dest.height * 40 / 128}, WHITE);
    if (b->fishing_phase) {
        float elapsed = fminf(2799, b->phase_milli + s->tick_minutes / 12 * 1000);
        int netframe = (int)(elapsed / 350);
        if (netframe > 7)
            netframe = 7;
        static const int dx[4] = {0, 1, 0, -1}, dy[4] = {-1, 0, 1, 0};
        float ns = kind ? 38.0f : 31.0f;
        sprite(a->assets.nets[kind][row], (Rectangle){netframe * 128.0f, 0, 128, 128},
               (Rectangle){x - dx[b->facing] * size * .65f - ns * .5f,
                           y - dy[b->facing] * size * .65f - ns * .5f, ns, ns},
               Fade(WHITE, .85f));
    }
}
static void hit_effect(GameApp *a, int index);
static void objects(GameApp *a) {
    int i, count = draw_queue(a);
    for (i = 0; i < count; i++) {
        GameDrawItem *item = &a->draw_items[i];
        int material = item->kind == DRAW_BUILDING ? GPU_MATERIAL_BUILDING
                       : item->kind == DRAW_PROP ? (prop_type(a, item->index % a->session.summary.map_width,
                                                              item->index / a->session.summary.map_width) < 4
                                                        ? GPU_MATERIAL_TREE
                                                        : GPU_MATERIAL_ROCK)
                       : item->kind == DRAW_BRIDGE_RAIL ? GPU_MATERIAL_ROAD
                       : item->kind == DRAW_HIT_FX      ? GPU_MATERIAL_UNLIT
                                                        : GPU_MATERIAL_ACTOR;
        uint32_t owner = (item->kind == DRAW_BUILDING ? 0x10000000u
                          : item->kind == DRAW_PROP   ? 0x20000000u
                                                      : 0x30000000u | ((uint32_t)item->kind << 24)) |
                         (uint32_t)(item->key + 1);
        if (item->kind == DRAW_BUILDING)
            owner = 0x10000000u | (uint32_t)item->key;
        game_gpu_object(material, owner, item->depth);
        if (item->kind == DRAW_VILLAGER) {
            GameActor *actor = game_actor(&a->session, a->session.villagers[item->index].id);
            if (actor && actor->indoor < .5f)
                game_gpu_actor(owner, actor->position.x * 16, item->depth, GAME_ACTOR_HEIGHT,
                               GAME_ACTOR_HEIGHT);
        } else if (item->kind == DRAW_ENEMY) {
            GameEnemy *enemy = &a->session.enemies[item->index];
            game_gpu_actor(owner, enemy->actor.position.x * 16, item->depth, 24, 24);
        } else if (item->kind == DRAW_ANIMAL) {
            CoreAnimalInfo info;
            core_expansion_animal_info(a->session.animals[item->index].kind, &info);
            game_gpu_actor(owner, a->session.animal_actors[item->index].position.x * 16, item->depth,
                           info.width_milli * .016f, info.height_milli * .016f);
        } else if (item->kind == DRAW_MERCHANT) {
            game_gpu_actor(owner, a->session.wandering.x_milli * .016f, item->depth, 24, 24);
        } else if (item->kind == DRAW_BOAT) {
            CoreBoatView *boat = &a->session.boats[item->index];
            game_gpu_actor(owner, boat->x_milli * .016f, item->depth, boat->boat_kind ? 42.0f : 34.0f, 12.0f);
        }
        switch (item->kind) {
        case DRAW_MERCHANT:
            merchant_sprite(a);
            break;
        case DRAW_BOAT:
            boat_sprite(a, item->index);
            break;
        case DRAW_BRIDGE_RAIL: {
            int x = item->index % a->session.summary.map_width,
                y = item->index / a->session.summary.map_width,
                mask = core_expansion_bridge_mask(a->session.world, x, y);
            for (int d = 1; d <= 2; d++)
                if (mask & (1 << d))
                    sprite(a->assets.bridge_parts, (Rectangle){d * 128.0f, 384, 128, 128},
                           (Rectangle){x * 16.0f, y * 16.0f - 5, 16, 21}, WHITE);
            break;
        }
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
        case DRAW_HIT_FX:
            hit_effect(a, item->index);
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
            Texture2D texture = core_expansion_animal_class(animal->kind) != 1 &&
                                        a->session.terrain[cell] == CORE_TERRAIN_WATER &&
                                        !a->session.roads[cell] && info.swim_milli
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
    game_gpu_object(GPU_MATERIAL_UNLIT, 0, 0);
    for (i = 0; i < a->session.projectile_count; i++) {
        GameProjectile *p = &a->session.projectiles[i];
        float t = fminf(1, p->elapsed / p->duration);
        Vector2 from = pixel(p->from), to = pixel(p->to),
                now = {from.x + (to.x - from.x) * t, from.y + (to.y - from.y) * t - 8};
        Texture2D arrow = a->assets.arrow;
        float height = arrow.width ? 8.0f * arrow.height / arrow.width : 2;
        Color shot = p->source_type == 6    ? (Color){65, 220, 185, 255}
                     : p->source_type == 12 ? SKYBLUE
                     : p->source_type == 13 ? PURPLE
                     : p->source_type == 16 ? GREEN
                     : p->source_type == 21 ? SKYBLUE
                     : p->source_type == 25 ? ORANGE
                     : p->source_type == 27 ? GOLD
                                            : WHITE;
        Vector2 trail = {now.x - (to.x - from.x) * .04f, now.y - (to.y - from.y) * .04f};
        DrawLineEx(trail, now, 1, Fade(shot, .65f));
        int type = p->source_type;
        if (type == 6 || type == 12 || type == 13 || type == 16 || type == 21 || type == 25 || type == 27 ||
            type == 100 + CORE_BLD_ARCANE_TOWER || type == 100 + CORE_BLD_CANNON_TOWER) {
            if (type == 100 + CORE_BLD_ARCANE_TOWER)
                shot = SKYBLUE;
            if (type == 100 + CORE_BLD_CANNON_TOWER)
                shot = DARKGRAY;
            DrawCircleV(now, 2.8f, Fade(shot, .45f));
            if (type == 21)
                DrawPoly(now, 4, 2.3f, 45 + a->session.visual_time * 100, shot);
            else if (type == 16)
                DrawTriangle((Vector2){now.x - 3, now.y}, (Vector2){now.x + 2, now.y - 1.8f},
                             (Vector2){now.x + 1, now.y + 2}, shot);
            else
                DrawCircleV(now, type == 12 ? 1.6f : 2, shot);
            DrawCircleV(now, .7f, WHITE);
            if (type == 27 && t > .8f)
                DrawCircleLines((int)to.x, (int)to.y, 16 * t, Fade(GOLD, (t - .8f) * 3));
        } else {
            float rotation = atan2f(to.y - from.y, to.x - from.x) * RAD2DEG;
            for (int k = 0; k < 4; k++)
                DrawTexturePro(arrow, (Rectangle){0, 0, (float)arrow.width, (float)arrow.height},
                               (Rectangle){now.x + (k == 0   ? .35f
                                                    : k == 1 ? -.35f
                                                             : 0),
                                           now.y + (k == 2   ? .35f
                                                    : k == 3 ? -.35f
                                                             : 0),
                                           8, height},
                               (Vector2){4, height / 2}, rotation, (Color){25, 20, 27, 230});
            DrawTexturePro(arrow, (Rectangle){0, 0, (float)arrow.width, (float)arrow.height},
                           (Rectangle){now.x, now.y, 8, height}, (Vector2){4, height / 2}, rotation,
                           type == 10 ? (Color){190, 230, 220, 255} : shot);
        }
    }
}
static void hit_effect(GameApp *a, int index) {
    GameSession *s = &a->session;
    GameHitFx *fx = &s->hit_fx[index];
    float elapsed = s->visual_time - fx->started;
    if (!fx->active || elapsed < 0 || elapsed >= .5f)
        return;
    int x = (int)fx->position.x, y = (int)fx->position.y;
    if (x < 0 || y < 0 || x >= s->summary.map_width || y >= s->summary.map_height ||
        (s->visibility && s->visibility[y * s->summary.map_width + x] < 2))
        return;
    int frame = (int)(elapsed * 12), style = fx->style;
    if (frame > 5)
        frame = 5;
    if (style < 0 || style > 7)
        style = 0;
    float size = style == 7 ? 36.0f : style == 0 || style == 3 ? 25.0f : 22.0f;
    Rectangle source = {frame * 128.0f, style * 128.0f, 128, 128};
    DrawTexturePro(a->assets.combat_fx, source,
                   (Rectangle){fx->position.x * 16, fx->position.y * 16 - 7, size, size},
                   (Vector2){size / 2, size / 2}, fx->angle, WHITE);
}
static void livestock_proxies(GameApp *a) {
    GameSession *s = &a->session;
    static const int kinds[7] = {7, 9, 8, 10, 32, 33, 34};
    for (int b = 0; b < s->building_count; b++) {
        CoreBuildingView *farm = &s->buildings[b];
        if ((farm->type != CORE_BLD_RANCH && farm->type != CORE_BLD_PASTURE) ||
            farm->state != CORE_BSTATE_ACTIVE)
            continue;
        if (s->visibility && s->visibility[farm->y * s->summary.map_width + farm->x] < 2)
            continue;
        for (int k = 0; k < 7; k++) {
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
                CoreLivestockInfo live = {0};
                core_expansion_livestock_info(kinds[k], farm->type, &live);
                int age = s->animals[representative].age, young = age < live.mature_days;
                if (young) {
                    dest.width *= .65f;
                    dest.height *= .65f;
                    dest.y = y - dest.height;
                    dest.x = x - dest.width / 2;
                }
                uint32_t owner = 0x40000000u | (uint32_t)(id * 2 + n);
                game_gpu_object(GPU_MATERIAL_ACTOR, owner, y);
                game_gpu_actor(owner, x, y, dest.width, dest.height);
                sprite(young ? a->assets.animal_babies : a->assets.animal_walk,
                       (Rectangle){frame * 96.0f, kinds[k] * 96.0f, cosf(phase) > 0 ? 96.0f : -96.0f, 96},
                       dest, effect_tint(a, 1, id));
                life_bar(a, 1, id, s->animals[representative].hp, info.hp, dest);
            }
        }
    }
    game_gpu_object(GPU_MATERIAL_UNLIT, 0, 0);
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
            source = (Rectangle){((fx->skin % 4) * 4 + 1) * 96.0f, (fx->skin / 4) * 3 * 96.0f, 96, 96};
        }
        float angle = fmaxf(0, fminf(1, elapsed / .55f)) * 60 * ((fx->id * 73) & 1 ? 1 : -1);
        Color tint =
            elapsed < 0 ? WHITE
                        : Fade((Color){255, 110, 110, 255}, elapsed < .55f ? 1 : 1 - (elapsed - .55f) / .45f);
        Rectangle destination = {fx->position.x * 16 - width / 2, fx->position.y * 16 - height, width,
                                 height};
        if (!game_gpu_floor_sprite(texture, source, destination, tint, angle))
            DrawTexturePro(texture, source,
                           (Rectangle){fx->position.x * 16, fx->position.y * 16, width, height},
                           (Vector2){width / 2, height}, angle, tint);
    }
}
void game_render_free(GameApp *a) {
    game_gpu_report();
    game_gpu_shutdown();
    free(a->draw_items);
    a->draw_items = NULL;
    a->draw_capacity = 0;
    free(a->decor_blocked);
    a->decor_blocked = NULL;
    a->decor_capacity = 0;
}
void game_render_villager_preview(GameApp *a) {
    static const char *const hairs[6] = {u8"갈색", u8"금발", u8"흑발", u8"적발", u8"은발", u8"남색"};
    static const char *const directions[4] = {u8"정면", u8"오른쪽", u8"후면", u8"왼쪽"};
    int direction = ((int)GetTime() / 3) % 4;
    float gap = (GetScreenWidth() - 48.0f) / 6, row_height = (GetScreenHeight() - 130.0f) / 8;
    float height = fminf(72, row_height - 24);
    ClearBackground((Color){23, 32, 33, 255});
    ui_text(&a->ui, 24, 18, UI_GOLD, u8"주민 외형48종 · 방향별 걷기");
    ui_small(&a->ui, 24, 54, UI_TEXT, u8"기존12종 + 추가36종 · 밝은 금발 · 현재 %s", directions[direction]);
    for (int appearance = 0; appearance < 48; appearance++) {
        int hair = appearance < 12 ? appearance / 2 : (appearance - 12) / 6;
        int variant = appearance < 12 ? 1 : 2 + (appearance - 12) % 6 / 2;
        float cx = 24 + (appearance % 6 + .5f) * gap, feet = 112 + (appearance / 6) * row_height + height;
        GameSpriteFrame frame = game_villager_frame(appearance + 1, direction, (float)GetTime() * .5f, 1);
        sprite(a->assets.villagers,
               (Rectangle){frame.column * 96.0f, frame.row * 96.0f, frame.flip ? -96.0f : 96.0f, 96},
               (Rectangle){cx - height / 2, feet - height, height, height}, WHITE);
        ui_small(&a->ui, cx - gap * .43f, feet + 3, UI_TEXT, u8"%02d %s %d · %s", appearance, hairs[hair],
                 variant, appearance % 2 ? u8"여" : u8"남");
    }
}
static int world_mouse(GameApp *a, int *x, int *y) {
    Vector2 mouse = GetMousePosition(), world = GetScreenToWorld2D(mouse, game_gpu_screen_camera(a));
    *x = (int)floorf(world.x / 16);
    *y = (int)floorf(world.y / 16);
    /* 위치로 이동하는 알림을 누를 때 뒤쪽 건물을 동시에 조작하지 않습니다. */
    for (int i = 0; i < a->toast_area_count; i++)
        if (CheckCollisionPointRec(mouse, a->toast_areas[i]))
            return 0;
    if (game_tutorial_covers(a, mouse)) /* 튜토리얼 안내 창 위의 클릭 */
        return 0;
    return a->panel == PANEL_NONE && mouse.y > 135 && mouse.y < GetScreenHeight() - 253 &&
           mouse.x < GetScreenWidth() - 350;
}
static void overlay(GameApp *a) {
    int x, y;
    CorePlacementView preview;
    GameSession *s = &a->session;
    int mouse_on_map = world_mouse(a, &x, &y);
    CoreBuildingView *outline = game_building(s, a->selected_building);
    if (outline)
        DrawRectangleLinesEx(
            (Rectangle){outline->x * 16.0f, outline->y * 16.0f, outline->w * 16.0f, outline->h * 16.0f}, .7f,
            UI_GOLD);
    if (a->demolish && mouse_on_map) {
        Rectangle tile = {x * 16.0f, y * 16.0f, 16, 16};
        DrawRectangleRec(tile, (Color){230, 40, 46, 100});
        DrawRectangleLinesEx(tile, .7f, (Color){255, 75, 75, 255});
    }
    CoreBuildingView *selected = game_building(s, a->selected_building);
    int candidate = a->build_type >= 0 && mouse_on_map;
    if (candidate || selected) {
        CoreFacilityView facility;
        int type = candidate ? a->build_type : selected->type;
        int bx = candidate ? x : selected->x, by = candidate ? y : selected->y;
        CoreExpansionBuildingInfo info;
        core_expansion_building_info(type, &info);
        int rotation = candidate ? a->rotation : selected->rotation;
        int bw = (rotation & 1) ? info.h : info.w, bh = (rotation & 1) ? info.w : info.h;
        if (candidate) {
            bx -= (bw - 1) / 2;
            by -= (bh - 1) / 2;
            a->build_preview_x = bx;
            a->build_preview_y = by;
        }
        if (core_expansion_facility(s->world, candidate ? 0 : selected->id, type, bx, by, rotation,
                                    &facility) == CORE_OK) {
            int radius = facility.range > 0 ? facility.range : facility.radius;
            if (radius > 0) {
                Vector2 center = {(bx + bw * .5f) * 16, (by + bh * .5f) * 16};
                Color color = facility.range ? SKYBLUE : GREEN;
                DrawCircleLinesV(center, radius * 16.0f, Fade(color, .7f));
                for (int yy = by - radius; yy < by + bh + radius; yy++)
                    for (int xx = bx - radius; xx < bx + bw + radius; xx++) {
                        if (xx < 0 || yy < 0 || xx >= s->summary.map_width || yy >= s->summary.map_height)
                            continue;
                        int lx = bx + (bw - 1) / 2, rx = bx + bw / 2, ty = by + (bh - 1) / 2,
                            bot = by + bh / 2;
                        int distance = (xx < lx   ? lx - xx
                                        : xx > rx ? xx - rx
                                                  : 0) +
                                       (yy < ty    ? ty - yy
                                        : yy > bot ? yy - bot
                                                   : 0);
                        if (distance <= radius &&
                            (!s->visibility || s->visibility[yy * s->summary.map_width + xx] >= 2)) {
                            DrawRectangle(xx * 16, yy * 16, 16, 16, Fade(color, .10f));
                            DrawRectangleLines(xx * 16, yy * 16, 16, 16, Fade(color, .20f));
                        }
                    }
            }
        }
    }
    if (a->build_type < 0 || !mouse_on_map)
        return;
    {
        CoreBuildingDefView *d = &s->building_defs[a->build_type];
        int bw = (a->rotation & 1) ? d->h : d->w, bh = (a->rotation & 1) ? d->w : d->h;
        x -= (bw - 1) / 2;
        y -= (bh - 1) / 2;
        if (core_can_place_rotated(s->world, a->build_type, x, y, a->rotation, &preview) != CORE_OK)
            return;

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
            ghost = building_rect(a, &temporary, t);
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
    Vector2 p = GetWorldToScreen2D(anchor, game_gpu_screen_camera(a));
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
    if (a->review && !game_review_interface_visible())
        return;
    GameSession *s = &a->session;
    int i;
    for (i = 0; i < s->building_count; i++) {
        CoreBuildingView *b = &s->buildings[i];
        Rectangle image = building_rect(a, b, building_texture(a, b));
        Vector2 anchor = {image.x + image.width / 2, image.y - 3};
        if (b->state != CORE_BSTATE_ACTIVE ||
            (s->visibility && s->visibility[b->y * s->summary.map_width + b->x] < 2))
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
        int x = (int)floorf(actor->position.x), y = (int)floorf(actor->position.y);
        if (x < 0 || y < 0 || x >= s->summary.map_width || y >= s->summary.map_height ||
            (s->visibility && s->visibility[y * s->summary.map_width + x] < 2))
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
                Rectangle image = building_rect(a, b, building_texture(a, b));
                p = (Vector2){image.x + image.width / 2 + (i % 3 - 1) * 12, image.y - 17};
            }
        }
        bubble(a, p, icon, icon == 12 || icon == 14);
    }
}
static void fog(GameApp *a) {
    GameSession *s = &a->session;
    if (!s->visibility)
        return;
    Texture2D clouds = game_gpu_clouds(a);
    if (clouds.id) {
        Vector2 canvas = game_gpu_canvas_size();
        DrawTexturePro(clouds, (Rectangle){0, 0, (float)clouds.width, -(float)clouds.height},
                       (Rectangle){0, 0, canvas.x, canvas.y}, (Vector2){0, 0}, 0, WHITE);
        return;
    }
    /* 그래픽 오류 중에는 화면 전체를 덮어 숨은 정보를 드러내지 않습니다. */
    Vector2 canvas = game_gpu_canvas_size();
    if (canvas.x <= 0 || canvas.y <= 0)
        canvas = (Vector2){(float)GetScreenWidth(), (float)GetScreenHeight()};
    DrawRectangleRec((Rectangle){0, 0, canvas.x, canvas.y}, (Color){45, 57, 65, 255});
}

void game_render_world(GameApp *a) {
    float time = (float)GetTime();
    int i;
    if (!a->session.world)
        return;
    /* 제목 배경의 시야 사본만 밝힙니다. 코어의 탐험 상태와 실제 게임은 바꾸지 않습니다. */
    if (!a->in_game && a->session.visibility)
        memset(a->session.visibility, 2, (size_t)a->session.cells);
    int gpu = game_gpu_begin(a);
    game_gpu_object(GPU_MATERIAL_GRASS, 0, 0);
    BeginMode2D(gpu ? game_gpu_camera(a) : a->camera);
    terrain(a);
    objects(a);
    livestock_proxies(a);
    corpses(a);
    EndMode2D();
    if (gpu && !game_gpu_finish())
        game_app_notice(a, u8"GPU 풍경을 그리지 못했습니다.");
    if (gpu)
        game_gpu_overlay_begin();
    BeginMode2D(gpu ? game_gpu_camera(a) : a->camera);
    if (a->session.wandering.state != CORE_MERCHANT_DORMANT) {
        CoreWanderingMerchantView *m = &a->session.wandering;
        int cell = m->y_milli / 1000 * a->session.summary.map_width + m->x_milli / 1000;
        if (cell >= 0 && cell < a->session.cells && a->session.visibility && a->session.visibility[cell] < 2)
            merchant_sprite(a);
    }
    for (i = 0; i < a->session.villager_count; i++) {
        CoreVillagerView *v = &a->session.villagers[i];
        GameActor *unit = game_actor(&a->session, v->id);
        if (!unit || (!v->incoming && v->state != CORE_VS_LEAVING))
            continue;
        int cell = (int)unit->position.y * a->session.summary.map_width + (int)unit->position.x;
        if (cell >= 0 && cell < a->session.cells && a->session.visibility && a->session.visibility[cell] < 2)
            villager(a, i);
    }
    if (a->in_game)
        overlay(a);
    EndMode2D();
    Vector2 canvas =
        gpu ? game_gpu_canvas_size() : (Vector2){(float)GetScreenWidth(), (float)GetScreenHeight()};
    if (!a->in_game) {
        float dusk = .10f + .22f * (.5f - .5f * cosf(a->menu.time * .035f));
        DrawRectangle(0, 0, (int)canvas.x, (int)canvas.y, Fade((Color){24, 33, 67, 255}, dusk));
        if (gpu)
            game_gpu_overlay_end();
        return;
    }
    if (a->session.summary.is_night && !gpu)
        DrawRectangle(0, 0, (int)canvas.x, (int)canvas.y, (Color){22, 28, 65, 85});
    if (a->session.summary.weather == CORE_WEATHER_RAIN || a->session.summary.weather == CORE_WEATHER_SNOW ||
        a->session.summary.weather == CORE_WEATHER_STORM) {
        int snow = a->session.summary.weather == CORE_WEATHER_SNOW;
        if (!snow)
            game_sound(&a->assets, SND_RAIN, a->settings.sfx_volume * .12f);
        for (i = 0; i < (a->settings.low_effects ? 25 : 100); i++) {
            float x = fmodf(i * 131.7f + time * 17, (float)(int)canvas.x);
            float y = fmodf(i * 83.3f + time * (snow ? 30 : 280), (float)(int)canvas.y);
            if (snow)
                DrawCircleV((Vector2){x, y}, 1.5f, Fade(WHITE, 0.5f));
            else
                DrawLine((int)x, (int)y, (int)x - 3, (int)y + 9, Fade(SKYBLUE, 0.3f));
        }
    }
    CoreEventLayers layers = {0};
    (void)core_expansion_events(a->session.world, &layers);
    int effect = a->session.summary.nature_event;
    if (!a->settings.low_effects && effect == CORE_NATURE_STORM && fmodf(time, 7) < .16f) {
        game_sound(&a->assets, SND_THUNDER, a->settings.sfx_volume * .3f);
        float x = (int)canvas.x * .68f;
        Vector2 last = {x, 0};
        for (int k = 1; k < 7; k++) {
            Vector2 next = {x + sinf(k * 2.7f) * 35, k * (int)canvas.y / 9.0f};
            DrawLineEx(last, next, 2, Fade(WHITE, .8f));
            last = next;
        }
    }
    if (layers.plague_days) {
        for (int k = 0; k < (a->settings.low_effects ? 4 : 18); k++) {
            float x = fmodf(k * 173.0f + time * 9, (int)canvas.x + 80.0f) - 40,
                  y = fmodf(k * 97.0f + time * 3, (int)canvas.y + 80.0f) - 40;
            DrawCircleGradient((int)x, (int)y, 25, (Color){100, 155, 80, 22}, BLANK);
        }
    }
    if (effect == CORE_NATURE_COLD_SNAP)
        DrawRectangleLinesEx((Rectangle){1, 1, (int)canvas.x - 2.0f, (int)canvas.y - 2.0f},
                             a->settings.low_effects ? 3.0f : 10.0f, (Color){175, 225, 255, 65});
    if (!a->settings.low_effects &&
        (effect == CORE_NATURE_BUMPER || effect == CORE_NATURE_DROUGHT || layers.economic_days))
        for (int k = 0; k < 20; k++) {
            float x = fmodf(k * 97.0f + time * 11, (float)(int)canvas.x),
                  y = fmodf(k * 61.0f + time * 4, (float)(int)canvas.y);
            DrawCircleV((Vector2){x, y}, 1,
                        Fade(effect == CORE_NATURE_DROUGHT                        ? BROWN
                             : layers.economic_event == CORE_NATURE_MINE_COLLAPSE ? GRAY
                                                                                  : GOLD,
                             .35f));
        }
    /* 안개는 불꽃·연기·날씨를 포함한 세계 층 뒤에 합성합니다. */
    fog(a);
    if (gpu)
        game_gpu_overlay_end();
    status_bubbles(a);
}
void game_world_input(GameApp *a, float delta) {
    if (a->raid_alert_id)
        return;
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
            game_select(a, 0, 0);
        else
            game_app_open(a, PANEL_PAUSE);
    }
    if (a->panel != PANEL_NONE)
        return;
    {
        float zoom = game_gpu_screen_camera(a).zoom;
        float amount = 230 * delta / zoom;
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
            if (GetMouseY() > GetScreenHeight() - 248)
                a->camera.target.y += amount;
        }
        if (IsMouseButtonDown(MOUSE_BUTTON_MIDDLE)) {
            Vector2 d = GetMouseDelta();
            a->camera.target.x -= d.x / zoom;
            a->camera.target.y -= d.y / zoom;
        }
        if (inside) {
            float wheel = GetMouseWheelMove();
            if (wheel != 0) {
                Vector2 before = GetScreenToWorld2D(GetMousePosition(), game_gpu_screen_camera(a)), after;
                a->camera.zoom = fmaxf(1, fminf(6, a->camera.zoom + (wheel > 0 ? 1 : -1)));
                after = GetScreenToWorld2D(GetMousePosition(), game_gpu_screen_camera(a));
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
            Vector2 mouse = GetScreenToWorld2D(GetMousePosition(), game_gpu_screen_camera(a));
            float best = 1000000;
            game_select(a, id ? 1 : 0, id);
            CoreWanderingMerchantView *m = &s->wandering;
            if (m->state != CORE_MERCHANT_DORMANT &&
                CheckCollisionPointRec(
                    mouse, (Rectangle){m->x_milli * .016f - 12, m->y_milli * .016f - 24, 24, 24})) {
                best = 0;
                game_select(a, 5, m->visit_id);
            }
            for (int i = 0; i < s->actor_count; i++) {
                GameActor *actor = &s->actors[i];
                CoreVillagerView *v = game_villager(s, actor->id);
                if (!v || v->inside_building)
                    continue;
                GameSpriteBox box =
                    game_actor_sprite(actor->position.x * 16, actor->position.y * 16, GAME_ACTOR_HEIGHT);
                Rectangle hit = {box.x, box.y, box.w, box.h};
                float dx = mouse.x - hit.x - hit.width / 2, dy = mouse.y - hit.y - hit.height / 2,
                      distance = dx * dx + dy * dy;
                if (CheckCollisionPointRec(mouse, hit) && distance < best) {
                    best = distance;
                    game_select(a, 2, v->id);
                }
            }
            for (int i = 0; i < s->animal_count; i++) {
                CoreAnimalView *animal = &s->animals[i];
                if (!animal->alive)
                    continue;
                int cell = animal->y_milli / 1000 * s->summary.map_width + animal->x_milli / 1000;
                if (s->visibility && s->visibility[cell] < 2)
                    continue;
                CoreAnimalInfo info;
                core_expansion_animal_info(animal->kind, &info);
                float width = info.width_milli * 16 / 1000.0f, height = info.height_milli * 16 / 1000.0f;
                Vector2 feet = {s->animal_actors[i].position.x * 16, s->animal_actors[i].position.y * 16};
                Rectangle hit = {feet.x - width / 2, feet.y - height, width, height};
                float dx = mouse.x - hit.x - hit.width / 2, dy = mouse.y - hit.y - hit.height / 2,
                      distance = dx * dx + dy * dy;
                if (CheckCollisionPointRec(mouse, hit) && distance < best) {
                    best = distance;
                    game_select(a, 3, animal->id);
                }
            }
            for (int i = 0; i < s->enemy_count; i++) {
                GameEnemy *enemy = &s->enemies[i];
                if (enemy->hp <= 0)
                    continue;
                int cell = (int)enemy->actor.position.y * s->summary.map_width + (int)enemy->actor.position.x;
                if (s->visibility && s->visibility[cell] < 2)
                    continue;
                float width = enemy->info.width_milli * 16 / 1000.0f,
                      height = enemy->info.height_milli * 16 / 1000.0f;
                Rectangle hit = {enemy->actor.position.x * 16 - width / 2,
                                 enemy->actor.position.y * 16 - height, width, height};
                float dx = mouse.x - hit.x - hit.width / 2, dy = mouse.y - hit.y - hit.height / 2,
                      distance = dx * dx + dy * dy;
                if (CheckCollisionPointRec(mouse, hit) && distance < best) {
                    best = distance;
                    game_select(a, 4, enemy->actor.id);
                }
            }
        }
    }
}

void game_render_uniform_preview(GameApp *a) {
    const char *const directions[] = {u8"정면 · 화면 아래로", u8"오른쪽", u8"후면 · 화면 위로"};
    ClearBackground((Color){25, 33, 33, 255});
    ui_text(&a->ui, 24, 20, UI_GOLD, u8"금발 사냥꾼 · 방향별 활 동작4프레임");
    ui_small(&a->ui, 24, 56, UI_TEXT, u8"기존1번 금발의 얼굴·밝은 노란 머리 · 준비 / 당기기 / 발사 / 복귀");
    float cell = (GetScreenWidth() - 48.0f) / 12, row = (GetScreenHeight() - 170.0f) / 4;
    for (int d = 0; d < 3; d++)
        ui_small(&a->ui, 24 + d * cell * 4, 94, UI_MUTED, "%s", directions[d]);
    for (int person = 0; person < 4; person++) {
        int outfit = person / 2, appearance = 2 + person % 2;
        float y = 130 + person * row, height = fminf(cell - 8, row - 35);
        ui_small(&a->ui, 24, y, UI_GOLD, u8"%s · %s", outfit ? u8"수렵기지" : u8"야영지",
                 person % 2 ? u8"여성" : u8"남성");
        for (int d = 0; d < 3; d++)
            for (int f = 0; f < 4; f++) {
                float x = 24 + (d * 4 + f + .5f) * cell;
                sprite(a->assets.uniforms[outfit],
                       (Rectangle){(d * 8 + 4 + f) * 96.0f, appearance * 96.0f, 96, 96},
                       (Rectangle){x - height / 2, y + 28, height, height}, WHITE);
            }
    }
}
void game_render_farm_preview(GameApp *a) {
    const char *const names[] = {u8"경작지",           u8"농장", u8"관개농장", u8"과수원 · 조성 전",
                                 u8"과수원 · 조성 후", u8"텃밭"};
    const char *const directions[] = {u8"정면", u8"오른쪽", u8"후면", u8"왼쪽"};
    ClearBackground((Color){25, 33, 33, 255});
    ui_text(&a->ui, 24, 20, UI_GOLD, u8"농장류 · 울타리와4방향 · 여름 원화");
    float cell = (GetScreenWidth() - 180.0f) / 4, row = (GetScreenHeight() - 110.0f) / 6;
    for (int d = 0; d < 4; d++)
        ui_small(&a->ui, 180 + d * cell + cell / 2 - 25, 66, UI_TEXT, "%s", directions[d]);
    for (int item = 0; item < 6; item++) {
        int atlas_row = item == 3 ? 20 : item == 4 ? 13 : item == 5 ? 17 : item * 4 + 1;
        float y = 110 + item * row, height = fminf(row - 8, cell - 16);
        ui_small(&a->ui, 24, y + row / 2, UI_GOLD, "%s", names[item]);
        for (int d = 0; d < 4; d++) {
            Rectangle target = {180 + d * cell + (cell - height) / 2, y, height, height};
            for (int ty = 0; ty < 3; ty++)
                for (int tx = 0; tx < 3; tx++)
                    DrawRectangleLinesEx((Rectangle){target.x + tx * height / 3, target.y + ty * height / 3,
                                                     height / 3, height / 3},
                                         1, UI_EDGE);
            sprite(a->assets.farms, a->assets.farm_sources[atlas_row * 4 + d], target, WHITE);
        }
    }
}
void game_render_effect_preview(GameApp *a) {
    ClearBackground((Color){25, 35, 35, 255});
    ui_text(&a->ui, 20, 16, UI_GOLD, u8"공격 이미지 · 위 / 오른쪽 / 아래 / 왼쪽 · 위쪽 공격은 인물 뒤");
    const char *const names[] = {u8"참격", u8"화살", u8"타격", u8"발톱",
                                 u8"불꽃", u8"냉기", u8"마법", u8"폭발"};
    float cell = (GetScreenWidth() - 180.0f) / 4, row = (GetScreenHeight() - 60.0f) / 8;
    for (int style = 0; style < 8; style++) {
        float y = 56 + style * row;
        ui_small(&a->ui, 18, y + row * .4f, UI_MUTED, "%s", names[style]);
        for (int d = 0; d < 4; d++) {
            float x = 180 + (d + .5f) * cell, feet = y + row - 3;
            float angle = (float[]){-90, 0, 90, 180}[d], dx = (float[]){0, 1, 0, -1}[d],
                  dy = (float[]){-1, 0, 1, 0}[d];
            Rectangle target = {x + dx * 30, feet - 35 + dy * 18, 64, 64};
            if (d == 0)
                DrawTexturePro(a->assets.combat_fx, (Rectangle){256, style * 128.0f, 128, 128}, target,
                               (Vector2){32, 32}, angle, WHITE);
            GameSpriteFrame frame =
                game_villager_frame(3, (int[]){FACE_UP, FACE_RIGHT, FACE_DOWN, FACE_LEFT}[d], 0, 0);
            sprite(a->assets.villagers,
                   (Rectangle){frame.column * 96.0f, frame.row * 96.0f, frame.flip ? -96.0f : 96.0f, 96},
                   (Rectangle){x - 32, feet - 64, 64, 64}, WHITE);
            if (d != 0)
                DrawTexturePro(a->assets.combat_fx, (Rectangle){256, style * 128.0f, 128, 128}, target,
                               (Vector2){32, 32}, angle, WHITE);
        }
    }
}
