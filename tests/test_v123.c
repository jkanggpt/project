/* test_v123.c — 160 지도와 새 상태의 경제·이동·저장 경계 검사 */
#include "expansion.h"
#include "population.h"
#include "test_util.h"
#include <stdio.h>
#include <string.h>
int test_legacy_world(const CoreNewGameParams *p, World **out) {
    int rc = world_create_generation(p, NULL, 10, out);
    if (rc == CORE_OK && (*out)->expansion) {
        rc = expansion_v123_migrate(*out);
        if (rc != CORE_OK) {
            core_destroy(*out);
            *out = NULL;
        }
    }
    return rc;
}
static World *fresh(int seed) {
    CoreNewGameParams p = {0};
    World *w = NULL;
    strcpy(p.castle_name, u8"1.2.3 검사");
    p.ruleset = 1;
    p.seed = seed;
    if (core_create(&p, NULL, &w) != CORE_OK)
        return NULL;
    return w;
}
void test_v123_world_and_clay(void) {
    World *w = fresh(42);
    CHECK(w);
    CHECK_EQ(w->grid.width, 160);
    CHECK_EQ(w->expansion->water_count, 20);
    int water = 0;
    for (int p = 0; p < w->expansion->cells; p++) {
        CoreExpansionTileView *t = &w->expansion->tiles[p];
        if (w->grid.terrain[p] == CORE_TERRAIN_WATER) {
            water++;
            CHECK_EQ(t->clay_milli, 0);
            CHECK(exp_dist(p % 160, p / 160, w->keep_x, w->keep_y) > 12);
        } else {
            CHECK(t->clay_milli > 0);
            CHECK_EQ(t->clay_milli, t->clay_initial_milli);
        }
    }
    CHECK_EQ(water, 2304);
    CHECK_EQ(core_validate(w), 0);
    Building b = {0};
    b.id = 18;
    b.x = w->keep_x;
    b.y = w->keep_y;
    b.w = b.h = 2;
    int before = 0, after = 0;
    for (int p = 0; p < w->expansion->cells; p++)
        before += w->expansion->tiles[p].clay_milli / 1000;
    CHECK_EQ(expansion_clay_take(w, &b, 4, 8000), 8000);
    int64_t exact = 0;
    for (int p = 0; p < w->expansion->cells; p++) {
        after += w->expansion->tiles[p].clay_milli / 1000;
        exact += w->expansion->tiles[p].clay_initial_milli - w->expansion->tiles[p].clay_milli;
    }
    CHECK_EQ(exact, 8000);
    CHECK(after <= before);
    uint64_t hash, copyhash;
    World *copy = NULL;
    int32_t line = 0;
    core_state_hash(w, &hash);
    CHECK_EQ(core_save(w, "v123_roundtrip.sav"), CORE_OK);
    CHECK_EQ(core_load("v123_roundtrip.sav", NULL, &copy, &line), CORE_OK);
    core_state_hash(copy, &copyhash);
    CHECK_EQ(hash, copyhash);
    core_destroy(copy);
    core_destroy(w);
    remove("v123_roundtrip.sav");
    remove("v123_roundtrip.sav.bak");
}
void test_v123_barter_atomic(void) {
    World *w = fresh(7);
    CHECK(w);
    CHECK_EQ(core_expansion_barter_count(), 85);
    for (int id = 1; id <= 29; id++) {
        CoreBarterRecipe r;
        CHECK_EQ(core_expansion_barter_recipe(id, &r), CORE_OK);
        CHECK(r.benefit_bp > 0);
        CHECK(r.pay_resource != 0 && r.get_resource != 0);
    }
    CoreWanderingMerchantView *m = &w->expansion->wandering;
    m->state = CORE_MERCHANT_TRADING;
    m->rank = 1;
    m->visit_id = 1;
    m->expires_tick = 432;
    m->offer_count = 1;
    m->recipe_ids[0] = 1;
    m->remaining[0] = 2;
    int32_t reason = 0;
    int logs = expansion_bank_have(w, 6), mana = expansion_bank_have(w, 1);
    CHECK_EQ(core_expansion_merchant_trade(w, 1, 1, 1, &reason), CORE_OK);
    CHECK_EQ(expansion_bank_have(w, 6), logs - 30000);
    CHECK_EQ(expansion_bank_have(w, 1), mana + 2000);
    uint64_t a, b;
    core_state_hash(w, &a);
    CHECK_EQ(core_expansion_merchant_trade(w, 1, 1, 1, &reason), CORE_IGNORED);
    core_state_hash(w, &b);
    CHECK_EQ(a, b);
    w->bank.amount[6] = 0;
    core_state_hash(w, &a);
    CHECK_EQ(core_expansion_merchant_trade(w, 1, 1, 2, &reason), CORE_ERR_REJECTED);
    core_state_hash(w, &b);
    CHECK_EQ(a, b);
    CHECK_EQ(reason, CORE_DENY_NO_GOODS);
    core_destroy(w);
}
void test_v123_exile_floor(void) {
    World *w = fresh(3);
    CHECK(w);
    while (w->villager_count < 10)
        CHECK(population_create(w, w->keep_x, w->keep_y + 5));
    CoreExileView q;
    int id0 = w->villagers[0]->id, id1 = w->villagers[1]->id, id2 = w->villagers[2]->id;
    CHECK_EQ(core_expansion_expel(w, id0), CORE_OK);
    CHECK_EQ(core_expansion_expel(w, id1), CORE_OK);
    CHECK_EQ(core_expansion_can_expel(w, id2, &q), CORE_OK);
    CHECK_EQ(q.reason, CORE_EXILE_FLOOR8);
    CHECK_EQ(core_expansion_expel(w, id2), CORE_ERR_REJECTED);
    expansion_people_tick(w);
    CHECK_EQ(entity_find_villager(w, id0)->state, CORE_VS_LEAVING);
    CHECK_EQ(entity_find_villager(w, id1)->state, CORE_VS_LEAVING);
    population_remove_departed(w, entity_find_villager(w, id0));
    population_remove_departed(w, entity_find_villager(w, id1));
    CHECK_EQ(w->villager_count, 8);
    core_destroy(w);
}
void test_v123_forecast_rng(void) {
    World *w = fresh(9);
    CHECK(w);
    Building *b = entity_add_building(w, CORE_BLD_OBSERVATORY);
    CHECK(b);
    b->state = CORE_BSTATE_ACTIVE;
    b->x = 20;
    b->y = 20;
    b->w = b->h = 3;
    b->door_x = 21;
    b->door_y = 23;
    b->hp = 760;
    Villager *v = w->villagers[0];
    CHECK_EQ(entity_link_worker(b, v, 4), CORE_OK);
    v->stats.intellect = 5;
    v->state = CORE_VS_WORKING;
    v->target = b;
    v->at_target = 1;
    v->inside_building = b->id;
    w->clock.tick = 54;
    Rng before = w->rng;
    expansion_forecast_refresh(w, b);
    CHECK_EQ(w->rng.state, before.state);
    CHECK_EQ(w->rng.inc, before.inc);
    CHECK(b->forecast_horizon >= 12 && b->forecast_horizon <= 48);
    CoreForecastView out;
    uint64_t a, c;
    core_state_hash(w, &a);
    for (int i = 0; i < 100; i++)
        CHECK_EQ(core_expansion_forecast(w, b->id, &out), CORE_OK);
    core_state_hash(w, &c);
    CHECK_EQ(a, c);
    CHECK_EQ(EXP_ANIMALS[28].swim_milli, 0);
    core_destroy(w);
}

