/* jobs.h — 현재 능률 조회와 일터 연결 */
#ifndef CASTLE_JOBS_H
#define CASTLE_JOBS_H
#include "world.h"
int32_t jobs_work_percent(const World *world, const Villager *villager, int32_t building_type);
void jobs_auto_assign(World *world);
void jobs_assign_housing(World *world);
void jobs_on_buildings_changed(World *world);
int jobs_assign_manual(World *world, Villager *villager, Building *building);
void jobs_unassign_manual(World *world, Villager *villager);
void jobs_release_manual(World *world, Villager *villager);
#endif
