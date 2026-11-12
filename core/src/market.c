/*
 * market.c — 시장과 떠돌이 상인 (설명은 market.h)
 */
#include "market.h"

#include "world.h"

/* 기준 가격 (한 묶음 = 10개, 금화). 0 = 시장에서 다루지 않음 */
static const int32_t BASE_PRICE[CORE_RES_COUNT] = {0, 0, 8, 6, 9, 12};

/* 계절별 목표 가격 (기준의 %): 겨울에는 식량·목재가 귀해서 비쌉니다 */
static const int32_t SEASON_DEMAND[CORE_SEASON_COUNT][CORE_RES_COUNT] = {
    {0, 0, 100, 100, 100, 110}, /* 봄 */
    {0, 0, 90, 90, 100, 100},   /* 여름 */
    {0, 0, 110, 115, 100, 100}, /* 가을 */
    {0, 0, 140, 140, 110, 120}, /* 겨울 */
};

void market_init(Market *market)
{
    int32_t i;
    for (i = 0; i < CORE_RES_COUNT; i++) {
        market->price[i] = BASE_PRICE[i] * 100;
    }
    market->spread_pct = MARKET_SPREAD_PCT;
}

void merchant_init(Merchant *merchant)
{
    merchant->days_left = 0;
    merchant->next_visit_day = MERCHANT_FIRST_VISIT_DAY;
    merchant->deal_count = 0;
}

int market_is_tradable(int32_t resource)
{
    return resource >= 0 && resource < CORE_RES_COUNT && BASE_PRICE[resource] > 0;
}

int32_t market_base_price(int32_t resource)
{
    return market_is_tradable(resource) ? BASE_PRICE[resource] : 0;
}

static int32_t clamp_price(int32_t resource, int32_t price)
{
    int32_t low = BASE_PRICE[resource] * MARKET_MIN_PCT;
    int32_t high = BASE_PRICE[resource] * MARKET_MAX_PCT;
    return price < low ? low : (price > high ? high : price);
}

/* 가격(× 100) → 살 때 금화: 차이의 절반만큼 비싸게, 올림 */
static int32_t buy_from(const Market *market, int32_t price)
{
    int32_t gold = (price * (200 + market->spread_pct) / 200 + 99) / 100;
    return gold < 1 ? 1 : gold;
}

/* 가격(× 100) → 팔 때 금화: 차이의 절반만큼 싸게, 내림. 사는 값보다 언제나 작게 */
static int32_t sell_from(const Market *market, int32_t price)
{
    int32_t gold = price * (200 - market->spread_pct) / 200 / 100;
    int32_t buy = buy_from(market, price);
    if (gold >= buy) {
        gold = buy - 1;
    }
    return gold < 0 ? 0 : gold;
}

int32_t market_buy_price(const Market *market, int32_t resource)
{
    return market_is_tradable(resource) ? buy_from(market, market->price[resource]) : 0;
}

int32_t market_sell_price(const Market *market, int32_t resource)
{
    return market_is_tradable(resource) ? sell_from(market, market->price[resource]) : 0;
}

int market_is_open(const World *world, int32_t *out_reason)
{
    int has_market = 0;
    int32_t i;
    for (i = 0; i < world->building_count; i++) {
        const Building *building = world->buildings[i];
        if (building->type != CORE_BLD_MARKET || building->state != CORE_BSTATE_ACTIVE) {
            continue;
        }
        has_market = 1;
        if (building->worker_count > 0) {
            *out_reason = CORE_DENY_NONE;
            return 1;
        }
    }
    *out_reason = has_market ? CORE_DENY_NO_TRADER : CORE_DENY_NO_MARKET;
    return 0;
}

/* 남은 저장 자리 */
static int32_t free_room(const World *world, int32_t resource)
{
    return eco_bank_limit(&world->bank, resource) - world->bank.amount[resource];
}