static Building *facility(World *w, int type, int x, int y) {
    Building *b = entity_add_building(w, type);
    if (!b)
        return NULL;
    const ExpansionBuildingSpec *s = expansion_spec(type);
    b->x = x;
    b->y = y;
    b->w = s->w;
    b->h = s->h;
    b->hp = s->hp;
    b->state = CORE_BSTATE_ACTIVE;
    b->door_x = x + s->w / 2;
    b->door_y = y + s->h;
    b->auto_assign = 0;
    return b;
}
void test_v123_bridge_and_boats(void) {
    World *w = fresh(16);
    CHECK(w);
    Grid *g = &w->grid;
    static const int dx[4] = {0, 1, 0, -1}, dy[4] = {-1, 0, 1, 0};
    int center = grid_index(g, 20, 20);
    for (int mask = 0; mask < 16; mask++) {
        g->terrain[center] = CORE_TERRAIN_WATER;
        g->road[center] = 3;
        for (int d = 0; d < 4; d++) {
            int p = grid_index(g, 20 + dx[d], 20 + dy[d]);
            g->terrain[p] = CORE_TERRAIN_WATER;
            g->road[p] = (mask & (1 << d)) ? 3 : 0;
            g->occupant[p] = 0;
        }
        CHECK_EQ(core_expansion_bridge_mask(w, 20, 20), (~mask) & 15);
    }
    g->terrain[center] = CORE_TERRAIN_GRASS;
    CHECK_EQ(core_expansion_bridge_mask(w, 20, 20), 0);
    g->road[center] = 0;
    Building *b = facility(w, CORE_BLD_PORT, 30, 30);
    CHECK(b);
    for (int y = 28; y < 48; y++)
        for (int x = 30; x < 56; x++) {
            int p = grid_index(g, x, y);
            g->terrain[p] = x >= b->x + b->w ? CORE_TERRAIN_WATER : CORE_TERRAIN_GRASS;
            g->road[p] = 0;
            g->occupant[p] = 0;
            w->expansion->tiles[p].tree_kind = 0;
            w->expansion->tiles[p].tree_logs_milli = 0;
        }
    expansion_water_bodies(w);
    expansion_rebuild(w);
    int water = expansion_adjacent_water_id(w, b);
    CHECK(water > 0);
    w->clock.tick = 54;
    for (int n = 1; n <= 4; n++) {
        Villager *v = w->villagers[n - 1];
        CHECK_EQ(entity_link_worker(b, v, 4), CORE_OK);
        v->at_target = 1;
        v->state = CORE_VS_WORKING;
        v->target = b;
        v->inside_building = b->id;
        v->current_x_milli = b->door_x * 1000 + 500;
        v->current_y_milli = b->door_y * 1000 + 500;
    }
    expansion_boats_tick(w);
    CHECK_EQ(w->expansion->boat_count, 2);
    CHECK_EQ(w->expansion->boats[0].crew_count, 2);
    CHECK_EQ(w->expansion->boats[1].crew_count, 2);
    for (int tick = 0; tick < 40; tick++) {
        w->clock.tick++;
        expansion_boats_tick(w);
        for (int i = 0; i < 2; i++) {
            CoreBoatView *boat = &w->expansion->boats[i];
            int p = grid_index(g, boat->x_milli / 1000, boat->y_milli / 1000);
            CHECK_EQ(g->terrain[p], CORE_TERRAIN_WATER);
            CHECK_EQ(g->road[p], 0);
        }
    }
    for (int i = 0; i < 4; i++)
        CHECK(w->villagers[i]->boat_port_id == b->id);
    w->clock.tick = 108;
    for (int tick = 0; tick < 200; tick++) {
        w->clock.tick++;
        expansion_boats_tick(w);
    }
    for (int i = 0; i < 4; i++)
        CHECK_EQ(w->villagers[i]->boat_port_id, 0);
    core_destroy(w);
}
void test_v123_merchant_visit_and_save(void) {
    World *w = fresh(17), *copy = NULL;
    CHECK(w);
    w->rank = 1;
    int arrived = 0;
    int distances[25600];
    memcpy(distances, w->expansion->distance, sizeof(distances));
    for (int t = 0; t < 1800; t++) {
        w->clock.tick = t;
        expansion_merchant_tick(w);
        if (w->expansion->wandering.state == CORE_MERCHANT_TRADING) {
            arrived = t;
            break;
        }
    }
    CHECK_EQ(memcmp(distances, w->expansion->distance, sizeof(distances)), 0);
    CHECK(arrived > 0);
    CoreWanderingMerchantView *m = &w->expansion->wandering;
    CHECK_EQ(m->offer_count, 3);
    CHECK_EQ(m->expires_tick, arrived + 432);
    int mana = 0;
    for (int k = 0; k < 3; k++) {
        CoreBarterRecipe r;
        CHECK_EQ(core_expansion_barter_recipe(m->recipe_ids[k], &r), CORE_OK);
        mana += r.get_resource == 1;
    }
    CHECK_EQ(mana, 1);
    uint64_t a, b;
    int32_t line = 0;
    CHECK_EQ(core_validate(w), 0);
    CHECK_EQ(core_save(w, "v123_merchant.sav"), CORE_OK);
    CHECK_EQ(core_load("v123_merchant.sav", NULL, &copy, &line), CORE_OK);
    core_state_hash(w, &a);
    core_state_hash(copy, &b);
    CHECK_EQ(a, b);
    core_destroy(copy);
    w->clock.tick = arrived + 431;
    expansion_merchant_tick(w);
    CHECK_EQ(m->state, CORE_MERCHANT_TRADING);
    w->clock.tick++;
    expansion_merchant_tick(w);
    CHECK_EQ(m->state, CORE_MERCHANT_LEAVING);
    for (int t = 0; t < 500 && m->state != CORE_MERCHANT_DORMANT; t++) {
        w->clock.tick++;
        expansion_merchant_tick(w);
    }
    CHECK_EQ(m->state, CORE_MERCHANT_DORMANT);
    CHECK(m->next_target_tick - arrived >= 24 * 144 && m->next_target_tick - arrived <= 36 * 144);
    core_destroy(w);
    remove("v123_merchant.sav");
    remove("v123_merchant.sav.bak");
}
void test_v123_water_and_medicine(void) {
    World *w = fresh(20);
    CHECK(w);
    Building *home = facility(w, CORE_BLD_HOUSE, 20, 20), *well = facility(w, CORE_BLD_WELL, 26, 20),
             *tower = facility(w, CORE_BLD_WATER_TOWER, 26, 21);
    CHECK(home && well && tower);
    while (w->villager_count < 65)
        CHECK(population_create(w, 21, 24));
    for (int i = 0; i < w->villager_count; i++) {
        Villager *v = w->villagers[i];
        entity_unlink_resident(v);
        v->home = home; /* 수용 인원과 무관한 급수 대상 위치만 독립 검사 */
    }
    expansion_water_supply(w);
    CHECK(tower->water_served <= 60);
    CHECK(well->water_served <= 18);
    int total = 0;
    for (int i = 0; i < w->villager_count; i++)
        total += w->villagers[i]->water_building != 0;
    CHECK(total > 60);
    Building *healer = facility(w, CORE_BLD_HEALER, 40, 40),
             *hospital = facility(w, CORE_BLD_HOSPITAL, 45, 40);
    CHECK(healer && hospital);
    Villager *v = w->villagers[0];
    v->health = 100;
    v->critical = 0;
    v->sick_ticks = 0;
    CHECK_EQ(expansion_heal_herbs_required(w, v, healer), 2);
    CHECK_EQ(expansion_heal_herbs_required(w, v, hospital), 3);
    v->health = 35;
    CHECK_EQ(expansion_heal_herbs_required(w, v, healer), 3);
    v->sick_ticks = 10;
    w->expansion->plague_days = 2;
    CHECK_EQ(expansion_heal_herbs_required(w, v, healer), 4);
    CHECK_EQ(expansion_heal_herbs_required(w, v, hospital), 5);
    for (int i = 0; i < w->villager_count; i++)
        w->villagers[i]->home = NULL;
    core_destroy(w);
}
void test_v123_real_v10_preserved(void) {
    World *w = NULL;
    int32_t line = 0;
    CHECK_EQ(core_load(CASTLE_SOURCE_DIR "/tests/fixtures/expansion_v10.sav", NULL, &w, &line), CORE_OK);
    CHECK(w);
    CHECK_EQ(w->grid.width, 192);
    CHECK_EQ(w->expansion->clay_initialized, 1);
    CHECK_EQ(core_validate(w), 0);
    core_destroy(w);
}

