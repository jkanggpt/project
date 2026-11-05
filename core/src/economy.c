/*
 * economy.c — 자원 은행 (설명은 economy.h)
 */
#include "economy.h"

/* 성채 하나가 주는 저장 한도 */
#define ECO_KEEP_CAPACITY 200

/* 난이도별 시작 자원. 순서는 CoreResource (금화, 마정석, 식량, 목재, 석재, 약초) */
static const int32_t START_RESOURCES[CORE_DIFF_COUNT][CORE_RES_COUNT] = {
    {300, 10, 80, 80, 40, 10}, /* 평화 */
    {200, 5, 60, 60, 30, 5},   /* 보통 */
    {150, 3, 40, 40, 20, 3},   /* 혹독 */
};

void eco_bank_init(ResourceBank *bank, int32_t difficulty)
{
    int32_t i;
    if (difficulty < 0 || difficulty >= CORE_DIFF_COUNT) {
        difficulty = CORE_DIFF_NORMAL;
    }
    bank->capacity = ECO_KEEP_CAPACITY;
    for (i = 0; i < CORE_RES_COUNT; i++) {
        bank->amount[i] = START_RESOURCES[difficulty][i];
    }
}

int eco_is_stored_good(int32_t resource)
{
    return resource == CORE_RES_FOOD || resource == CORE_RES_WOOD || resource == CORE_RES_STONE ||
           resource == CORE_RES_HERBS;
}

int32_t eco_bank_limit(const ResourceBank *bank, int32_t resource)
{
    return eco_is_stored_good(resource) ? bank->capacity : ECO_MAX_AMOUNT;
}

int32_t eco_bank_add(ResourceBank *bank, int32_t resource, int32_t amount)
{
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
        after = limit;
    }
    bank->amount[resource] = (int32_t)after;
    return (int32_t)(after - before);
}

int eco_bank_can_afford(const ResourceBank *bank, const int32_t cost[CORE_RES_COUNT])
{
    int32_t i;
    for (i = 0; i < CORE_RES_COUNT; i++) {
        if (cost[i] > 0 && bank->amount[i] < cost[i]) {
            return 0;
        }
    }
    return 1;
}

int eco_bank_spend(ResourceBank *bank, const int32_t cost[CORE_RES_COUNT])
{
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
