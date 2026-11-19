/* expansion_debug.c — 밸런스 봇과 분리한 성능 검사 전용 영지 */
#include "expansion.h"
#include "population.h"
#include "threat.h"
#include <string.h>
static Building *debug_build(World *w, int type) {
    const ExpansionBuildingSpec *s = expansion_spec(type);
    for (int dy = -25; dy <= 25; dy += 5)
        for (int dx = -25; dx <= 25; dx += 5) {
            int x = w->keep_x + dx, y = w->keep_y + dy, valid = 1;
            if (exp_dist(x, y, w->keep_x, w->keep_y) < 8)
                continue;
            for (int yy = y; yy <= y + s->h; yy++)
                for (int xx = x; xx < x + s->w; xx++)
                    if (!grid_in_bounds(&w->grid, xx, yy) || w->grid.occupant[grid_index(&w->grid, xx, yy)])
                        valid = 0;
            if (!valid)
                continue;
            Building *b = entity_add_building(w, type);
            if (!b)
                return NULL;
            b->x = x;
            b->y = y;
            b->w = s->w;
            b->h = s->h;
            b->door_x = x + s->w / 2;
            b->door_y = y + s->h;
            b->state = CORE_BSTATE_ACTIVE;
            b->hp = s->hp;
            b->auto_assign = 1;
            for (int yy = y; yy < y + s->h; yy++)
                for (int xx = x; xx < x + s->w; xx++) {
                    int p = grid_index(&w->grid, xx, yy);
                    w->grid.occupant[p] = b->id;
                    w->grid.terrain[p] = CORE_TERRAIN_GRASS;
                    w->expansion->tiles[p].biome = 0;
                    w->expansion->tiles[p].tree_kind = 0;
                    w->expansion->tiles[p].tree_logs_milli = 0;
                }
            return b;
        }
    return NULL;
}
CORE_API int32_t core_expansion_debug_wildlife(CoreWorld *w, int32_t count) {
    if (!w)
        return CORE_ERR_NULL_ARG;
    if (!w->expansion || count < 0 || count > 300)
        return CORE_ERR_INVALID_ARG;
    ExpansionState *e = w->expansion;
    int wild = 0;
    static const int kinds[9] = {0, 1, 2, 4, 5, 17, 20, 22, 23};
    for (int i = 0; i < e->animal_count; i++)
        if (!e->animals[i].owner)
            e->animals[i].alive = 0;
    while (wild < count) {
        int i, x = w->keep_x, y = w->keep_y, p, attempt;
        for (attempt = 0; attempt < 256; attempt++) {
            x = w->keep_x + rng_range(&w->rng, -24, 24);
            y = w->keep_y + rng_range(&w->rng, -24, 24);
            p = grid_index(&w->grid, x, y);
            if (w->grid.cost[p] && !w->grid.block[p])
                break;
        }
        if (attempt == 256)
            return CORE_ERR_REJECTED;
        for (i = 0; i < e->animal_count; i++)
            if (!e->animals[i].alive)
                break;
        if (i == e->animal_count) {
            if (i >= CORE_EXP_MAX_ANIMALS)
                return CORE_ERR_REJECTED;
            e->animal_count++;
        }
        CoreAnimalView *a = &e->animals[i];
        memset(a, 0, sizeof(*a));
        a->kind = kinds[wild % 9];
        a->id = e->next_animal_id++;
        a->alive = 1;
        a->age = 120;
        a->hp = EXP_ANIMALS[a->kind].hp;
        a->x_milli = x * 1000 + 500;
        a->y_milli = y * 1000 + 500;
        wild++;
    }
    expansion_ecology_recount(w);
    return CORE_OK;
}
int expansion_debug_town(World *w, int villagers, int enemies) {
    if (villagers < 0 || villagers > 300 || enemies < 0 || enemies > 64)
        return CORE_ERR_INVALID_ARG;
    if (w->villager_count < villagers) {
        w->rank = 3;
        for (int i = 0; i < w->expansion->cells; i++)
            w->expansion->tiles[i].explored = 1;
        for (int i = 0; i < (villagers + 7) / 8; i++)
            if (!debug_build(w, CORE_BLD_MANSION))
                return CORE_ERR_OUT_OF_MEMORY;
        for (int i = 0; i < 12; i++)
            if (!debug_build(w, i % 2 ? CORE_BLD_GARDEN : CORE_BLD_LOGGING_SITE))
                return CORE_ERR_OUT_OF_MEMORY;
        for (int i = 0; i < 4; i++)
            if (!debug_build(w, CORE_BLD_LOGISTICS))
                return CORE_ERR_OUT_OF_MEMORY;
        Building *ranch = debug_build(w, CORE_BLD_RANCH), *pasture = debug_build(w, CORE_BLD_PASTURE);
        if (!ranch || !pasture)
            return CORE_ERR_OUT_OF_MEMORY;
        for (int y = w->keep_y - 28; y <= w->keep_y + 28; y++)
            for (int x = w->keep_x - 28; x <= w->keep_x + 28; x++) {
                int p = grid_index(&w->grid, x, y);
                if (!w->grid.occupant[p] && w->grid.terrain[p] != CORE_TERRAIN_WATER)
                    w->grid.road[p] = 1;
            }
        while (w->villager_count < villagers)
            if (!population_create(w, w->keep_x, w->keep_y + 3))
                return CORE_ERR_OUT_OF_MEMORY;
        expansion_rebuild(w);
        expansion_assign(w);
        expansion_capacity(w);
        for (int r = 0; r < 15; r++)
            w->bank.amount[r] = r == CORE_RES_GOLD ? 100000 : eco_bank_limit(&w->bank, r);
        static const int kinds[4] = {7, 9, 8, 10};
        for (int i = 0; i < 4; i++) {
            core_expansion_buy_animal(w, ranch->id, kinds[i]);
            core_expansion_buy_animal(w, pasture->id, kinds[i]);
            core_expansion_buy_animal(w, pasture->id, kinds[i]);
        }
        expansion_visibility(w);
        if (core_expansion_debug_wildlife(w, 180) != CORE_OK)
            return CORE_ERR_REJECTED;
    }
    return enemies > 0 ? threat_debug_raid(w, CORE_THREAT_MONSTERS, CORE_DIR_NORTH, enemies) : CORE_OK;
}
