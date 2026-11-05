/*
 * world.h — 월드: 게임 상태 전체를 담는 구조체
 *
 * 게임의 모든 상태는 CoreWorld 하나 안에 있습니다. (전역 변수 없음)
 * 월드는 다른 구조체들을 "포함"합니다: 시계(Clock), 난수(Rng), 자원 은행(ResourceBank),
 * 지도(Grid), 이벤트 큐(EventQueue). 마일스톤 2부터 주민과 건물의 포인터 배열이 더해집니다.
 */
#ifndef CASTLE_WORLD_H
#define CASTLE_WORLD_H

#include <stdint.h>

#include "castle_core.h"
#include "clock.h"
#include "economy.h"
#include "events.h"
#include "grid.h"
#include "rng.h"

struct CoreWorld {
    char castle_name[CORE_NAME_BYTES]; /* UTF-8 */
    int32_t difficulty;                /* CoreDifficulty */
    int32_t seed;                      /* 새 게임 시드 (지도 생성에 씀) */
    int32_t keep_x;                    /* 성채 중심 타일 */
    int32_t keep_y;
    Clock clock;
    Rng rng;
    ResourceBank bank;
    Grid grid;
    EventQueue events;
};

typedef struct CoreWorld World;

/* 빈 월드를 할당합니다 (이벤트 큐 포함, 지도는 아직 없음). 실패하면 NULL */
World *world_alloc(void);
/* 월드와 월드가 가진 모든 메모리를 해제합니다. NULL 이어도 됩니다 */
void world_destroy(World *world);

/* 새 게임: 인자 검사 → 할당 → 지형 생성. 실패하면 *out 은 NULL 이고 메모리는 모두 해제됩니다 */
int world_create(const CoreNewGameParams *params, World **out);

/* ticks 만큼 시간을 진행합니다 */
int world_tick(World *world, int32_t ticks);

/* 이름 규칙: 1..CORE_NAME_BYTES-1 바이트, 올바른 UTF-8, 제어 문자 없음 */
int world_name_is_valid(const char *name, int32_t max_bytes);
/* 이름이 규칙에 맞으면 복사하고 CORE_OK, 아니면 CORE_ERR_INVALID_ARG */
int world_set_name(World *world, const char *name);

/* 저장되는 상태 전체의 해시 (FNV-1a 64비트) */
uint64_t world_state_hash(const World *world);

#endif
