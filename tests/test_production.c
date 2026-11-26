/* production — 현행 기능별 회귀 사례 */
#include "fixtures/content.h"
#include "fixtures/labor.h"

void test_labor_exact_crop_and_magic_cost(void) {
    World *w = fx_labor_world121();
    if (!w)
        return;
    Building *farm = fx_labor_building121(w, CORE_BLD_FARM, 80, 80);
    int baseline = 3 * 540 * 24 * 1000;
    w->clock.tick = 24 * 144 + 48;
    farm->crop_labor_milli = baseline / 24 * 21 + baseline / 24 * 3 / 4;
    expansion_produce(w);
    CHECK_EQ(farm->annual_harvest_total, 543000);
    farm->crop_year = 0;
    farm->pending_harvest_milli = 0;
    farm->annual_harvest_year = 0;
    farm->annual_harvest_total = 0;
    farm->crop_labor_milli = baseline / 24 * 21 + baseline / 24 * 3 * 24 / 10;
    expansion_produce(w);
    CHECK_EQ(farm->annual_harvest_total, 705000);
    Building *magic = fx_labor_building121(w, CORE_BLD_MAGIC_WORKSHOP, 70, 70);
    int n = w->defs.buildings[magic->type].worker_slots;
    magic->daily_work_milli = n * 540000 * 75 / 10;
    magic->actual_work_milli = n * 540000;
    w->expansion->distance[grid_index(&w->grid, magic->door_x, magic->door_y)] = 0;
    w->research.active = 0;
    w->bank.amount[CORE_RES_MANA] = 10;
    expansion_daily(w);
    CHECK_EQ(w->bank.amount[CORE_RES_MANA], 9);
    CHECK_EQ(w->research.progress_milli, 22500);
    core_destroy(w);
}

void test_content_harvest_six_days(void) {
    World *w = fx_content_content_world(42);
    CHECK(w);
    Building *b = fx_content_content_facility(w, CORE_BLD_CROPLAND, 20, 20);
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
    fx_content_current_roundtrip(w);
    core_destroy(w);
}

void test_content_harvest_actual_tick(void) {
    World *w = fx_content_content_world(42);
    CHECK(w);
    Building *b = fx_content_content_facility(w, CORE_BLD_CROPLAND, 20, 20);
    CHECK(b);
    b->crop_year = 1;
    b->crop_labor_milli = 2 * 540 * 24 * 1000;
    w->clock.tick = 24 * 144 + 47;
    CHECK_EQ(core_tick(w, 1), CORE_OK);
    CHECK_EQ(b->annual_harvest_total, 360000);
    CHECK_EQ(b->annual_harvest_released, 36000);
    CHECK_EQ(b->annual_harvest_day, 25);
    CHECK_EQ(core_tick(w, 1), CORE_OK);
    CHECK_EQ(b->annual_harvest_released, 36000);
    core_destroy(w);
}
