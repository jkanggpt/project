/* trade_rules — 현재 기능의 단일 구현 */
#include "expansion.h"
#include "placement.h"
#include "rank.h"
#include "villager_ai.h"
#include <stdlib.h>
#include <string.h>

int expansion_trade_efficiency(const World *w) {
    int level = expansion_market_level(w), total = 0, count = 0;
    if (!level)
        return 0;
    for (int i = 0; i < w->building_count; i++) {
        const Building *b = w->buildings[i];
        int n = b->type == CORE_BLD_STALL          ? 1
                : b->type == CORE_BLD_MARKET       ? 2
                : b->type == CORE_BLD_TRADING_POST ? 3
                                                   : 0;
        if (n != level || b->state != CORE_BSTATE_ACTIVE)
            continue;
        for (int j = 0; j < b->worker_count; j++)
            if (expansion_working_present(w, b, b->workers[j])) {
                total += expansion_work_percent(w, b->workers[j], b->type);
                count++;
            }
    }
    if (!count)
        return 0;
    return total / count;
}

int expansion_trade_limit(const World *w) {
    int average = expansion_trade_efficiency(w), level = expansion_market_level(w);
    if (!average || !level)
        return 0;
    int base = (int[]){0, 20, 80, 260}[level];
    return base + base * exp_clamp(average - 100, 0, 200) / 800 -
           base * exp_clamp(100 - average, 0, 75) / 750;
}
