/* livestock — 현행 기능별 회귀 사례 */
#include "fixtures/content.h"
#include "fixtures/labor.h"

void test_labor_livestock_age_and_breeding(void) {
    World *w = fx_labor_world121();
    if (!w)
        return;
    Building *b = fx_labor_building121(w, CORE_BLD_RANCH, 80, 80);
    w->bank.amount[CORE_RES_GOLD] = 1000;
    CHECK_EQ(core_expansion_buy_animal(w, b->id, 10), CORE_OK);
    CHECK_EQ(core_expansion_buy_animal(w, b->id, 10), CORE_OK);
    int first = w->expansion->animal_count - 2;
    CHECK_EQ(w->expansion->animals[first].age, 0);
    for (int i = first; i < first + 2; i++)
        w->expansion->animals[i].age = 7;
    b->daily_work_milli = w->defs.buildings[b->type].worker_slots * 540000;
    b->breeding_progress[3] = 7000;
    w->bank.amount[CORE_RES_FOOD] = 100;
    expansion_animal_daily(w);
    int adults = 0, babies = 0;
    for (int i = 0; i < w->expansion->animal_count; i++)
        if (w->expansion->animals[i].alive && w->expansion->animals[i].owner == b->id) {
            adults += w->expansion->animals[i].age >= 8;
            babies += w->expansion->animals[i].age == 0;
        }
    CHECK_EQ(adults, 2);
    CHECK_EQ(babies, 2);
    CHECK_EQ(b->breeding_progress[3], 0);
    CHECK_EQ(b->feed_milli, 150);
    CHECK_EQ(expansion_bank_have(w, CORE_RES_FOOD), 100550);
    for (int i = first; i < first + 2; i++)
        w->expansion->animals[i].age = 27;
    b->daily_work_milli = 0;
    expansion_animal_daily(w);
    CHECK(!w->expansion->animals[first].alive);
    core_destroy(w);
}

void test_content_livestock_purchase(void) {
    World *w = fx_content_content_world(10);
    CHECK(w);
    Building *ranch = fx_content_content_facility(w, CORE_BLD_RANCH, 20, 20),
             *pasture = fx_content_content_facility(w, CORE_BLD_PASTURE, 30, 20);
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
    fx_content_current_roundtrip(w);
    core_destroy(w);
}

void test_content_livestock_daily_budget(void) {
    World *w = fx_content_content_world(10);
    CHECK(w);
    Building *ranch = fx_content_content_facility(w, CORE_BLD_RANCH, 20, 20),
             *pasture = fx_content_content_facility(w, CORE_BLD_PASTURE, 30, 20);
    CHECK(ranch && pasture);
    const int kinds[7] = {7, 9, 8, 10, 32, 33, 34};
    w->bank.amount[CORE_RES_GOLD] = 1000;
    for (int p = 0; p < w->expansion->cells; p++)
        w->expansion->tiles[p].biome = CORE_BIOME_GRASS;
    for (int k = 0; k < 7; k++)
        CHECK_EQ(core_expansion_buy_animal(w, k == 1 || k == 3 ? ranch->id : pasture->id, kinds[k]), CORE_OK);
    const int feed[4] = {2480, 1614, 1240, 808};
    for (int mode = 0; mode < 4; mode++) {
        w->clock.tick = (mode % 2 ? 0 : 36 * 144);
        w->bank.amount[CORE_RES_FOOD] = 100;
        w->expansion->fractional[CORE_RES_FOOD] = 0;
        for (int i = 0; i < w->expansion->animal_count; i++)
            if (w->expansion->animals[i].owner) {
                CoreAnimalView *a = &w->expansion->animals[i];
                CoreLivestockInfo info;
                core_expansion_livestock_info(a->kind, CORE_BLD_PASTURE, &info);
                a->age = mode < 2 ? info.mature_days + 1 : 0;
            }
        int32_t daily[CORE_RES_COUNT];
        CHECK_EQ(core_expansion_daily_requirements(w, daily), CORE_OK);
        CHECK_EQ(daily[CORE_RES_FOOD], 8000 + feed[mode]);
        expansion_animal_daily(w);
        CHECK_EQ(ranch->feed_milli + pasture->feed_milli, feed[mode]);
        CHECK_EQ(expansion_bank_have(w, CORE_RES_FOOD), 100000 - feed[mode]);
    }
    core_destroy(w);
}
