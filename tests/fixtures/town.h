#ifndef CASTLE_TEST_FIXTURE_TOWN_H
#define CASTLE_TEST_FIXTURE_TOWN_H
#include "content.h"
#include "placement.h"
#include "test_town.h"
#include "villager_ai.h"

/* 자연 생성 결과를 시험하지 않는 단위 사례의 통제된 빈터입니다. */
static inline void fx_town_ground(World *w, int x, int y) {
    int p = grid_index(&w->grid, x, y);
    w->grid.terrain[p] = CORE_TERRAIN_GRASS;
    w->expansion->tiles[p] = (CoreExpansionTileView){0};
    w->expansion->tiles[p].ore_kind = -1;
    w->expansion->tiles[p].clay_shore_percent = 100;
    w->expansion->tiles[p].explored = 1;
    w->expansion->visible[p] = 2;
}
static inline void fx_town_road(World *w, int x, int y) {
    fx_town_ground(w, x, y);
    if (!w->grid.occupant[grid_index(&w->grid, x, y)])
        w->grid.road[grid_index(&w->grid, x, y)] = 1;
}
static inline void fx_town_connect(World *w, int x, int y) {
    int edge = w->keep_y + 20, xx, yy;
    for (yy = y; yy <= edge; yy++)
        fx_town_road(w, x, yy);
    for (xx = w->keep_x; xx <= x; xx++)
        fx_town_road(w, xx, edge);
    for (yy = world_keep(w)->door_y; yy <= edge; yy++)
        fx_town_road(w, w->keep_x, yy);
    w->expansion->dirty = 1;
    expansion_rebuild(w);
}
static inline World *fx_town_world(int seed) {
    World *w = test_make_world(seed, CORE_DIFF_NORMAL);
    if (!w)
        return NULL;
    for (int r = 0; r < CORE_RES_COUNT; r++)
        w->bank.amount[r] = 200;
    return w;
}
static inline Building *fx_town_building(World *w, int type, int slot) {
    int x = w->keep_x + 8 + slot % 5 * 10, y = w->keep_y - 24 + slot / 5 * 12;
    const ExpansionBuildingSpec *spec = expansion_spec(type);
    for (int yy = y; yy <= y + spec->h; yy++)
        for (int xx = x; xx <= x + spec->w; xx++)
            fx_town_ground(w, xx, yy);
    Building *b = fx_content_content_facility(w, type, x, y);
    if (!b)
        return NULL;
    fx_town_connect(w, b->door_x, b->door_y);
    expansion_ensure_building_serials(w);
    expansion_capacity(w);
    return b;
}
static inline void fx_town_worker(World *w, Building *b, Villager *v) {
    entity_unlink_worker(v);
    entity_link_worker(b, v, w->defs.buildings[b->type].worker_slots);
    v->flags |= CORE_VF_MANUAL;
    v->incoming = 0;
    v->state = CORE_VS_WORKING;
    v->target = b;
    v->target_x = (expansion_spec(b->type)->flags & CORE_EXP_PLOT) ? b->x + v->id % b->w : b->door_x;
    v->target_y = (expansion_spec(b->type)->flags & CORE_EXP_PLOT) ? b->y + (v->id / b->w) % b->h : b->door_y;
    v->loc_x = v->target_x;
    v->loc_y = v->target_y;
    v->at_target = 1;
    v->inside_building = b->id;
    v->current_x_milli = v->target_x * 1000 + 500;
    v->current_y_milli = v->target_y * 1000 + 500;
    v->meal_day = exp_day(w);
    v->hub_day = exp_day(w);
    v->needs.hunger = 100;
    v->health = 100;
    v->mission = CORE_MISSION_NONE;
    w->clock.tick = (exp_day(w) - 1) * 144 + 11 * 6;
}
static inline int fx_town_events(World *w, int type) {
    CoreEventView e;
    int count = 0;
    while (core_poll_event(w, &e) == 1)
        count += e.type == type;
    return count;
}
#endif
