/* water — 현재 기능의 단일 구현 */
#include "expansion.h"
#include "placement.h"
#include "rank.h"
#include "villager_ai.h"
#include <stdlib.h>
#include <string.h>

int expansion_water_capacity(int type) {
    return type == CORE_BLD_WELL ? 18 : type == CORE_BLD_WATER_TOWER ? 60 : 0;
}

int expansion_water_radius(int type) {
    return type == CORE_BLD_WELL ? 8 : type == CORE_BLD_WATER_TOWER ? 14 : 0;
}

int expansion_adjacent_water_id(const World *w, const Building *b) {
    int best = 0, best_contacts = 0, best_area = 0;
    for (int y = b->y - 1; y <= b->y + b->h; y++)
        for (int x = b->x - 1; x <= b->x + b->w; x++) {
            int dx = x < b->x ? b->x - x : x >= b->x + b->w ? x - b->x - b->w + 1 : 0;
            int dy = y < b->y ? b->y - y : y >= b->y + b->h ? y - b->y - b->h + 1 : 0;
            if (dx + dy != 1 || !grid_in_bounds(&w->grid, x, y))
                continue;
            int id = w->expansion->tiles[grid_index(&w->grid, x, y)].water_id;
            if (id < 1 || id > w->expansion->water_count)
                continue;
            int contacts = 0;
            for (int yy = b->y - 1; yy <= b->y + b->h; yy++)
                for (int xx = b->x - 1; xx <= b->x + b->w; xx++) {
                    int ax = xx < b->x ? b->x - xx : xx >= b->x + b->w ? xx - b->x - b->w + 1 : 0;
                    int ay = yy < b->y ? b->y - yy : yy >= b->y + b->h ? yy - b->y - b->h + 1 : 0;
                    if (ax + ay == 1 && grid_in_bounds(&w->grid, xx, yy) &&
                        w->grid.terrain[grid_index(&w->grid, xx, yy)] == CORE_TERRAIN_WATER &&
                        w->expansion->tiles[grid_index(&w->grid, xx, yy)].water_id == id)
                        contacts++;
                }
            int area = w->expansion->water_area[id - 1];
            if (contacts > best_contacts ||
                (contacts == best_contacts && (area > best_area || (area == best_area && id < best)))) {
                best = id;
                best_contacts = contacts;
                best_area = area;
            }
        }
    return best;
}

CORE_API int32_t core_expansion_adjacent_water(const CoreWorld *w, int32_t type, int32_t x, int32_t y,
                                               int32_t rotation) {
    if (!w || !w->expansion)
        return 0;
    const ExpansionBuildingSpec *s = expansion_spec(type);
    if (!s || rotation < 0 || rotation > 3)
        return 0;
    Building b = {0};
    b.x = x;
    b.y = y;
    b.w = rotation & 1 ? s->h : s->w;
    b.h = rotation & 1 ? s->w : s->h;
    return expansion_adjacent_water_id(w, &b);
}

CORE_API int32_t core_expansion_bridge_mask(const CoreWorld *w, int32_t x, int32_t y) {
    if (!w || !w->expansion || !grid_in_bounds(&w->grid, x, y))
        return -1;
    int p = grid_index(&w->grid, x, y);
    if (w->grid.road[p] != 3 || w->grid.terrain[p] != CORE_TERRAIN_WATER)
        return 0;
    static const int dx[4] = {0, 1, 0, -1}, dy[4] = {-1, 0, 1, 0};
    int connection = 0;
    for (int d = 0; d < 4; d++) {
        int xx = x + dx[d], yy = y + dy[d];
        if (!grid_in_bounds(&w->grid, xx, yy))
            continue;
        int q = grid_index(&w->grid, xx, yy);
        if (w->grid.road[q] == 3 || (w->grid.terrain[q] != CORE_TERRAIN_WATER && w->grid.cost[q] &&
                                     w->grid.block[q] != GRID_BLOCK_WALL && !w->grid.occupant[q]))
            connection |= 1 << d;
    }
    return (~connection) & 15;
}

CORE_API int32_t core_expansion_water_provider(const CoreWorld *w, int32_t id, int32_t *radius,
                                               int32_t *capacity, int32_t *served) {
    if (!w || !radius || !capacity || !served)
        return CORE_ERR_NULL_ARG;
    const Building *b = entity_find_building(w, id);
    if (!b || !w->expansion)
        return CORE_ERR_NOT_FOUND;
    *radius = expansion_water_radius(b->type);
    *capacity = b->state == CORE_BSTATE_ACTIVE ? expansion_water_capacity(b->type) : 0;
    *served = b->water_served;
    return CORE_OK;
}
