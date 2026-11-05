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
int placement_demolish(World *world, Building *building);
int placement_remove_road(World *world, int32_t x, int32_t y);

/* 건물이 문 앞 칸을 쓰는지 (주민이 오가는 건물) */
int placement_needs_door(const BuildingDef *def, int32_t type);
/* 작업 반경 안의 관련 지형 칸 수 (배치 규칙이 없는 건물은 -1) */
int32_t placement_count_terrain(const World *world, const BuildingDef *def, int32_t x, int32_t y, int32_t w,
                                int32_t h);
/* 지형 칸 수 → 생산성 % */
int32_t placement_productivity(const BuildingDef *def, int32_t terrain_count);

/* 건물 목록으로부터 점유·문·막힘 층을 다시 만들고 비용 격자를 다시 계산합니다 (불러온 뒤 등) */
void placement_rebuild_layers(World *world);
/* 모든 건물의 생산성을 지도에서 다시 계산합니다 */
void placement_refresh_productivity(World *world);
/* 한 틱: 짓는 중인 건물을 한 틱씩 진행하고, 다 지으면 알립니다 */
void placement_tick(World *world);

#endif
