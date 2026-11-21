/* expansion_ecology.c — 30종 정의, 연결 수역 온도와 지리 서식지의 생체량 */
#include "expansion.h"
#include <stdlib.h>
#include <string.h>

const CoreAnimalInfo EXP_ANIMALS[CORE_EXP_ANIMALS] = {
    {12, 0, 3, 0, 1, 1450, 0, 0, 700, 750, 0, 60, 1, 0, u8"토끼"},
    {30, 0, 15, 3, 3, 1300, 0, 0, 1650, 1650, 0, 60, 1, 0, u8"사슴"},
    {28, 0, 11, 3, 2, 1050, 0, 0, 1350, 1400, 0, 60, 1, 0, u8"산양"},
    {10, 0, 3, 0, 1, 850, 1100, 0, 750, 800, 0, 60, 1, 0, u8"오리"},
    {22, 0, 5, 2, 2, 1250, 0, 1, 1150, 1050, 0, 60, 1, 0, u8"여우"},
    {85, 0, 29, 5, 4, 1000, 0, 0, 2150, 2150, 0, 60, 1, 0, u8"낙타"},
    {38, 0, 14, 5, 3, 650, 1800, 0, 1650, 1050, 0, 60, 1, 0, u8"물개"},
    {65, 0, 30, 6, 0, 700, 0, 0, 2100, 1650, 1, 60, 1, 0, u8"소"},
    {28, 0, 12, 3, 0, 900, 0, 0, 1450, 1250, 1, 60, 1, 0, u8"양"},
    {36, 0, 18, 3, 0, 850, 0, 0, 1550, 1100, 1, 60, 1, 0, u8"돼지"},
    {9, 0, 2, 0, 0, 1000, 0, 0, 700, 750, 1, 60, 1, 0, u8"닭"},
    {36, 5, 8, 3, 4, 1300, 0, 2, 1650, 1300, 0, 60, 1, 0, u8"늑대"},
    {120, 15, 30, 9, 5, 780, 0, 2, 2200, 2100, 0, 60, 1, 0, u8"곰"},
    {80, 9, 15, 6, 5, 800, 1250, 2, 2550, 1000, 0, 60, 1, 0, u8"악어"},
    {95, 12, 33, 8, 5, 1120, 0, 2, 2450, 1850, 0, 60, 1, 0, u8"버팔로"},
    {16, 4, 2, 0, 1, 750, 0, 2, 1500, 500, 0, 60, 1, 0, u8"뱀"},
    {105, 16, 18, 6, 5, 1250, 0, 2, 2050, 1500, 0, 60, 1, 0, u8"사자"},
    {180, 20, 38, 12, 6, 950, 0, 1, 2750, 1950, 0, 60, 1, 0, u8"코뿔소"},
    {6, 0, 2, 0, 1, 600, 1000, 0, 550, 500, 0, 60, 1, 0, u8"개구리"},
    {42, 7, 8, 3, 3, 1200, 0, 2, 1500, 1150, 0, 60, 1, 0, u8"하이에나"},
    {34, 7, 8, 3, 2, 1380, 0, 1, 1350, 1200, 0, 60, 1, 0, u8"스라소니"},
    {14, 0, 5, 2, 1, 620, 1500, 0, 850, 1100, 0, 60, 1, 0, u8"펭귄"},
    {48, 0, 17, 3, 3, 1600, 0, 0, 1250, 2250, 0, 60, 1, 0, u8"타조"},
    {12, 0, 5, 0, 1, 1100, 0, 0, 800, 850, 0, 60, 1, 0, u8"꿩"},
    {170, 18, 24, 8, 4, 1450, 0, 1, 2200, 2050, 0, 24, 1, 5, u8"유니콘"},
    {150, 16, 11, 5, 3, 1100, 0, 2, 1550, 1750, 0, 25, 2, 3, u8"코카트리스"},
    {260, 28, 33, 12, 5, 1550, 0, 2, 3000, 2500, 0, 30, 1, 6, u8"그리핀"},
    {340, 34, 42, 15, 5, 1400, 0, 2, 2850, 2000, 0, 26, 1, 8, u8"펜리르"},
    {420, 30, 53, 18, 6, 700, 900, 2, 3400, 2800, 0, 32, 2, 10, u8"히드라"},
    {190, 20, 15, 6, 3, 1050, 0, 1, 2350, 1150, 0, 27, 2, 4, u8"샐러맨더"},
};
static const int WEIGHT[10][24] = {
    {30, 22, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 11, 0, 0, 5, 0, 0, 0, 0, 15, 17},
    {28, 28, 0, 0, 16, 0, 0, 0, 0, 0, 0, 0, 6, 0, 0, 0, 0, 0, 0, 0, 9, 0, 0, 13},
    {0, 18, 10, 0, 11, 0, 0, 0, 0, 0, 0, 30, 15, 0, 0, 0, 0, 0, 0, 0, 16, 0, 0, 0},
    {0, 10, 40, 0, 0, 0, 0, 0, 0, 0, 0, 17, 10, 0, 0, 12, 0, 0, 0, 0, 11, 0, 0, 0},
    {0, 0, 0, 29, 11, 0, 0, 0, 0, 0, 0, 0, 0, 12, 5, 13, 0, 0, 30, 0, 0, 0, 0, 0},
    {0, 0, 23, 0, 0, 0, 20, 0, 0, 0, 0, 21, 18, 0, 0, 0, 0, 0, 0, 0, 0, 18, 0, 0},
    {0, 0, 0, 0, 0, 38, 0, 0, 0, 0, 0, 0, 0, 0, 0, 27, 5, 0, 0, 20, 0, 0, 10, 0},
    {0, 0, 0, 40, 0, 0, 20, 0, 0, 0, 0, 0, 0, 14, 0, 0, 0, 0, 20, 0, 0, 6, 0, 0},
    {0, 0, 0, 12, 6, 0, 0, 0, 0, 0, 0, 0, 0, 32, 0, 24, 0, 0, 26, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 10, 0, 0, 0, 0, 0, 0, 0, 0, 25, 0, 14, 9, 0, 20, 0, 0, 22, 0},
};
CORE_API int32_t core_expansion_animal_info(int32_t kind, CoreAnimalInfo *out) {
    if (!out)
        return CORE_ERR_NULL_ARG;
    if (kind < 0 || kind >= CORE_EXP_ANIMALS)
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
    out->capacity_milli = exp_clamp(5000 + out->area * 220, 12000, 130000) *
                          (exp_season(w) == 1   ? 110
                           : exp_season(w) == 3 ? 75
                                                : 100) /
                          100;
    out->capacity_milli = out->capacity_milli * (out->cold ? 90 : 110) / 100;
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
        r->aggressive += a->kind < 24 && EXP_ANIMALS[a->kind].behavior == 2;
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
        int eligible = 0;
        for (j = 0; j < e->cells; j++)
            if (e->region_map[j] == i && (!w->grid.block[j]) && (w->grid.cost[j] || e->tiles[j].water_id)) eligible++;
        r->target = exp_clamp((eligible + 60) / 120,2,28);
        r->count_cap = (r->target * 14 + 9) / 10 + 2;
        if (r->count_cap < 3) r->count_cap = 3;
        r->capacity = exp_clamp((r->target * mass * 14 + (sum ? sum : 1) * 10 - 1) / ((sum ? sum : 1) * 10),8,95);
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
    if (total >= 360 || r->count >= r->count_cap || r->biomass >= r->capacity) {
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
            (EXP_ANIMALS[kind].behavior != 2 || r->aggressive < exp_clamp((r->count_cap + 3) / 4,2,100)))
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
    for (i = 0; i < 60 && tries < 40; i++) {
        int id = (start + i) % 30;
        CoreHabitatView *r = &e->regions[id];
        int chance = r->count * 2 < r->target ? 70 : 45;
        if (r->count >= r->target || r->count >= r->count_cap || r->biomass >= r->capacity)
            continue;
        tries++;
        if (e->region_pressure_until[id] > day)
            chance = chance * 70 / 100;
        if (rng_range(&w->rng, 0, 99) < chance)
            expansion_animal_spawn(w, id, 0);
    }
    if (day > 0) {
        static const int first[6] = {7,20,30,42,50,12};
        int alive[6] = {0}, occupied[6][2]={{0}};
        for (i = 0; i < e->animal_count; i++)
            if (e->animals[i].alive && !e->animals[i].owner && e->animals[i].kind >= 24) {
                alive[e->animals[i].kind-24]++; occupied[e->animals[i].kind-24][exp_clamp(e->animals[i].phantom_slot,0,1)]=1;
            }
        for (int k = 0; k < 6; k++) {
            int kind = k + 24, unlock = first[k];
            if (EXP_ANIMALS[kind].behavior == 2) unlock += w->difficulty == 0 ? 12 : w->difficulty == 2 ? -8 : 0;
            if (unlock < 1) unlock = 1;
            int phantom_slot=!occupied[k][0] && day>=e->phantom_next_day[k]?0:!occupied[k][1] && day>=e->phantom_second_day[k]?1:-1;
            if (alive[k]>=2 || day<unlock || (day-unlock)%4 || phantom_slot<0 || rng_range(&w->rng,0,99)>=20)continue;
            for (int attempt = 0; attempt < 1000; attempt++) {
                int p = rng_range(&w->rng,0,e->cells-1), biome = e->tiles[p].biome, region = e->region_map[p];
                int habitat = k == 0 ? biome == 0 : k == 1 ? biome == 1 : k == 2 ? biome == 3 : k == 3 ? biome == 2 || biome == 5 : k == 4 ? biome == 4 || biome == 8 : biome == 6 || biome == 9;
                CoreHabitatView *r = &e->regions[region];
                int total = 0;
                for (int n = 0; n < e->region_count; n++) total += e->regions[n].count;
                if (!habitat || total >= 360 || e->visible[p] || e->protected_cells[p] || w->grid.block[p] ||
                    (!w->grid.cost[p] && !EXP_ANIMALS[kind].swim_milli) || r->count >= r->count_cap || r->biomass + EXP_ANIMALS[kind].biomass > r->capacity) continue;
                int slot;
                for (slot = 0; slot < e->animal_count && e->animals[slot].alive; slot++);
                if (slot >= CORE_EXP_MAX_ANIMALS) break;
                if (slot == e->animal_count) e->animal_count++;
                CoreAnimalView *a = &e->animals[slot];
                *a = (CoreAnimalView){0}; a->id = e->next_animal_id++; a->kind = kind; a->hp = EXP_ANIMALS[kind].hp;
                a->alive = 1; a->age = 120; a->region = region; a->x_milli = p % w->grid.width * 1000 + 500; a->y_milli = p / w->grid.width * 1000 + 500;
                a->phantom_slot=phantom_slot; expansion_ecology_recount(w); break;
            }
        }
    }
}
