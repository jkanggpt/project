/*
 * economy.c — 자원 은행 (설명은 economy.h)
 */
#include "economy.h"

/* 처음 저장 한도 (성채의 storage 값이 곧 덮어씁니다) */
#define ECO_KEEP_CAPACITY 250

#include <string.h>

#include "difficulty.h"

const int32_t ECO_START_RESOURCES[CORE_RES_COUNT] = {160, 5, 125, 0, 12, 5, 65, 0, 0, 0, 0, 0, 0, 0, 0};

void eco_bank_init(ResourceBank *bank, int32_t difficulty) {
    int32_t i;
    memset(bank, 0, sizeof(*bank));
    (void)difficulty;
    bank->capacity = ECO_KEEP_CAPACITY;
    for (i = 0; i < CORE_RES_COUNT; i++) {
        bank->amount[i] = ECO_START_RESOURCES[i];
    }
}

int eco_is_stored_good(int32_t resource) {
    return resource == CORE_RES_FOOD || resource == CORE_RES_WOOD || resource == CORE_RES_STONE ||
           resource == CORE_RES_HERBS;
}

int32_t eco_bank_limit(const ResourceBank *bank, int32_t resource) {
    if (bank->individual_limits && resource >= 0 && resource < CORE_RES_COUNT)
        return bank->limits[resource];
    return eco_is_stored_good(resource) ? bank->capacity : ECO_MAX_AMOUNT;
}

int32_t eco_bank_add(ResourceBank *bank, int32_t resource, int32_t amount) {
    int64_t before;
    int64_t after;
    int32_t limit;

    if (resource < 0 || resource >= CORE_RES_COUNT) {
        return 0;
    }
    limit = eco_bank_limit(bank, resource);
    before = bank->amount[resource];
    after = before + amount; /* int64 로 계산해서 넘침을 막습니다 */
    if (after < 0) {
        after = 0;
    }
    if (after > limit) {
        if (!bank->individual_limits)
            after = limit;
        else if (amount > 0)
            after = before > limit ? before : limit;
    }
    bank->amount[resource] = (int32_t)after;
    return (int32_t)(after - before);
}

int eco_bank_can_afford(const ResourceBank *bank, const int32_t cost[CORE_RES_COUNT]) {
    int32_t i;
    for (i = 0; i < CORE_RES_COUNT; i++) {
        if (cost[i] > 0 && bank->amount[i] < cost[i]) {
            return 0;
        }
    }
    return 1;
}

int eco_bank_spend(ResourceBank *bank, const int32_t cost[CORE_RES_COUNT]) {
    int32_t i;
    if (!eco_bank_can_afford(bank, cost)) {
        return 0;
    }
    for (i = 0; i < CORE_RES_COUNT; i++) {
        if (cost[i] > 0) {
            bank->amount[i] -= cost[i];
        }
    }
    return 1;
}

int32_t eco_change(ResourceBank *bank, Ledger *ledger, int32_t resource, int32_t amount, int32_t category) {
    int32_t changed = eco_bank_add(bank, resource, amount);
    if (ledger != NULL && resource >= 0 && resource < CORE_RES_COUNT && category >= 0 &&
        category < CORE_LEDGER_COUNT) {
        ledger->today[resource][category] += changed;
    }
    return changed;
}

int eco_pay(ResourceBank *bank, Ledger *ledger, const int32_t cost[CORE_RES_COUNT], int32_t category) {
    int32_t i;
    if (!eco_bank_can_afford(bank, cost)) {
        return 0;
    }
    for (i = 0; i < CORE_RES_COUNT; i++) {
        if (cost[i] > 0) {
            (void)eco_change(bank, ledger, i, -cost[i], category);
        }
    }
    return 1;
}

void eco_roll_ledger(Ledger *ledger) {
    memcpy(ledger->yesterday, ledger->today, sizeof(ledger->today));
    memset(ledger->today, 0, sizeof(ledger->today));
}

int32_t eco_ledger_net(const Ledger *ledger, int32_t resource) {
    int32_t total = 0;
    int32_t category;
    if (resource < 0 || resource >= CORE_RES_COUNT) {
        return 0;
    }
    for (category = 0; category < CORE_LEDGER_COUNT; category++) {
        total += ledger->yesterday[resource][category];
    }
    return total;
}

void eco_set_capacity(ResourceBank *bank, Ledger *ledger, int32_t capacity) {
    int32_t i;
    bank->capacity = capacity > 0 ? capacity : 1;
    for (i = 0; i < CORE_RES_COUNT; i++) {
        if (eco_is_stored_good(i) && bank->amount[i] > bank->capacity) {
            (void)eco_change(bank, ledger, i, bank->capacity - bank->amount[i], CORE_LEDGER_OTHER);
        }
    }
}
