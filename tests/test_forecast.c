/* forecast — 현행 기능별 회귀 사례 */
#include "fixtures/services.h"

void test_services_forecast_rng(void) {
    World *w = fx_services_fresh(9);
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
