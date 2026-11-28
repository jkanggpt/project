/* 조명 검수 장면: 실제 GameSession·월드 렌더러·재료 쌍을 그대로 쓰고 저장하지 않습니다.
   건물 네 방향, 불·용광로, 농장, 세 종류의 길, 물·항구·등대, 나무·바위, 밤 군중과 구름 경계를
   한 화면 근처에 모읍니다. 게임 규칙 대신 검수용 배치를 직접 만듭니다. */
#include "review.h"
#include "app.h"
#include "expansion.h"
#include "population.h"
#include "render/gpu_bridge.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static int review_help = 1, review_clean;
int game_review_interface_visible(void) { return !review_clean; }
void game_review_set_clean(int clean) {
    review_clean = clean;
    review_help = !clean;
}
static void clear_cell(World *w, int x, int y, int terrain) {
    if (!grid_in_bounds(&w->grid, x, y))
        return;
    int p = grid_index(&w->grid, x, y);
    w->grid.terrain[p] = (uint8_t)terrain;
    w->grid.road[p] = 0;
    w->expansion->tiles[p].tree_kind = 0;
    w->expansion->tiles[p].tree_logs_milli = 0;
    w->expansion->tiles[p].rock_variant = 0;
    w->expansion->tiles[p].explored = 1;
    if (terrain == CORE_TERRAIN_WATER)
        w->expansion->tiles[p].biome = CORE_BIOME_LAKE;
}
static Building *place(World *w, int type, int x, int y, int rotation) {
    const ExpansionBuildingSpec *spec = expansion_spec(type);
    int bw = (rotation & 1) ? spec->h : spec->w, bh = (rotation & 1) ? spec->w : spec->h;
    for (int yy = y; yy < y + bh; yy++)
        for (int xx = x; xx < x + bw; xx++)
            if (!grid_in_bounds(&w->grid, xx, yy) || w->grid.occupant[grid_index(&w->grid, xx, yy)])
                return NULL;
    Building *b = entity_add_building(w, type);
    if (!b)
        return NULL;
    b->x = x;
    b->y = y;
    b->w = bw;
    b->h = bh;
    b->rotation = rotation;
    b->hp = spec->hp;
    b->state = CORE_BSTATE_ACTIVE;
    b->operating = 1;
    b->auto_assign = 1;
    b->mode_day = -1;
    b->crop_year = exp_year(w);
    b->door_x = x + bw / 2;
    b->door_y = y + bh;
    for (int yy = y; yy < y + bh; yy++)
        for (int xx = x; xx < x + bw; xx++)
            w->grid.occupant[grid_index(&w->grid, xx, yy)] = b->id;
    return b;
}
static void road(World *w, int x0, int x1, int y, int kind) {
    for (int x = x0; x <= x1; x++)
        if (grid_in_bounds(&w->grid, x, y) && !w->grid.occupant[grid_index(&w->grid, x, y)])
            w->grid.road[grid_index(&w->grid, x, y)] = (uint8_t)kind;
}
int game_review_start(GameApp *a, int seed, int crowd) {
    if (game_session_new(&a->session, u8"조명 검수", seed, 0) != CORE_OK)
        return 0;
    World *w = a->session.world;
    int kx = w->keep_x, ky = w->keep_y;
    w->rank = 5;
    for (int r = 0; r < CORE_RES_COUNT; r++)
        w->bank.amount[r] = r == CORE_RES_GOLD ? 100000 : eco_bank_limit(&w->bank, r);
    /* 오전 11시: 주민이 밖을 다니는 시각입니다. 밤·낮 표시는 검수 상태가 따로 정합니다. */
    w->clock.tick = (w->clock.tick / CLOCK_TICKS_PER_DAY) * CLOCK_TICKS_PER_DAY + 66;
    for (int y = ky + 3; y <= ky + 27; y++)
        for (int x = kx - 24; x <= kx + 26; x++)
            clear_cell(w, x, y, CORE_TERRAIN_GRASS);
    for (int y = ky + 15; y <= ky + 27; y++)
        for (int x = kx + 13; x <= kx + 26; x++)
            clear_cell(w, x, y, CORE_TERRAIN_WATER);
    /* 나무·바위 줄: 원래 자연 원화와 그림자·잎 흔들림을 함께 봅니다. */
    for (int i = 0; i < 9; i++) {
        int p = grid_index(&w->grid, kx - 23 + (i % 3) * 2, ky + 18 + (i / 3) * 3);
        CoreExpansionTileView *t = &w->expansion->tiles[p];
        t->tree_kind = 1 + i % 3;
        t->tree_age = 4000;
        t->tree_logs_milli = 8000;
        if (i % 3 == 2)
            t->rock_variant = 1 + i % 2, t->tree_kind = 0, t->tree_logs_milli = 0;
    }
    static const int row1[] = {CORE_BLD_HOUSE, CORE_BLD_HOUSE,    CORE_BLD_HOUSE,     CORE_BLD_HOUSE,
                               CORE_BLD_TAVERN, CORE_BLD_KILN,    CORE_BLD_SMELTER,   CORE_BLD_MAGE_TOWER};
    static const int row2[] = {CORE_BLD_MARKET,  CORE_BLD_BARRACKS, CORE_BLD_COTTAGE, CORE_BLD_MANSION,
                               CORE_BLD_WINDMILL, CORE_BLD_HUNTER_CAMP, CORE_BLD_GATHERER, CORE_BLD_TOWER};
    static const int row3[] = {CORE_BLD_FARM, CORE_BLD_CROPLAND, CORE_BLD_ORCHARD, CORE_BLD_GARDEN};
    int x = kx - 22;
    for (int i = 0; i < 8; i++) {
        Building *b = place(w, row1[i], x, ky + 4, i < 4 ? i : 0);
        x += b ? b->w + 2 : 3;
    }
    x = kx - 22;
    for (int i = 0; i < 8; i++) {
        Building *b = place(w, row2[i], x, ky + 10, row2[i] == CORE_BLD_COTTAGE ? 1 : 0);
        x += b ? b->w + 2 : 3;
    }
    x = kx - 22;
    for (int i = 0; i < 4; i++) {
        Building *b = place(w, row3[i], x, ky + 17, 0);
        x += b ? b->w + 1 : 3;
    }
    (void)place(w, CORE_BLD_PORT, kx + 11, ky + 17, 0);
    (void)place(w, CORE_BLD_LIGHTHOUSE, kx + 11, ky + 22, 0);
    (void)place(w, CORE_BLD_STONE_GATE, kx + 4, ky + 22, 0);
    for (int i = 1; i <= 3; i++) {
        (void)place(w, CORE_BLD_WALL, kx + 4 - i, ky + 22, 0);
        (void)place(w, CORE_BLD_WALL, kx + 6 + i, ky + 22, 0);
    }
    road(w, kx - 23, kx + 12, ky + 9, 1);
    road(w, kx - 23, kx + 12, ky + 15, 2);
    road(w, kx - 23, kx + 12, ky + 3, 4);
    expansion_water_bodies(w);
    expansion_rebuild(w);
    /* 군중은 지구 전체의 빈 칸에 흩어 세워 등불 수천 겹침과 그림자 예산을 함께 봅니다. */
    for (int i = 0, tries = 0; w->villager_count < crowd && tries < crowd * 8; tries++) {
        int sx = kx - 23 + (tries * 37) % 36, sy = ky + 3 + (tries * 13) % 15;
        if (!grid_in_bounds(&w->grid, sx, sy) || w->grid.occupant[grid_index(&w->grid, sx, sy)] ||
            w->grid.terrain[grid_index(&w->grid, sx, sy)] == CORE_TERRAIN_WATER)
            continue;
        if (!population_create(w, sx, sy))
            break;
        i++;
    }
    expansion_assign(w);
    expansion_capacity(w);
    for (int i = 0; i < w->villager_count; i++) {
        Villager *v = w->villagers[i];
        v->needs.hunger = v->needs.rest = 100;
        v->health = 100;
        v->happiness = 90;
        v->meal_day = exp_day(w);
    }
    expansion_boats_tick(w);
    expansion_ensure_building_serials(w);
    expansion_visibility(w);
    if (game_session_refresh(&a->session) != CORE_OK)
        return 0;
    game_gpu_review_enable(1);
    a->camera.target = (Vector2){(kx + 1) * 16.0f, (ky + 14) * 16.0f};
    a->camera.zoom = 2;
    a->speed = 1;
    printf("LIGHTING REVIEW: buildings=%d villagers=%d boats=%d seed=%d\n", a->session.building_count,
           a->session.villager_count, a->session.boat_count, seed);
    return 1;
}
void game_review_update(GameApp *a) {
    if (IsKeyPressed(KEY_H))
        review_help = !review_help;
    if (IsKeyPressed(KEY_U))
        review_clean = !review_clean;
    /* 검수 중에는 알림·자동 안내가 장면을 가리지 않게 합니다. */
    a->notice_count = 0;
    game_gpu_review_input();
}
void game_review_draw_help(GameApp *a) {
    static const char *const views[] = {u8"최종 조명", u8"알베도(명암 완화)", u8"법선",  u8"태양 그림자",
                                        u8"지역광 차폐",  u8"물 반사",          u8"재료(높이·발광·거칠기)",
                                        u8"칸별 광원 수"};
    GameReviewStatus st = game_gpu_review_status();
    Rectangle box = {12, 12, 430, review_help ? 236.0f : 40.0f};
    DrawRectangleRec(box, (Color){18, 22, 28, 214});
    DrawRectangleLinesEx(box, 1, (Color){120, 132, 140, 255});
    ui_text(&a->ui, 24, 20, (Color){236, 226, 196, 255}, u8"조명 검수 · %s · %s", st.phase_name,
            views[st.view & 7]);
    if (!review_help && review_clean)
        return;
    if (!review_help)
        return;
    const Color text = {214, 220, 222, 255};
    ui_small(&a->ui, 24, 52, text, u8"F7  밤 → 새벽 → 낮 → 해질녘");
    ui_small(&a->ui, 24, 74, text, u8"F8  진단 보기 바꾸기 (%d/8)", (st.view & 7) + 1);
    ui_small(&a->ui, 24, 96, text, u8"방향키  태양 방위 %.0f° · 고도 %.0f°", st.azimuth, st.elevation);
    ui_small(&a->ui, 24, 118, text, u8"오른쪽 마우스  누르는 동안 시험 횃불");
    ui_small(&a->ui, 24, 140, text, u8"가운데 마우스·휠  이동·확대, H 도움말, U 게임 UI 숨기기");
    ui_small(&a->ui, 24, 166, (Color){190, 200, 170, 255}, u8"광원 %d · 등불 %d · 그림자 체적 %d",
             st.lights, st.carried, st.casters);
    ui_small(&a->ui, 24, 188, (Color){190, 200, 170, 255}, u8"주민 %d · 건물 %d · 배 %d",
             a->session.villager_count, a->session.building_count, a->session.boat_count);
    ui_small(&a->ui, 24, 210, (Color){170, 176, 180, 255}, u8"저장하지 않는 검수 장면입니다.");
}
