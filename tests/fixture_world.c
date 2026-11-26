#include "fixtures/services.h"

int test_create_settlement(const CoreNewGameParams *p, World **out) {
    int rc = core_create(p, NULL, out);
    if (rc != CORE_OK)
        return rc;
    World *w = *out;
    int dx = 96 - w->keep_x, dy = 96 - w->keep_y;
    memset(w->grid.scratch_mark, 0, (size_t)w->expansion->cells);
    for (int y = 0; y < w->grid.height; y++)
        for (int x = 0; x < w->grid.width; x++)
            if (w->grid.road[grid_index(&w->grid, x, y)]) {
                int xx = x + dx, yy = y + dy;
                if (grid_in_bounds(&w->grid, xx, yy))
                    w->grid.scratch_mark[grid_index(&w->grid, xx, yy)] =
                        w->grid.road[grid_index(&w->grid, x, y)];
            }
    memset(w->grid.road, 0, (size_t)w->expansion->cells);
    for (int i = 0; i < w->expansion->cells; i++)
        w->grid.road[i] = w->grid.scratch_mark[i];
    memset(w->grid.scratch_mark, 0, (size_t)w->expansion->cells);
    w->keep_x += dx;
    w->keep_y += dy;
    for (int i = 0; i < w->building_count; i++) {
        Building *b = w->buildings[i];
        b->x += dx;
        b->y += dy;
        b->door_x += dx;
        b->door_y += dy;
        for (int y = b->y; y < b->y + b->h; y++)
            for (int x = b->x; x < b->x + b->w; x++) {
                int pindex = grid_index(&w->grid, x, y);
                w->grid.terrain[pindex] = CORE_TERRAIN_GRASS;
                CoreExpansionTileView *t = &w->expansion->tiles[pindex];
                t->biome = CORE_BIOME_GRASS;
                t->tree_kind = t->tree_logs_milli = t->tree_age = 0;
                if (!t->clay_shore_percent) {
                    t->clay_shore_percent = 25;
                    t->clay_milli = t->clay_initial_milli = 10000;
                }
            }
    }
    for (int i = 0; i < w->villager_count; i++) {
        Villager *v = w->villagers[i];
        v->loc_x += dx;
        v->loc_y += dy;
        v->target_x += dx;
        v->target_y += dy;
        v->current_x_milli += dx * 1000;
        v->current_y_milli += dy * 1000;
    }
    expansion_water_bodies(w);
    expansion_rebuild(w);
    expansion_visibility_cache(w, 0);
    return CORE_OK;
}
