/* test_v111.c — 명세 간 충돌과 경제·인구·경로 보존 회귀 검사 */
#include "expansion.h"
#include "population.h"
#include "test_util.h"
#include "villager_ai.h"
#include <stdio.h>
#include <string.h>

static World *make_world(void) {
    CoreNewGameParams p = {0};
    World *w = NULL;
    strcpy(p.castle_name, u8"1.1.1 시험");
    p.seed = 42;
    p.difficulty = 1;
    p.ruleset = 1;
    if (test_legacy_world(&p, &w) != CORE_OK)
        test_fail(__FILE__, __LINE__, "v111 world");
    return w;
}
static void add_people(World *w, int count) {
    for (int i = 0; i < count; i++) {
        Villager *v = population_create(w, w->keep_x, w->keep_y + 4);
        if (!v)
            test_fail(__FILE__, __LINE__, "v111 population");
    }
}
static Building *add_building(World *w, int type, int x, int y) {
    Building *b = entity_add_building(w, type);
    const ExpansionBuildingSpec *spec = expansion_spec(type);
    if (!b) {
        test_fail(__FILE__, __LINE__, "v111 building");
        return NULL;
    }
    b->x = x;
    b->y = y;
    b->w = spec->w;
    b->h = spec->h;
    b->door_x = x + b->w / 2;
    b->door_y = y + b->h;
    b->state = CORE_BSTATE_ACTIVE;
    b->hp = spec->hp;
    return b;
}
void test_v111_population_floor_and_refill(void) {
    World *w = make_world();
    if (!w)
        return;
    add_people(w, 5);
    Villager *a = w->villagers[0], *b = w->villagers[1];
    CHECK_EQ(expansion_permanent(w), 9);
    CHECK(expansion_departure_allowed(w, a, 1));
    a->state = CORE_VS_LEAVING;
    CHECK(!expansion_departure_allowed(w, b, 1));
    population_remove_departed(w, a);
    CHECK_EQ(expansion_permanent(w), 8);
    for (int rank = 0; rank < 6; rank++) {
        w->rank = rank;
        CHECK(!expansion_departure_allowed(w, b, 1));
        int before = w->villager_count;
        population_remove_departed(w, b);
        CHECK_EQ(w->villager_count, before);
    }
    b->state = CORE_VS_LEAVING;
    population_kill(w, w->villagers[w->villager_count - 1], CORE_DEATH_STARVATION);
    expansion_people_tick(w);
    CHECK(b->state != CORE_VS_LEAVING);
    w->rank = 0;
    w->expansion->starter_waves = 2;
    w->expansion->last_arrival_day = 13;
    w->expansion->refill_day = 16;
    w->clock.tick = 14 * 144 + 36;
    expansion_people_daily(w);
    CHECK_EQ(expansion_permanent(w), 7);
    w->clock.tick = 15 * 144 + 36;
    expansion_people_daily(w);
    CHECK_EQ(expansion_permanent(w), 8);
    CHECK_EQ(w->expansion->refill_day, 19);
    CHECK_EQ(w->expansion->last_arrival_day, 16);
    w->clock.tick = 18 * 144 + 36;
    expansion_people_daily(w);
    CHECK_EQ(expansion_permanent(w), 8);
    CHECK_EQ(w->expansion->refill_day, 22);
    core_destroy(w);
}
void test_v111_immigration_status(void) {
    World *w = make_world();
    CoreImmigrationView q;
    if (!w)
        return;
    w->rank = 1;
    w->expansion->last_arrival_day = 20;
    w->expansion->immigration_day = 24;
    w->clock.tick = 22 * 144 + 36;
    CHECK_EQ(core_expansion_immigration(w, &q), CORE_OK);
    CHECK_EQ(q.next_day, 24);
    CHECK(q.reasons & CORE_IMM_COOLDOWN);
    CHECK_EQ(q.required_happiness, 42);
    w->clock.tick = 26 * 144 + 36;
    core_expansion_immigration(w, &q);
    CHECK(!(q.reasons & CORE_IMM_COOLDOWN));
    CHECK(q.reasons & CORE_IMM_HOUSING);
    CHECK_EQ(w->expansion->last_arrival_day, 20);
    w->expansion->last_arrival_day = 28;
    w->expansion->immigration_day = 30;
    core_expansion_immigration(w, &q);
    CHECK_EQ(q.next_day, 30);
    w->rank = 0;
    w->expansion->starter_waves = 1;
    core_expansion_immigration(w, &q);
    CHECK_EQ(q.status, CORE_IMM_SPECIAL);
    CHECK_EQ(q.reasons, 0);
    core_destroy(w);
}
void test_v111_long_happiness(void) {
    World *w = make_world();
    if (!w)
        return;
    Villager *v = w->villagers[0];
    v->long_needs[0] = v->long_needs[1] = v->long_needs[2] = 100000;
    v->mood = 0;
    v->needs.hunger = 30;
    int before = expansion_mood(w, v);
    v->needs.hunger = 100;
    CHECK_EQ(expansion_mood(w, v), before);
    v->mood = -200;
    CHECK_EQ(expansion_mood(w, v), exp_clamp(before - 36, 0, 100));
    v->mood = 0;
    v->need_samples = 24;
    v->need_sums[0] = 24 * 20;
    v->need_sums[1] = 24 * 60;
    v->need_sums[2] = 24 * 80;
    expansion_wellbeing_daily(w, v);
    CHECK_EQ(v->long_needs[0], 80000);
    CHECK_EQ(v->long_needs[1], 90000);
    CHECK_EQ(v->long_needs[2], 95000);
    CHECK_EQ(v->need_samples, 0);
    v->health = 20;
    w->clock.tick = 100;
    expansion_hurt(w, v, 4, 8);
    CHECK_EQ(v->mood, -14);
    expansion_hurt(w, v, 4, 8);
    CHECK_EQ(v->mood, -14);
    w->clock.tick = 112;
    expansion_hurt(w, v, 4, 8);
    CHECK_EQ(v->mood, -19);
    core_destroy(w);
}
void test_v111_manor_promotion_save(void) {
    World *w = make_world(), *loaded = NULL;
    if (!w)
        return;
    int32_t why = 0, line = 0;
    Building *keep = world_keep(w);
    CHECK_EQ(keep->w, 4);
    CHECK_EQ(keep->h, 4);
    CHECK_EQ(core_rotate_building(w, keep->id, 1, &why), CORE_ERR_REJECTED);
    CHECK_EQ(keep->rotation, 0);
    add_people(w, 4);
    for (int i = 0; i < w->villager_count; i++)
        w->villagers[i]->happiness = 90;
    w->expansion->effective_roads = 10;
    int gold = w->bank.amount[CORE_RES_GOLD];
    CHECK_EQ(core_rank_up(w, &why), CORE_OK);
    CHECK_EQ(w->bank.amount[CORE_RES_GOLD], gold - 20);
    CHECK_EQ(core_rank_up(w, &why), CORE_ERR_REJECTED);
    CHECK_EQ(w->bank.amount[CORE_RES_GOLD], gold - 20);
    /* 공사 완료는 시작 때의 조건을 다시 검사하지 않습니다. */
    w->expansion->effective_roads = 0;
    for (int i = 0; i < 72; i++)
        expansion_promotion_tick(w);
    CHECK_EQ(w->rank, 0);
    CHECK_EQ(w->expansion->promotion_remaining, 72);
    CHECK_EQ(core_save(w, "v111_promotion.sav"), CORE_OK);
    CHECK_EQ(core_load("v111_promotion.sav", NULL, &loaded, &line), CORE_OK);
    if (loaded) {
        CHECK_EQ(loaded->expansion->promotion_remaining, 72);
        for (int i = 0; i < 72; i++)
            expansion_promotion_tick(loaded);
        CHECK_EQ(loaded->rank, 1);
        CHECK_EQ(loaded->expansion->promotion_state, 0);
        core_destroy(loaded);
    }
    core_destroy(w);
    remove("v111_promotion.sav");
    remove("v111_promotion.sav.bak");
}
void test_v111_weighted_hubs(void) {
    World *w = make_world();
    if (!w)
        return;
    Building *food = add_building(w, CORE_BLD_GRANARY, 110, 100),
             *all = add_building(w, CORE_BLD_WAREHOUSE, 116, 100);
    Villager *v = w->villagers[0];
    for (int y = 98; y < 109; y++)
        for (int x = 106; x < 124; x++) {
            int p = grid_index(&w->grid, x, y);
            w->grid.terrain[p] = CORE_TERRAIN_GRASS;
            w->expansion->tiles[p].biome = 0;
        }
    expansion_rebuild(w);
    w->expansion->connected[grid_index(&w->grid, food->door_x, food->door_y)] = 1;
    w->expansion->connected[grid_index(&w->grid, all->door_x, all->door_y)] = 1;
    v->current_x_milli = food->door_x * 1000 + 500;
    v->current_y_milli = food->door_y * 1000 + 500;
    v->hub_revision = -1;
    CHECK(expansion_hub(w, v, 1, NULL) == food);
    v->hub_revision = -1;
    CHECK(expansion_hub(w, v, 0, NULL) == all);
    v->hub_revision = -1;
    w->clock.tick = 42;
    v->meal_day = 0;
    v->needs.hunger = 20;
    v->target = food;
    v->target_x = food->door_x;
    v->target_y = food->door_y;
    v->at_target = 1;
    int before = expansion_bank_have(w, CORE_RES_FOOD);
    expansion_people_tick(w);
    CHECK_EQ(expansion_bank_have(w, CORE_RES_FOOD), before - 1000);
    expansion_people_tick(w);
    CHECK_EQ(expansion_bank_have(w, CORE_RES_FOOD), before - 1000);
    v->hub_revision = -1;
    w->expansion->alarm = 1;
    CHECK(expansion_hub(w, v, 1, NULL) == world_keep(w));
    core_destroy(w);
}
void test_v111_water_temperature_and_fishing(void) {
    World *w = make_world();
    if (!w)
        return;
    int boundary[512], land = 0;
    for (int p = 0; p < w->expansion->cells; p++) {
        w->grid.terrain[p] = CORE_TERRAIN_GRASS;
        w->expansion->tiles[p].biome = 0;
    }
    for (int y = 20; y < 30; y++)
        for (int x = 20; x < 40; x++) {
            int p = grid_index(&w->grid, x, y);
            w->grid.terrain[p] = CORE_TERRAIN_WATER;
            w->expansion->tiles[p].biome = CORE_BIOME_LAKE;
        }
    for (int y = 18; y <= 31; y++)
        for (int x = 18; x <= 41; x++) {
            int dx = x < 20 ? 20 - x : x > 39 ? x - 39 : 0, dy = y < 20 ? 20 - y : y > 29 ? y - 29 : 0;
            if (dx + dy > 0 && dx + dy <= 2)
                boundary[land++] = grid_index(&w->grid, x, y);
        }
    int threshold = (land * 6 + 9) / 10;
    for (int i = 0; i < (threshold - 1) / 2; i++)
        w->expansion->tiles[boundary[i]].biome = CORE_BIOME_SNOW;
    if ((threshold - 1) & 1)
        w->expansion->tiles[boundary[(threshold - 1) / 2]].biome = CORE_BIOME_TAIGA;
    expansion_water_bodies(w);
    CoreWaterView water;
    core_expansion_water(w, 1, &water);
    CHECK_EQ(water.area, 200);
    CHECK_EQ(water.cold, 0);
    int used = (threshold - 1) / 2;
    if ((threshold - 1) & 1)
        w->expansion->tiles[boundary[used]].biome = CORE_BIOME_SNOW;
    else
        w->expansion->tiles[boundary[used]].biome = CORE_BIOME_TAIGA;
    expansion_water_bodies(w);
    core_expansion_water(w, 1, &water);
    CHECK_EQ(water.cold, 1);
    CHECK_EQ(water.capacity_milli, 44100);
    Building *a = add_building(w, CORE_BLD_PORT, 18, 20), *b = add_building(w, CORE_BLD_PORT, 40, 24);
    CHECK_EQ(entity_link_worker(a, w->villagers[0], 8), CORE_OK);
    CHECK_EQ(entity_link_worker(b, w->villagers[1], 8), CORE_OK);
    expansion_rebuild(w);
    expansion_capacity(w);
    w->expansion->connected[grid_index(&w->grid, a->door_x, a->door_y)] = 1;
    w->expansion->connected[grid_index(&w->grid, b->door_x, b->door_y)] = 1;
    w->expansion->distance[grid_index(&w->grid, a->door_x, a->door_y)] = 0;
    w->expansion->distance[grid_index(&w->grid, b->door_x, b->door_y)] = 0;
    a->daily_work_milli = b->daily_work_milli = w->defs.buildings[CORE_BLD_PORT].worker_slots * 540000;
    w->bank.amount[CORE_RES_FOOD] = 0;
    w->expansion->fractional[CORE_RES_FOOD] = 0;
    expansion_daily(w);
    CHECK_EQ(expansion_bank_have(w, CORE_RES_FOOD), 30000);
    w->clock.tick = 36 * 144;
    w->nature.event = CORE_NATURE_NONE;
    a->daily_work_milli = b->daily_work_milli = w->defs.buildings[CORE_BLD_PORT].worker_slots * 540000;
    w->bank.amount[CORE_RES_FOOD] = 0;
    w->expansion->fractional[CORE_RES_FOOD] = 0;
    expansion_daily(w);
    CHECK_EQ(expansion_bank_have(w, CORE_RES_FOOD), 30000);
    core_destroy(w);
}
void test_v111_hunting_and_debug_cap(void) {
    World *w = make_world();
    if (!w)
        return;
    CoreAnimalView *a = &w->expansion->animals[0];
    int region = a->region, id = a->id, food = EXP_ANIMALS[a->kind].food;
    int biomass = w->expansion->regions[region].biomass, mass = EXP_ANIMALS[a->kind].biomass;
    int before = expansion_bank_have(w, CORE_RES_FOOD);
    CHECK_EQ(core_expansion_damage_animal(w, id, a->hp), CORE_OK);
    CHECK_EQ(w->expansion->regions[region].biomass, biomass - mass);
    CHECK_EQ(expansion_bank_have(w, CORE_RES_FOOD), before + food * 1000);
    CHECK_EQ(core_expansion_damage_animal(w, id, 100), CORE_ERR_NOT_FOUND);
    w->expansion->region_hunt_days[region][exp_day(w) % 12] = 5;
    expansion_ecology_daily(w);
    CHECK_EQ(w->expansion->region_pressure_until[region], exp_day(w) + 6);
    int expiry = w->expansion->region_pressure_until[region];
    w->clock.tick += 6 * 144;
    expansion_ecology_daily(w);
    CHECK_EQ(w->expansion->region_pressure_until[region], expiry);
    CHECK_EQ(core_expansion_debug_wildlife(w, 360), CORE_OK);
    CoreSummaryView summary;
    core_get_summary(w, &summary);
    CHECK_EQ(summary.wild_count, 360);
    CHECK_EQ(expansion_animal_spawn(w, 0, 1), 0);
    CHECK_EQ(core_expansion_debug_wildlife(w, 180), CORE_OK);
    core_get_summary(w, &summary);
    CHECK_EQ(summary.wild_count, 180);
    core_destroy(w);
}
void test_v111_market_split_quotes(void) {
    World *w = make_world();
    if (!w)
        return;
    Building *market = add_building(w, CORE_BLD_TRADING_POST, 40, 40);
    CHECK_EQ(entity_link_worker(market, w->villagers[0], 4), CORE_OK);
    w->clock.tick=54;Villager *staff=w->villagers[0];staff->incoming=0;staff->state=CORE_VS_WORKING;staff->target=market;staff->at_target=1;
    staff->target_x=market->door_x;staff->target_y=market->door_y;
    staff->current_x_milli=market->door_x*1000+500;staff->current_y_milli=market->door_y*1000+500;
    w->bank.amount[CORE_RES_GOLD] = 10000;
    w->expansion->trade_capacity_milli = 260000;
    w->expansion->trade_stock[CORE_RES_IRON] = 260;
    w->bank.limits[CORE_RES_IRON] = 1000;
    CoreTradeQuote all, first, second;
    int32_t gold = 0, why = 0;
    core_expansion_quote(w, CORE_RES_IRON, 20, &all);
    core_expansion_quote(w, CORE_RES_IRON, 10, &first);
    CHECK_EQ(core_trade(w, CORE_RES_IRON, 10, &gold, &why), CORE_OK);
    CHECK_EQ(-gold, first.total_gold);
    core_expansion_quote(w, CORE_RES_IRON, 10, &second);
    CHECK_EQ(first.total_gold + second.total_gold, all.total_gold);
    CHECK_EQ(second.next_pressure, all.next_pressure);
    CHECK_EQ(second.next_remainder, all.next_remainder);
    CHECK_EQ(core_trade(w, CORE_RES_IRON, 10, &gold, &why), CORE_OK);
    w->expansion->trade_used = 0;
    w->expansion->trade_capacity_milli = 260000;
    CHECK_EQ(core_trade(w, CORE_RES_IRON, 30, &gold, &why), CORE_OK);
    CHECK_EQ(w->expansion->trade_pressure[CORE_RES_IRON], 500);
    expansion_market_daily(w);
    CHECK_EQ(w->expansion->trade_pressure[CORE_RES_IRON], 400);
    w->clock.tick = 36 * 144;
    core_expansion_quote(w, CORE_RES_FOOD, 1, &all);
    CHECK_EQ(all.season_bp, 500);
    CHECK_EQ(all.unit_cents, 210);
    market->type = CORE_BLD_MARKET;
    core_expansion_quote(w, CORE_RES_MANA, 1, &all);
    CHECK_EQ(all.reason, CORE_DENY_NOT_TRADABLE);
    core_destroy(w);
}
void test_v111_ecology_water_and_roads(void) {
    World *w = make_world();
    if (!w)
        return;
    int wild = 0, attack = 0, swim = 0, livestock = 0;
    for (int k = 0; k < 24; k++) {
        CoreAnimalInfo info;
        core_expansion_animal_info(k, &info);
        attack += info.attack > 0;
        swim += info.swim_milli > 0;
        livestock += info.livestock;
    }
    CHECK_EQ(attack, 9);
    CHECK_EQ(swim, 5);
    CHECK_EQ(livestock, 4);
    CHECK(!EXP_ANIMALS[14].swim_milli);
    CHECK(!EXP_ANIMALS[15].swim_milli);
    CHECK_EQ(EXP_ANIMALS[17].behavior, 1);
    for (int i = 0; i < w->expansion->animal_count; i++) {
        CoreAnimalView *a = &w->expansion->animals[i];
        CHECK(!EXP_ANIMALS[a->kind].livestock);
        wild += a->alive;
    }
    CHECK(wild >= 280 && wild <= 320);
    CHECK_EQ(w->expansion->region_count, 30);
    for (int i = 0; i < 30; i++) {
        CHECK(w->expansion->regions[i].count <= w->expansion->regions[i].count_cap);
        CHECK(w->expansion->regions[i].biomass <= w->expansion->regions[i].capacity);
        CHECK(w->expansion->regions[i].aggressive <= (w->expansion->regions[i].count_cap+3)/4);
    }
    int p = grid_index(&w->grid, 20, 20);
    w->grid.terrain[p] = CORE_TERRAIN_GRASS;
    w->grid.block[p] = 0;
    w->grid.cost[p] = 10;
    w->expansion->tiles[p].biome = CORE_BIOME_SWAMP;
    expansion_terrain_costs(w);
    CHECK_EQ(w->grid.cost[p], 135);
    w->grid.road[p] = 1;
    expansion_terrain_costs(w);
    CHECK_EQ(w->grid.cost[p], 67);
    w->grid.road[p] = 2;
    expansion_terrain_costs(w);
    CHECK_EQ(w->grid.cost[p], 50);
    w->grid.terrain[p] = CORE_TERRAIN_WATER;
    w->grid.road[p] = 3;
    expansion_terrain_costs(w);
    CHECK_EQ(w->grid.cost[p], 80);
    core_destroy(w);
}
void test_v111_migrant_fog_and_save(void) {
    World *w = make_world(), *loaded = NULL;
    if (!w)
        return;
    Villager *v = w->villagers[0];
    int p = grid_index(&w->grid, 10, 10);
    uint64_t before = 0, after = 0;
    v->current_x_milli = v->current_y_milli = 10500;
    v->incoming = 1;
    memset(w->expansion->static_visible, 0, (size_t)w->expansion->cells);
    w->expansion->tiles[p].explored = 0;
    expansion_visibility(w);
    CHECK_EQ(w->expansion->visible[p], 0);
    v->incoming = 0;
    v->state = CORE_VS_LEAVING;
    expansion_visibility(w);
    CHECK_EQ(w->expansion->visible[p], 0);
    v->state = CORE_VS_IDLE;
    expansion_visibility(w);
    CHECK_EQ(w->expansion->visible[p], 1);
    v->mood = -123;
    v->happiness = 24;
    v->incoming = 1;
    w->expansion->trade_pressure[2] = 123;
    w->expansion->trade_remainder[2] = 829232;
    w->expansion->refill_day = 16;
    CHECK_EQ(core_state_hash(w, &before), CORE_OK);
    CHECK_EQ(core_save(w, "v111_state.sav"), CORE_OK);
    int32_t line = 0;
    CHECK_EQ(core_load("v111_state.sav", NULL, &loaded, &line), CORE_OK);
    if (loaded) {
        core_state_hash(loaded, &after);
        CHECK_EQ(before, after);
        CHECK_EQ(loaded->villagers[0]->mood, -123);
        core_destroy(loaded);
    }
    core_destroy(w);
    remove("v111_state.sav");
    remove("v111_state.sav.bak");
}
void test_v111_attack_impact_and_save(void) {
    World *w = make_world(), *loaded = NULL;
    if (!w)
        return;
    Villager *v = w->villagers[0];
    CoreAnimalView *a = &w->expansion->animals[0];
    v->current_x_milli = v->current_y_milli = 75500;
    v->inside_building = v->incoming = 0;
    v->state = CORE_VS_IDLE;
    v->health = 100;
    a->kind = 11;
    a->hp = 36;
    a->alive = 1;
    a->owner = 0;
    a->x_milli = a->y_milli = 75500;
    a->target_id = v->id;
    a->attack_tick = w->clock.tick;
    a->attack_remaining_milli = 2400;
    expansion_ecology_recount(w);
    CHECK_EQ(core_expansion_advance_animal_attacks(w, 2399), CORE_OK);
    CHECK_EQ(v->health, 100);
    CHECK_EQ(core_expansion_advance_animal_attacks(w, 1), CORE_OK);
    CHECK_EQ(v->health, 95);
    CHECK_EQ(core_expansion_advance_animal_attacks(w, 2400), CORE_OK);
    CHECK_EQ(v->health, 95);
    a->attack_remaining_milli = 2400;
    core_expansion_advance_animal_attacks(w, 1200);
    CHECK_EQ(core_save(w, "v111_attack.sav"), CORE_OK);
    int32_t line = 0;
    CHECK_EQ(core_load("v111_attack.sav", NULL, &loaded, &line), CORE_OK);
    if (loaded) {
        CHECK_EQ(loaded->expansion->animals[0].attack_remaining_milli, 1200);
        core_expansion_advance_animal_attacks(loaded, 1199);
        CHECK_EQ(loaded->villagers[0]->health, 95);
        core_expansion_advance_animal_attacks(loaded, 1);
        CHECK_EQ(loaded->villagers[0]->health, 90);
        core_destroy(loaded);
    }
    core_destroy(w);
    remove("v111_attack.sav");
    remove("v111_attack.sav.bak");
}
