/* inspect_worlds.c — 현재 지도·호수·광물과 결정론 골든 검사 */
#include "expansion.h"
#include "platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
static uint64_t fingerprint(uint64_t h, const void *data, size_t n) {
    const unsigned char *p = data;
    for (size_t i = 0; i < n; i++) {
        h ^= p[i];
        h *= UINT64_C(1099511628211);
    }
    return h;
}
int main(int argc, char **argv) {
    int seeds = argc > 1 ? atoi(argv[1]) : 1000;
    if (seeds < 1 || seeds > 1000)
        return 2;
    int errors = 0, targets = 0;
    clock_t begin = clock();
    puts("seed,water,lakes,small,medium,large,grass_bp,forest_bp,taiga_bp,mountain_bp,wetland_bp,snow_bp,"
         "desert_bp,swamp_bp,savanna_bp,grass_patches,forest_patches,grass_largest_pct,forest_largest_pct,"
         "mid_biomes,gold_tiles,reference_gold_tiles,mana_tiles,reference_mana_tiles,hard_errors,target_"
         "misses");
    for (int seed = 1; seed <= seeds; seed++) {
        CoreNewGameParams params = {0};
        World *w = NULL;
        strcpy(params.castle_name, u8"지도 통계");
        params.seed = seed;
        params.ruleset = 1;
        params.difficulty = 0;
        if (core_create(&params, NULL, &w) != CORE_OK) {
            errors++;
            continue;
        }
        ExpansionState *e = w->expansion;
        Grid *g = &w->grid;
        int counts[10] = {0}, patches[10] = {0}, largest[10] = {0}, ore[15] = {0}, water = 0, bad = 0,
            miss = 0;
        int64_t total[15] = {0};
        for (int p = 0; p < e->cells; p++) {
            CoreExpansionTileView *t = &e->tiles[p];
            counts[t->biome]++;
            water += g->terrain[p] == CORE_TERRAIN_WATER;
            total[CORE_RES_STONE] += t->stone_milli;
            if (t->ore_kind >= 0) {
                total[t->ore_kind] += t->ore_milli;
                ore[t->ore_kind] += t->ore_milli > 0;
            }
        }
        int small = 0, medium = 0, large = 0;
        for (int i = 0; i < e->water_count; i++) {
            small += e->water_area[i] == 45;
            medium += e->water_area[i] == 130;
            large += e->water_area[i] == 322;
        }
        memset(g->scratch_mark, 0, (size_t)e->cells);
        static const int dx[4] = {0, 1, 0, -1}, dy[4] = {-1, 0, 1, 0};
        for (int p = 0; p < e->cells; p++)
            if (!g->scratch_mark[p]) {
                int head = 0, tail = 0, kind = e->tiles[p].biome;
                e->queue[tail++] = p;
                g->scratch_mark[p] = 1;
                while (head < tail) {
                    int q = e->queue[head++], x = q % g->width, y = q / g->width;
                    for (int d = 0; d < 4; d++) {
                        int xx = x + dx[d], yy = y + dy[d];
                        if (!grid_in_bounds(g, xx, yy))
                            continue;
                        int n = grid_index(g, xx, yy);
                        if (!g->scratch_mark[n] && e->tiles[n].biome == kind) {
                            g->scratch_mark[n] = 1;
                            e->queue[tail++] = n;
                        }
                    }
                }
                if (tail >= 64) {
                    patches[kind]++;
                    if (tail > largest[kind])
                        largest[kind] = tail;
                }
            }
        for (int p = 0; p < e->cells; p++)
            e->route_distance[p] = -1;
        int head = 0, tail = 0;
        Building *keep = world_keep(w);
        int start = grid_index(g, keep->door_x, keep->door_y);
        e->route_distance[start] = 0;
        e->queue[tail++] = start;
        int mid[10] = {0}, distinct = 0;
        while (head < tail) {
            int p = e->queue[head++], x = p % g->width, y = p / g->width, dist = e->route_distance[p];
            if (dist >= 25 && dist <= 65)
                mid[e->tiles[p].biome] = 1;
            if (dist >= 65)
                continue;
            for (int d = 0; d < 4; d++) {
                int xx = x + dx[d], yy = y + dy[d];
                if (!grid_in_bounds(g, xx, yy))
                    continue;
                int q = grid_index(g, xx, yy);
                if (e->route_distance[q] < 0 && g->cost[q] && g->block[q] != GRID_BLOCK_WALL) {
                    e->route_distance[q] = dist + 1;
                    e->queue[tail++] = q;
                }
            }
        }
        for (int k = 0; k < 10; k++)
            if (k != 7)
                distinct += mid[k];
        static const int shares[10] = {23, 18, 12, 12, 9, 7, 5, 0, 5, 9};
        for (int k = 0; k < 10; k++)
            if (k != 7) {
                int actual = counts[k] * 10000 / (e->cells - water);
                miss += abs(actual - shares[k] * 100) > 200;
            }
        miss += patches[0] < 2 || largest[0] * 100 > counts[0] * 55;
        miss += patches[1] < 2 || largest[1] * 100 > counts[1] * 55;
        miss += distinct < 6;
        bad += g->width != 160 || water != 2304 || e->water_count != 20 || small != 8 || medium != 10 ||
               large != 2;
        const int kinds[5] = {CORE_RES_STONE, CORE_RES_COAL, CORE_RES_IRON_ORE, CORE_RES_GOLD_ORE,
                              CORE_RES_MANA};
        for (int k = 0; k < 5; k++)
            bad += total[kinds[k]] > e->ore_budget_milli[kinds[k]];
        bad += ore[CORE_RES_GOLD_ORE] * 100 < e->ore_reference_tiles[CORE_RES_GOLD_ORE] * 135 ||
               ore[CORE_RES_MANA] * 100 < e->ore_reference_tiles[CORE_RES_MANA] * 135;
        bad += core_validate(w) != 0;
        errors += bad;
        targets += miss;
        printf("%d,%d,%d,%d,%d,%d", seed, water, e->water_count, small, medium, large);
        for (int k = 0; k < 10; k++)
            if (k != 7)
                printf(",%d", counts[k] * 10000 / (e->cells - water));
        printf(",%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d", patches[0], patches[1], largest[0] * 100 / counts[0],
               largest[1] * 100 / counts[1], distinct, ore[CORE_RES_GOLD_ORE],
               e->ore_reference_tiles[CORE_RES_GOLD_ORE], ore[CORE_RES_MANA],
               e->ore_reference_tiles[CORE_RES_MANA], bad, miss);
        uint64_t hash = fingerprint(UINT64_C(14695981039346656037), g->terrain, (size_t)e->cells);
        hash = fingerprint(hash, e->tiles, (size_t)e->cells * sizeof(*e->tiles));
        int biomass = 0;
        for (int r = 0; r < e->region_count; r++)
            biomass += e->regions[r].biomass;
        printf(",%llu,%llu", (unsigned long long)hash, (unsigned long long)w->rng.state);
        for (int k = 0; k < 5; k++)
            printf(",%lld", (long long)total[kinds[k]]);
        printf(",%d\n", biomass);
        fflush(stdout);
        core_destroy(w);
    }
    fprintf(stderr, "worlds=%d hard_errors=%d proposal_target_misses=%d cpu_seconds=%.3f\n", seeds, errors,
            targets, (clock() - begin) / (double)CLOCKS_PER_SEC);
    return errors ? 1 : 0;
}
