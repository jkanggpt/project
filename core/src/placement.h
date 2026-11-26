/*
 * placement.h — 배치, 건설, 철거, 도로
 *
 * 지도 위에 무엇을 지을 수 있는지(규칙), 짓는 일(비용 지불, 칸 차지, 문 앞 정하기),
 * 짓는 시간(건설 틱), 허무는 일(절반 환불)을 맡습니다.
 * 건물이 바뀌면 지도의 점유·문·막힘 층과 이동 비용 격자를 다시 계산합니다.
 */
#ifndef CASTLE_PLACEMENT_H
#define CASTLE_PLACEMENT_H

#include <stdint.h>

#include "world.h"

#define PLACEMENT_MAX_LINE 256 /* 한 번에 놓을 수 있는 도로/성벽 칸 수 */

/* 배치 검사. out 은 NULL 이 아니어야 합니다. 결과는 out->result (CorePlaceResult) */
void placement_check(const World *world, int32_t type, int32_t x, int32_t y, CorePlacementView *out);
/* 검사하고 짓습니다. CORE_OK, CORE_ERR_REJECTED(이유는 *out_reason), CORE_ERR_OUT_OF_MEMORY */
int placement_place(World *world, int32_t type, int32_t x, int32_t y, int32_t *out_id, int32_t *out_reason);
/* 도로·성벽 줄 놓기 */
int placement_line(World *world, int32_t type, int32_t x0, int32_t y0, int32_t x1, int32_t y1,
                   int32_t *out_placed, int32_t *out_reason);
/* 철거 (성채는 CORE_ERR_REJECTED) */

/* 건물이 문 앞 칸을 쓰는지 (주민이 오가는 건물) */
/* 생산성을 셀 때 세는 지형 (CoreTerrain), 지형을 세지 않는 건물이면 -1 */
int32_t placement_counted_terrain(const BuildingDef *def);
/* 작업 반경 안의 관련 지형 칸 수 (배치 규칙이 없는 건물은 -1) */
/* 지형 칸 수 → 생산성 % */

/* 건물 목록으로부터 점유·문·막힘 층을 다시 만들고 비용 격자를 다시 계산합니다 (불러온 뒤 등) */
void placement_rebuild_layers(World *world);
/* 모든 건물의 생산성을 지도에서 다시 계산합니다 */
/* 한 틱: 짓는 중인 건물을 한 틱씩 진행하고, 다 지으면 알립니다 */
/* 짓는 중인 건물을 바로 다 지음 (디버그 전용: core_debug_stress) */
/* 저장 한도를 다시 셉니다: 다 지은 저장 건물(성채·창고)의 한도 합 + 승급으로 늘어난 한도 */
void placement_refresh_capacity(World *world);

/* 건물에 피해를 줍니다. 체력이 0 이 되면 무너집니다 (일꾼·거주민 연결을 양쪽 모두 끊음) */
void placement_damage(World *world, Building *building, int32_t damage);
/* 수리 비용: 건설 비용의 절반 × 잃은 체력 비율 (올림). 고칠 곳이 없으면 모두 0 */
void placement_repair_cost(const World *world, const Building *building, int32_t out_cost[CORE_RES_COUNT]);
/* 수리: CORE_OK, 또는 CORE_ERR_REJECTED 와 *out_reason (CoreDenyReason) */
int placement_repair(World *world, Building *building, int32_t *out_reason);

#endif
