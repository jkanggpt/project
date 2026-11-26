/* resource_budget — 광물 총량의 고정 표본 투영. 플레이 가능한 별도 월드는 생성하지 않습니다. */
#include "expansion.h"
#include "mem.h"
#include <stdlib.h>
#include <string.h>
#define BUDGET_SAMPLE_SIZE 192
typedef struct BudgetSampler {
    Grid grid;
    Rng rng;
    int seed, keep_x, keep_y, cells;
    CoreExpansionTileView *tiles;
} BudgetSampler;
static uint32_t budget_noise(int x, int y, int seed) {
    uint32_t n = (uint32_t)x * 0x9e3779b9u ^ (uint32_t)y * 0x85ebca6bu ^ (uint32_t)seed;
    n ^= n >> 16;
    n *= 0x7feb352du;
    n ^= n >> 15;
    n *= 0x846ca68bu;
    return n ^ (n >> 16);
}
static int budget_biome(int x, int y, int seed) {
    static const int layout[16] = {5, 2, 3, 1, 7, 2, 1, 1, 0, 0, 0, 4, 6, 9, 7, 8};
    static const int weight[10] = {6, 6, 6, 13, 6, 6, 5, 4, 5, 9};
    if ((unsigned)seed & 1u)
        x = 191 - x;
    if ((unsigned)seed & 2u)
        y = 191 - y;
    if ((unsigned)seed & 4u) {
        int swap = x;
        x = y;
        y = swap;
    }
    int best = INT32_MAX, chosen = 0;
    for (int k = 0; k < 16; k++) {
        uint32_t n = budget_noise(k % 4, k / 4, seed);
        int cx = 24 + (k % 4) * 48 + (int)(n % 17) - 8;
        int cy = 24 + (k / 4) * 48 + (int)((n >> 8) % 17) - 8;
        int dx = x - cx, dy = y - cy, type = layout[k];
        int distance = (dx * dx + dy * dy) * 100 / weight[type];
        if (distance < best) {
            best = distance;
            chosen = type;
        }
    }
    return chosen;
}
static int sample_base_stone(const BudgetSampler *w, int p) {
    int weight = (int[]){6, 6, 6, 10, 5, 8, 7, 4, 5, 6}[w->tiles[p].biome];
    return (8000 + (int)(expansion_map_hash(w->seed, p % w->grid.width, p / w->grid.width) % 10001)) *
           weight / 6;
}
static void sample_deposits(BudgetSampler *w) {
    BudgetSampler *e = w;
    /* 연결 성분별로 호수를 배치하여 물의 수와 실제 수역 수가 일치하게 합니다. */
    for (int p = 0; p < e->cells; p++)
        if (w->grid.terrain[p] == CORE_TERRAIN_WATER) {
            w->grid.terrain[p] = CORE_TERRAIN_GRASS;
            e->tiles[p].biome = CORE_BIOME_GRASS;
        }
    static const int positions[7][2] = {{1, 1}, {3, 1}, {5, 1}, {1, 3}, {5, 3}, {1, 5}, {5, 5}};
    static const int radii[7] = {4, 5, 6, 7, 8, 10, 12};
    for (int k = 0; k < 7; k++) {
        uint32_t n = expansion_map_hash(w->seed, k, 701);
        int cx = positions[k][0] * w->grid.width / 6 + (int)(n % 9) - 4;
        int cy = positions[k][1] * w->grid.height / 6 + (int)((n >> 8) % 9) - 4, r = radii[k];
        for (int y = cy - r - 2; y <= cy + r + 2; y++)
            for (int x = cx - r - 2; x <= cx + r + 2; x++) {
                if (!grid_in_bounds(&w->grid, x, y) || exp_dist(x, y, w->keep_x, w->keep_y) < 12)
                    continue;
                int dx = x - cx, dy = y - cy;
                int wave = (int)(expansion_map_hash(w->seed, x / 3 + k * 31, y / 3) % 9) - 4;
                if (dx * dx * 100 + dy * dy * 135 > r * r * 100 + wave * r * 12)
                    continue;
                int p = grid_index(&w->grid, x, y);
                if (w->grid.occupant[p])
                    continue;
                e->tiles[p].biome = CORE_BIOME_LAKE;
                e->tiles[p].tree_kind = 0;
                e->tiles[p].tree_logs_milli = 0;
                e->tiles[p].tree_age = 0;
                e->tiles[p].regrow_milli = 0;
                w->grid.terrain[p] = CORE_TERRAIN_WATER;
            }
    }
    for (int p = 0; p < e->cells; p++) {
        e->tiles[p].stone_milli = sample_base_stone(w, p);
        e->tiles[p].ore_kind = -1;
        e->tiles[p].ore_milli = 0;
    }
    static const int resources[5] = {CORE_RES_STONE, CORE_RES_COAL, CORE_RES_IRON_ORE, CORE_RES_GOLD_ORE,
                                     CORE_RES_MANA};
    static const int lengths[5][2] = {{12, 38}, {9, 30}, {8, 26}, {4, 14}, {3, 12}},
                     widths[5] = {3, 2, 2, 1, 1}, clusters[5] = {40, 34, 34, 16, 20};
    static const int weights[5][10] = {{6, 6, 6, 10, 5, 8, 7, 3, 5, 6},
                                       {3, 4, 6, 8, 4, 5, 2, 0, 3, 3},
                                       {3, 4, 5, 9, 2, 6, 4, 0, 2, 3},
                                       {1, 1, 2, 7, 1, 4, 3, 0, 1, 2},
                                       {1, 3, 4, 5, 3, 3, 1, 0, 4, 2}};
    static const int dx[4] = {0, 1, 0, -1}, dy[4] = {-1, 0, 1, 0};
    for (int kind = 0; kind < 5; kind++)
        for (int c = 0; c < clusters[kind]; c++) {
            int x = 0, y = 0;
            for (int trial = 0; trial < 100; trial++) {
                x = rng_range(&w->rng, 3, w->grid.width - 4);
                y = rng_range(&w->rng, 3, w->grid.height - 4);
                if (rng_range(&w->rng, 0, 9) < weights[kind][e->tiles[grid_index(&w->grid, x, y)].biome])
                    break;
            }
            int length = rng_range(&w->rng, lengths[kind][0], lengths[kind][1]),
                direction = rng_range(&w->rng, 0, 3);
            for (int step = 0; step < length; step++) {
                int r =
                    kind == 0 ? rng_range(&w->rng, 1, widths[kind]) : rng_range(&w->rng, 0, widths[kind] - 1);
                for (int yy = y - r; yy <= y + r; yy++)
                    for (int xx = x - r; xx <= x + r; xx++) {
                        if (!grid_in_bounds(&w->grid, xx, yy) || exp_dist(xx, yy, x, y) > r)
                            continue;
                        CoreExpansionTileView *t = &e->tiles[grid_index(&w->grid, xx, yy)];
                        int amount = (int[]){45000, 30000, 24000, 7000, 5000}[kind] *
                                     (850 + (int)(expansion_map_hash(w->seed, xx, yy) % 301)) / 1000;
                        if (kind == 0)
                            t->stone_milli += amount;
                        else if (t->biome != CORE_BIOME_LAKE &&
                                 (t->ore_kind < 0 || t->ore_kind == resources[kind])) {
                            t->ore_kind = resources[kind];
                            t->ore_milli += amount;
                        }
                    }
                if (step % 3 == 0)
                    direction = (direction + (rng_range(&w->rng, 0, 2) == 0 ? 1 : 0)) % 4;
                int xx = x + dx[direction], yy = y + dy[direction];
                if (xx > 2 && yy > 2 && xx < w->grid.width - 3 && yy < w->grid.height - 3) {
                    x = xx;
                    y = yy;
                } else
                    direction = (direction + 2) % 4;
            }
        }
    int stone = grid_index(&w->grid, w->keep_x - 10, w->keep_y - 8),
        iron = grid_index(&w->grid, w->keep_x + 20, w->keep_y - 4);
    e->tiles[stone].stone_milli += 750000;
    e->tiles[iron].ore_kind = CORE_RES_IRON_ORE;
    e->tiles[iron].ore_milli += 500000;
}

