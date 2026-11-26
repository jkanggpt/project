/* medical — 현행 기능별 회귀 사례 */
#include "fixtures/content.h"
#include "fixtures/labor.h"
#include "fixtures/services.h"

void test_labor_medical_actual_staff(void) {
    World *w = fx_labor_world121();
    if (!w)
        return;
    Building *hospital = fx_labor_building121(w, CORE_BLD_HOSPITAL, 70, 70);
    w->clock.tick = 54;
    for (int i = 0; i < 3; i++) {
        Villager *v = w->villagers[i];
        entity_link_worker(hospital, v, 3);
        v->stats.intellect = i ? 4 : 6;
        fx_labor_present121(v, hospital);
    }
    Villager *a = w->villagers[0], *b = w->villagers[1], *off = w->villagers[2];
    off->at_target = 0;
    int sum = expansion_work_percent(w, a, hospital->type) + expansion_work_percent(w, b, hospital->type);
    expansion_begin_work(w, a, hospital);
    expansion_begin_work(w, b, hospital);
    CHECK_EQ(hospital->service_credit_milli, sum * 100 * 5 / (2 * 540));
    off->at_target = 1;
    CHECK_EQ(expansion_working_count(w, hospital), 3);
    core_destroy(w);
}

void test_services_water_and_medicine(void) {
    World *w = fx_services_fresh(20);
    CHECK(w);
    Building *home = fx_services_facility(w, CORE_BLD_HOUSE, 20, 20),
             *well = fx_services_facility(w, CORE_BLD_WELL, 26, 20),
             *tower = fx_services_facility(w, CORE_BLD_WATER_TOWER, 26, 21);
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
    Building *healer = fx_services_facility(w, CORE_BLD_HEALER, 40, 40),
             *hospital = fx_services_facility(w, CORE_BLD_HOSPITAL, 45, 40);
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

void test_content_medical_qualification(void) {
    World *w = fx_content_content_world(11);
    CHECK(w);
    w->clock.tick = 54;
    const int types[3] = {CORE_BLD_HEALER, CORE_BLD_CLINIC, CORE_BLD_HOSPITAL};
    const int stats[12][3] = {{3, 0, 0}, {4, 0, 0}, {5, 0, 0}, {6, 0, 0}, {7, 0, 0}, {4, 4, 0},
                              {5, 2, 0}, {6, 4, 0}, {6, 3, 0}, {5, 5, 0}, {6, 2, 2}, {4, 4, 4}};
    for (int t = 0; t < 3; t++) {
        Building *b = fx_content_content_facility(w, types[t], 20 + t * 8, 20);
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
