/*
 * world.h — 월드: 게임 상태 전체를 담는 구조체
 *
 * 게임의 모든 상태는 CoreWorld 하나 안에 있습니다. (전역 변수 없음)
 * 월드는 다른 구조체들을 "포함"합니다: 시계(Clock), 난수(Rng), 자원 은행(ResourceBank), 장부(Ledger),
 * 지도(Grid), 이벤트 큐(EventQueue), 정의(Defs), 날씨·자연 사건(Nature), 시장(Market), 떠돌이 상인(Merchant),
 * 위협과 습격(ThreatState: 습격의 적 목록은 동적 배열), 연구(ResearchState), 한 판의 기록(RunStats).
 * 주민과 건물은 하나씩 따로 할당하고, 월드는 그 포인터들의 배열을 realloc 으로 키워 가며 가집니다.
 */
#ifndef CASTLE_WORLD_H
#define CASTLE_WORLD_H

#include <stdint.h>

#include "castle_core.h"
#include "clock.h"
#include "defs.h"
#include "economy.h"
#include "entity.h"
#include "events.h"
#include "grid.h"
#include "market.h"
#include "nature.h"
#include "research.h"
#include "rng.h"
#include "threat.h"

/* 하루 일정 (분) */
#define WORLD_WORK_START_MIN (7 * 60)  /* 07:00 일 시작 */
#define WORLD_WORK_END_MIN (18 * 60)   /* 18:00 일 끝 */
#define WORLD_DAILY_MIN (6 * 60)       /* 06:00 하루 결산 (임금, 세금, 이주) */

/* 한 판의 기록 (승리·패배 화면). 저장함 */
typedef struct RunStats {
    int32_t peak_population;
    int32_t enemies_killed;
    int32_t villagers_died;
    int32_t villagers_left;
    int32_t buildings_built;  /* 다 지은 건물 (성벽 칸 제외) */
    int32_t victory_day;      /* 왕성이 된 날 (0 = 아직) */
} RunStats;

struct CoreWorld {
    char castle_name[CORE_NAME_BYTES]; /* UTF-8 */
    int32_t difficulty;                /* CoreDifficulty */
    int32_t seed;                      /* 새 게임 시드 (지도 생성에 씀) */
    int32_t keep_x;                    /* 성채 중심 타일 */
    int32_t keep_y;
    int32_t keep_id;                   /* 성채 건물 id */
    int32_t rank;                      /* 등급 (CoreRank) */
    int32_t rank_ready_announced;      /* 승급 조건을 채웠다고 알린 등급 (같은 알림을 두 번 하지 않음) */
    int32_t outcome;                   /* CoreOutcome: 진행 중 / 승리 / 패배 */
    int32_t lose_reason;               /* CoreLoseReason */
    int32_t tutorial_done;             /* 이룬 튜토리얼 목표 비트 */
    int32_t tutorial_flags;            /* 튜토리얼이 기억하는 일 (주민을 직접 배치했음) */
    int32_t next_villager_id;          /* 다음 주민 id (다시 쓰지 않음, 저장함) */
    int32_t next_building_id;          /* 다음 건물 id */
    int32_t mourning;                  /* 최근 죽음으로 인한 슬픔 0..30 */
    Clock clock;
    Rng rng;
    ResourceBank bank;
    Ledger ledger;
    Grid grid;
    EventQueue events;
    Defs defs;                         /* 이 월드가 쓰는 정의 (복사본) */
    Nature nature;                     /* 날씨, 자연 사건, 예보 */
    Market market;                     /* 시장 가격 */
    Merchant merchant;                 /* 떠돌이 상인 */
    ThreatState threat;                /* 예고된 위협과 진행 중인 습격 */
    ResearchState research;            /* 끝낸 연구, 연구 중인 것 */
    RunStats stats;                    /* 한 판의 기록 */
    /* 연구 효과를 적용하기 전의 건물 정의. 효과는 늘 여기서 시작해 다시 계산합니다 (research_reapply) */
    BuildingDef base_buildings[CORE_BLD_COUNT];

    Villager **villagers;              /* 주민 포인터 배열 */
    int32_t villager_count;
    int32_t villager_capacity;
    Building **buildings;              /* 건물 포인터 배열 */
    int32_t building_count;
    int32_t building_capacity;

    /* 알림을 너무 자주 보내지 않기 위한 기록 (저장하지 않음, 게임 결과에 영향 없음) */
    int32_t storage_full_day[CORE_RES_COUNT];
};

typedef struct CoreWorld World;

/* 빈 월드를 할당합니다 (이벤트 큐와 정의 복사본 포함, 지도는 아직 없음). defs 가 NULL 이면 내장 기본값.
 * 실패하면 NULL */
World *world_alloc(const Defs *defs);
/* 월드와 월드가 가진 모든 메모리를 해제합니다. NULL 이어도 됩니다 */
void world_destroy(World *world);

/* 새 게임: 인자 검사 → 할당 → 지형 생성 → 성채와 첫 주민. 실패하면 *out 은 NULL 이고 메모리는 모두 해제됩니다 */
int world_create(const CoreNewGameParams *params, const Defs *defs, World **out);

/* 이 월드의 건물 정의 (종류가 잘못되면 NULL) */
const BuildingDef *world_building_def(const World *world, int32_t type);
/* 성채 건물 (없으면 NULL) */
Building *world_keep(const World *world);

/* ticks 만큼 시간을 진행합니다 */
int world_tick(World *world, int32_t ticks);

/* 이름 규칙: 1..CORE_NAME_BYTES-1 바이트, 올바른 UTF-8, 제어 문자 없음 */
int world_name_is_valid(const char *name, int32_t max_bytes);
/* 이름이 규칙에 맞으면 복사하고 CORE_OK, 아니면 CORE_ERR_INVALID_ARG */
int world_set_name(World *world, const char *name);

/* 저장되는 상태 전체의 해시 (FNV-1a 64비트) */
uint64_t world_state_hash(const World *world);

#endif
