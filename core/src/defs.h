/*
 * defs.h — 정의 (건물·특성 수치, 주민 이름 목록)
 *
 * 게임의 밸런스 수치는 모두 여기에 모입니다. 기본값은 defs_builtin.c 의 const 표에 있고,
 * game/data/ 의 .def 파일이 있으면 그 값으로 덮어씁니다. 파일이 없거나 망가졌으면 기본값을 쓰고 경고를 남깁니다.
 *
 * - 건물 종류는 고정(열거형)이고, 수치만 파일로 바꿀 수 있습니다.
 * - 특성, 이름, 연구는 개수를 미리 모르므로 동적 배열(realloc 으로 키움)입니다.
 * - 등급은 다섯 개로 정해져 있고(촌락 … 왕성), 조건 수치만 파일로 바꿉니다.
 * - 월드는 정의를 "복사"해서 가집니다. 그래서 Godot 이 정의 묶음을 해제해도 월드는 안전합니다.
 */
#ifndef CASTLE_DEFS_H
#define CASTLE_DEFS_H

#include <stddef.h>
#include <stdint.h>

#include "castle_core.h"

#define DEFS_MAX_WARNINGS 8
#define DEFS_MIN_NAMES 10
#define DEFS_ENEMY_COUNT 6 /* 적 종류 수 (그림과 습격 구성이 종류마다 있어서 정의 파일로 새로 만들 수는 없음) */

/* 적 종류 번호 (data/enemies.def 의 순서) */
enum {
    ENEMY_BANDIT = 0,  /* 도적 */
    ENEMY_BRUTE = 1,   /* 도적 두목 */
    ENEMY_GOBLIN = 2,  /* 마물: 고블린 */
    ENEMY_OGRE = 3,    /* 마물: 오우거 */
    ENEMY_SOLDIER = 4, /* 영주군 병사 */
    ENEMY_ARCHER = 5   /* 영주군 궁수 */
};

typedef struct BuildingDef {
    char key[CORE_KEY_BYTES]; /* 정의 파일에서 쓰는 영어 이름 (예: "farm") */
    int32_t w;
    int32_t h;
    int32_t cost[CORE_RES_COUNT];
    int32_t build_ticks;
    int32_t max_hp;
    int32_t worker_slots;
    int32_t resident_slots;
    int32_t storage;
    int32_t produces;       /* 자원 번호, -1 = 없음 */
    int32_t rate;           /* 일꾼 한 명이 일하는 한 틱에 만드는 양 × 1000 */
    int32_t secondary;
    int32_t secondary_rate;
    int32_t work_radius;
    int32_t placement;      /* CorePlacementRule */
    int32_t min_terrain;
    int32_t ideal_terrain;
    int32_t wage;
    int32_t required_rank;
    int32_t category;       /* CoreBuildCategory */
    int32_t line;
    int32_t attack;         /* 감시탑 화살 / 경비병 한 명의 피해 (0 = 싸우지 않음) */
    int32_t attack_range;   /* 칸 */
    int32_t attack_cooldown;/* 게임 속 분 */
    int32_t defense;        /* 방어 점수 (승급 조건). 다 지은 건물만 셈 */
} BuildingDef;

typedef struct EnemyDef {
    char key[CORE_KEY_BYTES];
    int32_t hp;
    int32_t damage;
    int32_t cooldown;      /* 공격 사이 (게임 속 분) */
    int32_t speed;         /* 한 틱에 걷는 거리 × 1000 */
    int32_t range;         /* 칸 */
    int32_t behavior;      /* CoreEnemyBehavior */
    int32_t loot_resource; /* -1 = 없음 */
    int32_t loot_amount;
    int32_t mana_reward;
} EnemyDef;

typedef struct TraitDef {
    char key[CORE_KEY_BYTES];
    int32_t work_pct;
    int32_t food_pct;
    int32_t mood;
    int32_t bravery;
    int32_t magic_pct;
} TraitDef;

