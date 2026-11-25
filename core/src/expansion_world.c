/* expansion_world.c — 확장 월드·생물군계·자원 은행과 저빈도 시계 */
#include "expansion.h"
#include "mem.h"
#include "nature.h"
#include "placement.h"
#include "population.h"
#include "rank.h"
#include <stdlib.h>
#include <string.h>

int expansion_allocate(World *w) {
    if (w->expansion)
        return CORE_OK;
    w->expansion = (ExpansionState *)mem_calloc(1, sizeof(*w->expansion));
    if (!w->expansion)
        return CORE_ERR_OUT_OF_MEMORY;
    for (int r = 0; r < 30; r++)
        w->expansion->trade_remainder[r] = 500000;
    w->expansion->tax = 2;
    w->expansion->tax_day = -1;
    w->expansion->dirty = 1;
    w->expansion->next_person_id = 1;
    w->expansion->next_animal_id = 1;
    w->expansion->immigration_day = 7;
    w->expansion->candidate_day = -1;
    if (!w->defs.expanded)
        expansion_fill_definitions(&w->defs, 1);
    memcpy(w->base_buildings, w->defs.buildings, sizeof(w->base_buildings));
    return expansion_setup_research(w);
}
int expansion_resize(World *w) {
    ExpansionState *e = w->expansion;
    size_t cells = (size_t)w->grid.width * w->grid.height;
    if (!e || !cells || cells > 256 * 256)
        return CORE_ERR_INVALID_ARG;
    if (e->cells == (int)cells)
        return CORE_OK;
    if (e->cells)
        return CORE_ERR_INVALID_ARG;
    e->tiles = (CoreExpansionTileView *)mem_calloc(cells, sizeof(*e->tiles));
    e->visible = (uint8_t *)mem_calloc(cells, 1);
    e->static_visible = (uint8_t *)mem_calloc(cells, 1);
    e->work_visible = (uint8_t *)mem_calloc(cells, 1);
    e->protected_cells = (uint8_t *)mem_calloc(cells, 1);
    e->connected = (uint8_t *)mem_calloc(cells, 1);
    e->distance = (int32_t *)mem_calloc(cells, sizeof(int32_t));
    e->queue = (int32_t *)mem_calloc(cells, sizeof(int32_t));
    e->water_area = (int32_t *)mem_calloc(cells, sizeof(int32_t));
    if (!e->tiles || !e->visible || !e->static_visible || !e->work_visible || !e->protected_cells || !e->connected ||
        !e->distance || !e->queue || !e->water_area)
        return CORE_ERR_OUT_OF_MEMORY;
    e->water_cold = (uint8_t *)mem_calloc(cells, 1);
    e->region_map = (uint8_t *)mem_calloc(cells, 1);
    e->route_distance = (int32_t *)mem_calloc(cells, sizeof(int32_t));
    e->route_heap = (int32_t *)mem_calloc(cells, sizeof(int32_t));
    e->route_heap_pos = (int32_t *)mem_calloc(cells, sizeof(int32_t));
    if (!e->water_cold || !e->region_map || !e->route_distance || !e->route_heap || !e->route_heap_pos)
        return CORE_ERR_OUT_OF_MEMORY;
    e->cells = (int)cells;
    return CORE_OK;
}
void expansion_free(World *w) {
    ExpansionState *e = w->expansion;
    if (!e)
        return;
    mem_free(e->tiles);
    mem_free(e->visible);
    mem_free(e->static_visible);
    mem_free(e->work_visible);
    mem_free(e->protected_cells);
    mem_free(e->connected);
    mem_free(e->distance);
    mem_free(e->queue);
    mem_free(e->water_area);
    mem_free(e->water_cold);
    mem_free(e->region_map);
    mem_free(e->route_distance);
    mem_free(e->route_heap);
    mem_free(e->route_heap_pos);
    mem_free(e);
    w->expansion = NULL;
}
static uint32_t terrain_noise(int x, int y, int seed) {
    uint32_t n = (uint32_t)x * 0x9e3779b9u ^ (uint32_t)y * 0x85ebca6bu ^ (uint32_t)seed;
    n ^= n >> 16;
    n *= 0x7feb352du;
    n ^= n >> 15;
    n *= 0x846ca68bu;
    return n ^ (n >> 16);
}
static int biome_at(int x, int y, int seed) {
    static const int layout[16] = {5,2,3,1,7,2,1,1,0,0,0,4,6,9,7,8};
    static const int weight[10] = {6,6,6,13,6,6,5,4,5,9};
    if ((unsigned)seed & 1u) x = 191 - x;
    if ((unsigned)seed & 2u) y = 191 - y;
    if ((unsigned)seed & 4u) { int swap = x; x = y; y = swap; }
    int best = INT32_MAX, chosen = 0;
    for (int k = 0; k < 16; k++) {
        uint32_t n = terrain_noise(k % 4, k / 4, seed);
        int cx = 24 + (k % 4) * 48 + (int)(n % 17) - 8;
        int cy = 24 + (k / 4) * 48 + (int)((n >> 8) % 17) - 8;
        int dx = x - cx, dy = y - cy, type = layout[k];
        int distance = (dx * dx + dy * dy) * 100 / weight[type];
        if (distance < best) { best = distance; chosen = type; }
    }
    return chosen;
}
static void set_tree(CoreExpansionTileView *t, int kind, int mature) {
    static const int logs[4] = {12, 28, 50, 24};
    t->tree_kind = kind;
    t->tree_age = mature ? core_expansion_tree_mature_age(kind) : 1000;
    t->tree_logs_milli = logs[kind - 1] * 1000;
}
void expansion_water_bodies(World *w) {
    ExpansionState *e = w->expansion;
    int i;
    static const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
    e->water_count = 0;
    memset(e->water_area, 0, (size_t)e->cells * sizeof(int32_t));
    for (i = 0; i < e->cells; i++)
        e->tiles[i].water_id = 0;
    for (i = 0; i < e->cells; i++)
        if (w->grid.terrain[i] == CORE_TERRAIN_WATER && !e->tiles[i].water_id) {
            int head = 0, tail = 0, id = ++e->water_count;
            e->queue[tail++] = i;
            e->tiles[i].water_id = id;
            while (head < tail) {
                int p = e->queue[head++], k;
                for (k = 0; k < 4; k++) {
                    int x = p % w->grid.width + dx[k], y = p / w->grid.width + dy[k], n;
                    if (!grid_in_bounds(&w->grid, x, y))
                        continue;
                    n = grid_index(&w->grid, x, y);
                    if (w->grid.terrain[n] == CORE_TERRAIN_WATER && !e->tiles[n].water_id) {
                        e->tiles[n].water_id = id;
                        e->queue[tail++] = n;
                    }
                }
            }
            e->water_area[id - 1] = tail;
            memset(w->grid.scratch_mark, 0, (size_t)e->cells);
            int cold = 0, land = 0;
            for (int q = 0; q < tail; q++) {
                int p = e->queue[q], xx, yy;
                for (yy = p / w->grid.width - 2; yy <= p / w->grid.width + 2; yy++)
                    for (xx = p % w->grid.width - 2; xx <= p % w->grid.width + 2; xx++) {
                        int n, b;
                        if (!grid_in_bounds(&w->grid, xx, yy) ||
                            exp_dist(xx, yy, p % w->grid.width, p / w->grid.width) > 2)
                            continue;
                        n = grid_index(&w->grid, xx, yy);
                        if (w->grid.terrain[n] == CORE_TERRAIN_WATER || w->grid.scratch_mark[n])
                            continue;
                        w->grid.scratch_mark[n] = 1;
                        land++;
                        b = e->tiles[n].biome;
                        cold += b == CORE_BIOME_SNOW ? 2 : b == CORE_BIOME_TAIGA ? 1 : 0;
                    }
            }
            e->water_cold[id - 1] = (uint8_t)(land && cold * 10 >= land * 6);
        }
}
int expansion_new_world(World *w) {
    ExpansionState *e = w->expansion;
    int i, x, y, rc;
    if (w->generation_version >= 11) {
        int status = expansion_v123_biomes(w);
        if (status != CORE_OK)
            return status;
    }
    static const int starts[CORE_RES_COUNT] = {160, 5, 125, 0, 12, 5, 65, 0, 0, 0, 0, 0, 0, 0, 0};
    for (i = 0; i < e->cells; i++) {
        CoreExpansionTileView *t = &e->tiles[i];
        uint32_t n;
        int chance, near;
        x = i % w->grid.width;
        y = i / w->grid.width;
        n = terrain_noise(x, y, w->seed);
        if (w->generation_version < 11)
            t->biome = biome_at(x, y, w->seed);
        t->ore_kind = -1;
        near = exp_dist(x, y, w->keep_x, w->keep_y);
        if (near < 10 && t->biome == 7)
            t->biome = 0;
        w->grid.terrain[i] =
            t->biome == 7 ? CORE_TERRAIN_WATER : (t->biome == 3 ? CORE_TERRAIN_ROCK : CORE_TERRAIN_GRASS);
        chance = t->biome == 1   ? 65
                 : t->biome == 2 ? 75
                 : t->biome == 4 ? 20
                 : t->biome == 5 ? 15
                 : t->biome == 0 ? 8
                                 : 0;
        if ((int)(n % 100) < chance) {
            int kind = t->biome == 2 ? 2 : ((n >> 12) % 5 == 0 ? 1 : 3);
            if (t->biome == 2 && (n >> 20) % 16 == 0)
                kind = 4;
            set_tree(t, kind, (n >> 8) % 5 != 0);
            w->grid.terrain[i] = CORE_TERRAIN_FOREST;
        }
        if (t->biome != 7 && (n >> 16) % 1000 < (unsigned)(t->biome == 3 ? 90 : 5)) {
            static const int ores[5] = {CORE_RES_STONE, CORE_RES_IRON_ORE, CORE_RES_COAL, CORE_RES_GOLD_ORE,
                                        CORE_RES_MANA};
            static const int sizes[5][3] = {
                {250, 750, 1800}, {180, 500, 1200}, {150, 450, 1000}, {40, 110, 280}, {30, 90, 220}};
            int kind = (int)((n >> 5) % 100);
            kind = kind < 40 ? 0 : kind < 69 ? 1 : kind < 95 ? 2 : kind < 98 ? 3 : 4;
            t->ore_kind = ores[kind];
            t->ore_milli = sizes[kind][(n >> 22) % 3] * 1000;
        }
        if (abs(x - w->keep_x) <= 3 && abs(y - w->keep_y) <= 3) {
            memset(t, 0, sizeof(*t));
            t->ore_kind = -1;
            w->grid.terrain[i] = CORE_TERRAIN_GRASS;
        }
    }
    for (y = -2; y <= 2; y++)
        for (x = 0; x < 5; x++) {
            i = grid_index(&w->grid, w->keep_x + 10 + x, w->keep_y + y);
            if(e->tiles[i].biome==CORE_BIOME_LAKE)e->tiles[i].biome=CORE_BIOME_GRASS;
            set_tree(&e->tiles[i], 2, 1);
            w->grid.terrain[i] = CORE_TERRAIN_FOREST;
        }
    i = grid_index(&w->grid, w->keep_x - 10, w->keep_y - 8);
    e->tiles[i].ore_kind = CORE_RES_STONE;
    e->tiles[i].ore_milli = 750000;
    i = grid_index(&w->grid, w->keep_x + 20, w->keep_y - 4);
    e->tiles[i].ore_kind = CORE_RES_IRON_ORE;
    e->tiles[i].ore_milli = 500000;
    memcpy(w->bank.amount, starts, sizeof(starts));
    w->bank.individual_limits = 1;
    w->bank.capacity = 250;
    expansion_water_bodies(w);
    {
        Building *well = entity_add_building(w, CORE_BLD_WELL);
        const BuildingDef *d = world_building_def(w, CORE_BLD_WELL);
        if (!well)
            return CORE_ERR_OUT_OF_MEMORY;
        well->x = w->keep_x + 4;
        well->y = w->keep_y + 2;
        well->w = well->h = 1;
        well->door_x = well->x;
        well->door_y = well->y + 1;
        well->state = CORE_BSTATE_ACTIVE;
        well->hp = d->max_hp;
        well->completed_day = 1;
        i = grid_index(&w->grid, well->x, well->y);
        memset(&e->tiles[i], 0, sizeof(e->tiles[i]));
        e->tiles[i].ore_kind = -1;
        w->grid.terrain[i] = CORE_TERRAIN_GRASS;
    }
    if (w->generation_version < 11)
        expansion_legacy_generate(w);
    else
        expansion_v123_generate(w);
    if (e->error)
        return e->error;

    expansion_capacity(w);
    expansion_rebuild(w);
    expansion_ecology_init(w);
    for (int pass = 0; pass < 100; pass++)
        for (int region = 0; region < e->region_count; region++)
            if (e->regions[region].count < e->regions[region].target)
                expansion_animal_spawn(w, region, 1);
    e->candidate_day = -1;
    expansion_candidates(w);
    e->dirty = 1;
    rc = e->error;
    return rc ? rc : CORE_OK;
}
int expansion_bank_have(const World *w, int resource) {
    int64_t value = (int64_t)w->bank.amount[resource] * 1000 + w->expansion->fractional[resource];
    return value > INT32_MAX ? INT32_MAX : (int)value;
}
int expansion_bank_add(World *w, int resource, int amount, int category) {
    int64_t before = (int64_t)w->bank.amount[resource] * 1000 + w->expansion->fractional[resource],
            after = before + amount;
    int64_t limit = (int64_t)eco_bank_limit(&w->bank, resource) * 1000;
    int old = w->bank.amount[resource];
    if (after < 0)
        after = 0;
    if (amount > 0 && after > limit)
        after = before > limit ? before : limit;
    w->bank.amount[resource] = (int32_t)(after / 1000);
    w->expansion->fractional[resource] = (int32_t)(after % 1000);
    if (resource == CORE_RES_LOGS && w->expansion->magic_logs_milli > after)
        w->expansion->magic_logs_milli = (int32_t)after;
    w->ledger.today[resource][category] += w->bank.amount[resource] - old;
    return (int)(after - before);
}
int expansion_fuel(World *w, int requested, int consume) {
    static const int order[3] = {CORE_RES_COAL, CORE_RES_LOGS, CORE_RES_WOOD};
    int available = w->expansion->fuel_milli, i, need = requested;
    for (i = 0; i < 3; i++) {
        int64_t add = (int64_t)w->bank.amount[order[i]] * (i == 0 ? 2000 : 1000);
        available = add > INT32_MAX - available ? INT32_MAX : available + (int)add;
    }
    if (!consume)
        return available;
    if (need > available)
        need = available;
    {
        int target = need, credit = w->expansion->fuel_milli;
        for (i = 0; i < 3 && credit < target; i++) {
            int unit = i == 0 ? 2000 : 1000, take = (target - credit + unit - 1) / unit;
            if (take > w->bank.amount[order[i]])
                take = w->bank.amount[order[i]];
            (void)eco_change(&w->bank, &w->ledger, order[i], -take, CORE_LEDGER_HEATING);
            if (order[i] == CORE_RES_LOGS &&
                w->expansion->magic_logs_milli > expansion_bank_have(w, CORE_RES_LOGS))
                w->expansion->magic_logs_milli = expansion_bank_have(w, CORE_RES_LOGS);
            credit += take * unit;
        }
        w->expansion->fuel_milli = credit - target;
    }
    return need;
}
void expansion_capacity(World *w) {
    static const int base[CORE_RES_COUNT] = {
        ECO_MAX_AMOUNT, 200, 250, 150, 150, 100, 200, 100, 100, 100, 100, 100, 60, 60, 100};
    int r, i;
    memcpy(w->bank.limits, base, sizeof(base));
    for (i = 0; i < w->building_count; i++) {
        Building *b = w->buildings[i];
        if (b->state != CORE_BSTATE_ACTIVE)
            continue;
        if (b->type == CORE_BLD_GRANARY)
            w->bank.limits[CORE_RES_FOOD] += 650;
        if (b->type == CORE_BLD_WAREHOUSE || b->type == CORE_BLD_LOGISTICS) {
            int big = b->type == CORE_BLD_LOGISTICS;
            for (r = 1; r < CORE_RES_COUNT; r++)
                w->bank.limits[r] += r == CORE_RES_FOOD ? (big ? 2000 : 500) : (big ? 750 : 250);
        }
    }
    if (expansion_has_research(w, 6))
        for (r = 1; r < CORE_RES_COUNT; r++)
            w->bank.limits[r] = w->bank.limits[r] * 120 / 100;
}
static void expansion_goals(World *w) {
    int ready[5] = {0}, i, k, count = 0, changed = 0;
    for (i = 0; i < w->building_count; i++) {
        Building *b = w->buildings[i];
        if (b->state != CORE_BSTATE_ACTIVE)
            continue;
        if (b->type == CORE_BLD_GARDEN)
            ready[0] = 1;
        if (w->defs.buildings[b->type].resident_slots)
            ready[1] = 1;
    }
    for (i = 0; i < w->villager_count; i++)
        if (w->villagers[i]->workplace && (w->villagers[i]->flags & CORE_VF_MANUAL))
            ready[2] = 1;
    ready[3] = w->expansion->effective_roads >= 10;
    ready[4] = w->rank >= 1;
    for (i = 0; i < 5; i++)
        if (ready[i] && !(w->tutorial_done & (1 << i))) {
            w->tutorial_done |= 1 << i;
            changed = 1;
            count = 0;
            for (k = 0; k < 5; k++)
                count += (w->tutorial_done & (1 << k)) != 0;
            events_push_progress(&w->events, CORE_EVT_TUTORIAL_STEP, i, count, 5, NULL);
        }
    if (changed && w->tutorial_done == 31)
        events_push_progress(&w->events, CORE_EVT_TUTORIAL_DONE, 0, 0, 0, NULL);
}
void expansion_tick(World *w) {
    ExpansionState *e = w->expansion;
    int i, minute = exp_minute(w);
    if (!w->threat.raid.id)
        e->alarm = 0;
    for (i = 0; i < w->building_count; i++) {
        Building *b = w->buildings[i];
        if (b->state == CORE_BSTATE_CONSTRUCTING && !core_expansion_build_locked(w)) {
            if (++b->build_progress >= world_building_def(w, b->type)->build_ticks) {
                b->state = CORE_BSTATE_ACTIVE;
                b->completed_day = exp_day(w);
                e->dirty = 1;
                events_push_building(&w->events, CORE_EVT_BUILDING_COMPLETED, b->id, b->type, b->x, b->y);
            }
        }
    }
    if (e->dirty) {
        expansion_rebuild(w);
        expansion_capacity(w);
        expansion_assign(w);
        expansion_rebuild(w);
    }
    expansion_v122_numbers(w);
    if (!core_expansion_build_locked(w)) expansion_promotion_tick(w);
    expansion_merchant_tick(w);
    expansion_boats_tick(w);
    expansion_people_tick(w);
    if (minute % 60 == 0)
        for (int index = 0; index < w->building_count; index++)
            expansion_forecast_refresh(w, w->buildings[index]);
    expansion_animals_tick(w);
    if (minute == 0) {
        for (int region = 0; region < e->region_count; region++)
            e->region_hunt_days[region][exp_day(w) % 12] = 0;
        eco_roll_ledger(&w->ledger);
        nature_daily(w);
        expansion_growth(w);
        expansion_daily(w);
    }
    if (minute == 360) {
        expansion_people_daily(w);
        expansion_ecology_daily(w);
        expansion_market_daily(w);
        expansion_candidates(w);
    }
    if (minute == 480 && (exp_day(w) - 1) % 48 == 24)
        expansion_produce(w);
    expansion_collect_harvest(w);
    expansion_visibility(w);
    if (minute % 60 == 0)
        expansion_goals(w);
    expansion_raids_tick(w);
    if (w->villager_count > w->stats.peak_population)
        w->stats.peak_population = w->villager_count;
    if (!w->villager_count)
        rank_lose(w, CORE_LOSE_POPULATION);
    {
        Building *keep = world_keep(w);
        if (!keep || keep->state == CORE_BSTATE_RUINED)
            rank_lose(w, CORE_LOSE_KEEP);
    }
}
