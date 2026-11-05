/*
 * economy.h — 자원 은행 (금화, 마정석, 식량, 목재, 석재, 약초)
 *
 * 금화와 마정석은 화폐라서 저장 한도가 없습니다.
 * 식량·목재·석재·약초는 물건이라 성채와 창고가 정하는 저장 한도(capacity)를 넘지 못합니다.
 * (생산, 소비, 임금, 장부, 시장은 마일스톤 2~3에서 이 파일에 더해집니다)
 */
#ifndef CASTLE_ECONOMY_H
#define CASTLE_ECONOMY_H

#include <stdint.h>

#include "castle_core.h"

#define ECO_MAX_AMOUNT 1000000000 /* 금화·마정석의 상한 (int32 넘침 방지) */

typedef struct ResourceBank {
    int32_t amount[CORE_RES_COUNT];
    int32_t capacity; /* 물건 하나당 저장 한도 */
} ResourceBank;

/* 난이도별 시작 자원으로 채웁니다. */
void eco_bank_init(ResourceBank *bank, int32_t difficulty);
/* 저장 한도가 적용되는 자원이면 1 */
int eco_is_stored_good(int32_t resource);
/* 이 자원이 가질 수 있는 최댓값 */
int32_t eco_bank_limit(const ResourceBank *bank, int32_t resource);
/* amount 만큼 더하고(음수면 뺌), 한도와 0 사이로 자릅니다. 실제로 바뀐 양을 돌려줍니다. */
int32_t eco_bank_add(ResourceBank *bank, int32_t resource, int32_t amount);
int eco_bank_can_afford(const ResourceBank *bank, const int32_t cost[CORE_RES_COUNT]);
/* 모두 낼 수 있을 때만 빼고 1, 아니면 아무것도 바꾸지 않고 0 */
int eco_bank_spend(ResourceBank *bank, const int32_t cost[CORE_RES_COUNT]);

#endif
