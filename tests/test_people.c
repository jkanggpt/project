/* people — 현행 기능별 회귀 사례 */
#include "fixtures/content.h"
#include "fixtures/labor.h"
#include "fixtures/services.h"
#include "fixtures/settlement.h"

void test_settlement_population_floor_and_refill(void) {
    World *w = fx_settlement_make_world();
    if (!w)
        return;
    fx_settlement_add_people(w, 5);
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

void test_settlement_immigration_status(void) {
    World *w = fx_settlement_make_world();
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

void test_labor_identity_and_save(void) {
    World *w = fx_labor_world121(), *loaded = NULL;
    if (!w)
        return;
    CHECK_EQ(w->defs.name_count, 324);
    int male = 0, female = 0, hair = 0;
    for (int i = 0; i < 80; i++) {
        Villager *v = population_create(w, w->keep_x, w->keep_y + 4);
        CHECK(v != NULL);
        CHECK_EQ(v->appearance % 2, v->sex);
        CHECK(!strchr(v->name, ':'));
        male += !v->sex;
        female += v->sex;
        hair |= 1 << (v->appearance < 12 ? v->appearance / 2 : (v->appearance - 12) / 6);
        for (int n = 0; n < w->defs.name_count; n++)
            if (strcmp(v->name, w->defs.names[n].text + 2) == 0)
                CHECK_EQ(w->defs.names[n].text[0], v->sex ? 'F' : 'M');
    }
    CHECK(male && female);
    CHECK_EQ(hair, 63);
    w->villagers[0]->xp_work_minutes[5] = 359;
    w->villagers[0]->healing_progress_milli = 12345;
    w->expansion->plague_days = 3;
    w->expansion->economic_event = CORE_NATURE_TRADE_BOOM;
    w->expansion->economic_days = 4;
    w->expansion->phantom_next_day[4] = 123;
    w->expansion->next_raid_tick = 5678;
    expansion_rebuild(w);
    expansion_visibility(w);
    uint64_t before, after;
    CHECK_EQ(core_state_hash(w, &before), CORE_OK);
    CHECK_EQ(core_save(w, "labor_roundtrip.sav"), CORE_OK);
    CHECK_EQ(core_load("labor_roundtrip.sav", NULL, &loaded, NULL), CORE_OK);
    if (loaded) {
        CHECK_EQ(core_state_hash(loaded, &after), CORE_OK);
        CHECK_EQ(before, after);
        core_destroy(loaded);
    }
    core_destroy(w);
    remove("labor_roundtrip.sav");
    remove("labor_roundtrip.sav.bak");
}

void test_services_exile_floor(void) {
    World *w = fx_services_fresh(3);
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

void test_content_immigration_samples(void) {
    World *w = fx_content_content_world(5);
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
