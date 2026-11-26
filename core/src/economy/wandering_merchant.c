/* expansion_services_trade.c — 방문당 고정 제안과 비금화 원자적 물물교환 */
#include "expansion.h"
#include <stdlib.h>
#include <string.h>
static const int BUY[15] = {0, 45, 2, 8, 6, 6, 5, 5, 11, 9, 7, 22, 30, 60, 12};
static const int SELL[15] = {0, 25, 1, 4, 3, 3, 2, 2, 5, 4, 3, 12, 18, 40, 6};
/* 등급, 지불 자원/수량, 수령 자원/수량, 제한, 가중치 */
static const int RECIPES[][7] = {
    {1, 6, 30, 1, 2, 2, 12},   {1, 5, 12, 1, 1, 3, 12},   {1, 2, 50, 1, 2, 2, 12},  {1, 4, 22, 1, 2, 2, 12},
    {1, 7, 24, 1, 2, 2, 12},   {2, 14, 18, 1, 3, 2, 10},  {2, 3, 18, 1, 2, 2, 10},  {2, 8, 14, 1, 2, 2, 10},
    {3, 9, 18, 1, 2, 3, 10},   {3, 10, 20, 1, 2, 2, 10},  {3, 11, 4, 1, 2, 2, 10},  {4, 12, 4, 1, 2, 2, 8},
    {5, 13, 2, 1, 2, 2, 8},    {1, 6, 20, 2, 45, 3, 12},  {1, 2, 40, 3, 12, 3, 12}, {1, 5, 10, 14, 7, 3, 12},
    {1, 7, 16, 8, 11, 3, 12},  {2, 4, 25, 9, 13, 2, 10},  {3, 3, 20, 10, 18, 2, 9}, {3, 14, 12, 5, 13, 3, 9},
    {4, 10, 14, 11, 4, 2, 8},  {1, 6, 14, 4, 12, 3, 12},  {1, 5, 8, 2, 25, 3, 12},  {1, 4, 18, 6, 18, 3, 12},
    {2, 7, 20, 10, 10, 2, 10}, {2, 2, 36, 14, 8, 3, 10},  {3, 9, 15, 8, 12, 2, 9},  {4, 11, 6, 12, 4, 2, 8},
    {5, 12, 5, 13, 3, 2, 8},   {1, 6, 18, 4, 19, 2, 11},  {1, 4, 18, 6, 25, 2, 11}, {1, 2, 30, 6, 15, 3, 11},
    {1, 6, 20, 5, 19, 2, 11},  {1, 5, 12, 2, 45, 2, 11},  {1, 7, 20, 4, 20, 2, 11}, {1, 4, 12, 7, 17, 3, 11},
    {1, 6, 24, 7, 28, 2, 11},  {1, 7, 24, 6, 28, 2, 11},  {1, 2, 36, 5, 15, 2, 11}, {1, 6, 24, 14, 12, 2, 11},
    {1, 14, 9, 2, 65, 2, 11},  {1, 5, 14, 3, 13, 2, 11},  {1, 4, 16, 8, 11, 2, 11}, {2, 3, 12, 8, 11, 2, 10},
    {2, 8, 10, 3, 17, 2, 10},  {2, 4, 20, 9, 16, 2, 10},  {2, 9, 10, 4, 18, 2, 10}, {2, 7, 22, 8, 12, 2, 10},
    {2, 14, 12, 9, 19, 2, 10}, {2, 9, 14, 14, 13, 2, 10}, {2, 5, 18, 8, 12, 2, 10}, {2, 8, 11, 5, 23, 2, 10},
    {2, 3, 14, 9, 15, 2, 10},  {2, 2, 60, 10, 20, 2, 10}, {2, 6, 30, 9, 19, 2, 10}, {3, 9, 16, 10, 23, 2, 9},
    {3, 10, 20, 9, 18, 2, 9},  {3, 9, 12, 11, 6, 2, 9},   {3, 10, 16, 11, 6, 2, 9}, {3, 3, 16, 11, 7, 2, 9},
    {3, 11, 4, 3, 13, 2, 9},   {3, 14, 15, 11, 10, 1, 9}, {3, 4, 30, 9, 24, 2, 9},  {3, 2, 80, 8, 17, 2, 9},
    {3, 5, 20, 10, 21, 2, 9},  {3, 8, 15, 10, 26, 2, 9},  {3, 7, 30, 9, 20, 2, 9},  {4, 12, 3, 13, 2, 1, 8},
    {4, 12, 4, 11, 7, 2, 8},   {4, 11, 6, 12, 5, 2, 8},   {4, 13, 1, 11, 4, 2, 8},  {4, 11, 7, 13, 3, 1, 8},
    {4, 10, 25, 12, 7, 2, 8},  {4, 12, 3, 8, 10, 2, 8},   {4, 8, 20, 11, 12, 1, 8}, {4, 14, 20, 12, 9, 2, 8},
    {4, 3, 23, 12, 7, 2, 8},   {5, 13, 3, 11, 10, 2, 7},  {5, 13, 2, 12, 5, 2, 7},  {5, 12, 8, 13, 5, 1, 7},
    {5, 11, 10, 12, 9, 2, 7},  {5, 13, 2, 5, 25, 2, 7},   {5, 10, 28, 13, 4, 1, 7}, {5, 14, 25, 13, 6, 1, 7},
    {5, 8, 18, 13, 4, 1, 7}};
