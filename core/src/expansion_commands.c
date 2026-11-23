/* expansion_commands.c — 회전 배치, 성채 도로망, 폐쇄 성벽과 시야 */
#include "expansion.h"
#include "mem.h"
#include "placement.h"
#include <stdlib.h>
#include <string.h>

static const int DX[4] = {1, -1, 0, 0}, DY[4] = {0, 0, 1, -1};
int expansion_edge(World *w, int near_x, int near_y, int *x, int *y) {
    Grid *g = &w->grid;
    Building *keep = world_keep(w);
    int head = 0, tail = 0, i, best = INT32_MAX;
    if (!keep)
        return 0;
    memset(g->scratch_mark, 0, (size_t)w->expansion->cells);
    i = grid_index(g, keep->door_x, keep->door_y);
    g->scratch_mark[i] = 1;
    g->scratch_queue[tail++] = i;
    while (head < tail) {
        int p = g->scratch_queue[head++], k, px = p % g->width, py = p / g->width;
        if (px == 0 || py == 0 || px == g->width - 1 || py == g->height - 1) {
            int distance = exp_dist(px, py, near_x, near_y);
            if (distance < best) {
                best = distance;
                *x = px;
                *y = py;
            }
        }
        for (k = 0; k < 4; k++) {
            int xx = px + DX[k], yy = py + DY[k], n;
            if (!grid_in_bounds(g, xx, yy))
                continue;
            n = grid_index(g, xx, yy);
            if (!g->scratch_mark[n] && g->cost[n] && g->block[n] != GRID_BLOCK_WALL) {
                g->scratch_mark[n] = 1;
                g->scratch_queue[tail++] = n;
            }
        }
    }
    return best != INT32_MAX;
}
static void door(int x, int y, int w, int h, int rot, int *dx, int *dy) {
    *dx = x + w / 2;
    *dy = y + h;
    if (rot == 1) {
        *dx = x + w;
        *dy = y + h / 2;
    }
    if (rot == 2) {
        *dx = x + w / 2;
        *dy = y - 1;
    }
    if (rot == 3) {
        *dx = x - 1;
        *dy = y + h / 2;
    }
}
static int road_cell(const World *w, int i) { return w->grid.road[i] || w->grid.block[i] == GRID_BLOCK_GATE; }
static void reveal(World *w, int cx, int cy, int radius, uint8_t *mask) {
    int x, y;
    for (y = cy - radius; y <= cy + radius; y++)
        for (x = cx - radius; x <= cx + radius; x++)
            if (grid_in_bounds(&w->grid, x, y) &&
                (x - cx) * (x - cx) + (y - cy) * (y - cy) <= radius * radius)
                mask[grid_index(&w->grid, x, y)] = 1;
}
void expansion_rebuild(World *w) {
    ExpansionState *e = w->expansion;
    Grid *g = &w->grid;
    Building *keep = world_keep(w);
    int i, x, y, head = 0, tail = 0;
    memset(g->occupant, 0, (size_t)e->cells * sizeof(int32_t));
    memset(g->block, 0, (size_t)e->cells);
    memset(g->door_count, 0, (size_t)e->cells);
    for (i = 0; i < w->building_count; i++) {
        Building *b = w->buildings[i];
        int flags = expansion_spec(b->type)->flags;
        for (y = b->y; y < b->y + b->h; y++)
            for (x = b->x; x < b->x + b->w; x++) {
                int p = grid_index(g, x, y);
                g->occupant[p] = b->id;
                if (b->state != CORE_BSTATE_RUINED)
                    g->block[p] = (uint8_t)(b->state == CORE_BSTATE_CONSTRUCTING ? GRID_BLOCK_SOLID
                                            : (flags & CORE_EXP_GATE)            ? GRID_BLOCK_GATE
                                            : (flags & CORE_EXP_WALL)            ? GRID_BLOCK_WALL
                                            : (flags & CORE_EXP_OUTDOOR)         ? 0
                                                                                 : GRID_BLOCK_SOLID);
            }
        if (!(flags & (CORE_EXP_WALL | CORE_EXP_GATE)) && grid_in_bounds(g, b->door_x, b->door_y))
            g->door_count[grid_index(g, b->door_x, b->door_y)]++;
    }
    grid_rebuild_costs(g);
    expansion_terrain_costs(w);
    memset(e->connected, 0, (size_t)e->cells);
    for (i = 0; i < e->cells; i++)
        e->distance[i] = -1;
    if (keep && grid_in_bounds(g, keep->door_x, keep->door_y)) {
        i = grid_index(g, keep->door_x, keep->door_y);
        if (road_cell(w, i)) {
            e->connected[i] = 1;
            e->distance[i] = 0;
            e->queue[tail++] = i;
        }
    }
    while (head < tail) {
        int p = e->queue[head++], k;
        for (k = 0; k < 4; k++) {
            x = p % g->width + DX[k];
            y = p / g->width + DY[k];
            if (grid_in_bounds(g, x, y)) {
                int n = grid_index(g, x, y);
                if (!e->connected[n] && road_cell(w, n)) {
                    e->connected[n] = 1;
                    e->distance[n] = e->distance[p] + 1;
                    e->queue[tail++] = n;
                }
            }
        }
    }
    /* 외부에서 들어올 수 없는 칸만 보호 구역입니다. 성문도 외부 홍수 채우기에는 벽입니다. */
    memset(e->protected_cells, 0, (size_t)e->cells);
    head = tail = 0;
    for (i = 0; i < e->cells; i++)
        if ((i < g->width || i >= e->cells - g->width || i % g->width == 0 || i % g->width == g->width - 1) &&
            g->block[i] != GRID_BLOCK_WALL && g->block[i] != GRID_BLOCK_GATE) {
            e->protected_cells[i] = 1;
            e->queue[tail++] = i;
        }
    while (head < tail) {
        int p = e->queue[head++], k;
        for (k = 0; k < 4; k++) {
            x = p % g->width + DX[k];
            y = p / g->width + DY[k];
            if (grid_in_bounds(g, x, y)) {
                int n = grid_index(g, x, y);
                if (!e->protected_cells[n] && g->block[n] != GRID_BLOCK_WALL &&
                    g->block[n] != GRID_BLOCK_GATE) {
                    e->protected_cells[n] = 1;
                    e->queue[tail++] = n;
                }
            }
        }
    }
    e->protected_area = 0;
    for (i = 0; i < e->cells; i++) {
        e->protected_cells[i] = (uint8_t)(!e->protected_cells[i] && g->block[i] != GRID_BLOCK_WALL &&
                                          g->block[i] != GRID_BLOCK_GATE);
        e->protected_area += e->protected_cells[i] != 0;
    }
    /* 승급 도로는 실제 시설의 입구에 도달하는 최단 서비스 경로만 셉니다. */
    memset(g->scratch_mark, 0, (size_t)e->cells);
    e->effective_roads = 0;
    for (i = 0; i < w->building_count; i++) {
        Building *b = w->buildings[i];
        int p, k, d;
        if (b->state != CORE_BSTATE_ACTIVE ||
            (expansion_spec(b->type)->flags & (CORE_EXP_WALL | CORE_EXP_GATE)) ||
            !grid_in_bounds(g, b->door_x, b->door_y))
            continue;
        p = grid_index(g, b->door_x, b->door_y);
        d = e->distance[p];
        while (d >= 0) {
            g->scratch_mark[p] = 1;
            if (!d)
                break;
            for (k = 0; k < 4; k++) {
                x = p % g->width + DX[k];
                y = p / g->width + DY[k];
                if (grid_in_bounds(g, x, y)) {
                    int n = grid_index(g, x, y);
                    if (e->distance[n] == d - 1) {
                        p = n;
                        break;
                    }
                }
            }
            if (k == 4)
                break;
            d--;
        }
    }
    for (i = 0; i < e->cells; i++)
        if (g->scratch_mark[i] && road_cell(w, i))
            e->effective_roads++;
    e->protected_population = 0;
    for (i = 0; i < w->villager_count; i++) {
        Villager *v = w->villagers[i];
        if (!v->contract_end_tick && v->home && e->protected_cells[grid_index(g, v->home->x, v->home->y)])
            e->protected_population++;
    }
    memset(e->static_visible, 0, (size_t)e->cells);
    if (keep && keep->state == CORE_BSTATE_ACTIVE)
        for (y = keep->y - 8; y < keep->y + keep->h + 8; y++)
            for (x = keep->x - 8; x < keep->x + keep->w + 8; x++)
                if (grid_in_bounds(g,x,y) && expansion_center_distance(keep,x,y) <= 8)
                    e->static_visible[grid_index(g,x,y)] = 1;
    for (i = 0; i < w->building_count; i++) {
        const Building *b = w->buildings[i];
        if(b->state != CORE_BSTATE_ACTIVE) continue;
        int radius = (expansion_spec(b->type)->flags & CORE_EXP_WALL) ? 0 : 1;
        for(y=b->y-radius;y<b->y+b->h+radius;y++) for(x=b->x-radius;x<b->x+b->w+radius;x++) {
            int distance=(x<b->x?b->x-x:x>=b->x+b->w?x-b->x-b->w+1:0)+(y<b->y?b->y-y:y>=b->y+b->h?y-b->y-b->h+1:0);
            if(grid_in_bounds(g,x,y) && distance<=radius)e->static_visible[grid_index(g,x,y)]=1;
        }
    }
    e->work_visibility_signature = 0;
    for (i = 0; i < e->cells; i++)
        if (road_cell(w, i) && e->connected[i])
            reveal(w, i % g->width, i / g->width, 1, e->static_visible);
    expansion_water_supply(w);
    e->dirty = 0;
}
void expansion_visibility_cache(World *w,int discover) {
    ExpansionState *e = w->expansion;
    int i;
    uint64_t signature = UINT64_C(1469598103934665603);
    for (i = 0; i < w->building_count; i++) {
        Building *b = w->buildings[i];
        if (b->state == CORE_BSTATE_ACTIVE && expansion_working_count(w,b)) {
            signature ^= (uint64_t)(b->id * 131 + b->type * 17 + b->x * 3 + b->y);
            signature *= UINT64_C(1099511628211);
        }
    }
    signature ^= w->research.done;
    if (signature != e->work_visibility_signature) {
        memset(e->work_visible,0,(size_t)e->cells);
        for (i = 0; i < w->building_count; i++) {
            Building *b = w->buildings[i];
            if (b->state != CORE_BSTATE_ACTIVE || !expansion_working_count(w,b)) continue;
            const ExpansionBuildingSpec *s = expansion_spec(b->type);
            if (s->flags & (CORE_EXP_WALL | CORE_EXP_GATE | CORE_EXP_ROAD)) continue;
            int radius = b->type == CORE_BLD_OUTPOST ? 6 : b->type == CORE_BLD_WATCHTOWER ? 10 :
                b->type == CORE_BLD_TOWER ? 12 : b->type == CORE_BLD_CROSSBOW_TOWER || b->type == CORE_BLD_CANNON_TOWER ? 9 :
                b->type == CORE_BLD_ARCANE_TOWER ? 14 : b->type == CORE_BLD_HUNTER_CAMP ? 6 : b->type == CORE_BLD_HUNTING_LODGE ? 12 : 2;
            if ((s->category == CORE_CAT_TOWERS || b->type == CORE_BLD_OUTPOST || b->type == CORE_BLD_WATCHTOWER) && expansion_has_research(w,8)) radius += 2;
            for (int y = b->y - radius; y < b->y + b->h + radius; y++)
                for (int x = b->x - radius; x < b->x + b->w + radius; x++)
                    if (grid_in_bounds(&w->grid,x,y) && expansion_center_distance(b,x,y) <= radius)
                        e->work_visible[grid_index(&w->grid,x,y)] = 1;
        }
        e->work_visibility_signature = signature;
    }
    for (i = 0; i < e->cells; i++) e->visible[i] = e->static_visible[i] || e->work_visible[i];
    for (i = 0; i < w->villager_count; i++) {
        Villager *v = w->villagers[i];
        if (!v->incoming && v->state != CORE_VS_LEAVING)
            reveal(w, v->current_x_milli / 1000, v->current_y_milli / 1000, 4, e->visible);
    }
    if(!discover)return;
    for (i = 0; i < e->cells; i++)
        if (e->visible[i])
            e->tiles[i].explored = 1;
    if (expansion_has_research(w, 16)) {
        for (i = 0; i < e->cells; i++)
            if (e->tiles[i].ore_kind == CORE_RES_MANA && !e->tiles[i].explored) {
                int x = i % w->grid.width, y = i / w->grid.width, dx, dy, found = 0;
                for (dy = -3; dy <= 3 && !found; dy++)
                    for (dx = -3; dx <= 3; dx++)
                        if (dx * dx + dy * dy <= 9 && grid_in_bounds(&w->grid, x + dx, y + dy) &&
                            e->visible[grid_index(&w->grid, x + dx, y + dy)]) {
                            found = 1;
                            break;
                        }
                if (found)
                    e->tiles[i].explored = 1;
            }
    }
}
void expansion_visibility(World *w) { expansion_visibility_cache(w,1); }
void expansion_place_check(const World *w, int type, int x, int y, int rotation, int ignore,
                           CorePlacementView *out) {
    const ExpansionBuildingSpec *s = expansion_spec(type);
    int xx, yy, dx, dy;
    memset(out, 0, sizeof(*out));
    out->result = CORE_PLACE_BAD_TYPE;
    if (!s || type == CORE_BLD_KEEP || rotation < 0 || rotation > 3)
        return;
    if (type == CORE_BLD_STALL || type == CORE_BLD_MARKET || type == CORE_BLD_TRADING_POST)
        for (int n = 0; n < w->building_count; n++) {
            const Building *b = w->buildings[n];
            if (b->id != ignore && (b->type == CORE_BLD_STALL || b->type == CORE_BLD_MARKET || b->type == CORE_BLD_TRADING_POST)) {
                out->result = CORE_PLACE_TRADE_EXISTS; return;
            }
        }
    if (core_expansion_build_locked(w)) { out->result = CORE_PLACE_RAID_BUILD_LOCK; return; }
    out->w = (rotation & 1) ? s->h : s->w;
    out->h = (rotation & 1) ? s->w : s->h;
    memcpy(out->cost, w->defs.buildings[type].cost, sizeof(out->cost));
    out->productivity = 100;
    if (s->rank > w->rank) {
        out->result = CORE_PLACE_LOCKED;
        return;
    }
    if (x < 1 || y < 1 || x + out->w >= w->grid.width || y + out->h >= w->grid.height) {
        out->result = CORE_PLACE_OUT_OF_BOUNDS;
        return;
    }
    for (yy = y; yy < y + out->h; yy++)
        for (xx = x; xx < x + out->w; xx++) {
            int p = grid_index(&w->grid, xx, yy);
            if (!w->expansion->visible[p]) {
                out->result = CORE_PLACE_NOT_VISIBLE;
                return;
            }
            if (w->grid.occupant[p] && w->grid.occupant[p] != ignore) {
                out->result = CORE_PLACE_OCCUPIED;
                return;
            }
            if (type == CORE_BLD_BRIDGE ? w->grid.terrain[p] != CORE_TERRAIN_WATER
                                        : w->grid.terrain[p] == CORE_TERRAIN_WATER) {
                out->result = CORE_PLACE_BAD_TERRAIN;
                return;
            }
            if (w->grid.road[p] && !(s->flags & (CORE_EXP_ROAD | CORE_EXP_GATE))) {
                out->result = CORE_PLACE_ROAD_IN_WAY;
                return;
            }
            if (w->grid.door_count[p] && !(s->flags & CORE_EXP_ROAD) && !(s->flags & CORE_EXP_GATE)) {
                const Building *self = ignore ? entity_find_building(w, ignore) : NULL;
                if (!self || w->grid.door_count[p] > 1 || self->door_x != xx || self->door_y != yy) {
                    out->result = CORE_PLACE_BLOCKS_DOOR;
                    return;
                }
            }
            if ((s->flags & CORE_EXP_ROAD) && w->grid.road[p] == (type == CORE_BLD_STONE_ROAD ? 2
                                                                  : type == CORE_BLD_BRIDGE   ? 3
                                                                                              : 1)) {
                out->result = CORE_PLACE_ALREADY_THERE;
                return;
            }
        }
    if (type == CORE_BLD_PORT || type == CORE_BLD_IRRIGATED_FARM) {
        int water = 0, radius = type == CORE_BLD_PORT ? 1 : 2;
        for (yy = y - radius; yy < y + out->h + radius; yy++)
            for (xx = x - radius; xx < x + out->w + radius; xx++) {
                int ax = xx < x ? x - xx : xx >= x + out->w ? xx - (x + out->w - 1) : 0;
                int ay = yy < y ? y - yy : yy >= y + out->h ? yy - (y + out->h - 1) : 0;
                if (ax + ay <= radius && grid_in_bounds(&w->grid, xx, yy) &&
                    w->grid.terrain[grid_index(&w->grid, xx, yy)] == CORE_TERRAIN_WATER)
                    water = 1;
            }
        if (!water) {
            out->result = CORE_PLACE_NEEDS_WATER;
            return;
        }
    }
    door(x, y, out->w, out->h, rotation, &dx, &dy);
    out->door_x = dx;
    out->door_y = dy;
    if (!(s->flags & (CORE_EXP_ROAD | CORE_EXP_WALL | CORE_EXP_GATE))) {
        int p = grid_index(&w->grid, dx, dy);
        if (!w->grid.cost[p] || (w->grid.occupant[p] && w->grid.occupant[p] != ignore)) {
            out->result = CORE_PLACE_NO_DOOR;
            return;
        }
        if (w->rank >= 1 && s->rank >= 1 && !w->expansion->connected[p]) {
            out->result = CORE_PLACE_UNREACHABLE;
            return;
        }
    }
    if (!ignore && !eco_bank_can_afford(&w->bank, out->cost)) {
        out->result = CORE_PLACE_CANNOT_AFFORD;
        return;
    }
    out->result = CORE_PLACE_OK;
}
int expansion_place(World *w, int type, int x, int y, int rotation, int *id, int *reason) {
    CorePlacementView p;
    const ExpansionBuildingSpec *s = expansion_spec(type);
    Building *b;
    int xx, yy;
    expansion_place_check(w, type, x, y, rotation, 0, &p);
    *id = 0;
    *reason = p.result;
    if (p.result)
        return CORE_ERR_REJECTED;
    if (s->flags & CORE_EXP_ROAD) {
        w->grid.road[grid_index(&w->grid, x, y)] = (uint8_t)(type == CORE_BLD_STONE_ROAD ? 2
                                                             : type == CORE_BLD_BRIDGE   ? 3
                                                                                         : 1);
    } else {
        b = entity_add_building(w, type);
        if (!b)
            return CORE_ERR_OUT_OF_MEMORY;
        *id = b->id;
        b->x = x;
        b->y = y;
        b->w = p.w;
        b->h = p.h;
        b->rotation = rotation;
        b->door_x = p.door_x;
        b->door_y = p.door_y;
        b->hp = w->defs.buildings[type].max_hp;
        b->auto_assign = 1;
        b->state = CORE_BSTATE_CONSTRUCTING;
        b->mode_day = -1;
        b->crop_year = exp_year(w);
    }
    (void)eco_pay(&w->bank, &w->ledger, p.cost, CORE_LEDGER_CONSTRUCTION);
    if (w->expansion->magic_logs_milli > expansion_bank_have(w, CORE_RES_LOGS))
        w->expansion->magic_logs_milli = expansion_bank_have(w, CORE_RES_LOGS);
    for (yy = y; yy < y + p.h; yy++)
        for (xx = x; xx < x + p.w; xx++) {
            int n = grid_index(&w->grid, xx, yy);
            CoreExpansionTileView *t = &w->expansion->tiles[n];
            t->tree_kind = 0;
            t->tree_logs_milli = 0;
            t->regrow_milli = 0;
            if (w->grid.terrain[n] == CORE_TERRAIN_FOREST)
                w->grid.terrain[n] = CORE_TERRAIN_GRASS;
        }
    w->expansion->dirty = 1;
    expansion_v122_numbers(w);
    expansion_rebuild(w);
    events_push_building(&w->events, CORE_EVT_BUILDING_PLACED, *id, type, x, y);
    return CORE_OK;
}
static int service_cell(const World *w, const Building *b) {
    if (expansion_spec(b->type)->flags & CORE_EXP_GATE)
        return grid_index(&w->grid, b->x, b->y);
    if (expansion_spec(b->type)->flags & CORE_EXP_WALL)
        return -1;
    return grid_in_bounds(&w->grid, b->door_x, b->door_y) ? grid_index(&w->grid, b->door_x, b->door_y) : -1;
}
static int connection_removal_allowed(World *w, int road, Building *gate) {
    ExpansionState *e = w->expansion;
    uint8_t *before;
    int i, old = road >= 0 ? w->grid.road[road] : 0, state = gate ? gate->state : 0, reject = 0;
    before = (uint8_t *)mem_calloc((size_t)w->building_count, 1);
    if (!before)
        return CORE_ERR_OUT_OF_MEMORY;
    for (i = 0; i < w->building_count; i++) {
        Building *b = w->buildings[i];
        int cell = service_cell(w, b);
        if (b != gate && (b->state == CORE_BSTATE_ACTIVE || b->state == CORE_BSTATE_CONSTRUCTING) && expansion_spec(b->type)->rank > 0 &&
            !(expansion_spec(b->type)->flags & (CORE_EXP_WALL | CORE_EXP_GATE)) && cell >= 0)
            before[i] = e->connected[cell];
    }
    if (road >= 0)
        w->grid.road[road] = 0;
    if (gate)
        gate->state = CORE_BSTATE_RUINED;
    expansion_rebuild(w);
    for (i = 0; i < w->building_count; i++)
        if (before[i]) {
            int cell = service_cell(w, w->buildings[i]);
            if (cell < 0 || !e->connected[cell])
                reject = 1;
        }
    if (road >= 0)
        w->grid.road[road] = (uint8_t)old;
    if (gate)
        gate->state = state;
    expansion_rebuild(w);
    mem_free(before);
    return reject ? CORE_ERR_REJECTED : CORE_OK;
}
int expansion_remove_road(World *w, int x, int y) {
    int p, rc;
    if (!grid_in_bounds(&w->grid, x, y))
        return CORE_ERR_INVALID_ARG;
    p = grid_index(&w->grid, x, y);
    if (!w->grid.road[p])
        return CORE_ERR_NOT_FOUND;
    rc = connection_removal_allowed(w, p, NULL);
    if (rc != CORE_OK)
        return rc;
    w->grid.road[p] = 0;
    expansion_rebuild(w);
    return CORE_OK;
}
int expansion_person_protected(const World *w, const Villager *v) {
    const Building *inside = v->inside_building ? entity_find_building(w, v->inside_building) : NULL;
    int x = inside ? inside->x : v->current_x_milli / 1000,
        y = inside ? inside->y : v->current_y_milli / 1000;
    return grid_in_bounds(&w->grid, x, y) && w->expansion->protected_cells[grid_index(&w->grid, x, y)];
}
Building *expansion_refuge(World *w, const Villager *v, int *x, int *y) {
    ExpansionState *e = w->expansion;
    Grid *g = &w->grid;
    Building *keep = world_keep(w);
    int head = 0, tail = 0, start, i;
    *x = keep ? keep->door_x : w->keep_x;
    *y = keep ? keep->door_y : w->keep_y;
    if (!e->protected_area)
        return keep;
    if (!grid_in_bounds(g, v->current_x_milli / 1000, v->current_y_milli / 1000))
        return keep;
    start = grid_index(g, v->current_x_milli / 1000, v->current_y_milli / 1000);
    memset(g->scratch_mark, 0, (size_t)e->cells);
    g->scratch_mark[start] = 1;
    g->scratch_queue[tail++] = start;
    while (head < tail) {
        int p = g->scratch_queue[head++], k;
        if (e->protected_cells[p] && g->cost[p] && g->block[p] != GRID_BLOCK_WALL) {
            *x = p % g->width;
            *y = p / g->width;
            for (i = 0; i < w->building_count; i++) {
                Building *b = w->buildings[i];
                if (b->state == CORE_BSTATE_ACTIVE &&
                    !(expansion_spec(b->type)->flags & (CORE_EXP_OUTDOOR | CORE_EXP_WALL | CORE_EXP_GATE)) &&
                    b->door_x == *x && b->door_y == *y)
                    return b;
            }
            return NULL;
        }
        for (k = 0; k < 4; k++) {
            int xx = p % g->width + DX[k], yy = p / g->width + DY[k], n;
            if (!grid_in_bounds(g, xx, yy))
                continue;
            n = grid_index(g, xx, yy);
            if (!g->scratch_mark[n] && g->cost[n] && g->block[n] != GRID_BLOCK_WALL) {
                g->scratch_mark[n] = 1;
                g->scratch_queue[tail++] = n;
            }
        }
    }
    return keep;
}
CORE_API int32_t core_expansion_copy_protection(const CoreWorld *w, uint8_t *out, int32_t capacity) {
    if (!w || !out)
        return CORE_ERR_NULL_ARG;
    if (!w->expansion)
        return CORE_ERR_INVALID_ARG;
    if (capacity < w->expansion->cells)
        return CORE_ERR_BUFFER_TOO_SMALL;
    memcpy(out, w->expansion->protected_cells, (size_t)w->expansion->cells);
    return CORE_OK;
}
int expansion_demolish(World *w, Building *b) {
    int r;
    if (b->type == CORE_BLD_KEEP)
        return CORE_ERR_REJECTED;
    if (expansion_spec(b->type)->flags & CORE_EXP_GATE) {
        int rc = connection_removal_allowed(w, -1, b);
        if (rc != CORE_OK)
            return rc;
    }
    for (r = 0; r < CORE_RES_COUNT; r++)
        (void)expansion_bank_add(w, r, w->defs.buildings[b->type].cost[r] * 500, CORE_LEDGER_CONSTRUCTION);
    for (r = 0; r < w->expansion->animal_count; r++)
        if (w->expansion->animals[r].owner == b->id) {
            w->expansion->animals[r].alive = 0;
            w->expansion->animals[r].owner = 0;
        }
    for (r = 0; r < w->villager_count; r++)
        if (w->villagers[r]->inside_building == b->id)
            w->villagers[r]->inside_building = 0;
    entity_remove_building(w, b);
    w->expansion->dirty = 1;
    expansion_rebuild(w);
    expansion_capacity(w);
    expansion_assign(w);
    return CORE_OK;
}
CORE_API int32_t core_can_place_rotated(const CoreWorld *w, int32_t type, int32_t x, int32_t y, int32_t rot,
                                        CorePlacementView *out) {
    if (!w || !out)
        return CORE_ERR_NULL_ARG;
    if (!w->expansion)
        return CORE_ERR_INVALID_ARG;
    expansion_place_check(w, type, x, y, rot, 0, out);
    return CORE_OK;
}
CORE_API int32_t core_place_rotated(CoreWorld *w, int32_t type, int32_t x, int32_t y, int32_t rot,
                                    int32_t *id, int32_t *reason) {
    int a, b;
    if (!w)
        return CORE_ERR_NULL_ARG;
    if (!w->expansion)
        return CORE_ERR_INVALID_ARG;
    return expansion_place(w, type, x, y, rot, id ? id : &a, reason ? reason : &b);
}
CORE_API int32_t core_rotate_building(CoreWorld *w, int32_t id, int32_t rot, int32_t *reason) {
    Building *b;
    CorePlacementView p;
    if (!w)
        return CORE_ERR_NULL_ARG;
    if (!w->expansion)
        return CORE_ERR_INVALID_ARG;
    b = entity_find_building(w, id);
    if (!b)
        return CORE_ERR_NOT_FOUND;
    if (b->type == CORE_BLD_KEEP) {
        if (reason)
            *reason = CORE_PLACE_BAD_TYPE;
        return CORE_ERR_REJECTED;
    }
    expansion_place_check(w, b->type, b->x, b->y, rot, b->id, &p);
    if (reason)
        *reason = p.result;
    if (p.result)
        return CORE_ERR_REJECTED;
    if (b->state == CORE_BSTATE_ACTIVE &&
        w->expansion->connected[grid_index(&w->grid, b->door_x, b->door_y)] &&
        !w->expansion->connected[grid_index(&w->grid, p.door_x, p.door_y)]) {
        if (reason)
            *reason = CORE_PLACE_BLOCKS_PATH;
        return CORE_ERR_REJECTED;
    }
    b->rotation = rot;
    b->w = p.w;
    b->h = p.h;
    b->door_x = p.door_x;
    b->door_y = p.door_y;
    w->expansion->dirty = 1;
    expansion_rebuild(w);
    return CORE_OK;
}
CORE_API int32_t core_expansion_copy_tiles(const CoreWorld *w, CoreExpansionTileView *out, int32_t cap) {
    if (!w || !out)
        return CORE_ERR_NULL_ARG;
    if (!w->expansion)
        return CORE_ERR_INVALID_ARG;
    if (cap < w->expansion->cells)
        return CORE_ERR_BUFFER_TOO_SMALL;
    memcpy(out, w->expansion->tiles, (size_t)w->expansion->cells * sizeof(*out));
    return CORE_OK;
}
CORE_API int32_t core_expansion_copy_visibility(const CoreWorld *w, uint8_t *out, int32_t cap) {
    int i;
    if (!w || !out)
        return CORE_ERR_NULL_ARG;
    if (!w->expansion)
        return CORE_ERR_INVALID_ARG;
    if (cap < w->expansion->cells)
        return CORE_ERR_BUFFER_TOO_SMALL;
    for (i = 0; i < w->expansion->cells; i++)
        out[i] = (uint8_t)(w->expansion->visible[i] ? 2 : w->expansion->tiles[i].explored ? 1 : 0);
    return CORE_OK;
}
CORE_API int32_t core_expansion_report_position(CoreWorld *w, int32_t id, int32_t x, int32_t y) {
    Villager *v;
    if (!w)
        return CORE_ERR_NULL_ARG;
    if (!w->expansion)
        return CORE_ERR_INVALID_ARG;
    v = entity_find_villager(w, id);
    if (!v)
        return CORE_ERR_NOT_FOUND;
    if (x < 0 || y < 0 || x >= w->grid.width * 1000 || y >= w->grid.height * 1000)
        return CORE_ERR_INVALID_ARG;
    v->current_x_milli = x;
    v->current_y_milli = y;
    return CORE_OK;
}
CORE_API int32_t core_expansion_spotted(CoreWorld *w, int32_t x, int32_t y) {
    if (!w)
        return CORE_ERR_NULL_ARG;
    if (!w->expansion || !grid_in_bounds(&w->grid, x, y))
        return CORE_ERR_INVALID_ARG;
    if (!w->expansion->visible[grid_index(&w->grid, x, y)])
        return CORE_IGNORED;
    expansion_raid_discover(w);
    return CORE_OK;
}
CORE_API int32_t core_expansion_fire(CoreWorld *w, int32_t id) {
    Building *b;
    if (!w)
        return CORE_ERR_NULL_ARG;
    b = entity_find_building(w, id);
    if (!b)
        return CORE_ERR_NOT_FOUND;
    if (!w->expansion || b->state != CORE_BSTATE_ACTIVE || (b->type!=CORE_BLD_KEEP && !expansion_working_count(w,b)) ||
        (!expansion_spec(b->type)->attack && b->type!=CORE_BLD_KEEP))
        return CORE_ERR_REJECTED;
    if (b->type == CORE_BLD_ARCANE_TOWER && b->shots % 3 == 0) {
        int cost = expansion_has_research(w, 22) ? 900 : 1000;
        if (expansion_bank_have(w, CORE_RES_MANA) < cost)
            return CORE_ERR_REJECTED;
        (void)expansion_bank_add(w, CORE_RES_MANA, -cost, CORE_LEDGER_PRODUCTION);
    }
    if(expansion_tower(b->type))for(int i=0;i<b->worker_count;i++)if(expansion_working_present(w,b,b->workers[i])) {
        b->combat_guard_id=b->workers[i]->id;b->combat_until_tick=w->clock.tick+6;break;
    }
    b->shots++;
    return CORE_OK;
}
CORE_API int32_t core_place_line_rotated(CoreWorld *w, int32_t type, int32_t x0, int32_t y0, int32_t x1,
                                         int32_t y1, int32_t rotation, int32_t *placed, int32_t *reason) {
    int x = x0, y = y0, n = 0, last = 0, steps = 0;
    const ExpansionBuildingSpec *s = expansion_spec(type);
    if (!w)
        return CORE_ERR_NULL_ARG;
    if (!w->expansion || !s || !(s->flags & (CORE_EXP_ROAD | CORE_EXP_WALL | CORE_EXP_GATE)) ||
        rotation < 0 || rotation > 3)
        return CORE_ERR_INVALID_ARG;
    for (;;) {
        int id = 0, why = 0, rc = expansion_place(w, type, x, y, rotation, &id, &why);
        if (rc == CORE_ERR_OUT_OF_MEMORY) {
            if (placed)
                *placed = n;
            return rc;
        }
        if (rc == CORE_OK)
            n++;
        else if (why != CORE_PLACE_ALREADY_THERE && !last)
            last = why;
        if ((x == x1 && y == y1) || ++steps >= 256 || why == CORE_PLACE_CANNOT_AFFORD)
            break;
        if (x != x1)
            x += x1 > x ? 1 : -1;
        else
            y += y1 > y ? 1 : -1;
    }
    if (placed)
        *placed = n;
    if (reason)
        *reason = last;
    return n || !last ? CORE_OK : CORE_ERR_REJECTED;
}
