/* expansion_prices.c — 시세와 거래별 반올림 잔액을 공유하는 단일 견적 */
#include "expansion.h"
#include <string.h>
static const int BUY[15] = {0, 45, 2, 8, 6, 6, 5, 5, 11, 9, 7, 22, 30, 60, 12};
static const int SELL[15] = {0, 25, 1, 4, 3, 3, 2, 2, 5, 4, 3, 12, 18, 40, 6};
static int seasonal(const World *w, int r) {
    static const int food[4] = {200, -100, -400, 500}, herbs[4] = {-200, -200, 100, 300},
                     timber[4] = {0, -100, 100, 300}, coal[4] = {-100, -100, 0, 400};
    int s = exp_season(w);
    return r == CORE_RES_FOOD                           ? food[s]
           : r == CORE_RES_HERBS                        ? herbs[s]
           : (r == CORE_RES_LOGS || r == CORE_RES_WOOD) ? timber[s]
           : r == CORE_RES_COAL                         ? coal[s]
                                                        : 0;
}
static int event_rate(const World *w, int r) {
    int extra = 0;
    if (expansion_event_active(w, CORE_NATURE_MINE_COLLAPSE) &&
        (r == CORE_RES_IRON_ORE || r == CORE_RES_IRON))
        extra += 500;
    if (expansion_event_active(w, CORE_NATURE_TRADE_BOOM) &&
        (r == CORE_RES_GOLD_ORE || r == CORE_RES_GOLD_BAR || r == CORE_RES_LEATHER))
        extra += 300;
    if (r == CORE_RES_FOOD) {
        if (w->nature.event == CORE_NATURE_DROUGHT)
            extra += 600;
        if (w->nature.event == CORE_NATURE_BUMPER)
            extra -= 500;
    }
    if (w->nature.event == CORE_NATURE_COLD_SNAP &&
        (r == CORE_RES_LOGS || r == CORE_RES_WOOD || r == CORE_RES_COAL))
        extra += 400;
    return exp_clamp(extra, -600, 700);
}
static int tradable(int level, int r) {
    if (r < 1 || r >= 15 || !level)
        return 0;
    return level == 3 ||
           (level == 2 && r != CORE_RES_MANA && r != CORE_RES_GOLD_ORE && r != CORE_RES_GOLD_BAR) ||
           (level == 1 && (r == CORE_RES_FOOD || r == CORE_RES_HERBS || r == CORE_RES_LOGS ||
                           r == CORE_RES_STONE || r == CORE_RES_LEATHER));
}
CORE_API int32_t core_expansion_quote(const CoreWorld *w, int32_t res, int32_t units, CoreTradeQuote *q) {
    const ExpansionState *e;
    int level, amount, i, pressure, base, mult, slot;
    int64_t value;
    if (!w || !q)
        return CORE_ERR_NULL_ARG;
    e = w->expansion;
    if (!e || res < 1 || res >= 15 || !units || units < -260 || units > 260)
        return CORE_ERR_INVALID_ARG;
    memset(q, 0, sizeof(*q));
    q->resource = res;
    q->units = units;
    level = expansion_market_level(w);
    q->stock = e->trade_stock[res];
    q->remaining = expansion_trade_limit(w) - e->trade_used;
    q->season_bp = seasonal(w, res);
    q->event_bp = event_rate(w, res);
    q->pressure_bp = e->trade_pressure[res];
    q->rate_bp = exp_clamp(q->season_bp + q->event_bp + q->pressure_bp, -1200, 1200);
    base = units > 0 ? BUY[res] : SELL[res];
    mult = units < 0 && expansion_has_research(w, 7) ? 110 : 100;
    q->unit_cents = (base * (10000 + q->rate_bp) * mult + 5000) / 10000;
    amount = units < 0 ? -units : units;
    pressure = e->trade_pressure[res];
    slot = res + (units < 0 ? 15 : 0);
    value = e->trade_remainder[slot];
    /* 한 개씩 같은 압력을 누적하고 잔액을 보존하므로 분할 주문도 합계가 같습니다. */
    for (i = 0; i < amount; i++) {
        int rate = exp_clamp(q->season_bp + q->event_bp + pressure, -1200, 1200);
        value += (int64_t)base * (10000 + rate) * mult;
        pressure = exp_clamp(pressure + (units > 0 ? 10 : -10), -500, 500);
    }
    q->total_gold = (int32_t)(value / 1000000);
    q->next_remainder = (int32_t)(value % 1000000);
    q->next_pressure = pressure;
    q->reason = !level                                                       ? CORE_DENY_NO_MARKET
                : !tradable(level, res)                                      ? CORE_DENY_NOT_TRADABLE
                : amount > q->remaining                                      ? CORE_DENY_BAD_AMOUNT
                : units > 0 && amount > q->stock                             ? CORE_DENY_NO_GOODS
                : units > 0 && w->bank.amount[CORE_RES_GOLD] < q->total_gold ? CORE_DENY_NO_GOLD
                : units < 0 && w->bank.amount[res] < amount                  ? CORE_DENY_NO_GOODS
                : units > 0 && (int64_t)expansion_bank_have(w, res) + amount * 1000 >
                                   (int64_t)eco_bank_limit(&w->bank, res) * 1000
                    ? CORE_DENY_STORAGE_FULL
                    : 0;
    return CORE_OK;
}
int expansion_trade(World *w, int res, int units, int *gold, int *reason) {
    CoreTradeQuote q;
    int rc = core_expansion_quote(w, res, units, &q);
    *gold = 0;
    *reason = 0;
    if (rc != CORE_OK)
        return rc;
    *reason = q.reason;
    if (q.reason)
        return CORE_ERR_REJECTED;
    *gold = units > 0 ? -q.total_gold : q.total_gold;
    expansion_bank_add(w, CORE_RES_GOLD, *gold * 1000, CORE_LEDGER_TRADE);
    expansion_bank_add(w, res, units * 1000, CORE_LEDGER_TRADE);
    w->expansion->trade_used += units < 0 ? -units : units;
    w->expansion->trade_stock[res] -= units;
    w->expansion->trade_volume[res] += units;
    w->expansion->trade_pressure[res] = q.next_pressure;
    w->expansion->trade_remainder[res + (units < 0 ? 15 : 0)] = q.next_remainder;
    return CORE_OK;
}
void expansion_read_market(const World *w, CoreMarketView *out) {
    int r, level = expansion_market_level(w);
    memset(out, 0, sizeof(*out));
    out->open = level > 0;
    out->closed_reason = level ? 0 : CORE_DENY_NO_MARKET;
    for (r = 1; r < 15; r++)
        if (tradable(level, r)) {
            CoreTradeQuote buy = {0}, sell = {0};
            core_expansion_quote(w, r, 1, &buy);
            core_expansion_quote(w, r, -1, &sell);
            out->buy_price[r] = buy.total_gold;
            out->sell_price[r] = sell.total_gold;
            out->base_price[r] = BUY[r];
            out->trend[r] = 100 + buy.rate_bp / 100;
        }
}
void expansion_market_daily(World *w) {
    int r;
    for (r = 0; r < 15; r++) {
        int p = w->expansion->trade_pressure[r];
        w->expansion->trade_pressure[r] += p > 0 ? -exp_clamp(p, 0, 100) : exp_clamp(-p, 0, 100);
        w->expansion->trade_volume[r] = 0;
    }
}
CORE_API int32_t core_expansion_display_resource(int32_t i) {
    static const int order[15] = {0, 1, 2, 6, 3, 4, 5, 14, 7, 8, 10, 9, 11, 12, 13};
    return i >= 0 && i < 15 ? order[i] : -1;
}
CORE_API int32_t core_expansion_display_building(int32_t i) {
    static const int order[72] = {16, 17, 1,  18, 19, 20, 2,  21, 22, 23, 24, 25, 26, 27, 4,  28, 66, 5,
                                  29, 6,  30, 31, 32, 33, 11, 34, 35, 36, 12, 37, 38, 39, 10, 40, 41, 42,
                                  9,  43, 8,  44, 45, 7,  46, 47, 48, 49, 50, 13, 51, 52, 67, 53, 54, 55,
                                  56, 57, 69, 58, 59, 14, 60, 68, 3,  61, 62, 15, 63, 64, 65, 70, 71, 72};
    if (i < 0 || i >= CORE_BLD_COUNT - 1)
        return -1;
    int at = 0;
    for (int rank = 0; rank <= 5; rank++)
        for (int j = 0; j < CORE_BLD_COUNT - 1; j++)
            if (expansion_spec(order[j])->rank == rank && at++ == i)
                return order[j];
    return -1;
}

CORE_API int32_t core_expansion_tradable(const CoreWorld *w, int32_t res) {
    return w && w->expansion && tradable(expansion_market_level(w), res);
}
CORE_API int32_t core_expansion_restock(int32_t level, int32_t efficiency, int32_t *base, int32_t *mult,
                                        int32_t *quantity) {
    if (!base || !mult || !quantity)
        return CORE_ERR_NULL_ARG;
    if (level < 0 || level > 3)
        return CORE_ERR_INVALID_ARG;
    int e = exp_clamp(efficiency, 25, 750);
    *base = (int[]){0, 30, 120, 390}[level];
    /* E를 백분율로 받아 사용자 지정 선형 구간을 정수 만분율로 계산합니다. */
    *mult = 10000 + (3000 * exp_clamp(e - 100, 0, 650) + 325) / 650 -
            (1000 * exp_clamp(100 - e, 0, 75) + 37) / 75;
    int denominator = e >= 100 ? 6500 : 750;
    int numerator = e >= 100 ? 6500 + 3 * (e - 100) : 650 + e;
    *quantity = (int)(((int64_t)*base * numerator + denominator * 5 / 2) / (denominator * 5)) * 5;
    return CORE_OK;
}
