/*
 * nature.h — 계절의 압박: 날씨, 자연 사건, 난방, 병
 *
 * 하루에 한 번(06:00 결산) 날씨를 정하고, 자연 사건을 예보하고 시작·진행·끝냅니다.
 * 사건마다 할 일은 처리 함수 표(NATURE_EVENTS, 함수 포인터)에 있습니다:
 *   가뭄(여름, 농장 −50%), 역병(병이 퍼짐), 폭풍(건물 피해), 혹한(겨울, 난방 나무 +1), 풍년(농장 +50%).
 * 사건은 언제나 하루 전에 예보합니다 ("위협은 공정하게": 준비할 시간이 있도록).
 *
 * 다른 모듈은 효과를 직접 계산하지 않고 여기 함수에 묻습니다:
 *   농장 생산 % → nature_farm_percent, 걷기 속도 % → nature_walk_percent, 난방 → nature_heating_per_house.
 */
#ifndef CASTLE_NATURE_H
#define CASTLE_NATURE_H

#include <stdint.h>

#include "castle_core.h"

typedef struct CoreWorld World;
typedef struct Villager Villager;
typedef struct Building Building;

#define NATURE_GRACE_DAYS 6          /* 처음 며칠은 사건이 없음 */
#define NATURE_CALM_DAYS 4           /* 사건이 끝나고 다음 예보까지 쉬는 날 */
#define NATURE_SICK_TICKS 432        /* 병이 저절로 낫는 기본 시간 (3일, 체력에 따라 달라짐) */
#define NATURE_HEAL_TICKS 6          /* 치료소에서 낫는 데 걸리는 시간 (1시간) */
#define NATURE_FOOD_PER_DAY_X10 12   /* 겨울 준비 계산: 주민 한 명이 하루에 먹는 식량 × 10 */

typedef struct Nature {
    int32_t weather;    /* CoreWeather */
    int32_t event;      /* 진행 중인 사건 (CoreNatureEvent) */
    int32_t event_days; /* 남은 날 (오늘 포함) */
    int32_t forecast;   /* 내일 시작할 사건 */
    int32_t forecast_days;
    int32_t calm_days;  /* 마지막 사건이 끝난 뒤 지난 날 */
    int32_t cold_houses;/* 지난밤 난방을 못 한 집 수 (저장함: 기분에 영향) */
} Nature;

void nature_init(Nature *nature);

/* 06:00 결산: 사건 진행/시작 → 날씨 → 난방 → 겨울 경고 → 다음 사건 예보 */
void nature_daily(World *world);

/* 지금 농장 생산 % (계절 × 날씨 × 사건) */
int32_t nature_farm_percent(const World *world);
/* 지금 걷기 속도 % (눈·폭풍이면 느림) */
int32_t nature_walk_percent(const World *world);
/* 집 한 채가 하룻밤에 쓰는 목재 */
int32_t nature_heating_per_house(const World *world);
/* 가을·겨울: 봄까지 필요한 식량과 목재 (다른 계절은 0) */
void nature_winter_needs(const World *world, int32_t *out_food, int32_t *out_wood);

/* 주민을 앓게 합니다 (이미 아프거나 떠나는 중이면 아무 일 없음). 앓게 했으면 1 */
int nature_make_sick(World *world, Villager *villager);
/* 아픈 주민의 한 틱: 남은 시간을 줄이고, 건강을 깎고, 시간이 다 되면 저절로 낫습니다 */
void nature_sick_tick(World *world, Villager *villager);
/* 병을 고칩니다 (by_healer = 1 이면 치료소에서) */
void nature_cure(World *world, Villager *villager, int by_healer);

/* 다 지었고 일꾼이 지금 일하고 있는 치료소 중 가장 가까운 곳 (약초가 없으면 NULL) */
Building *nature_open_healer(const World *world, const Villager *villager);

#endif