int market_trade(World *world, int32_t resource, int32_t lots, int32_t *out_gold, int32_t *out_reason)
{
    int32_t count = lots < 0 ? -lots : lots;
    int32_t price;
    int32_t gold = 0;
    int32_t i;

    *out_gold = 0;
    if (!market_is_open(world, out_reason)) {
        return CORE_ERR_REJECTED;
    }
    if (!market_is_tradable(resource)) {
        *out_reason = CORE_DENY_NOT_TRADABLE;
        return CORE_ERR_REJECTED;
    }
    if (count < 1 || count > CORE_MARKET_MAX_LOTS) {
        *out_reason = CORE_DENY_BAD_AMOUNT;
        return CORE_ERR_REJECTED;
    }
    /* 먼저 가격을 한 묶음씩 움직여 보며 합계를 셉니다 (실제로 바꾸는 것은 전부 할 수 있을 때만) */
    price = world->market.price[resource];
    for (i = 0; i < count; i++) {
        if (lots > 0) {
            gold += buy_from(&world->market, price);
            price = clamp_price(resource, price + price * MARKET_IMPACT_PCT / 100);
        } else {
            gold += sell_from(&world->market, price);
            price = clamp_price(resource, price - price * MARKET_IMPACT_PCT / 100);
        }
    }
    if (lots > 0) {
        if (world->bank.amount[CORE_RES_GOLD] < gold) {
            *out_reason = CORE_DENY_NO_GOLD;
            return CORE_ERR_REJECTED;
        }
        if (free_room(world, resource) < count * CORE_MARKET_LOT) {
            *out_reason = CORE_DENY_STORAGE_FULL;
            return CORE_ERR_REJECTED;
        }
        (void)eco_change(&world->bank, &world->ledger, CORE_RES_GOLD, -gold, CORE_LEDGER_TRADE);
        (void)eco_change(&world->bank, &world->ledger, resource, count * CORE_MARKET_LOT, CORE_LEDGER_TRADE);
        *out_gold = -gold;
    } else {
        if (world->bank.amount[resource] < count * CORE_MARKET_LOT) {
            *out_reason = CORE_DENY_NO_GOODS;
            return CORE_ERR_REJECTED;
        }
        (void)eco_change(&world->bank, &world->ledger, resource, -count * CORE_MARKET_LOT, CORE_LEDGER_TRADE);
        (void)eco_change(&world->bank, &world->ledger, CORE_RES_GOLD, gold, CORE_LEDGER_TRADE);
        *out_gold = gold;
    }
    world->market.price[resource] = price;
    *out_reason = CORE_DENY_NONE;
    return CORE_OK;
}

/* ---- 떠돌이 상인 ------------------------------------------------------ */

static void add_deal(Merchant *merchant, int32_t give_resource, int32_t give_amount, int32_t get_resource,
                     int32_t get_amount)
{
    MerchantDeal *deal;
    if (merchant->deal_count >= CORE_MERCHANT_MAX_DEALS || give_amount <= 0 || get_amount <= 0) {
        return;
    }
    deal = &merchant->deals[merchant->deal_count++];
    deal->give_resource = give_resource;
    deal->give_amount = give_amount;
    deal->get_resource = get_resource;
    deal->get_amount = get_amount;
    deal->taken = 0;
}

