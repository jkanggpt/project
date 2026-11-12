/*
 * population.h — 주민 수의 변화: 첫 주민, 이주, 떠남, 죽음
 */
#ifndef CASTLE_POPULATION_H
#define CASTLE_POPULATION_H

#include <stdint.h>

#include "world.h"

#define POP_IMMIGRATION_MOOD 55 /* 평균 기분이 이 이상이고 빈 집이 있으면 이주민이 옴 */
#define POP_MAX_IMMIGRANTS 3    /* 하루에 오는 최대 이주민 수 */
#define POP_FOOD_DAYS_FOR_IMMIGRANTS 5 /* 지금 주민이 이만큼의 날을 먹을 식량이 있어야 이주민이 옴 */
#define POP_MOURNING_PER_DEATH 10
#define POP_MOURNING_MAX 30

/* (x, y) 에 새 주민을 만듭니다 (이름, 나이, 능력치, 특성은 난수). 실패하면 NULL */
Villager *population_create(World *world, int32_t x, int32_t y);
/* 새 게임의 첫 주민들 (성채 문 앞). CORE_OK 또는 CORE_ERR_OUT_OF_MEMORY */
int population_spawn_initial(World *world);
/* 하루 결산: 이주민 */
void population_daily(World *world);
/* 마을을 떠나기 시작함 (일터·집 연결을 끊고 가장자리로 감) */
void population_begin_leaving(World *world, Villager *villager);
/* 가장자리에 도착해 떠남: 목록에서 지움 */
void population_remove_departed(World *world, Villager *villager);
/* 죽음: 알림, 슬픔, 목록에서 지움 */
void population_kill(World *world, Villager *villager, int32_t cause);

#endif
