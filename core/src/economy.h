/*
 * economy.h — 자원 은행 (금화, 마정석, 식량, 목재, 석재, 약초)
 *
 * 금화와 마정석은 화폐라서 저장 한도가 없습니다.
 * 식량·목재·석재·약초는 물건이라 성채와 창고가 정하는 저장 한도(capacity)를 넘지 못합니다.
 * 모든 자원 변화는 eco_change 를 거쳐 장부(Ledger)에 항목별로 적힙니다. 시장과 상인은 market.c.
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

/* 장부 항목은 공개 헤더의 CoreLedgerCategory (Godot 툴팁도 같은 번호를 씀) */

/* 하루치 장부 두 장: 오늘 쌓는 것과 어제 끝난 것 */
typedef struct Ledger {
    int32_t today[CORE_RES_COUNT][CORE_LEDGER_COUNT];
    int32_t yesterday[CORE_RES_COUNT][CORE_LEDGER_COUNT];
} Ledger;

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

/* 은행에 amount 를 더하고(음수면 뺌) 실제 바뀐 양을 장부의 category 에 적습니다 */
int32_t eco_change(ResourceBank *bank, Ledger *ledger, int32_t resource, int32_t amount, int32_t category);
/* 비용 전체를 냅니다 (모두 낼 수 있을 때만). 장부의 category 에 적습니다 */
int eco_pay(ResourceBank *bank, Ledger *ledger, const int32_t cost[CORE_RES_COUNT], int32_t category);
/* 하루가 끝나면 오늘 장부를 어제로 옮기고 오늘을 비웁니다 */
void eco_roll_ledger(Ledger *ledger);
/* 어제 하루의 순변화 */
int32_t eco_ledger_net(const Ledger *ledger, int32_t resource);
/* 저장 한도를 바꾸고, 넘친 물건은 버립니다 (장부: 기타) */
void eco_set_capacity(ResourceBank *bank, Ledger *ledger, int32_t capacity);

#endif
