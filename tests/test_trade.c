/* trade — 현행 기능별 회귀 사례 */
#include "fixtures/services.h"
#include "fixtures/settlement.h"

void test_settlement_market_split_quotes(void) {
    World *w = fx_settlement_make_world();
    if (!w)
        return;
    Building *market = fx_settlement_add_building(w, CORE_BLD_TRADING_POST, 40, 40);
    CHECK_EQ(entity_link_worker(market, w->villagers[0], 4), CORE_OK);
    w->clock.tick = 54;
    Villager *staff = w->villagers[0];
    staff->incoming = 0;
    staff->state = CORE_VS_WORKING;
    staff->target = market;
    staff->at_target = 1;
    staff->target_x = market->door_x;
    staff->target_y = market->door_y;
    staff->current_x_milli = market->door_x * 1000 + 500;
    staff->current_y_milli = market->door_y * 1000 + 500;
    w->bank.amount[CORE_RES_GOLD] = 10000;
    w->expansion->trade_capacity_milli = 260000;
    w->expansion->trade_stock[CORE_RES_IRON] = 260;
    w->bank.limits[CORE_RES_IRON] = 1000;
    CoreTradeQuote all, first, second;
    int32_t gold = 0, why = 0;
    core_expansion_quote(w, CORE_RES_IRON, 20, &all);
    core_expansion_quote(w, CORE_RES_IRON, 10, &first);
    CHECK_EQ(core_trade(w, CORE_RES_IRON, 10, &gold, &why), CORE_OK);
    CHECK_EQ(-gold, first.total_gold);
    core_expansion_quote(w, CORE_RES_IRON, 10, &second);
    CHECK_EQ(first.total_gold + second.total_gold, all.total_gold);
    CHECK_EQ(second.next_pressure, all.next_pressure);
    CHECK_EQ(second.next_remainder, all.next_remainder);
    CHECK_EQ(core_trade(w, CORE_RES_IRON, 10, &gold, &why), CORE_OK);
    w->expansion->trade_used = 0;
    w->expansion->trade_capacity_milli = 260000;
    CHECK_EQ(core_trade(w, CORE_RES_IRON, 30, &gold, &why), CORE_OK);
    CHECK_EQ(w->expansion->trade_pressure[CORE_RES_IRON], 500);
    expansion_market_daily(w);
    CHECK_EQ(w->expansion->trade_pressure[CORE_RES_IRON], 400);
    w->clock.tick = 36 * 144;
    core_expansion_quote(w, CORE_RES_FOOD, 1, &all);
    CHECK_EQ(all.season_bp, 500);
    CHECK_EQ(all.unit_cents, 210);
    market->type = CORE_BLD_MARKET;
    core_expansion_quote(w, CORE_RES_MANA, 1, &all);
    CHECK_EQ(all.reason, CORE_DENY_NOT_TRADABLE);
    core_destroy(w);
}

void test_services_barter_atomic(void) {
    World *w = fx_services_fresh(7);
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

void test_services_merchant_visit_and_save(void) {
    World *w = fx_services_fresh(17), *copy = NULL;
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
    CHECK_EQ(core_save(w, "services_merchant.sav"), CORE_OK);
    CHECK_EQ(core_load("services_merchant.sav", NULL, &copy, &line), CORE_OK);
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
    remove("services_merchant.sav");
    remove("services_merchant.sav.bak");
}
