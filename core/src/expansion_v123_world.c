/* expansion_v123_world.c — 새 지도만 생성하며 옛 지도는 점토층만 한 번
 * 보정합니다. */
#include "expansion.h"
#include "mem.h"
#include <stdlib.h>
#include <string.h>

uint32_t expansion_map_hash(int seed, int x, int y) {
    uint32_t n = (uint32_t)seed * 2654435761u + (uint32_t)x * 374761393u + (uint32_t)y * 668265263u;
    n = (n ^ (n >> 13)) * 1274126177u;
    return n ^ (n >> 16);
}
#define PATCHES 18
static int patch_cost(const World *w, int x, int y, int cx, int cy, int index, int attempt) {
    int wx = (int)(expansion_map_hash(w->seed, x / 6, y / 6) % 7) - 3;
    int wy = (int)(expansion_map_hash(w->seed + 17, x / 7, y / 7) % 7) - 3;
    int dx = x + wx - cx, dy = y + wy - cy;
    int axis = 80 + (int)(expansion_map_hash(w->seed, index, attempt + 91) % 41);
    return (dx * dx * axis + dy * dy * (200 - axis)) / 100;
}
int expansion_v123_biomes(World *w) {
    static const int kinds[PATCHES] = {0, 0, 0, 1, 1, 1, 2, 2, 2, 3, 3, 4, 4, 5, 6, 8, 9, 9};
    static const int shares[10] = {23, 18, 12, 12, 9, 7, 5, 0, 5, 9},
                     pieces[10] = {3, 3, 3, 2, 2, 1, 1, 0, 1, 2};
    ExpansionState *e = w->expansion;
    int *owner = e->route_distance, *best_map = e->route_heap;
    int best_error = INT32_MAX;
    int *costs = mem_malloc((size_t)e->cells * PATCHES * sizeof(int));
    if (!costs)
        return CORE_ERR_OUT_OF_MEMORY;
    for (int attempt = 0; attempt < 8; attempt++) {
        int cx[PATCHES], cy[PATCHES], bias[PATCHES] = {0}, counts[PATCHES] = {0};
        cx[0] = w->keep_x;
        cy[0] = w->keep_y;
        for (int k = 1; k < PATCHES; k++)
            for (int trial = 0; trial < 4000; trial++) {
                uint32_t n = expansion_map_hash(w->seed, k * 4001 + trial, attempt + 31);
                int x = 7 + (int)(n % (unsigned)(w->grid.width - 14)),
                    y = 7 + (int)((n >> 16) % (unsigned)(w->grid.height - 14)), valid = 1;
                for (int q = 0; q < k; q++) {
                    int dx = x - cx[q], dy = y - cy[q], minimum = kinds[k] == kinds[q] ? 25 : 18;
                    if (dx * dx + dy * dy < minimum * minimum)
                        valid = 0;
                }
                if (valid || trial == 3999) {
                    cx[k] = x;
                    cy[k] = y;
                    break;
                }
            }
        for (int p = 0; p < e->cells; p++)
            for (int k = 0; k < PATCHES; k++)
                costs[p * PATCHES + k] =
                    patch_cost(w, p % w->grid.width, p / w->grid.width, cx[k], cy[k], k, attempt);
        for (int iteration = 0; iteration < 24; iteration++) {
            memset(counts, 0, sizeof(counts));
            for (int p = 0; p < e->cells; p++) {
                int chosen = 0, score = INT32_MAX;
                for (int k = 0; k < PATCHES; k++) {
                    int d = costs[p * PATCHES + k] - bias[k];
                    if (d < score) {
                        score = d;
                        chosen = k;
                    }
                }
                owner[p] = chosen;
                counts[chosen]++;
            }
            int error = 0;
            for (int k = 0; k < PATCHES; k++) {
                int target = e->cells * shares[kinds[k]] / (100 * pieces[kinds[k]]);
                int diff = target - counts[k];
                error += abs(diff);
                bias[k] += diff / 3;
            }
            if (error < e->cells / 100)
                break;
        }
        int total[10] = {0}, largest[10] = {0}, components[10] = {0};
        for (int p = 0; p < e->cells; p++) {
            int kind = kinds[owner[p]], x = p % w->grid.width, y = p / w->grid.width, cut = 0;
            static const int dx[4] = {0, 1, 0, -1}, dy[4] = {-1, 0, 1, 0};
            for (int d = 0; d < 4; d++) {
                int xx = x + dx[d], yy = y + dy[d];
                if (grid_in_bounds(&w->grid, xx, yy)) {
                    int q = owner[grid_index(&w->grid, xx, yy)];
                    if (q != owner[p] && kinds[q] == kind)
                        cut = 1;
                }
            }
            if (cut) {
                int score = INT32_MAX;
                for (int k = 0; k < PATCHES; k++)
                    if (kinds[k] != kind) {
                        int d = patch_cost(w, x, y, cx[k], cy[k], k, attempt) - bias[k];
                        if (d < score) {
                            score = d;
                            e->queue[p] = kinds[k];
                        }
                    }
            } else
                e->queue[p] = kind;
            if (exp_dist(x, y, w->keep_x, w->keep_y) <= 9)
                e->queue[p] = CORE_BIOME_GRASS;
            total[e->queue[p]]++;
        }
        /* 연결 성분의 면적으로 단일 거대 초원/숲과 잡음을 검출합니다. */
        memset(w->grid.scratch_mark, 0, (size_t)e->cells);
        int *front = e->route_heap_pos;
        for (int p = 0; p < e->cells; p++)
            if (!w->grid.scratch_mark[p]) {
                int head = 0, tail = 0, kind = e->queue[p];
                front[tail++] = p;
                w->grid.scratch_mark[p] = 1;
                while (head < tail) {
                    int q = front[head++], x = q % w->grid.width, y = q / w->grid.width;
                    static const int dx[4] = {0, 1, 0, -1}, dy[4] = {-1, 0, 1, 0};
                    for (int d = 0; d < 4; d++) {
                        int xx = x + dx[d], yy = y + dy[d];
                        if (!grid_in_bounds(&w->grid, xx, yy))
                            continue;
                        int n = grid_index(&w->grid, xx, yy);
                        if (!w->grid.scratch_mark[n] && e->queue[n] == kind) {
                            w->grid.scratch_mark[n] = 1;
                            front[tail++] = n;
                        }
                    }
                }
                if (tail >= 12) {
                    components[kind]++;
                    if (tail > largest[kind])
                        largest[kind] = tail;
                }
            }
        int error = 0;
        for (int k = 0; k < 10; k++)
            if (k != CORE_BIOME_LAKE)
                error += abs(total[k] * 100 - e->cells * shares[k]);
        for (int k = 0; k < 2; k++)
            if (components[k] < 2 || largest[k] * 100 > total[k] * 60)
                error += e->cells * 100;
        if (error < best_error) {
            best_error = error;
            memcpy(best_map, e->queue, (size_t)e->cells * sizeof(int));
        }
        if (error < e->cells * 6)
            break;
    }
    for (int p = 0; p < e->cells; p++)
        e->tiles[p].biome = best_map[p];
    mem_free(costs);
    return CORE_OK;
}
static int protected_start(const World *w, int x, int y) {
    int dx = x - w->keep_x, dy = y - w->keep_y;
    return exp_dist(x, y, w->keep_x, w->keep_y) <= 12 || (dx >= 9 && dx <= 15 && abs(dy) <= 3) ||
           exp_dist(dx, dy, -10, -8) <= 2 || exp_dist(dx, dy, 20, -4) <= 2 ||
           w->grid.occupant[grid_index(&w->grid, x, y)] || (x == w->keep_x && y == w->keep_y + 2);
}
static int lake_cell(const World *w, int x, int y) {
    if (x < 2 || y < 2 || x >= w->grid.width - 2 || y >= w->grid.height - 2 || protected_start(w, x, y))
        return 0;
    for (int yy = y - 2; yy <= y + 2; yy++)
        for (int xx = x - 2; xx <= x + 2; xx++)
            if (exp_dist(xx, yy, x, y) <= 2 &&
                w->grid.terrain[grid_index(&w->grid, xx, yy)] == CORE_TERRAIN_WATER)
                return 0;
    return 1;
}
static int grow_lake(World *w, int cx, int cy, int area, int index, int *remaining, int tolerance) {
    ExpansionState *e = w->expansion;
    int count = 0, tail = 1;
    int *front = e->queue, *picked = e->route_heap;
    memset(w->grid.scratch_mark, 0, (size_t)e->cells);
    front[0] = grid_index(&w->grid, cx, cy);
    w->grid.scratch_mark[front[0]] = 1;
    int axis = 65 + (int)(expansion_map_hash(w->seed, index, 772) % 71);
    while (tail && count < area) {
        int chosen = 0, score = INT32_MAX;
        for (int q = 0; q < tail; q++) {
            int p = front[q], x = p % w->grid.width, y = p / w->grid.width, dx = x - cx, dy = y - cy;
            int noise = (int)(expansion_map_hash(w->seed + index * 97, x / 2, y / 2) % 101) - 50;
            int d = dx * dx * axis + dy * dy * (200 - axis) + noise * 12;
            if (d < score) {
                score = d;
                chosen = q;
            }
        }
        int p = front[chosen];
        front[chosen] = front[--tail];
        int x = p % w->grid.width, y = p / w->grid.width;
        if (!lake_cell(w, x, y))
            continue;
        picked[count++] = p;
        static const int dx[4] = {0, 1, 0, -1}, dy[4] = {-1, 0, 1, 0};
        for (int d = 0; d < 4; d++) {
            int xx = x + dx[d], yy = y + dy[d];
            if (!grid_in_bounds(&w->grid, xx, yy))
                continue;
            int n = grid_index(&w->grid, xx, yy);
            if (!w->grid.scratch_mark[n]) {
                w->grid.scratch_mark[n] = 1;
                front[tail++] = n;
            }
        }
    }
    if (count != area)
        return 0;
    int used[10] = {0}, overshoot = 0;
    for (int k = 0; k < count; k++)
        used[e->tiles[picked[k]].biome]++;
    for (int k = 0; k < 10; k++)
        if (used[k] > remaining[k])
            overshoot += used[k] - exp_clamp(remaining[k], 0, e->cells);
    if (overshoot > tolerance)
        return 0;
    for (int k = 0; k < 10; k++)
        remaining[k] -= used[k];
    for (int k = 0; k < count; k++) {
        int p = picked[k];
        CoreExpansionTileView *t = &e->tiles[p];
        t->biome = CORE_BIOME_LAKE;
        t->tree_kind = t->tree_age = t->tree_logs_milli = t->regrow_milli = 0;
        w->grid.terrain[p] = CORE_TERRAIN_WATER;
    }
    return 1;
}
static int stone_amount(const World *w, int p) {
    int factor = (int[]){6, 6, 6, 10, 5, 8, 7, 4, 5, 6}[w->expansion->tiles[p].biome];
    return (8000 + (int)(expansion_map_hash(w->seed, p % w->grid.width, p / w->grid.width) % 10001)) *
           factor / 6;
}
static int legacy_budget(World *w, int64_t *extra_stone) {
    CoreNewGameParams params = {0};
    World *old = NULL;
    strcpy(params.castle_name, u8"구지도 광맥 원장");
    params.ruleset = 1;
    params.seed = w->seed;
    params.difficulty = w->difficulty;
    int rc = world_create_generation(&params, NULL, 10, &old);
    if (rc != CORE_OK)
        return rc;
    int64_t totals[15] = {0};
    int counts[15] = {0};
    *extra_stone = 0;
    for (int p = 0; p < old->expansion->cells; p++) {
        const CoreExpansionTileView *t = &old->expansion->tiles[p];
        int base = stone_amount(old, p);
        totals[CORE_RES_STONE] += t->stone_milli;
        *extra_stone += t->stone_milli - base;
        counts[CORE_RES_STONE] += t->stone_milli > base;
        if (t->ore_kind >= 0 && t->ore_milli > 0) {
            totals[t->ore_kind] += t->ore_milli;
            counts[t->ore_kind]++;
        }
    }
    for (int r = 0; r < 15; r++) {
        if (totals[r] > INT32_MAX) {
            world_destroy(old);
            return CORE_ERR_INVALID_ARG;
        }
        w->expansion->ore_budget_milli[r] = (int)totals[r];
        w->expansion->ore_old_tiles[r] = counts[r];
    }
    world_destroy(old);
    return CORE_OK;
}
static int ore_allowed(const World *w, int x, int y, int resource) {
    if (x < 2 || y < 2 || x >= w->grid.width - 2 || y >= w->grid.height - 2)
        return 0;
    int p = grid_index(&w->grid, x, y), iron = grid_index(&w->grid, w->keep_x + 20, w->keep_y - 4);
    const CoreExpansionTileView *t = &w->expansion->tiles[p];
    if (resource == CORE_RES_STONE)
        return 1;
    return w->grid.terrain[p] != CORE_TERRAIN_WATER && (t->ore_kind < 0 || t->ore_kind == resource) &&
           (p != iron || resource == CORE_RES_IRON_ORE);
}
static void ore_network(World *w, int resource, int target, int budget) {
    ExpansionState *e = w->expansion;
    int *mask = e->route_distance, count = 0;
    memset(mask, 0, (size_t)e->cells * sizeof(int));
    int rare = resource == CORE_RES_GOLD_ORE || resource == CORE_RES_MANA;
    static const int dx[4] = {0, 1, 0, -1}, dy[4] = {-1, 0, 1, 0};
    Rng stream;
    rng_seed(&stream, (uint32_t)w->seed, 991u + (unsigned)resource);
    for (int group = 0; group < 400 && count < target; group++) {
        int x = rng_range(&stream, 3, w->grid.width - 4), y = rng_range(&stream, 3, w->grid.height - 4);
        for (int trial = 0; trial < 40; trial++) {
            int biome = e->tiles[grid_index(&w->grid, x, y)].biome;
            int preferred = resource == CORE_RES_MANA ? (biome == 2 || biome == 8 || biome == 3)
                                                      : (biome == 3 || biome == 5);
            if (preferred || rng_range(&stream, 0, 3) == 0)
                break;
            x = rng_range(&stream, 3, w->grid.width - 4);
            y = rng_range(&stream, 3, w->grid.height - 4);
        }
        int length = rng_range(&stream, rare ? 7 : 12, rare ? 20 : 30), direction = rng_range(&stream, 0, 3);
        for (int step = 0; step < length && count < target; step++) {
            int width = rare ? (step % 3 == 0 ? 1 : 0) : 1;
            for (int yy = y - width; yy <= y + width && count < target; yy++)
                for (int xx = x - width; xx <= x + width && count < target; xx++) {
                    if (!ore_allowed(w, xx, yy, resource) || exp_dist(xx, yy, x, y) > width)
                        continue;
                    int p = grid_index(&w->grid, xx, yy);
                    if (!mask[p]) {
                        mask[p] = xx == x && yy == y ? 3 : 1;
                        count++;
                    }
                }
            if (step % 4 == 0)
                direction = (direction + (rng_range(&stream, 0, 1) ? 1 : 3)) % 4;
            int xx = x + dx[direction], yy = y + dy[direction];
            if (xx > 2 && yy > 2 && xx < w->grid.width - 3 && yy < w->grid.height - 3) {
                x = xx;
                y = yy;
            } else
                direction = (direction + 2) % 4;
        }
    }
    int reserved = resource == CORE_RES_STONE      ? grid_index(&w->grid, w->keep_x - 10, w->keep_y - 8)
                   : resource == CORE_RES_IRON_ORE ? grid_index(&w->grid, w->keep_x + 20, w->keep_y - 4)
                                                   : -1;
    if (reserved >= 0) {
        mask[reserved] = 0;
        CoreExpansionTileView *t = &e->tiles[reserved];
        int amount = resource == CORE_RES_STONE ? 750000 : 500000;
        if (amount > budget)
            amount = budget;
        if (resource == CORE_RES_STONE)
            t->stone_milli += amount;
        else {
            t->ore_kind = resource;
            t->ore_milli = amount;
        }
        budget -= amount;
    }
    int sum = 0;
    for (int p = 0; p < e->cells; p++)
        sum += mask[p];
    if (!sum || budget <= 0)
        return;
    int used = 0;
    for (int p = 0; p < e->cells; p++)
        if (mask[p]) {
            int amount = (int)((int64_t)budget * mask[p] / sum);
            used += amount;
            CoreExpansionTileView *t = &e->tiles[p];
            if (resource == CORE_RES_STONE)
                t->stone_milli += amount;
            else {
                t->ore_kind = resource;
                t->ore_milli = amount;
            }
        }
    int residual = budget - used;
    for (int p = 0; residual > 0 && p < e->cells; p++)
        if (mask[p]) {
            if (resource == CORE_RES_STONE)
                e->tiles[p].stone_milli++;
            else
                e->tiles[p].ore_milli++;
            residual--;
        }
}
void expansion_clay_initialize(World *w) {
    ExpansionState *e = w->expansion;
    if (e->clay_initialized)
        return;
    static const int lower[10] = {130, 115, 95, 55, 360, 60, 40, 0, 480, 115},
                     upper[10] = {190, 175, 155, 95, 520, 100, 80, 0, 680, 175};
    for (int p = 0; p < e->cells; p++) {
        CoreExpansionTileView *t = &e->tiles[p];
        int biome = t->biome, shore = 100;
        if (w->grid.terrain[p] == CORE_TERRAIN_WATER) {
            t->clay_milli = t->clay_initial_milli = 0;
            t->clay_shore_percent = 100;
            continue;
        }
        int x = p % w->grid.width, y = p / w->grid.width;
        for (int yy = y - 2; yy <= y + 2; yy++)
            for (int xx = x - 2; xx <= x + 2; xx++)
                if (grid_in_bounds(&w->grid, xx, yy) &&
                    w->grid.terrain[grid_index(&w->grid, xx, yy)] == CORE_TERRAIN_WATER) {
                    int d = exp_dist(x, y, xx, yy);
                    if (d == 1)
                        shore = 140;
                    else if (d == 2 && shore < 120)
                        shore = 120;
                }
        int base = lower[biome] + (int)(expansion_map_hash(w->seed, x + 771, y + 1103) %
                                        (unsigned)(upper[biome] - lower[biome] + 1));
        t->clay_initial_milli = t->clay_milli = ((base * shore + 50) / 100) * 1000;
        t->clay_shore_percent = shore;
    }
    e->clay_initialized = 1;
}
void expansion_v123_generate(World *w) {
    ExpansionState *e = w->expansion;
    static const int shares[10] = {23, 18, 12, 12, 9, 7, 5, 0, 5, 9};
    int remaining[10] = {0};
    for (int p = 0; p < e->cells; p++)
        remaining[e->tiles[p].biome]++;
    for (int k = 0; k < 10; k++)
        remaining[k] -= (e->cells - 2304) * shares[k] / 100;
    for (int k = 0; k < 20; k++) {
        int area = k < 2 ? 322 : k < 12 ? 130 : 45, placed = 0;
        for (int trial = 0; trial < 2000 && !placed; trial++) {
            uint32_t n = expansion_map_hash(w->seed, k * 2003 + trial, 8891);
            int x = 3 + (int)(n % (unsigned)(w->grid.width - 6)),
                y = 3 + (int)((n >> 16) % (unsigned)(w->grid.height - 6));
            if (lake_cell(w, x, y))
                placed = grow_lake(w, x, y, area, k, remaining, trial < 1800 ? area / 10 + 8 : area);
        }
        if (!placed) {
            e->error = CORE_ERR_INVALID_ARG;
            return;
        }
    }
    for (int p = 0; p < e->cells; p++) {
        e->tiles[p].stone_milli = stone_amount(w, p);
        e->tiles[p].ore_kind = -1;
        e->tiles[p].ore_milli = 0;
    }
    int64_t stone = 0;
    int rc = legacy_budget(w, &stone);
    if (rc != CORE_OK) {
        e->error = rc;
        return;
    }
    const int resources[5] = {CORE_RES_STONE, CORE_RES_COAL, CORE_RES_IRON_ORE, CORE_RES_GOLD_ORE,
                              CORE_RES_MANA};
    for (int k = 0; k < 5; k++) {
        int resource = resources[k], target = e->ore_old_tiles[resource] * (k >= 3 ? 150 : 120) / 100 + 8;
        if (target > e->cells / 3)
            target = e->cells / 3;
        ore_network(w, resource, target,
                    resource == CORE_RES_STONE ? (int)stone : e->ore_budget_milli[resource]);
    }
    for (int p = 0; p < e->cells; p++) {
        CoreExpansionTileView *t = &e->tiles[p];
        if (t->ore_milli > 0 || t->stone_milli > 50000)
            t->rock_variant = 1 + (int)(expansion_map_hash(w->seed, p, 21) % 3);
    }
    expansion_water_bodies(w);
    expansion_clay_initialize(w);
    expansion_v122_numbers(w);
    e->market_next_restock_day = 5;
    e->loaded_version = 11;
}
CORE_API int32_t core_expansion_clay_tile(const CoreWorld *w, int32_t x, int32_t y, int32_t *current,
                                          int32_t *initial, int32_t *shore) {
    if (!w || !current || !initial || !shore)
        return CORE_ERR_NULL_ARG;
    if (!w->expansion || !grid_in_bounds(&w->grid, x, y))
        return CORE_ERR_INVALID_ARG;
    const CoreExpansionTileView *t = &w->expansion->tiles[grid_index(&w->grid, x, y)];
    *current = t->clay_milli;
    *initial = t->clay_initial_milli;
    *shore = t->clay_shore_percent;
    return CORE_OK;
}