void test_v123_hunter_retaliation(void) {
    World *w = fresh(42);
    CHECK(w);
    Building *b = facility(w, CORE_BLD_HUNTER_CAMP, 20, 20);
    CHECK(b);
    Villager *v = w->villagers[0];
    CHECK_EQ(entity_link_worker(b, v, 2), CORE_OK);
    w->clock.tick = 54;
    v->health = 50;
    v->meal_day = 1;
    v->needs.hunger = v->needs.rest = 100;
    v->mission = CORE_MISSION_SCOUT;
    v->mission_target = b->id;
    v->at_target = 0;
    v->current_x_milli = 21500;
    v->current_y_milli = 21500;
    CoreAnimalView *a = &w->expansion->animals[0];
    a->kind = 11;
    a->owner = 0;
    a->alive = 1;
    a->x_milli = 22500;
    a->y_milli = 21500;
    CHECK_EQ(expansion_hunter_threat(w, v, a->id), 1);
    CHECK_EQ(v->mission_target, b->id);
    CHECK_EQ(core_expansion_outfit(w, v->id), 1);
    expansion_people_tick(w);
    CHECK(v->state != CORE_VS_FLEE);
    CHECK_EQ(core_expansion_outfit(w, v->id), 1);
    v->health = 35;
    CHECK_EQ(expansion_hunter_threat(w, v, a->id), 1);
    expansion_people_tick(w);
    CHECK_EQ(v->mission, CORE_MISSION_RETURN);
    CHECK_EQ(v->target, b);
    v->at_target = 1;
    v->health = 69;
    v->current_x_milli = b->door_x * 1000 + 500;
    v->current_y_milli = b->door_y * 1000 + 500;
    expansion_people_tick(w);
    CHECK_EQ(v->health, 70);
    expansion_people_tick(w);
    CHECK(v->mission != CORE_MISSION_RETURN);
    a->kind = 12;
    v->health = 80;
    CHECK_EQ(expansion_hunter_threat(w, v, a->id), 1);
    CHECK_EQ(v->hunt_retreat_reason, 2);
    CHECK_EQ(v->mission_target, b->id);
    w->clock.tick = 120;
    CHECK_EQ(expansion_hunter_threat(w, v, a->id), 0);
    core_destroy(w);
}
