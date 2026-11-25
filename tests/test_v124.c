/* test_v124.c — 원본 표·원자적 명령·버전 경계를 실제 월드로 검사합니다. */
#include "expansion.h"
#include "population.h"
#include "test_util.h"
#include "v124_animal_expected.h"
#include "v124_recipe_expected.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static World *fresh124(int seed) {
    CoreNewGameParams p = {0};
    World *w = NULL;
    strcpy(p.castle_name, u8"1.2.4 검사");
    p.ruleset = 1;
    p.seed = seed;
    if (core_create(&p, NULL, &w) != CORE_OK)
        return NULL;
    while (w->villager_count < 8)
        if (!population_create(w, w->keep_x, w->keep_y + 5)) {
            core_destroy(w);
            return NULL;
        }
    return w;
}
static Building *facility124(World *w, int type, int x, int y) {
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
    b->operating = 1;
    return b;
}
static void roundtrip124(World *w) {
    char file[128], backup[132];
    snprintf(file, sizeof(file), "%s.sav", test_current_name());
    snprintf(backup, sizeof(backup), "%s.bak", file);
    World *copy = NULL;
    int32_t line = 0;
    uint64_t a = 0, b = 0;
    CHECK_EQ(core_state_hash(w, &a), CORE_OK);
    CHECK_EQ(core_save(w, file), CORE_OK);
    CHECK_EQ(core_load(file, NULL, &copy, &line), CORE_OK);
    CHECK(copy);
    CHECK_EQ(core_state_hash(copy, &b), CORE_OK);
    CHECK_EQ(a, b);
    CHECK_EQ(core_validate(copy), 0);
    core_destroy(copy);
    remove(file);
    remove(backup);
}
static void recipe124(int id) {
    World *w = fresh124(id);
    CHECK(w);
    CoreBarterRecipe r;
    CHECK_EQ(core_expansion_barter_recipe(id, &r), CORE_OK);
    const int *v = EXPECTED_RECIPES[id - 1];
    CHECK_EQ(r.rank, v[0]);
    CHECK_EQ(r.pay_resource, v[1]);
    CHECK_EQ(r.pay_count, v[2]);
    CHECK_EQ(r.get_resource, v[3]);
    CHECK_EQ(r.get_count, v[4]);
    CHECK_EQ(r.max_uses, v[5]);
    CHECK_EQ(r.weight, v[6]);
    CHECK(abs(r.benefit_bp - v[7]) <= 1);
    CHECK(r.benefit_bp > 0);
    CoreWanderingMerchantView *m = &w->expansion->wandering;
    m->state = CORE_MERCHANT_TRADING;
    m->rank = r.rank;
    m->visit_id = id;
    m->expires_tick = 432;
    m->offer_count = (int[]){0, 3, 4, 5, 5, 6}[r.rank];
    m->recipe_ids[0] = id;
    m->remaining[0] = r.max_uses;
    int target_mana = (int[]){0, 1, 1, 2, 2, 3}[r.rank], mana = r.get_resource == CORE_RES_MANA,
        next_mana = 1, next_general = 14;
    for (int slot = 1; slot < m->offer_count; slot++) {
        int chosen = 0;
        CoreBarterRecipe extra;
        do {
            chosen = mana < target_mana ? next_mana++ : next_general++;
        } while (chosen == id);
        core_expansion_barter_recipe(chosen, &extra);
        m->recipe_ids[slot] = chosen;
        m->remaining[slot] = extra.max_uses;
        mana += extra.get_resource == CORE_RES_MANA;
    }
    w->rank = r.rank;
    w->bank.amount[r.pay_resource] = r.pay_count - 1;
    w->expansion->fractional[r.pay_resource] = 999;
    w->bank.amount[r.get_resource] = 0;
    w->expansion->fractional[r.get_resource] = 0;
    int32_t reason = 0;
    uint64_t a = 0, b = 0;
    core_state_hash(w, &a);
    CHECK_EQ(core_expansion_merchant_trade(w, id, id, 1, &reason), CORE_ERR_REJECTED);
    CHECK_EQ(reason, CORE_DENY_NO_GOODS);
    core_state_hash(w, &b);
    CHECK_EQ(a, b);
    w->bank.amount[r.pay_resource] = r.pay_count;
    w->expansion->fractional[r.pay_resource] = 0;
    int before = expansion_bank_have(w, r.pay_resource);
    CHECK_EQ(core_expansion_merchant_trade(w, id, id, 1, &reason), CORE_OK);
    CHECK_EQ(expansion_bank_have(w, r.pay_resource), before - r.pay_count * 1000);
    CHECK_EQ(expansion_bank_have(w, r.get_resource), r.get_count * 1000);
    CHECK_EQ(m->remaining[0], r.max_uses - 1);
    core_state_hash(w, &a);
    CHECK_EQ(core_expansion_merchant_trade(w, id, id, 1, &reason), CORE_IGNORED);
    core_state_hash(w, &b);
    CHECK_EQ(a, b);
    roundtrip124(w);
    core_destroy(w);
}
static void animal124(int kind) {
    CoreAnimalInfo a;
    CHECK_EQ(core_expansion_animal_info(kind, &a), CORE_OK);
    int actual[14] = {a.hp,          a.attack,         a.food,         a.leather,     a.biomass,
                      a.speed_milli, a.swim_milli,     a.behavior,     a.width_milli, a.height_milli,
                      a.livestock,   a.attack_minutes, a.attack_range, a.mana_drop};
    for (int i = 0; i < 14; i++)
        CHECK_EQ(actual[i], EXPECTED_ANIMALS[kind][i]);
    CHECK(a.name[0]);
    int expected = a.livestock ? 2 : kind >= 24 && kind <= 29 ? 1 : 0;
    CHECK_EQ(core_expansion_animal_class(kind), expected);
    if (a.livestock) {
        CoreLivestockInfo ranch, pasture;
        CHECK_EQ(core_expansion_livestock_info(kind, CORE_BLD_RANCH, &ranch), CORE_OK);
        CHECK_EQ(core_expansion_livestock_info(kind, CORE_BLD_PASTURE, &pasture), CORE_OK);
        CHECK(ranch.allowed || pasture.allowed);
        CHECK(ranch.price > 0);
        CHECK(ranch.mature_days < ranch.life_days);
    }
    World *w = fresh124(42);
    CHECK(w);
    int p = grid_index(&w->grid, 10, 10);
    w->expansion->tiles[p].water_id = 1;
    w->grid.terrain[p] = CORE_TERRAIN_WATER;
    w->expansion->water_cold[0] = 0;
    CHECK_EQ(expansion_animal_habitat(w, kind, 10, 10),
             !a.livestock && a.swim_milli > 0 && kind != 6 && kind != 21);
    w->expansion->water_cold[0] = 1;
    CHECK_EQ(expansion_animal_habitat(w, kind, 10, 10),
             !a.livestock && a.swim_milli > 0 && kind != 13 && kind != 18);
    core_destroy(w);
}
int test_v124_parameter(const char *name) {
    int id = -1;
    if (sscanf(name, "v124_recipe_%d", &id) == 1 && id >= 1 && id <= 85) {
        recipe124(id);
        return 1;
    }
    if (sscanf(name, "v124_animal_%d", &id) == 1 && id >= 0 && id < 35) {
        animal124(id);
        return 1;
    }
    return 0;
}
void test_v124_harvest_six_days(void) {
    World *w = fresh124(42);
    CHECK(w);
    Building *b = facility124(w, CORE_BLD_CROPLAND, 20, 20);
    CHECK(b);
    const int totals[4] = {103000, 100000, 0, INT32_MAX}, percent[6] = {10, 25, 45, 70, 90, 100};
    for (int k = 0; k < 4; k++) {
        b->annual_harvest_year = 1;
        b->annual_harvest_total = totals[k];
        b->annual_harvest_released = b->annual_harvest_day = b->harvest_deferred_hi = b->harvest_deferred_lo =
            0;
        for (int d = 0; d < 6; d++) {
            w->clock.tick = (24 + d) * 144 + 47;
            expansion_harvest_release(w);
            if (d == 0)
                CHECK_EQ(expansion_harvest_deferred(b), 0);
            w->clock.tick++;
            expansion_harvest_release(w);
            CHECK_EQ(expansion_harvest_deferred(b), (int64_t)totals[k] * percent[d] / 100);
            expansion_harvest_release(w);
            CHECK_EQ(expansion_harvest_deferred(b), (int64_t)totals[k] * percent[d] / 100);
        }
        CHECK_EQ(b->annual_harvest_released, totals[k]);
    }
    b->annual_harvest_total = b->annual_harvest_released = 103000;
    b->annual_harvest_day = 30;
    b->harvest_deferred_hi = b->harvest_deferred_lo = 0;
    expansion_harvest_defer(b, 5000000000LL);
    CHECK_EQ(expansion_harvest_deferred(b), 5000000000LL);
    roundtrip124(w);
    core_destroy(w);
}
void test_v124_immigration_samples(void) {
    World *w = fresh124(5);
    CHECK(w);
    const int probability[6] = {0, 35, 40, 45, 50, 55}, lo[6] = {2, 2, 3, 3, 4, 4},
              hi[6] = {2, 4, 5, 6, 6, 7};
    for (int rank = 1; rank < 6; rank++) {
        w->rank = rank;
        int count = 0;
        w->clock.tick = 144;
        for (int i = 0; i < 100000; i++) {
            int wait = expansion_immigration_wait(w);
            CHECK(wait == 2 || wait == 3);
            count += wait == 3;
            int n = expansion_immigration_count(w, 100);
            CHECK(n >= lo[rank] && n <= hi[rank]);
        }
        CHECK(abs(count - probability[rank] * 1000) < 1000);
        w->clock.tick = 36 * 144;
        for (int i = 0; i < 1000; i++) {
            int n = expansion_immigration_count(w, 100);
            CHECK(n >= (lo[rank] + 1) / 2 && n <= (hi[rank] + 1) / 2);
        }
        CHECK_EQ(expansion_immigration_count(w, 0), 0);
        CHECK_EQ(expansion_immigration_count(w, 1), 1);
    }
    core_destroy(w);
}
void test_v124_livestock_purchase(void) {
    World *w = fresh124(10);
    CHECK(w);
    Building *ranch = facility124(w, CORE_BLD_RANCH, 20, 20),
             *pasture = facility124(w, CORE_BLD_PASTURE, 30, 20);
    CHECK(ranch && pasture);
    const int kinds[7] = {7, 9, 8, 10, 32, 33, 34}, prices[7] = {35, 14, 18, 5, 20, 27, 9};
    for (int type = 0; type < 2; type++)
        for (int k = 0; k < 7; k++) {
            Building *b = type ? pasture : ranch;
            CoreLivestockInfo info;
            core_expansion_livestock_info(kinds[k], b->type, &info);
            CHECK_EQ(info.price, prices[k]);
            w->bank.amount[CORE_RES_GOLD] = prices[k] - 1;
            w->expansion->fractional[CORE_RES_GOLD] = 999;
            int n = w->expansion->animal_count;
            CHECK_EQ(core_expansion_buy_animal(w, b->id, kinds[k]), CORE_ERR_REJECTED);
            CHECK_EQ(w->expansion->animal_count, n);
            CHECK_EQ(expansion_bank_have(w, CORE_RES_GOLD), prices[k] * 1000 - 1);
            w->bank.amount[CORE_RES_GOLD] = prices[k];
            w->expansion->fractional[CORE_RES_GOLD] = 0;
            int rc = core_expansion_buy_animal(w, b->id, kinds[k]);
            CHECK_EQ(rc, info.allowed ? CORE_OK : CORE_ERR_REJECTED);
            CHECK_EQ(expansion_bank_have(w, CORE_RES_GOLD), info.allowed ? 0 : prices[k] * 1000);
        }
    roundtrip124(w);
    core_destroy(w);
}
void test_v124_rank_groups(void) {
    World *w = fresh124(14);
    CHECK(w);
    CoreRankRequirementView rows[24];
    for (int rank = 0; rank < 5; rank++) {
        w->rank = rank;
        int n = core_expansion_rank_requirements(w, rows, 24);
        CHECK(n > 0 && n <= 24);
        for (int i = 0; i < n; i++)
            if (rows[i].kind == 2) {
                CHECK(rows[i].type_count > 0);
                for (int j = 0; j < rows[i].type_count; j++) {
                    CoreExpansionBuildingInfo info;
                    core_expansion_building_info(rows[i].types[j], &info);
                    CHECK(info.rank <= rank);
                }
                for (int j = 0; j < w->building_count; j++)
                    for (int k = 0; k < rows[i].type_count; k++)
                        if (w->buildings[j]->type == rows[i].types[k])
                            w->buildings[j]->state = CORE_BSTATE_RUINED;
                Building *b = facility124(w, rows[i].types[0], 10 + rank * 10, 20 + i * 3);
                CHECK(b);
                b->state = CORE_BSTATE_CONSTRUCTING;
                CoreRankRequirementView copy[24];
                CHECK_EQ(core_expansion_rank_requirements(w, copy, 24), n);
                CHECK(!copy[i].satisfied);
                b->state = CORE_BSTATE_ACTIVE;
                CHECK_EQ(core_expansion_rank_requirements(w, copy, 24), n);
                CHECK(copy[i].satisfied);
                CHECK_EQ(copy[i].building_id, b->id);
                b->state = CORE_BSTATE_RUINED;
            }
    }
    w->rank = 5;
    CHECK_EQ(core_expansion_rank_requirements(w, rows, 24), 0);
    core_destroy(w);
}
void test_v124_medical_qualification(void) {
    World *w = fresh124(11);
    CHECK(w);
    w->clock.tick = 54;
    const int types[3] = {CORE_BLD_HEALER, CORE_BLD_CLINIC, CORE_BLD_HOSPITAL};
    const int stats[12][3] = {{3, 0, 0}, {4, 0, 0}, {5, 0, 0}, {6, 0, 0}, {7, 0, 0}, {4, 4, 0},
                              {5, 2, 0}, {6, 4, 0}, {6, 3, 0}, {5, 5, 0}, {6, 2, 2}, {4, 4, 4}};
    for (int t = 0; t < 3; t++) {
        Building *b = facility124(w, types[t], 20 + t * 8, 20);
        CHECK(b);
        for (int k = 0; k < 12; k++) {
            for (int i = 0; i < w->villager_count; i++)
                entity_unlink_worker(w->villagers[i]);
            int workers = 0, sum = 0, highest = 0;
            for (int i = 0; i < 3 && i < w->defs.buildings[types[t]].worker_slots; i++)
                if (stats[k][i]) {
                    Villager *v = w->villagers[i];
                    CHECK_EQ(entity_link_worker(b, v, 3), CORE_OK);
                    v->stats.intellect = stats[k][i];
                    v->at_target = 1;
                    v->inside_building = b->id;
                    v->state = CORE_VS_WORKING;
                    v->target = b;
                    v->meal_day = 1;
                    v->needs.hunger = v->needs.rest = 100;
                    workers++;
                    sum += stats[k][i];
                    if (stats[k][i] > highest)
                        highest = stats[k][i];
                }
            CoreMedicalView med;
            CHECK_EQ(core_expansion_medical(w, b->id, &med), CORE_OK);
            int qualified = t == 0   ? workers >= 1 && highest >= 4
                            : t == 1 ? workers >= 1 && highest >= 5 && sum >= 7
                                     : workers >= 2 && highest >= 6 && sum >= 10;
            CHECK_EQ(med.qualified, qualified);
            CHECK_EQ(med.workers, workers);
            CHECK_EQ(med.base_patients, qualified ? (t == 0   ? 2
                                                     : t == 1 ? (workers == 1 ? 3 : 4)
                                                              : (workers == 2 ? 5 : 8))
                                                  : 0);
            Villager *v = w->villagers[7];
            for (int sick = 0; sick < 4; sick++) {
                v->health = sick & 1 ? 35 : 100;
                v->critical = 0;
                v->sick_ticks = sick & 2 ? 10 : 0;
                w->expansion->plague_days = sick & 2 ? 1 : 0;
                CHECK_EQ(expansion_heal_herbs_required(w, v, b),
                         (t == 2 ? 3 : 2) + (!!(sick & 1)) + (!!(sick & 2)));
            }
        }
    }
    core_destroy(w);
}
void test_v124_real_v11_preserved(void) {
    World *w = NULL;
    int32_t line = 0;
    CHECK_EQ(core_load(CASTLE_SOURCE_DIR "/tests/fixtures/expansion_v11.sav", NULL, &w, &line), CORE_OK);
    CHECK(w);
    CHECK_EQ(w->grid.width, 160);
    CHECK_EQ(core_validate(w), 0);
    CHECK(w->villager_count >= 8);
    roundtrip124(w);
    core_destroy(w);
}
void test_v124_migrate_barracks(void) {
    World *w = fresh124(12);
    CHECK(w);
    Building *b = facility124(w, CORE_BLD_BARRACKS, 20, 20),
             *pasture = facility124(w, CORE_BLD_PASTURE, 30, 20);
    CHECK(b && pasture);
    w->defs.buildings[CORE_BLD_BARRACKS].worker_slots = 4;
    for (int i = 0; i < 4; i++) {
        CHECK_EQ(entity_link_worker(b, w->villagers[i], 4), CORE_OK);
        CHECK_EQ(entity_link_worker(pasture, w->villagers[i + 4], 5), CORE_OK);
    }
    Villager *lowest = NULL;
    int score = INT32_MAX;
    for (int i = 0; i < 4; i++) {
        Villager *v = b->workers[i];
        int e = expansion_work_percent(w, v, b->type);
        if (e < score || (e == score && (!lowest || v->id > lowest->id))) {
            lowest = v;
            score = e;
        }
    }
    int id = lowest->id;
    w->generation_version = 11;
    expansion_v124_migrate(w);
    CHECK_EQ(b->worker_count, 3);
    CHECK_EQ(pasture->worker_count, 4);
    CHECK(entity_find_villager(w, id)->workplace == NULL);
    CHECK_EQ(w->villager_count, 8);
    CHECK_EQ(w->defs.buildings[CORE_BLD_PASTURE].worker_slots, 5);
    core_destroy(w);
}
void test_v124_catalog(void) {
    CHECK_EQ(CORE_ABI_VERSION, 14);
    CHECK_EQ(CORE_SAVE_VERSION, 12);
    CHECK_EQ(CORE_BLD_COUNT, 73);
    CHECK_EQ(CORE_EXP_ANIMALS, 35);
    CHECK_EQ(core_expansion_barter_count(), 85);
    CHECK(!strcmp(core_expansion_building_name(CORE_BLD_STALL), u8"가판대"));
    CHECK(!strcmp(core_expansion_building_name(CORE_BLD_RANCH), u8"축사"));
    CHECK(!strcmp(core_expansion_building_name(CORE_BLD_STONE_ROAD), u8"자갈길"));
    int seen[73] = {0};
    for (int i = 0; i < 72; i++) {
        int type = core_expansion_display_building(i);
        CHECK(type > 0 && type < 73);
        CHECK(!seen[type]);
        seen[type] = 1;
    }
    for (int i = 1; i < 73; i++)
        CHECK(seen[i]);
    CoreExpansionBuildingInfo clinic, light, road;
    core_expansion_building_info(CORE_BLD_CLINIC, &clinic);
    core_expansion_building_info(CORE_BLD_LIGHTHOUSE, &light);
    core_expansion_building_info(CORE_BLD_PAVED_ROAD, &road);
    CHECK_EQ(clinic.workers, 2);
    CHECK_EQ(clinic.rank, 2);
    CHECK_EQ(light.sight, 12);
    CHECK_EQ(light.workers, 0);
    CHECK_EQ(road.cost[CORE_RES_GOLD], 3);
    CHECK_EQ(road.cost[CORE_RES_STONE], 3);
    CHECK_EQ(road.cost[CORE_RES_BRICKS], 2);
}

void test_v124_harvest_actual_tick(void) {
    World *w=fresh124(42);CHECK(w);Building *b=facility124(w,CORE_BLD_CROPLAND,20,20);CHECK(b);b->crop_year=1;b->crop_labor_milli=2*540*24*1000;
    w->clock.tick=24*144+47;CHECK_EQ(core_tick(w,1),CORE_OK);CHECK_EQ(b->annual_harvest_total,360000);CHECK_EQ(b->annual_harvest_released,36000);CHECK_EQ(b->annual_harvest_day,25);
    CHECK_EQ(core_tick(w,1),CORE_OK);CHECK_EQ(b->annual_harvest_released,36000);core_destroy(w);
}