/* 연구 하나 (data/research.def). 파일이 목록 전체를 정합니다 (개수를 미리 모름 → 동적 배열) */
typedef struct ResearchDef {
    char key[CORE_KEY_BYTES];
    int32_t branch;        /* CoreResearchBranch */
    int32_t requires;      /* 먼저 끝내야 하는 연구 번호 (-1 = 없음). 파일에는 앞에 나온 연구의 키로 적음 */
    int32_t required_rank; /* 시작하려면 필요한 등급 */
    int32_t cost_gold;
    int32_t cost_mana;
    int32_t points;        /* 마탑에서 모아야 하는 연구 점수 */
    int32_t effect;        /* CoreResearchEffect (research.c 의 효과 함수 표 번호) */
    int32_t value;         /* 효과의 크기 */
} ResearchDef;

/* 등급 하나 (data/ranks.def): 이 등급으로 오르는 조건과 공물, 오르면 늘어나는 저장 한도 */
typedef struct RankDef {
    char key[CORE_KEY_BYTES];
    int32_t population;
    int32_t mood;          /* 평균 기분 */
    int32_t defense;       /* 방어 점수 */
    int32_t research;      /* 끝낸 연구 수 */
    int32_t tribute_gold;
    int32_t tribute_mana;
    int32_t storage;       /* 오르면 늘어나는 저장 한도 */
} RankDef;

typedef struct NameEntry {
    char text[CORE_NAME_BYTES];
} NameEntry;

/* 망가진 정의 파일 기록 (새 월드를 만들 때 경고 이벤트로 나감) */
typedef struct DefWarning {
    int32_t file_id;
    int32_t line;
} DefWarning;

typedef struct Defs {
    BuildingDef buildings[CORE_BLD_COUNT];
    EnemyDef enemies[DEFS_ENEMY_COUNT];
    TraitDef *traits;   /* 동적 배열 */
    int32_t trait_count;
    int32_t trait_capacity;
    NameEntry *names;   /* 동적 배열 */
    int32_t name_count;
    int32_t name_capacity;
    ResearchDef *research; /* 동적 배열 */
    int32_t research_count;
    int32_t research_capacity;
    RankDef ranks[CORE_RANK_COUNT];
    DefWarning warnings[DEFS_MAX_WARNINGS];
    int32_t warning_count;
} Defs;

/* 공개 핸들 CoreDefs 는 Defs 하나를 감싼 것입니다 */
struct CoreDefs {
    Defs defs;
};

/* 내장 기본값 (defs_builtin.c) */
extern const BuildingDef BUILTIN_BUILDINGS[CORE_BLD_COUNT];
extern const EnemyDef BUILTIN_ENEMIES[DEFS_ENEMY_COUNT];
extern const TraitDef BUILTIN_TRAITS[];
extern const int32_t BUILTIN_TRAIT_COUNT;
extern const char *const BUILTIN_NAMES[];
extern const int32_t BUILTIN_NAME_COUNT;
extern const ResearchDef BUILTIN_RESEARCH[];
extern const int32_t BUILTIN_RESEARCH_COUNT;
extern const RankDef BUILTIN_RANKS[CORE_RANK_COUNT];

/* 기본값으로 채웁니다. CORE_OK 또는 CORE_ERR_OUT_OF_MEMORY (실패하면 아무것도 남기지 않음) */
int defs_init_builtin(Defs *defs);
void defs_free(Defs *defs);
/* src 를 dst 로 깊은 복사 (dst 는 비어 있어야 함) */
int defs_copy(Defs *dst, const Defs *src);
/* 정의 파일 내용을 읽어 덮어씁니다. 실패하면 defs 는 그대로이고 경고가 남습니다 */
int defs_load_text(Defs *defs, int32_t file_id, const char *text, size_t length, int32_t *out_line);

/* 정의를 정의 파일 형식으로 씁니다 (기본값으로 data 파일을 다시 만들 때). CORE_OK / FILE_OPEN / FILE_IO */
int defs_write_file(const Defs *defs, int32_t file_id, const char *path);

const BuildingDef *defs_building(const Defs *defs, int32_t type);
const EnemyDef *defs_enemy(const Defs *defs, int32_t type);
const ResearchDef *defs_research(const Defs *defs, int32_t index);
/* 연구 키 → 번호, 없으면 -1 */
int32_t defs_find_research(const Defs *defs, const char *key);
/* 특성 키 → 번호, 없으면 -1 */
int32_t defs_find_trait(const Defs *defs, const char *key);
/* 자원 키("food") → 번호, "none" → -1, 모르면 -2 */
int32_t defs_resource_from_key(const char *key);
const char *defs_resource_key(int32_t resource);

#endif
