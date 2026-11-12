/*
 * jobs.h — 일자리: 직업별 작업 함수 표, 일꾼 자동 배치, 집 배정, 임금과 세금
 *
 * 주민이 일터에서 WORKING 상태로 도착해 있을 때만 생산이 쌓입니다 (걷는 시간 = 일 못 하는 시간).
 * 어떤 일을 하는지는 일터 건물 종류로 고르는 함수 포인터 표 JOB_TABLE[type] 이 정합니다.
 */
#ifndef CASTLE_JOBS_H
#define CASTLE_JOBS_H

#include <stdint.h>

#include "world.h"

#define JOBS_TAX_PER_HOUSED 3 /* 집이 있는 주민 한 명이 하루에 내는 세금 (금화) */

/* 한 틱 일하기 (villager 는 일터에 도착해 WORKING 상태여야 함) */
void jobs_work(World *world, Villager *villager);
/* 일 속도 % (기분, 특성, 능력치를 합친 값). 기분 50, 보통 능력치면 약 100 */
int32_t jobs_work_percent(const World *world, const Villager *villager, int32_t building_type);
/* 이 일에 쓰이는 능력치 */
int32_t jobs_relevant_stat(const Villager *villager, int32_t building_type);

/* 빈 일자리에 놀고 있는 주민을, 집 없는 주민에게 빈 집을 배정합니다 */
void jobs_auto_assign(World *world);
void jobs_assign_housing(World *world);
/* 건물이 생기거나 없어졌을 때 (배정을 다시 함) */
void jobs_on_buildings_changed(World *world);

/* 플레이어의 직접 배치 / 빼기 / 자동에 맡기기 */
int jobs_assign_manual(World *world, Villager *villager, Building *building);
void jobs_unassign_manual(World *world, Villager *villager);
void jobs_release_manual(World *world, Villager *villager);

/* 하루 결산: 임금을 주고 세금을 걷습니다 */
void jobs_pay_wages_and_taxes(World *world);

#endif