int expansion_resource_budget(World *w, int64_t *extra_stone) {
    BudgetSampler sample = {0};
    sample.seed = w->seed;
    sample.keep_x = sample.keep_y = BUDGET_SAMPLE_SIZE / 2;
    sample.cells = BUDGET_SAMPLE_SIZE * BUDGET_SAMPLE_SIZE;
    rng_seed(&sample.rng, (uint32_t)sample.seed, 0x4D43u);
    int rc = grid_alloc(&sample.grid, BUDGET_SAMPLE_SIZE, BUDGET_SAMPLE_SIZE);
    if (rc != CORE_OK)
        return rc;
    rc = grid_generate(&sample.grid, (uint32_t)sample.seed, &sample.rng, sample.keep_x, sample.keep_y);
    sample.tiles = mem_calloc((size_t)sample.cells, sizeof(*sample.tiles));
    if (rc != CORE_OK || !sample.tiles) {
        grid_free(&sample.grid);
        mem_free(sample.tiles);
        return rc != CORE_OK ? rc : CORE_ERR_OUT_OF_MEMORY;
    }
    for (int p = 0; p < sample.cells; p++) {
        int x = p % BUDGET_SAMPLE_SIZE, y = p / BUDGET_SAMPLE_SIZE;
        int biome = budget_biome(x, y, sample.seed);
        if ((exp_dist(x, y, sample.keep_x, sample.keep_y) < 10 && biome == CORE_BIOME_LAKE) ||
            (abs(x - sample.keep_x) <= 3 && abs(y - sample.keep_y) <= 3))
            biome = CORE_BIOME_GRASS;
        if (x >= sample.keep_x + 10 && x < sample.keep_x + 15 && abs(y - sample.keep_y) <= 2 &&
            biome == CORE_BIOME_LAKE)
            biome = CORE_BIOME_GRASS;
        if (x == sample.keep_x + 4 && y == sample.keep_y + 2)
            biome = CORE_BIOME_GRASS;
        sample.tiles[p].biome = biome;
        sample.grid.terrain[p] = biome == CORE_BIOME_LAKE ? CORE_TERRAIN_WATER : CORE_TERRAIN_GRASS;
    }
    sample_deposits(&sample);
    int64_t totals[CORE_RES_COUNT] = {0};
    int counts[CORE_RES_COUNT] = {0};
    *extra_stone = 0;
    for (int p = 0; p < sample.cells; p++) {
        CoreExpansionTileView *t = &sample.tiles[p];
        int base = sample_base_stone(&sample, p);
        totals[CORE_RES_STONE] += t->stone_milli;
        *extra_stone += t->stone_milli - base;
        counts[CORE_RES_STONE] += t->stone_milli > base;
        if (t->ore_kind >= 0 && t->ore_milli > 0) {
            totals[t->ore_kind] += t->ore_milli;
            counts[t->ore_kind]++;
        }
    }
    for (int r = 0; r < CORE_RES_COUNT; r++) {
        if (totals[r] > INT32_MAX) {
            rc = CORE_ERR_INVALID_ARG;
            break;
        }
        w->expansion->ore_budget_milli[r] = (int)totals[r];
        w->expansion->ore_reference_tiles[r] = counts[r];
    }
    mem_free(sample.tiles);
    grid_free(&sample.grid);
    return rc;
}
