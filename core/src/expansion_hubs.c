/* expansion_hubs.c — 같은 이동 비용을 쓰는 공유 은행 방문 경로 */
#include "expansion.h"
#include <limits.h>

static int step_cost(const World *w, int i) {
    int cost = w->grid.cost[i];
    return cost == CORE_COST_GATE ? 67 : cost == CORE_COST_WALL ? 0 : cost;
}
static void heap_swap(ExpansionState *e, int a, int b) {
    int p = e->route_heap[a];
    e->route_heap[a] = e->route_heap[b];
    e->route_heap[b] = p;
    e->route_heap_pos[e->route_heap[a]] = a;
    e->route_heap_pos[p] = b;
}
static void route(World *w, int x, int y) {
    ExpansionState *e = w->expansion;
    static const int dx[4] = {0, 1, 0, -1}, dy[4] = {-1, 0, 1, 0};
    int i, size = 0, start = grid_index(&w->grid, x, y);
    for (i = 0; i < e->cells; i++) {
        e->route_distance[i] = INT_MAX;
        e->route_heap_pos[i] = -1;
    }
    e->route_distance[start] = 0;
    e->route_heap[size++] = start;
    e->route_heap_pos[start] = 0;
    while (size) {
        int p = e->route_heap[0], k, at = 0;
        e->route_heap_pos[p] = -2;
        if (--size) {
            e->route_heap[0] = e->route_heap[size];
            e->route_heap_pos[e->route_heap[0]] = 0;
        }
        for (;;) {
            int child = at * 2 + 1;
            if (child >= size)
                break;
            if (child + 1 < size &&
                e->route_distance[e->route_heap[child + 1]] < e->route_distance[e->route_heap[child]])
                child++;
            if (e->route_distance[e->route_heap[at]] <= e->route_distance[e->route_heap[child]])
                break;
            heap_swap(e, at, child);
            at = child;
        }
        for (k = 0; k < 4; k++) {
            int xx = p % w->grid.width + dx[k], yy = p / w->grid.width + dy[k], n, c, next;
            if (!grid_in_bounds(&w->grid, xx, yy))
                continue;
            n = grid_index(&w->grid, xx, yy);
            c = step_cost(w, n);
            if (!c || e->route_heap_pos[n] == -2)
                continue;
            next = e->route_distance[p] + c;
            if (next >= e->route_distance[n])
                continue;
            e->route_distance[n] = next;
            at = e->route_heap_pos[n];
            if (at < 0) {
                at = size++;
                e->route_heap[at] = n;
                e->route_heap_pos[n] = at;
            }
            while (at > 0) {
                int parent = (at - 1) / 2;
                if (e->route_distance[e->route_heap[parent]] <= e->route_distance[n])
                    break;
                heap_swap(e, at, parent);
                at = parent;
            }
        }
    }
}
static int suitable(const World *w, const Building *b, int food) {
    int p;
    if (b->state != CORE_BSTATE_ACTIVE)
        return 0;
    if (b->type != CORE_BLD_KEEP && b->type != CORE_BLD_WAREHOUSE && b->type != CORE_BLD_LOGISTICS &&
        !(food && b->type == CORE_BLD_GRANARY))
        return 0;
    p = grid_index(&w->grid, b->door_x, b->door_y);
    if (b->type != CORE_BLD_KEEP && !w->expansion->connected[p])
        return 0;
    return !w->expansion->alarm || b->type == CORE_BLD_KEEP || w->expansion->protected_cells[p];
}
Building *expansion_hub(World *w, Villager *v, int food, Building *job) {
    ExpansionState *e = w->expansion;
    Building *best = NULL, *old = entity_find_building(w, v->hub_id), *keep = world_keep(w);
    int i, bestcost = INT_MAX, oldcost = INT_MAX, manorcost = INT_MAX;
    int kind = food + 1 + (e->alarm ? 4 : 0), jobid = job ? job->id : 0;
    if (v->hub_revision == w->grid.cost_rev && v->hub_kind == kind && v->hub_job == jobid && old &&
        suitable(w, old, food))
        return old;
    route(w, v->current_x_milli / 1000, v->current_y_milli / 1000);
    if (job) {
        for (i = 0; i < w->building_count; i++) {
            Building *b = w->buildings[i];
            e->queue[i] = e->route_distance[grid_index(&w->grid, b->door_x, b->door_y)];
        }
        route(w, job->door_x, job->door_y);
    }
    /* 지도 경로는 재사용 배열 한 벌로 계산하며 건물 수만큼의 작은 임시 비용만 보관합니다. */
    for (i = 0; i < w->building_count; i++) {
        Building *b = w->buildings[i];
        int cost;
        if (!suitable(w, b, food))
            continue;
        cost = e->route_distance[grid_index(&w->grid, b->door_x, b->door_y)];
        if (job)
            cost = cost == INT_MAX || e->queue[i] == INT_MAX ? INT_MAX : cost + e->queue[i];
        if (b == keep)
            manorcost = cost;
        if (b == old)
            oldcost = cost;
        if (cost < bestcost || (cost == bestcost && best && b->id < best->id)) {
            best = b;
            bestcost = cost;
        }
    }
    if (old && suitable(w, old, food) && oldcost < INT_MAX &&
        (int64_t)oldcost * 100 <= (int64_t)bestcost * 105)
        best = old;
    if (best && manorcost < INT_MAX && bestcost < manorcost)
        e->saved_travel_ticks += (manorcost - bestcost) / 100;
    v->hub_id = best ? best->id : 0;
    v->hub_revision = w->grid.cost_rev;
    v->hub_kind = kind;
    v->hub_job = jobid;
    return best;
}
void expansion_terrain_costs(World *w) {
    static const uint8_t costs[10] = {100, 108, 113, 130, 112, 118, 108, 0, 135, 103};
    int i;
    for (i = 0; i < w->expansion->cells; i++) {
        if (w->grid.block[i] || !w->grid.cost[i])
            continue;
        w->grid.cost[i] = w->grid.road[i] == 3   ? 80
                          : w->grid.road[i] == 2 ? 50
                          : w->grid.road[i]      ? 67
                                                 : costs[w->expansion->tiles[i].biome];
    }
}
