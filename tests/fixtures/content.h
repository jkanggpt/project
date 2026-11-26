#ifndef CASTLE_TEST_FIXTURE_CONTENT_H
#define CASTLE_TEST_FIXTURE_CONTENT_H
/* content.h — 현재 통합 테스트의 공통 준비 조건 */
#include "animal_expected.h"
#include "barter_expected.h"
#include "expansion.h"
#include "population.h"
#include "test_util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static inline World *fx_content_content_world(int seed) {
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

static inline Building *fx_content_content_facility(World *w, int type, int x, int y) {
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

static inline void fx_content_current_roundtrip(World *w) {
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

static inline void fx_content_recipe124(int id) {
    World *w = fx_content_content_world(id);
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
    fx_content_current_roundtrip(w);
    core_destroy(w);
}

static inline void fx_content_animal124(int kind) {
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
    World *w = fx_content_content_world(42);
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
#endif
