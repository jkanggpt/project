/*
 * market.h — 시장과 떠돌이 상인 (경제 모듈의 거래 부분)
 *
 * 시장: 일꾼이 있는 시장 건물이 있으면 식량·목재·석재·약초를 금화로 사고팝니다 (한 묶음 = 10개).
 *   - 가격은 금화 × 100 (소수 둘째 자리까지)으로 들고 있다가, 보여 줄 때 정수 금화로 바꿉니다.
 *   - 살 때는 비싸게, 팔 때는 싸게 (차이 = spread). 그래서 사서 바로 팔면 손해입니다.
 *   - 한 묶음 살 때마다 값이 오르고, 팔 때마다 내립니다 (수요와 공급).
 *   - 매일 아침 계절에 맞는 목표 가격 쪽으로 조금씩 돌아가고, 약간 흔들립니다. (겨울엔 식량·목재가 비쌈)
 * 떠돌이 상인: 열흘 남짓마다 이틀 머물며 거래 제안 3~4개를 합니다. 마정석을 살 수 있는 유일한 곳입니다.
 */
#ifndef CASTLE_MARKET_H
#define CASTLE_MARKET_H

#include <stdint.h>

#include "castle_core.h"

typedef struct CoreWorld World;

#define MARKET_SPREAD_PCT 25      /* 사는 값과 파는 값의 차이 (기준의 25%) */
#define MARKET_IMPACT_PCT 4       /* 한 묶음 거래가 움직이는 값 */
#define MARKET_RECOVER_PCT 15     /* 하루에 목표 가격 쪽으로 돌아가는 비율 */
#define MARKET_MIN_PCT 40         /* 기준 가격의 40%~300% 안에서만 움직임 */
#define MARKET_MAX_PCT 300
#define MERCHANT_STAY_DAYS 2
#define MERCHANT_FIRST_VISIT_DAY 7
#define MERCHANT_MIN_GAP 10       /* 다음 방문까지 10~16일 */
#define MERCHANT_MAX_GAP 16

typedef struct Market {
    int32_t price[CORE_RES_COUNT]; /* 한 묶음의 값 × 100 (금화). 다루지 않는 자원은 0 */
} Market;

typedef struct MerchantDeal {
    int32_t give_resource; /* 플레이어가 내는 것 */
    int32_t give_amount;
    int32_t get_resource;  /* 플레이어가 받는 것 */
    int32_t get_amount;
    int32_t taken;
} MerchantDeal;

typedef struct Merchant {
    int32_t days_left;      /* 0 = 없음 */
    int32_t next_visit_day; /* 다음에 올 날 */
    int32_t deal_count;
    MerchantDeal deals[CORE_MERCHANT_MAX_DEALS];
} Merchant;

void market_init(Market *market);
void merchant_init(Merchant *merchant);

/* 시장에서 다루는 자원인가 */
int market_is_tradable(int32_t resource);
/* 기준 가격 (한 묶음, 금화). 다루지 않으면 0 */
int32_t market_base_price(int32_t resource);
/* 한 묶음 살 때/팔 때 금화 (지금 가격 기준) */
int32_t market_buy_price(const Market *market, int32_t resource);
int32_t market_sell_price(const Market *market, int32_t resource);
/* 거래할 수 있으면 1. 아니면 0 과 *out_reason (CoreDenyReason) */
int market_is_open(const World *world, int32_t *out_reason);

/* 사거나(lots > 0) 팝니다(lots < 0). 전부 할 수 있을 때만 하고 CORE_OK, 아니면 CORE_ERR_REJECTED */
int market_trade(World *world, int32_t resource, int32_t lots, int32_t *out_gold, int32_t *out_reason);

/* 상인의 제안을 받아들입니다 */
int merchant_accept(World *world, int32_t index, int32_t *out_reason);

/* 06:00 결산: 가격이 목표 쪽으로 움직이고, 상인이 오거나 떠납니다 */
void market_daily(World *world);

#endif
