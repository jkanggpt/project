/* expansion_ecology.c — 24종 정의, 연결 수역 온도와 지리 서식지의 생체량 */
#include "expansion.h"
#include <stdlib.h>
#include <string.h>

#define A(h, a, f, l, b, s, w, be, x, y, lv, n)                                                              \
    { h, a, f, l, b, s, w, be, x, y, lv, u8##n }
const CoreAnimalInfo EXP_ANIMALS[24] = {A(12, 0, 2, 0, 1, 1450, 0, 0, 400, 420, 0, "토끼"),
                                        A(30, 0, 10, 2, 3, 1300, 0, 0, 900, 1050, 0, "사슴"),
                                        A(28, 0, 7, 2, 2, 1050, 0, 0, 820, 920, 0, "산양"),
                                        A(10, 0, 2, 0, 1, 850, 1100, 0, 420, 480, 0, "오리"),
                                        A(22, 0, 3, 1, 2, 1250, 0, 1, 650, 600, 0, "여우"),
                                        A(85, 0, 19, 3, 4, 1000, 0, 0, 1350, 1500, 0, "낙타"),
                                        A(38, 0, 9, 3, 3, 650, 1800, 0, 950, 700, 0, "물개"),
                                        A(65, 0, 30, 6, 0, 700, 0, 0, 1250, 1150, 1, "소"),
                                        A(28, 0, 12, 3, 0, 900, 0, 0, 850, 850, 1, "양"),
                                        A(36, 0, 18, 3, 0, 850, 0, 0, 880, 780, 1, "돼지"),
                                        A(9, 0, 2, 0, 0, 1000, 0, 0, 360, 420, 1, "닭"),
                                        A(36, 5, 5, 2, 3, 1300, 0, 2, 950, 800, 0, "늑대"),
                                        A(120, 15, 20, 6, 5, 780, 0, 2, 1480, 1440, 0, "곰"),
                                        A(80, 9, 10, 4, 4, 800, 1250, 2, 1350, 750, 0, "악어"),
                                        A(95, 12, 22, 5, 5, 1120, 0, 2, 1450, 1200, 0, "물소"),
                                        A(16, 4, 1, 0, 1, 750, 0, 2, 750, 340, 0, "뱀"),
                                        A(105, 16, 12, 4, 4, 1250, 0, 2, 1300, 1050, 0, "사자"),
                                        A(180, 20, 25, 8, 6, 950, 0, 1, 1750, 1420, 0, "코뿔소"),
                                        A(6, 0, 1, 0, 1, 600, 1000, 0, 280, 280, 0, "개구리"),
                                        A(42, 7, 5, 2, 2, 1200, 0, 2, 900, 780, 0, "하이에나"),
                                        A(34, 7, 5, 2, 2, 1380, 0, 1, 780, 760, 0, "스라소니"),
                                        A(14, 0, 3, 1, 1, 620, 1500, 0, 480, 680, 0, "펭귄"),
                                        A(48, 0, 11, 2, 3, 1600, 0, 0, 900, 1650, 0, "타조"),
                                        A(12, 0, 3, 0, 1, 1100, 0, 0, 430, 460, 0, "꿩")};
#undef A
static const int WEIGHT[10][24] = {
    {33, 22, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 11, 0, 0, 0, 0, 0, 0, 0, 16, 18},
    {28, 28, 0, 0, 16, 0, 0, 0, 0, 0, 0, 0, 6, 0, 0, 0, 0, 0, 0, 0, 9, 0, 0, 13},
    {0, 18, 10, 0, 11, 0, 0, 0, 0, 0, 0, 30, 15, 0, 0, 0, 0, 0, 0, 0, 16, 0, 0, 0},
    {0, 10, 40, 0, 0, 0, 0, 0, 0, 0, 0, 17, 10, 0, 0, 12, 0, 0, 0, 0, 11, 0, 0, 0},
    {0, 0, 0, 30, 12, 0, 0, 0, 0, 0, 0, 0, 0, 13, 0, 13, 0, 0, 32, 0, 0, 0, 0, 0},
    {0, 0, 23, 0, 0, 0, 20, 0, 0, 0, 0, 21, 18, 0, 0, 0, 0, 0, 0, 0, 0, 18, 0, 0},
    {0, 0, 0, 0, 0, 40, 0, 0, 0, 0, 0, 0, 0, 0, 0, 28, 0, 0, 0, 20, 0, 0, 12, 0},
    {0, 0, 0, 40, 0, 0, 20, 0, 0, 0, 0, 0, 0, 14, 0, 0, 0, 0, 20, 0, 0, 6, 0, 0},
    {0, 0, 0, 12, 6, 0, 0, 0, 0, 0, 0, 0, 0, 32, 0, 24, 0, 0, 26, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 10, 0, 0, 0, 0, 0, 0, 0, 0, 25, 0, 14, 9, 0, 20, 0, 0, 22, 0}};
CORE_API int32_t core_expansion_animal_info(int32_t kind, CoreAnimalInfo *out) {
    if (!out)
        return CORE_ERR_NULL_ARG;
    if (kind < 0 || kind >= 24)
        return CORE_ERR_INVALID_ARG;
    *out = EXP_ANIMALS[kind];
    return CORE_OK;
}
CORE_API int32_t core_expansion_water(const CoreWorld *w, int32_t id, CoreWaterView *out) {
    if (!w || !out)
        return CORE_ERR_NULL_ARG;
    if (!w->expansion || id < 1 || id > w->expansion->water_count)
        return CORE_ERR_NOT_FOUND;
    out->id = id;
    out->area = w->expansion->water_area[id - 1];
    out->cold = w->expansion->water_cold[id - 1];
    out->capacity_milli = exp_clamp(3000 + out->area * 120, 8000, 70000) *
                          (exp_season(w) == 1   ? 110
                           : exp_season(w) == 3 ? 75
                                                : 100) /
                          100;
    return CORE_OK;
}
CORE_API int32_t core_expansion_habitats(const CoreWorld *w, CoreHabitatView *out, int32_t capacity) {
    if (!w || !out)
        return CORE_ERR_NULL_ARG;
    if (!w->expansion)
        return CORE_ERR_INVALID_ARG;
    if (capacity < w->expansion->region_count)
        return CORE_ERR_BUFFER_TOO_SMALL;
    memcpy(out, w->expansion->regions, (size_t)w->expansion->region_count * sizeof(*out));
    return w->expansion->region_count;
}
int expansion_animal_habitat(const World *w, int kind, int x, int y) {
    const ExpansionState *e = w->expansion;
    const CoreAnimalInfo *a = &EXP_ANIMALS[kind];
    int p = grid_index(&w->grid, x, y), water = e->tiles[p].water_id, biome = e->tiles[p].biome, xx, yy,
        near = 0;
    if (a->livestock)
        return 0;
    if (a->behavior == 2 && exp_day(w) <= 10 && exp_dist(x, y, w->keep_x, w->keep_y) < 12)
        return 0;
    if (water && !a->swim_milli)
        return 0;
    if (kind != 6 && kind != 13 && kind != 18 && kind != 21)
        return 1;
    if (water && ((kind == 6 || kind == 21) ? !e->water_cold[water - 1]
                  : kind == 13              ? e->water_cold[water - 1]
                                            : 0))
        return 0;
    for (yy = y - 8; yy <= y + 8 && !near; yy++)
        for (xx = x - 8; xx <= x + 8; xx++) {
            int b;
            if (!grid_in_bounds(&w->grid, xx, yy) || exp_dist(x, y, xx, yy) > 8)
                continue;
            b = e->tiles[grid_index(&w->grid, xx, yy)].biome;
            near |= kind == 21  ? b == CORE_BIOME_SNOW
                    : kind == 6 ? (b == CORE_BIOME_SNOW || b == CORE_BIOME_TAIGA)
                                : b == CORE_BIOME_SWAMP || b == CORE_BIOME_WETLAND ||
                                      (kind == 18 && water && b != CORE_BIOME_LAKE);
        }
    return near || (!water && (kind == 18 && (biome == CORE_BIOME_WETLAND || biome == CORE_BIOME_SWAMP)));
}
int expansion_wild_kind(World *w, int x, int y) {
    int kind, sum = 0, roll, weights[24], b = w->expansion->tiles[grid_index(&w->grid, x, y)].biome;
    for (kind = 0; kind < 24; kind++) {
        weights[kind] = WEIGHT[b][kind] && expansion_animal_habitat(w, kind, x, y) ? WEIGHT[b][kind] : 0;
        sum += weights[kind];
    }
    if (!sum)
        return -1;
    roll = rng_range(&w->rng, 1, sum);
    for (kind = 0; kind < 24; kind++)
        if ((roll -= weights[kind]) <= 0)
            return kind;
    return -1;
}
void expansion_ecology_recount(World *w) {
    ExpansionState *e = w->expansion;
    int i;
    for (i = 0; i < e->region_count; i++)
        e->regions[i].count = e->regions[i].biomass = e->regions[i].aggressive = 0;
    for (i = 0; i < e->animal_count; i++) {
        CoreAnimalView *a = &e->animals[i];
        CoreHabitatView *r;
        if (!a->alive || a->owner)
            continue;
        a->region = e->region_map[grid_index(&w->grid, a->x_milli / 1000, a->y_milli / 1000)];
        r = &e->regions[a->region];
        r->count++;
        r->biomass += EXP_ANIMALS[a->kind].biomass;
        r->aggressive += EXP_ANIMALS[a->kind].behavior == 2;
    }
}
void expansion_ecology_init(World *w) {
    ExpansionState *e = w->expansion;
    int i, j, centers[30][2], counts[30][10] = {{0}};
    e->region_count = 30;
    memset(e->regions, 0, sizeof(e->regions));
    for (i = 0; i < 30; i++) {
        uint32_t h = (uint32_t)w->seed * 2654435761u + (uint32_t)i * 2246822519u;
        centers[i][0] = (i % 6) * w->grid.width / 6 + w->grid.width / 12 + (int)(h % 9) - 4;
        centers[i][1] = (i / 6) * w->grid.height / 5 + w->grid.height / 10 + (int)((h >> 10) % 9) - 4;
        e->regions[i].id = i;
        e->regions[i].target = 5;
    }
    for (i = 0; i < e->cells; i++) {
        int x = i % w->grid.width, y = i / w->grid.width, best = INT32_MAX, chosen = 0;
        for (j = 0; j < 30; j++) {
            int dx = x - centers[j][0], dy = y - centers[j][1], d = dx * dx + dy * dy;
            if (d < best) {
                best = d;
                chosen = j;
            }
        }
        e->region_map[i] = (uint8_t)chosen;
        e->regions[chosen].area++;
        counts[chosen][e->tiles[i].biome]++;
    }
    for (i = 0; i < 30; i++) {
        CoreHabitatView *r = &e->regions[i];
        int b = 0, sum = 0, mass = 0, sample = -1;
        for (j = 1; j < 10; j++)
            if (counts[i][j] > counts[i][b])
                b = j;
        r->biome = b;
        for (j = 0; j < e->cells; j++)
            if (e->region_map[j] == i && e->tiles[j].biome == b) {
                sample = j;
                break;
            }
        for (j = 0; j < 24; j++)
            if (sample >= 0 &&
                expansion_animal_habitat(w, j, sample % w->grid.width, sample / w->grid.width)) {
                sum += WEIGHT[b][j];
                mass += WEIGHT[b][j] * EXP_ANIMALS[j].biomass;
            }
        r->capacity = exp_clamp((5 * mass + sum / 2) / (sum ? sum : 1), 8, 22);
        if (r->area < 320)
            r->capacity = exp_clamp(r->area / 40, 3, r->capacity);
        if (e->region_capacity[i])
            r->capacity = e->region_capacity[i];
        else
            e->region_capacity[i] = r->capacity;
        r->pressure_until = e->region_pressure_until[i];
    }
    expansion_ecology_recount(w);
}
int expansion_animal_spawn(World *w, int region, int initial) {
    ExpansionState *e = w->expansion;
    CoreHabitatView *r = &e->regions[region];
    int n, i, total = 0, p = -1, x, y, kind = -1;
    e->spawn_attempts++;
    for (i = 0; i < e->region_count; i++)
        total += e->regions[i].count;
    if (total >= 180 || r->count >= 7 || r->biomass >= r->capacity) {
        e->spawn_capacity_rejected++;
        return 0;
    }
    for (n = 0; n < 256; n++) {
        i = rng_range(&w->rng, 0, e->cells - 1);
        if (e->region_map[i] != region || w->grid.occupant[i] || e->protected_cells[i] ||
            (!initial && e->visible[i]) || (!w->grid.cost[i] && w->grid.terrain[i] != CORE_TERRAIN_WATER))
            continue;
        p = i;
        break;
    }
    if (p < 0) {
        e->spawn_tile_rejected++;
        return 0;
    }
    x = p % w->grid.width;
    y = p / w->grid.width;
    for (n = 0; n < 5; n++) {
        kind = expansion_wild_kind(w, x, y);
        if (kind >= 0 && r->biomass + EXP_ANIMALS[kind].biomass <= r->capacity &&
            (EXP_ANIMALS[kind].behavior != 2 || r->aggressive < 2))
            break;
        kind = -1;
    }
    if (kind < 0) {
        e->spawn_species_rejected++;
        return 0;
    }
    for (i = 0; i < e->animal_count; i++)
        if (!e->animals[i].alive)
            break;
    if (i == e->animal_count) {
        if (i >= CORE_EXP_MAX_ANIMALS)
            return 0;
        e->animal_count++;
    }
    e->animals[i] = (CoreAnimalView){0};
    e->animals[i].id = e->next_animal_id++;
    e->animals[i].kind = kind;
    e->animals[i].hp = EXP_ANIMALS[kind].hp;
    e->animals[i].alive = 1;
    e->animals[i].age = 120;
    e->animals[i].region = region;
    e->animals[i].x_milli = x * 1000 + 500;
    e->animals[i].y_milli = y * 1000 + 500;
    r->count++;
    r->biomass += EXP_ANIMALS[kind].biomass;
    r->aggressive += EXP_ANIMALS[kind].behavior == 2;
    return 1;
}
void expansion_ecology_daily(World *w) {
    ExpansionState *e = w->expansion;
    int i, tries = 0, day = exp_day(w), start = day % 30;
    e->spawn_attempts = e->spawn_capacity_rejected = e->spawn_tile_rejected = e->spawn_species_rejected = 0;
    expansion_ecology_recount(w);
    for (i = 0; i < 30; i++) {
        int j, kills = 0;
        for (j = 0; j < 12; j++)
            kills += e->region_hunt_days[i][j];
        if (kills >= 5 && e->region_pressure_until[i] <= day) {
            e->region_pressure_until[i] = day + 6;
            memset(e->region_hunt_days[i], 0, sizeof(e->region_hunt_days[i]));
        }
        e->regions[i].pressure_until = e->region_pressure_until[i];
    }
    for (i = 0; i < 30 && tries < 20; i++) {
        int id = (start + i) % 30;
        CoreHabitatView *r = &e->regions[id];
        int chance = r->count * 2 < r->target ? 70 : 45;
        if (r->count >= r->target || r->count >= 7 || r->biomass >= r->capacity)
            continue;
        tries++;
        if (e->region_pressure_until[id] > day)
            chance = chance * 70 / 100;
        if (rng_range(&w->rng, 0, 99) < chance)
            expansion_animal_spawn(w, id, 0);
    }
}