_Static_assert(sizeof(RECIPES) / sizeof(RECIPES[0]) == CORE_BARTER_RECIPE_COUNT, "barter recipe count");
CORE_API int32_t core_expansion_barter_count(void) { return (int)(sizeof(RECIPES) / sizeof(RECIPES[0])); }
CORE_API int32_t core_expansion_barter_recipe(int32_t id, CoreBarterRecipe *out) {
    if (!out)
        return CORE_ERR_NULL_ARG;
    if (id < 1 || id > core_expansion_barter_count())
        return CORE_ERR_INVALID_ARG;
    const int *r = RECIPES[id - 1];
    *out = (CoreBarterRecipe){0};
    out->id = id;
    out->rank = r[0];
    out->pay_resource = r[1];
    out->pay_count = r[2];
    out->get_resource = r[3];
    out->get_count = r[4];
    out->max_uses = r[5];
    out->weight = r[6];
    out->benefit_bp = (r[4] * BUY[r[3]] - r[2] * SELL[r[1]]) * 10000 / (r[2] * SELL[r[1]]);
    (void)snprintf(out->code, sizeof(out->code), "%c%02d", id <= 13 ? 'M' : 'U', id <= 13 ? id : id - 13);
    return CORE_OK;
}
CORE_API int32_t core_expansion_merchant_view(const CoreWorld *w, CoreWanderingMerchantView *out) {
    if (!w || !out)
        return CORE_ERR_NULL_ARG;
    if (!w->expansion)
        return CORE_ERR_INVALID_ARG;
    *out = w->expansion->wandering;
    return CORE_OK;
}
static int offer_cycle(const CoreWanderingMerchantView *m, int slot, const CoreBarterRecipe *candidate) {
    unsigned char edges[CORE_RES_COUNT][CORE_RES_COUNT] = {{0}};
    for (int k = 0; k < slot; k++)
        if (m->recipe_ids[k] > 13) {
            CoreBarterRecipe r;
            core_expansion_barter_recipe(m->recipe_ids[k], &r);
            edges[r.pay_resource][r.get_resource] = 1;
        }
    for (int k = 0; k < CORE_RES_COUNT; k++)
        for (int i = 0; i < CORE_RES_COUNT; i++)
            for (int j = 0; j < CORE_RES_COUNT; j++)
                edges[i][j] |= edges[i][k] && edges[k][j];
    return candidate->pay_resource == candidate->get_resource ||
           edges[candidate->get_resource][candidate->pay_resource];
}
static void choose_offers(World *w) {
    CoreWanderingMerchantView *m = &w->expansion->wandering;
    Rng rng;
    expansion_event_rng(w, m->visit_id, 61, &rng);
    m->rank = w->rank;
    m->offer_count = (int[]){0, 3, 4, 5, 5, 6}[m->rank];
    int mana = (int[]){0, 1, 1, 2, 2, 3}[m->rank];
    memset(m->recipe_ids, 0, sizeof(m->recipe_ids));
    memset(m->remaining, 0, sizeof(m->remaining));
    m->last_nonce = m->last_offer = 0;
    for (int slot = 0; slot < m->offer_count; slot++) {
        int pool[CORE_BARTER_RECIPE_COUNT], n = 0, total = 0;
        for (int id = 1; id <= core_expansion_barter_count(); id++) {
            CoreBarterRecipe r;
            core_expansion_barter_recipe(id, &r);
            if (r.rank > m->rank || (slot < mana) != (id <= 13) || r.benefit_bp <= 0)
                continue;
            int used = 0;
            for (int k = 0; k < slot; k++)
                used |= m->recipe_ids[k] == id;
            if (used || (id > 13 && offer_cycle(m, slot, &r)))
                continue;
            pool[n++] = id;
            total += r.weight;
        }
        if (!n || !total) {
            for (int k = 0; k < m->offer_count; k++) {
                int id = k < mana ? k + 1 : (int[]){14, 16, 17}[k - mana];
                CoreBarterRecipe fallback;
                core_expansion_barter_recipe(id, &fallback);
                m->recipe_ids[k] = id;
                m->remaining[k] = fallback.max_uses;
            }
            return;
        }
        int roll = rng_range(&rng, 1, total);
        for (int k = 0; k < n; k++) {
            CoreBarterRecipe r;
            core_expansion_barter_recipe(pool[k], &r);
            roll -= r.weight;
            if (roll <= 0) {
                m->recipe_ids[slot] = r.id;
                m->remaining[slot] = r.max_uses;
                break;
            }
        }
    }
}
static int merchant_walk(World *w) {
    CoreWanderingMerchantView *m = &w->expansion->wandering;
    Grid *g = &w->grid;
    ExpansionState *e = w->expansion;
    int budget = 650;
    while (budget > 0) {
        int x = m->x_milli / 1000, y = m->y_milli / 1000, tx = m->target_x, ty = m->target_y;
        if (x == tx && y == ty && m->x_milli == tx * 1000 + 500 && m->y_milli == ty * 1000 + 500)
            return 1;
        int start = grid_index(g, x, y), target = grid_index(g, tx, ty), head = 0, tail = 0;
        for (int p = 0; p < e->cells; p++)
            e->route_distance[p] = -1;
        e->queue[tail++] = target;
        e->route_distance[target] = 0;
        static const int dx[4] = {0, 1, 0, -1}, dy[4] = {-1, 0, 1, 0};
        while (head < tail && e->route_distance[start] < 0) {
            int p = e->queue[head++], xx = p % g->width, yy = p / g->width;
            for (int d = 0; d < 4; d++) {
                int ax = xx + dx[d], ay = yy + dy[d];
                if (!grid_in_bounds(g, ax, ay))
                    continue;
                int q = grid_index(g, ax, ay);
                if (e->route_distance[q] < 0 && g->cost[q] && g->block[q] != GRID_BLOCK_WALL) {
                    e->route_distance[q] = e->route_distance[p] + 1;
                    e->queue[tail++] = q;
                }
            }
        }
        if (e->route_distance[start] < 0) {
            m->retry_tick = w->clock.tick + 6;
            m->delayed = 1;
            return 0;
        }
        int gx = x * 1000 + 500, gy = y * 1000 + 500,
            best = e->route_distance[start] * 1000 + abs(gx - m->x_milli) + abs(gy - m->y_milli);
        for (int d = 0; d < 4; d++) {
            int xx = x + dx[d], yy = y + dy[d];
            if (!grid_in_bounds(g, xx, yy))
                continue;
            int q = grid_index(g, xx, yy);
            if (e->route_distance[q] < 0)
                continue;
            int ax = xx * 1000 + 500, ay = yy * 1000 + 500;
            if (ax != m->x_milli && ay != m->y_milli)
                continue;
            int score = e->route_distance[q] * 1000 + abs(ax - m->x_milli) + abs(ay - m->y_milli);
            if (score < best || (score == best && e->route_distance[q] < e->route_distance[start])) {
                best = score;
                gx = ax;
                gy = ay;
            }
        }
        int delta = gx - m->x_milli;
        if (delta) {
            int step = abs(delta) < budget ? abs(delta) : budget;
            m->x_milli += delta > 0 ? step : -step;
            budget -= step;
        } else {
            delta = gy - m->y_milli;
            int step = abs(delta) < budget ? abs(delta) : budget;
            m->y_milli += delta > 0 ? step : -step;
            budget -= step;
            if (!step)
                break;
        }
    }
    m->delayed = 0;
    return m->x_milli == m->target_x * 1000 + 500 && m->y_milli == m->target_y * 1000 + 500;
}
void expansion_merchant_tick(World *w) {
    CoreWanderingMerchantView *m = &w->expansion->wandering;
    Building *keep = world_keep(w);
    if (!keep || w->rank < 1)
        return;
    if (!m->next_target_tick) {
        Rng r;
        expansion_event_rng(w, exp_day(w), 60, &r);
        m->next_target_tick = w->clock.tick + rng_range(&r, 4, 8) * 144;
    }
    if (m->state == CORE_MERCHANT_DORMANT) {
        int travel = (w->grid.width / 2) * 1000 / 650;
        if (w->clock.tick < m->next_target_tick - travel)
            return;
        int x = 0, y = 0;
        Rng r;
        expansion_event_rng(w, m->visit_id + 1, 62, &r);
        int edge = rng_range(&r, 0, 3),
            nearx = edge == 1   ? w->grid.width - 1
                    : edge == 3 ? 0
                                : keep->x,
            neary = edge == 0   ? 0
                    : edge == 2 ? w->grid.height - 1
                                : keep->y;
        if (!expansion_edge(w, nearx, neary, &x, &y))
            return;
        m->entry_x = x;
        m->entry_y = y;
        m->x_milli = x * 1000 + 500;
        m->y_milli = y * 1000 + 500;
        m->wait_x = keep->door_x;
        m->wait_y = keep->door_y;
        m->target_x = m->wait_x;
        m->target_y = m->wait_y;
        m->state = CORE_MERCHANT_ENTERING;
        m->visit_id++;
        m->revision++;
        choose_offers(w);
    }
    if (m->state == CORE_MERCHANT_TRADING && w->clock.tick >= m->expires_tick) {
        m->state = CORE_MERCHANT_LEAVING;
        int x, y;
        if (expansion_edge(w, m->x_milli / 1000, m->y_milli / 1000, &x, &y)) {
            m->entry_x = x;
            m->entry_y = y;
        }
        m->target_x = m->entry_x;
        m->target_y = m->entry_y;
        m->revision++;
    }
    if ((m->state == CORE_MERCHANT_ENTERING || m->state == CORE_MERCHANT_LEAVING) &&
        w->clock.tick >= m->retry_tick && merchant_walk(w)) {
        if (m->state == CORE_MERCHANT_ENTERING) {
            if (w->clock.tick < m->next_target_tick)
                return;
            m->state = CORE_MERCHANT_TRADING;
            m->arrived_tick = w->clock.tick;
            m->expires_tick = w->clock.tick + 432;
            m->arrivals++;
            expansion_notice(w, 170, m->visit_id, 0, u8"떠돌이 상인 도착 · 3일 동안 물물교환");
        } else {
            m->state = CORE_MERCHANT_DORMANT;
            Rng r;
            expansion_event_rng(w, m->visit_id, 63, &r);
            m->next_target_tick = m->arrived_tick + rng_range(&r, 24, 36) * 144;
        }
    }
}
CORE_API int32_t core_expansion_merchant_trade(CoreWorld *w, int32_t visit, int32_t recipe, int32_t nonce,
                                               int32_t *reason) {
    if (!w || !reason)
        return CORE_ERR_NULL_ARG;
    *reason = 0;
    if (!w->expansion)
        return CORE_ERR_INVALID_ARG;
    CoreWanderingMerchantView *m = &w->expansion->wandering;
    if (visit != m->visit_id || m->state != CORE_MERCHANT_TRADING || w->clock.tick >= m->expires_tick) {
        *reason = CORE_DENY_NO_MERCHANT;
        return CORE_ERR_REJECTED;
    }
    if (nonce > 0 && nonce == m->last_nonce && recipe == m->last_offer)
        return CORE_IGNORED;
    if (nonce <= m->last_nonce) {
        *reason = CORE_DENY_DEAL_TAKEN;
        return CORE_ERR_REJECTED;
    }
    int slot = -1;
    for (int i = 0; i < m->offer_count; i++)
        if (m->recipe_ids[i] == recipe)
            slot = i;
    CoreBarterRecipe r;
    if (slot < 0 || core_expansion_barter_recipe(recipe, &r) != CORE_OK || !m->remaining[slot]) {
        *reason = CORE_DENY_DEAL_TAKEN;
        return CORE_ERR_REJECTED;
    }
    if (expansion_bank_have(w, r.pay_resource) < r.pay_count * 1000) {
        *reason = CORE_DENY_NO_GOODS;
        return CORE_ERR_REJECTED;
    }
    if (eco_bank_limit(&w->bank, r.get_resource) * 1000 - expansion_bank_have(w, r.get_resource) <
        r.get_count * 1000) {
        *reason = CORE_DENY_STORAGE_FULL;
        return CORE_ERR_REJECTED;
    }
    (void)expansion_bank_add(w, r.pay_resource, -r.pay_count * 1000, CORE_LEDGER_TRADE);
    (void)expansion_bank_add(w, r.get_resource, r.get_count * 1000, CORE_LEDGER_TRADE);
    m->remaining[slot]--;
    m->last_nonce = nonce;
    m->last_offer = recipe;
    if (r.get_resource == CORE_RES_MANA)
        m->barter_mana += r.get_count;
    return CORE_OK;
}
CORE_API int32_t core_expansion_merchant_position(CoreWorld *w, int32_t x, int32_t y, int32_t arrived) {
    (void)x;
    (void)y;
    (void)arrived;
    if (!w)
        return CORE_ERR_NULL_ARG;
    return CORE_ERR_REJECTED; /* 코어가 실제 도보 경로를 소유합니다. */
}
