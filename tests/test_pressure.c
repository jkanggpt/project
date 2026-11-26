/* 계절·사건·의료·거래·장부의 현재 규칙 */
#include "fixtures/town.h"
#include "market.h"
#include "nature.h"

void test_winter_farms_stop_and_houses_burn_wood(void) {
    World *w = fx_town_world(301);
    CHECK(w);
    Building *h = fx_town_building(w, CORE_BLD_SHACK, 0);
    CHECK(h);
    CHECK_EQ(entity_link_resident(h, w->villagers[0], 6), CORE_OK);
    w->nature.weather = CORE_WEATHER_CLEAR;
    w->clock.tick = 12 * 144 + 72;
    CHECK_EQ(nature_farm_percent(w), 120);
    w->clock.tick = 36 * 144 + 72;
    CHECK_EQ(nature_farm_percent(w), 0);
    int req[CORE_RES_COUNT];
    CHECK_EQ(core_expansion_daily_requirements(w, req), CORE_OK);
    CHECK(req[CORE_RES_COAL] + req[CORE_RES_LOGS] + req[CORE_RES_WOOD] > 0);
    w->bank.amount[CORE_RES_COAL] = w->bank.amount[CORE_RES_LOGS] = w->bank.amount[CORE_RES_WOOD] = 0;
    expansion_daily(w);
    CHECK(h->flags & CORE_BF_COLD);
    w->bank.amount[CORE_RES_LOGS] = 200;
    expansion_daily(w);
    CHECK(!(h->flags & CORE_BF_COLD));
    CHECK(w->bank.amount[CORE_RES_LOGS] < 200);
    core_destroy(w);
}
void test_nature_forecast_then_drought(void) {
    World *a = fx_town_world(302), *b = fx_town_world(302);
    CHECK(a && b);
    a->clock.tick = b->clock.tick = 13 * 144;
    a->nature.event = b->nature.event = CORE_NATURE_DROUGHT;
    a->nature.event_days = b->nature.event_days = 2;
    nature_daily(a);
    nature_daily(b);
    CHECK_EQ(a->nature.event, CORE_NATURE_DROUGHT);
    CHECK_EQ(a->nature.event_days, 1);
    CHECK_EQ(a->nature.event, b->nature.event);
    CHECK_EQ(a->rng.state, b->rng.state);
    a->clock.tick += 144;
    nature_daily(a);
    CHECK(a->nature.event != CORE_NATURE_DROUGHT || a->nature.event_days > 1);
    CHECK(fx_town_events(a, CORE_EVT_NATURE_ENDED) > 0);
    core_destroy(a);
    core_destroy(b);
}
void test_nature_events_follow_seasons(void) {
    World *a = fx_town_world(303), *b = fx_town_world(303);
    CHECK(a && b);
    for (int day = 1; day <= 96; day++) {
        a->clock.tick = b->clock.tick = (day - 1) * 144;
        nature_daily(a);
        nature_daily(b);
        CHECK_EQ(a->nature.event, b->nature.event);
        CHECK_EQ(a->nature.event_days, b->nature.event_days);
        CHECK_EQ(a->expansion->plague_days, b->expansion->plague_days);
        CHECK(a->nature.event != CORE_NATURE_DROUGHT || exp_season(a) == 1);
        CHECK(a->nature.event != CORE_NATURE_COLD_SNAP || exp_season(a) == 3);
        CHECK(a->expansion->plague_days <= 6);
    }
    core_destroy(a);
    core_destroy(b);
}
void test_storm_damage_ruin_and_repair(void) {
    World *w = fx_town_world(304);
    CHECK(w);
    Building *b = fx_town_building(w, CORE_BLD_GARDEN, 0);
    CHECK(b);
    int hp = b->hp, reason = 0;
    placement_damage(w, b, 7);
    CHECK_EQ(b->hp, hp - 7);
    CHECK_EQ(core_repair(w, b->id, &reason), CORE_OK);
    CHECK_EQ(b->hp, hp);
    placement_damage(w, b, hp);
    CHECK_EQ(b->state, CORE_BSTATE_RUINED);
    CHECK_EQ(core_repair(w, b->id, &reason), CORE_OK);
    CHECK_EQ(b->state, CORE_BSTATE_ACTIVE);
    CHECK(b->hp > 0);
    core_destroy(w);
}
void test_sickness_and_healer(void) {
    World *w = fx_town_world(305);
    CHECK(w);
    Building *b = fx_town_building(w, CORE_BLD_HEALER, 0);
    CHECK(b);
    Villager *staff = w->villagers[0], *patient = w->villagers[1];
    staff->stats.intellect = staff->initial_stats[4] = 4;
    fx_town_worker(w, b, staff);
    CoreMedicalView s;
    CHECK_EQ(core_expansion_medical(w, b->id, &s), CORE_OK);
    CHECK_EQ(s.workers, 1);
    CHECK(s.qualified);
    CHECK(nature_make_sick(w, patient));
    CHECK(patient->sick_ticks > 0);
    CHECK_EQ(core_expansion_healing_herbs(w, patient->id, b->id), 2);
    patient->health = 60;
    nature_cure(w, patient, 1);
    CHECK_EQ(patient->sick_ticks, 0);
    CHECK_EQ(patient->health, 80);
    CHECK(fx_town_events(w, CORE_EVT_VILLAGER_CURED) > 0);
    core_destroy(w);
}
void test_market_trade_rules(void) {
    World *w = fx_town_world(306);
    CHECK(w);
    CoreTradeQuote q;
    int gold = 0, reason = 0;
    CHECK_EQ(core_trade(w, CORE_RES_FOOD, 1, &gold, &reason), CORE_ERR_REJECTED);
    Building *b = fx_town_building(w, CORE_BLD_STALL, 0);
    CHECK(b);
    fx_town_worker(w, b, w->villagers[0]);
    w->expansion->trade_capacity_milli = 260000;
    w->expansion->trade_stock[CORE_RES_FOOD] = 30;
    w->bank.amount[CORE_RES_FOOD] = 0;
    CHECK_EQ(core_expansion_quote(w, CORE_RES_FOOD, 1, &q), CORE_OK);
    CHECK_EQ(q.reason, CORE_DENY_NONE);
    int before = w->bank.amount[CORE_RES_GOLD];
    CHECK_EQ(core_trade(w, CORE_RES_FOOD, 1, &gold, &reason), CORE_OK);
    CHECK_EQ(w->bank.amount[CORE_RES_FOOD], 1);
    CHECK_EQ(w->bank.amount[CORE_RES_GOLD], before + gold);
    CHECK_EQ(-gold, q.total_gold);
    CHECK_EQ(core_expansion_quote(w, CORE_RES_GOLD_ORE, 1, &q), CORE_OK);
    CHECK_EQ(q.reason, CORE_DENY_NOT_TRADABLE);
    core_destroy(w);
}
void test_merchant_visits_and_deals(void) {
    World *w = fx_town_world(307);
    CHECK(w);
    w->rank = 1;
    CoreWanderingMerchantView *m = &w->expansion->wandering;
    m->next_target_tick = w->clock.tick + 1;
    for (int i = 0; i < 800 && m->state != CORE_MERCHANT_TRADING; i++) {
        w->clock.tick++;
        expansion_merchant_tick(w);
    }
    CHECK_EQ(m->state, CORE_MERCHANT_TRADING);
    CHECK_EQ(m->offer_count, 3);
    CHECK_EQ(m->expires_tick - m->arrived_tick, 432);
    for (int i = 0; i < m->offer_count; i++) {
        CoreBarterRecipe r;
        CHECK_EQ(core_expansion_barter_recipe(m->recipe_ids[i], &r), CORE_OK);
        CHECK(r.benefit_bp > 0);
        CHECK(r.rank <= w->rank);
    }
    w->clock.tick = m->expires_tick;
    expansion_merchant_tick(w);
    CHECK_EQ(m->state, CORE_MERCHANT_LEAVING);
    core_destroy(w);
}
void test_ledger_matches_bank(void) {
    World *w = fx_town_world(308);
    CHECK(w);
    int before[CORE_RES_COUNT];
    memcpy(before, w->bank.amount, sizeof(before));
    CHECK_EQ(expansion_bank_add(w, CORE_RES_GOLD, 7000, CORE_LEDGER_TRADE), 7000);
    CHECK_EQ(expansion_bank_add(w, CORE_RES_LOGS, -3000, CORE_LEDGER_CONSTRUCTION), -3000);
    for (int r = 0; r < CORE_RES_COUNT; r++) {
        int delta = 0;
        for (int c = 0; c < CORE_LEDGER_COUNT; c++)
            delta += w->ledger.today[r][c];
        CHECK_EQ(w->bank.amount[r] - before[r], delta);
    }
    eco_roll_ledger(&w->ledger);
    CHECK_EQ(w->ledger.today[CORE_RES_GOLD][CORE_LEDGER_TRADE], 0);
    CHECK_EQ(w->ledger.yesterday[CORE_RES_GOLD][CORE_LEDGER_TRADE], 7);
    core_destroy(w);
}
void test_save_roundtrip_world_pressure(void) {
    World *w = fx_town_world(309);
    CHECK(w);
    Building *b = fx_town_building(w, CORE_BLD_SHACK, 0);
    CHECK(b);
    CHECK_EQ(entity_link_resident(b, w->villagers[0], 6), CORE_OK);
    w->clock.tick = 13 * 144;
    w->nature.event = CORE_NATURE_DROUGHT;
    w->nature.event_days = 2;
    w->expansion->plague_days = 2;
    w->expansion->plague_cooldown = 10;
    w->expansion->economic_event = CORE_NATURE_TRADE_BOOM;
    w->expansion->economic_days = 2;
    fx_content_current_roundtrip(w);
    core_destroy(w);
}
