/* hunting — 현행 기능별 회귀 사례 */
#include "fixtures/services.h"
#include "fixtures/settlement.h"

void test_settlement_hunting_and_debug_cap(void) {
    World *w = fx_settlement_make_world();
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
    CHECK_EQ(w->expansion->region_pressure_until[region], 0);
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

void test_services_hunter_retaliation(void) {
    World *w = fx_services_fresh(42);
    CHECK(w);
    Building *b = fx_services_facility(w, CORE_BLD_HUNTER_CAMP, 20, 20);
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