/* 상인의 제안: 마정석 사기, 물건 팔기(시장보다 비싸게), 물건 사기(시장보다 싸게), 가끔 약초 ↔ 마정석 */
static void make_deals(World *world)
{
    static const int32_t SELLABLE[3] = {CORE_RES_WOOD, CORE_RES_STONE, CORE_RES_HERBS};
    static const int32_t OFFERED[3] = {CORE_RES_FOOD, CORE_RES_WOOD, CORE_RES_STONE};
    Merchant *merchant = &world->merchant;
    Rng *rng = &world->rng;
    int32_t season = clock_season(&world->clock);
    int32_t mana = rng_range(rng, 3, 6);
    int32_t resource;
    int32_t lots;

    merchant->deal_count = 0;
    add_deal(merchant, CORE_RES_GOLD, mana * rng_range(rng, 20, 28), CORE_RES_MANA, mana);

    resource = SELLABLE[rng_range(rng, 0, 2)];
    lots = rng_range(rng, 2, 4);
    add_deal(merchant, resource, lots * CORE_MARKET_LOT, CORE_RES_GOLD,
             lots * market_sell_price(&world->market, resource) * 130 / 100 + 1);

    resource = (season == CORE_SEASON_AUTUMN || season == CORE_SEASON_WINTER) ? CORE_RES_FOOD
                                                                             : OFFERED[rng_range(rng, 0, 2)];
    lots = rng_range(rng, 2, 5);
    add_deal(merchant, CORE_RES_GOLD, lots * market_buy_price(&world->market, resource) * 80 / 100 + 1, resource,
             lots * CORE_MARKET_LOT);

    if (rng_chance_permille(rng, 500)) {
        add_deal(merchant, CORE_RES_HERBS, rng_range(rng, 15, 25), CORE_RES_MANA, rng_range(rng, 2, 3));
    }
}

int merchant_accept(World *world, int32_t index, int32_t *out_reason)
{
    Merchant *merchant = &world->merchant;
    MerchantDeal *deal;
    if (merchant->days_left <= 0) {
        *out_reason = CORE_DENY_NO_MERCHANT;
        return CORE_ERR_REJECTED;
    }
    if (index < 0 || index >= merchant->deal_count) {
        *out_reason = CORE_DENY_BAD_AMOUNT;
        return CORE_ERR_REJECTED;
    }
    deal = &merchant->deals[index];
    if (deal->taken) {
        *out_reason = CORE_DENY_DEAL_TAKEN;
        return CORE_ERR_REJECTED;
    }
    if (world->bank.amount[deal->give_resource] < deal->give_amount) {
        *out_reason = deal->give_resource == CORE_RES_GOLD ? CORE_DENY_NO_GOLD : CORE_DENY_NO_GOODS;
        return CORE_ERR_REJECTED;
    }
    if (free_room(world, deal->get_resource) < deal->get_amount) {
        *out_reason = CORE_DENY_STORAGE_FULL;
        return CORE_ERR_REJECTED;
    }
    (void)eco_change(&world->bank, &world->ledger, deal->give_resource, -deal->give_amount, CORE_LEDGER_TRADE);
    (void)eco_change(&world->bank, &world->ledger, deal->get_resource, deal->get_amount, CORE_LEDGER_TRADE);
    deal->taken = 1;
    *out_reason = CORE_DENY_NONE;
    return CORE_OK;
}

void market_daily(World *world)
{
    Merchant *merchant = &world->merchant;
    int32_t season = clock_season(&world->clock);
    int32_t day = clock_day(&world->clock);
    int32_t i;
    for (i = 0; i < CORE_RES_COUNT; i++) {
        int32_t target;
        int32_t price = world->market.price[i];
        if (!market_is_tradable(i)) {
            continue;
        }
        target = BASE_PRICE[i] * SEASON_DEMAND[season][i];
        price += (target - price) * MARKET_RECOVER_PCT / 100;
        price += price * rng_range(&world->rng, -3, 3) / 100; /* 날마다 조금씩 흔들림 */
        world->market.price[i] = clamp_price(i, price);
    }
    if (merchant->days_left > 0) {
        merchant->days_left--;
        if (merchant->days_left == 0) {
            merchant->deal_count = 0;
            events_push_count(&world->events, CORE_EVT_MERCHANT_LEFT, 0);
        }
    } else if (day >= merchant->next_visit_day) {
        make_deals(world);
        merchant->days_left = MERCHANT_STAY_DAYS;
        merchant->next_visit_day = day + rng_range(&world->rng, MERCHANT_MIN_GAP, MERCHANT_MAX_GAP);
        events_push_pair(&world->events, CORE_EVT_MERCHANT_ARRIVED, MERCHANT_STAY_DAYS, merchant->deal_count);
    }
}
