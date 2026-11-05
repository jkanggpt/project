/*
 * defs.h — 정의 (건물·특성 수치, 주민 이름 목록)
 *
 * 게임의 밸런스 수치는 모두 여기에 모입니다. 기본값은 defs_builtin.c 의 const 표에 있고,
 * game/data/ 의 .def 파일이 있으면 그 값으로 덮어씁니다. 파일이 없거나 망가졌으면 기본값을 쓰고 경고를 남깁니다.
 *
 * - 건물 종류는 고정(열거형)이고, 수치만 파일로 바꿀 수 있습니다.
 * - 특성과 이름은 개수를 미리 모르므로 동적 배열(realloc 으로 키움)입니다.
 * - 월드는 정의를 "복사"해서 가집니다. 그래서 Godot 이 정의 묶음을 해제해도 월드는 안전합니다.
 */
#ifndef CASTLE_DEFS_H
#define CASTLE_DEFS_H

#include <stddef.h>
#include <stdint.h>

#include "castle_core.h"

#define DEFS_MAX_WARNINGS 8
#define DEFS_MIN_NAMES 10

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
} BuildingDef;

typedef struct TraitDef {
    char key[CORE_KEY_BYTES];
    int32_t work_pct;
    int32_t food_pct;
    int32_t mood;
    int32_t bravery;
    int32_t magic_pct;
} TraitDef;

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
    TraitDef *traits;   /* 동적 배열 */
    int32_t trait_count;
    int32_t trait_capacity;
    NameEntry *names;   /* 동적 배열 */
    int32_t name_count;
    int32_t name_capacity;
    DefWarning warnings[DEFS_MAX_WARNINGS];
    int32_t warning_count;
} Defs;

/* 공개 핸들 CoreDefs 는 Defs 하나를 감싼 것입니다 */
struct CoreDefs {
    Defs defs;
};

/* 내장 기본값 (defs_builtin.c) */
extern const BuildingDef BUILTIN_BUILDINGS[CORE_BLD_COUNT];
extern const TraitDef BUILTIN_TRAITS[];
extern const int32_t BUILTIN_TRAIT_COUNT;
extern const char *const BUILTIN_NAMES[];
extern const int32_t BUILTIN_NAME_COUNT;

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
/* 특성 키 → 번호, 없으면 -1 */
int32_t defs_find_trait(const Defs *defs, const char *key);
/* 자원 키("food") → 번호, "none" → -1, 모르면 -2 */
int32_t defs_resource_from_key(const char *key);
const char *defs_resource_key(int32_t resource);

#endif
