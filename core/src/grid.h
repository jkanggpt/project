/*
 * grid.h — 지도: 지형 격자, 이동 비용 격자, 지형 생성
 *
 * 지도는 width × height 타일이고, 격자는 행 우선 1차원 배열(index = y * width + x)로 저장합니다.
 * 크기는 월드를 만들 때 정해지므로 calloc 으로 할당합니다.
 * 이동 비용 격자는 지형(나중에는 도로, 건물, 성벽)으로부터 계산되며, C 그래픽 프런트엔드은 이것으로 길찾기 격자를 만듭니다.
 */
#ifndef CASTLE_GRID_H
#define CASTLE_GRID_H

#include <stdint.h>

#include "castle_core.h"
#include "rng.h"

#define GRID_DEFAULT_SIZE 96
#define GRID_MIN_SIZE 32
#define GRID_MAX_SIZE 256

#define GRID_KEEP_SIZE 3        /* 성채가 차지하는 칸 (3×3, 중심이 keep_x, keep_y) */
#define GRID_KEEP_CLEAR 3       /* 성채 중심에서 이 거리(체비쇼프)까지는 풀밭으로 비웁니다 */
#define GRID_NO_WATER_RADIUS 9  /* 성채 근처에는 호수를 만들지 않습니다 */
#define GRID_FAIR_RADIUS 14     /* "공정한 시작" 검사 반지름 (유클리드) */

/* 이동 비용: 0 = 지나갈 수 없음, 10 = 보통 풀밭 걸음 (값은 castle_core.h 의 CORE_COST_*) */
#define GRID_COST_BLOCKED CORE_COST_BLOCKED
#define GRID_COST_NORMAL CORE_COST_NORMAL
#define GRID_COST_ROAD 5 /* 도로는 풀밭보다 두 배 빨리 걷습니다 */

/* 건물이 칸을 막는 방식 (block 층) */
#define GRID_BLOCK_NONE 0  /* 막지 않음 */
#define GRID_BLOCK_SOLID 1 /* 보통 건물: 아무도 못 지나감 */
#define GRID_BLOCK_WALL 2  /* 성벽: 주민은 못 지나가고, 적은 부수고 지나감 */
#define GRID_BLOCK_GATE 3  /* 성문: 주민은 지나가고, 적은 막힘 */

typedef struct Grid {
    int32_t width;
    int32_t height;
    uint8_t *terrain;    /* CoreTerrain 값, width*height 개 (저장함) */
    uint8_t *road;       /* 1 = 도로 (저장함) */
    int32_t *occupant;   /* 그 칸을 차지한 건물 id, 0 = 없음 (건물 목록에서 다시 계산) */
    uint8_t *door_count; /* 그 칸을 문 앞으로 쓰는 건물 수 (다시 계산) */
    uint8_t *block;      /* GRID_BLOCK_* (다시 계산) */
    uint8_t *cost;       /* 이동 비용 (다시 계산) */
    int32_t cost_rev;    /* 비용 격자가 바뀔 때마다 1 증가 → C 그래픽 프런트엔드이 길찾기를 다시 만들지 판단 */
    uint8_t *scratch_mark;  /* 길 검사용 작업 공간 (배치 검사 때마다 새로 할당하지 않으려고 미리 마련) */
    int32_t *scratch_queue;
} Grid;

/* 격자 메모리를 할당합니다. 실패하면 아무것도 남기지 않고 CORE_ERR_OUT_OF_MEMORY */
int grid_alloc(Grid *grid, int32_t width, int32_t height);
void grid_free(Grid *grid);

/* 아주 자주 불리므로 헤더에 inline 으로 둡니다 (길 검사, 비용 계산의 안쪽 반복문) */
static inline int grid_in_bounds(const Grid *grid, int32_t x, int32_t y)
{
    return x >= 0 && y >= 0 && x < grid->width && y < grid->height;
}

static inline int32_t grid_index(const Grid *grid, int32_t x, int32_t y)
{
    return y * grid->width + x;
}

/* 지도 밖이면 CORE_TERRAIN_WATER (지나갈 수 없는 칸으로 취급) */
uint8_t grid_terrain_at(const Grid *grid, int32_t x, int32_t y);

/* 지형 하나의 기본 이동 비용 */
uint8_t grid_terrain_cost(uint8_t terrain);
/* 지형, 도로, 건물(block 층)으로부터 비용 격자를 다시 계산하고 cost_rev 를 올립니다 */
void grid_rebuild_costs(Grid *grid);
/* 칸 하나가 가져야 할 비용 (불변 조건 검사용) */
uint8_t grid_expected_cost(const Grid *grid, int32_t index);

/* (cx, cy) 에서 유클리드 거리 radius 안에 있는 terrain 칸 수 */
int32_t grid_count_in_radius(const Grid *grid, int32_t cx, int32_t cy, int32_t radius, uint8_t terrain);

/* (sx, sy) 에서 상하좌우로 걸어서 닿는 칸을 out_mark 에 1로 표시합니다.
 * 닿은 칸 수를 돌려주고, 메모리가 부족하면 -1 */
int32_t grid_flood_reachable(const Grid *grid, int32_t sx, int32_t sy, uint8_t *out_mark);

/* 시드로 지형을 만듭니다. 성채 주변을 비우고, 공정한 시작과 연결성을 보장합니다.
 * CORE_OK 또는 CORE_ERR_OUT_OF_MEMORY */
int grid_generate(Grid *grid, uint32_t seed, Rng *rng, int32_t keep_x, int32_t keep_y);

/* 저장 파일용 지형 글자: . 풀밭  T 숲  ^ 바위  ~ 물  * 마정석 광맥 */
char grid_terrain_symbol(uint8_t terrain);
/* 모르는 글자면 -1 */
int grid_terrain_from_symbol(char symbol);

#endif
