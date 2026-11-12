/*
 * defs.c — 정의 파일 읽기 (설명은 defs.h)
 *
 * 정의 파일 형식 (UTF-8 텍스트):
 *   # 주석
 *   [building farm]        ← 섹션: 종류 이름 + 키
 *   cost_gold = 15         ← 필드 = 값
 *   produces = food
 *
 * 섹션 종류 이름 → 처리 함수 표(SECTION_PARSERS)로 줄을 나눠 보냅니다.
 * 필드는 이름 → 구조체 안의 위치(offsetof) 표(BUILDING_FIELDS, TRAIT_FIELDS)로 찾아 값을 넣습니다.
 * 파일 하나를 읽을 때는 임시 사본에 먼저 적용하고, 끝까지 문제가 없을 때만 진짜 정의와 바꿉니다.
 * 그래서 중간에 틀린 줄이 있어도 정의가 반쯤 바뀐 채로 남지 않습니다.
 */
#include "defs.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "mem.h"
#include "platform.h"
#include "text.h"

#define DEFS_LINE_MAX 256

static const char *const RESOURCE_KEYS[CORE_RES_COUNT] = {"gold", "mana", "food", "wood", "stone", "herbs"};
static const char *const PLACEMENT_KEYS[] = {"any", "near_forest", "adjacent_rock", "on_mana"};
static const char *const CATEGORY_KEYS[CORE_CAT_COUNT] = {"special", "housing", "resource", "storage",
                                                          "defense", "service", "road"};
static const char *const BEHAVIOR_KEYS[CORE_BEHAVIOR_COUNT] = {"thief", "raider", "monster"};
static const char *const BRANCH_KEYS[CORE_BRANCH_COUNT] = {"economy", "defense", "magic"};
/* 연구 효과 키 (순서는 CoreResearchEffect, 효과 함수는 research.c 의 RESEARCH_EFFECTS) */
static const char *const EFFECT_KEYS[CORE_EFFECT_COUNT] = {
    "farm_yield", "extract_yield", "storage", "market", "tower_damage", "wall_hp",
    "guard_damage", "keep_damage", "mana_yield", "healing", "heating", "ward"};

/* ---- 기본 함수 -------------------------------------------------------- */

int32_t defs_resource_from_key(const char *key)
{
    int32_t i;
    if (strcmp(key, "none") == 0) {
        return -1;
    }
    for (i = 0; i < CORE_RES_COUNT; i++) {
        if (strcmp(key, RESOURCE_KEYS[i]) == 0) {
            return i;
        }
    }
    return -2;
}

const char *defs_resource_key(int32_t resource)
{
    return (resource >= 0 && resource < CORE_RES_COUNT) ? RESOURCE_KEYS[resource] : "none";
}

const EnemyDef *defs_enemy(const Defs *defs, int32_t type)
{
    if (type < 0 || type >= DEFS_ENEMY_COUNT) {
        return NULL;
    }
    return &defs->enemies[type];
}

const BuildingDef *defs_building(const Defs *defs, int32_t type)
{
    if (type < 0 || type >= CORE_BLD_COUNT) {
        return NULL;
    }
    return &defs->buildings[type];
}

const ResearchDef *defs_research(const Defs *defs, int32_t index)
{
    if (index < 0 || index >= defs->research_count) {
        return NULL;
    }
    return &defs->research[index];
}

int32_t defs_find_research(const Defs *defs, const char *key)
{
    int32_t i;
    for (i = 0; i < defs->research_count; i++) {
        if (strcmp(defs->research[i].key, key) == 0) {
            return i;
        }
    }
    return -1;
}

int32_t defs_find_trait(const Defs *defs, const char *key)
{
    int32_t i;
    for (i = 0; i < defs->trait_count; i++) {
        if (strcmp(defs->traits[i].key, key) == 0) {
            return i;
        }
    }
    return -1;
}

void defs_free(Defs *defs)
{
    mem_free(defs->traits);
    mem_free(defs->names);
    mem_free(defs->research);
    memset(defs, 0, sizeof(*defs));
}

/* 동적 배열 하나를 count 개 이상 담을 수 있게 키웁니다 (임시 포인터로 받아 실패해도 원래 배열 유지) */
static int grow_array(void **array, int32_t *capacity, int32_t needed, size_t element_size)
{
    int32_t new_capacity;
    void *grown;
    if (needed <= *capacity) {
        return CORE_OK;
    }
    new_capacity = *capacity > 0 ? *capacity * 2 : 16;
    while (new_capacity < needed) {
        new_capacity *= 2;
    }
    grown = mem_realloc(*array, (size_t)new_capacity * element_size);
    if (grown == NULL) {
        return CORE_ERR_OUT_OF_MEMORY;
    }
    *array = grown;
    *capacity = new_capacity;
    return CORE_OK;
}

static int add_trait(Defs *defs, const TraitDef *trait)
{
    void *array = defs->traits;
    int rc = grow_array(&array, &defs->trait_capacity, defs->trait_count + 1, sizeof(TraitDef));
    defs->traits = (TraitDef *)array;
    if (rc != CORE_OK) {
        return rc;
    }
    defs->traits[defs->trait_count++] = *trait;
    return CORE_OK;
}

static int add_research(Defs *defs, const ResearchDef *research)
{
    void *array = defs->research;
    int rc;
    if (defs->research_count >= CORE_MAX_RESEARCH) {
        return CORE_ERR_FORMAT; /* 연구 창에 다 보여 줄 수 있는 만큼만 */
    }
    rc = grow_array(&array, &defs->research_capacity, defs->research_count + 1, sizeof(ResearchDef));
    defs->research = (ResearchDef *)array;
    if (rc != CORE_OK) {
        return rc;
    }
    defs->research[defs->research_count++] = *research;
    return CORE_OK;
}

static int add_name(Defs *defs, const char *name)
{
    void *array = defs->names;
    int rc = grow_array(&array, &defs->name_capacity, defs->name_count + 1, sizeof(NameEntry));
    defs->names = (NameEntry *)array;
    if (rc != CORE_OK) {
        return rc;
    }
    if (!text_copy(defs->names[defs->name_count].text, CORE_NAME_BYTES, name)) {
        return CORE_ERR_FORMAT;
    }
    defs->name_count++;
    return CORE_OK;
}

int defs_init_builtin(Defs *defs)
{
    int32_t i;
    memset(defs, 0, sizeof(*defs));
    memcpy(defs->buildings, BUILTIN_BUILDINGS, sizeof(defs->buildings));
    memcpy(defs->enemies, BUILTIN_ENEMIES, sizeof(defs->enemies));
    memcpy(defs->ranks, BUILTIN_RANKS, sizeof(defs->ranks));
    for (i = 0; i < BUILTIN_RESEARCH_COUNT; i++) {
        if (add_research(defs, &BUILTIN_RESEARCH[i]) != CORE_OK) {
            defs_free(defs);
            return CORE_ERR_OUT_OF_MEMORY;
        }
    }
    for (i = 0; i < BUILTIN_TRAIT_COUNT; i++) {
        if (add_trait(defs, &BUILTIN_TRAITS[i]) != CORE_OK) {
            defs_free(defs);
            return CORE_ERR_OUT_OF_MEMORY;
        }
    }
    for (i = 0; i < BUILTIN_NAME_COUNT; i++) {
        if (add_name(defs, BUILTIN_NAMES[i]) != CORE_OK) {
            defs_free(defs);
            return CORE_ERR_OUT_OF_MEMORY;
        }
    }
    return CORE_OK;
}

int defs_copy(Defs *dst, const Defs *src)
{
    memset(dst, 0, sizeof(*dst));
    memcpy(dst->buildings, src->buildings, sizeof(dst->buildings));
    memcpy(dst->enemies, src->enemies, sizeof(dst->enemies));
    memcpy(dst->ranks, src->ranks, sizeof(dst->ranks));
    memcpy(dst->warnings, src->warnings, sizeof(dst->warnings));
    if (src->research_count > 0) {
        dst->research = (ResearchDef *)mem_malloc((size_t)src->research_count * sizeof(ResearchDef));
        if (dst->research == NULL) {
            defs_free(dst);
            return CORE_ERR_OUT_OF_MEMORY;
        }
        memcpy(dst->research, src->research, (size_t)src->research_count * sizeof(ResearchDef));
        dst->research_count = src->research_count;
        dst->research_capacity = src->research_count;
    }
    dst->warning_count = src->warning_count;
    if (src->trait_count > 0) {
        dst->traits = (TraitDef *)mem_malloc((size_t)src->trait_count * sizeof(TraitDef));
        if (dst->traits == NULL) {
            defs_free(dst);
            return CORE_ERR_OUT_OF_MEMORY;
        }
        memcpy(dst->traits, src->traits, (size_t)src->trait_count * sizeof(TraitDef));
        dst->trait_count = src->trait_count;
        dst->trait_capacity = src->trait_count;
    }
    if (src->name_count > 0) {
        dst->names = (NameEntry *)mem_malloc((size_t)src->name_count * sizeof(NameEntry));
        if (dst->names == NULL) {
            defs_free(dst);
            return CORE_ERR_OUT_OF_MEMORY;
        }
        memcpy(dst->names, src->names, (size_t)src->name_count * sizeof(NameEntry));
        dst->name_count = src->name_count;
        dst->name_capacity = src->name_count;
    }
    return CORE_OK;
}

/* ---- 필드 표 ---------------------------------------------------------- */

typedef enum FieldKind {
    FIELD_INT,       /* 정수 */
    FIELD_RESOURCE,  /* 자원 키 (food, wood, …, none) */
    FIELD_PLACEMENT, /* 배치 규칙 키 */
    FIELD_CATEGORY,  /* 분류 키 */
    FIELD_BEHAVIOR,  /* 적 행동 키 (thief, raider, monster) */
    FIELD_BRANCH,    /* 연구 갈래 키 (economy, defense, magic) */
    FIELD_EFFECT     /* 연구 효과 키 (farm_yield, …) */
} FieldKind;

/* 필드 이름 → 구조체 안에서의 위치(바이트)와 허용 범위 */
typedef struct FieldDef {
    const char *name;
    size_t offset;
    FieldKind kind;
    int32_t min;
    int32_t max;
} FieldDef;

#define COST_FIELD(res) (offsetof(BuildingDef, cost) + (size_t)(res) * sizeof(int32_t))

static const FieldDef BUILDING_FIELDS[] = {
    {"width", offsetof(BuildingDef, w), FIELD_INT, 1, 4},
    {"height", offsetof(BuildingDef, h), FIELD_INT, 1, 4},
    {"cost_gold", COST_FIELD(CORE_RES_GOLD), FIELD_INT, 0, 100000},
    {"cost_mana", COST_FIELD(CORE_RES_MANA), FIELD_INT, 0, 100000},
    {"cost_food", COST_FIELD(CORE_RES_FOOD), FIELD_INT, 0, 100000},
    {"cost_wood", COST_FIELD(CORE_RES_WOOD), FIELD_INT, 0, 100000},
    {"cost_stone", COST_FIELD(CORE_RES_STONE), FIELD_INT, 0, 100000},
    {"cost_herbs", COST_FIELD(CORE_RES_HERBS), FIELD_INT, 0, 100000},
    {"build_ticks", offsetof(BuildingDef, build_ticks), FIELD_INT, 0, 10000},
    {"max_hp", offsetof(BuildingDef, max_hp), FIELD_INT, 1, 100000},
    {"workers", offsetof(BuildingDef, worker_slots), FIELD_INT, 0, 8},
    {"residents", offsetof(BuildingDef, resident_slots), FIELD_INT, 0, 12},
    {"storage", offsetof(BuildingDef, storage), FIELD_INT, 0, 100000},
    {"produces", offsetof(BuildingDef, produces), FIELD_RESOURCE, -1, CORE_RES_COUNT - 1},
    {"rate", offsetof(BuildingDef, rate), FIELD_INT, 0, 100000},
    {"secondary", offsetof(BuildingDef, secondary), FIELD_RESOURCE, -1, CORE_RES_COUNT - 1},
    {"secondary_rate", offsetof(BuildingDef, secondary_rate), FIELD_INT, 0, 100000},
    {"work_radius", offsetof(BuildingDef, work_radius), FIELD_INT, 0, 8},
    {"placement", offsetof(BuildingDef, placement), FIELD_PLACEMENT, 0, 3},
    {"min_terrain", offsetof(BuildingDef, min_terrain), FIELD_INT, 0, 200},
    {"ideal_terrain", offsetof(BuildingDef, ideal_terrain), FIELD_INT, 1, 200},
    {"wage", offsetof(BuildingDef, wage), FIELD_INT, 0, 100},
    {"required_rank", offsetof(BuildingDef, required_rank), FIELD_INT, 0, 4},
    {"category", offsetof(BuildingDef, category), FIELD_CATEGORY, 0, CORE_CAT_COUNT - 1},
    {"attack", offsetof(BuildingDef, attack), FIELD_INT, 0, 1000},
    {"attack_range", offsetof(BuildingDef, attack_range), FIELD_INT, 0, 12},
    {"attack_cooldown", offsetof(BuildingDef, attack_cooldown), FIELD_INT, 0, 1440},
    {"defense", offsetof(BuildingDef, defense), FIELD_INT, 0, 100},
};

static const FieldDef ENEMY_FIELDS[] = {
    {"hp", offsetof(EnemyDef, hp), FIELD_INT, 1, 100000},
    {"damage", offsetof(EnemyDef, damage), FIELD_INT, 0, 10000},
    {"cooldown", offsetof(EnemyDef, cooldown), FIELD_INT, 1, 1440},
    {"speed", offsetof(EnemyDef, speed), FIELD_INT, 100, 20000},
    {"range", offsetof(EnemyDef, range), FIELD_INT, 1, 12},
    {"behavior", offsetof(EnemyDef, behavior), FIELD_BEHAVIOR, 0, CORE_BEHAVIOR_COUNT - 1},
    {"loot", offsetof(EnemyDef, loot_resource), FIELD_RESOURCE, -1, CORE_RES_COUNT - 1},
    {"loot_amount", offsetof(EnemyDef, loot_amount), FIELD_INT, 0, 100000},
    {"mana_reward", offsetof(EnemyDef, mana_reward), FIELD_INT, 0, 1000},
};

/* requires(앞선 연구)는 키를 번호로 바꿔야 해서 field_research 가 따로 처리합니다 */
static const FieldDef RESEARCH_FIELDS[] = {
    {"branch", offsetof(ResearchDef, branch), FIELD_BRANCH, 0, CORE_BRANCH_COUNT - 1},
    {"required_rank", offsetof(ResearchDef, required_rank), FIELD_INT, 0, CORE_RANK_COUNT - 1},
    {"cost_gold", offsetof(ResearchDef, cost_gold), FIELD_INT, 0, 100000},
    {"cost_mana", offsetof(ResearchDef, cost_mana), FIELD_INT, 0, 100000},
    {"points", offsetof(ResearchDef, points), FIELD_INT, 1, 100000},
    {"effect", offsetof(ResearchDef, effect), FIELD_EFFECT, 0, CORE_EFFECT_COUNT - 1},
    {"value", offsetof(ResearchDef, value), FIELD_INT, 0, 1000},
};

static const FieldDef RANK_FIELDS[] = {
    {"population", offsetof(RankDef, population), FIELD_INT, 0, 10000},
    {"mood", offsetof(RankDef, mood), FIELD_INT, 0, 100},
    {"defense", offsetof(RankDef, defense), FIELD_INT, 0, 10000},
    {"research", offsetof(RankDef, research), FIELD_INT, 0, CORE_MAX_RESEARCH},
    {"tribute_gold", offsetof(RankDef, tribute_gold), FIELD_INT, 0, 1000000},
    {"tribute_mana", offsetof(RankDef, tribute_mana), FIELD_INT, 0, 100000},
    {"storage", offsetof(RankDef, storage), FIELD_INT, 0, 100000},
};

static const FieldDef TRAIT_FIELDS[] = {
    {"work", offsetof(TraitDef, work_pct), FIELD_INT, -90, 200},
    {"food", offsetof(TraitDef, food_pct), FIELD_INT, -90, 200},
    {"mood", offsetof(TraitDef, mood), FIELD_INT, -50, 50},
    {"bravery", offsetof(TraitDef, bravery), FIELD_INT, -3, 3},
    {"magic", offsetof(TraitDef, magic_pct), FIELD_INT, -90, 300},
};

#define COUNT_OF(array) ((int32_t)(sizeof(array) / sizeof((array)[0])))

/* 10진 정수 (앞뒤 공백 없이) */
static int parse_int(const char *text, int32_t *out)
{
    char *end = NULL;
    long value;
    if (!((text[0] >= '0' && text[0] <= '9') || (text[0] == '-' && text[1] >= '0' && text[1] <= '9'))) {
        return 0;
    }
    errno = 0;
    value = strtol(text, &end, 10);
    if (errno != 0 || end == NULL || *end != '\0' || value < INT32_MIN || value > INT32_MAX) {
        return 0;
    }
    *out = (int32_t)value;
    return 1;
}

static int32_t key_index(const char *const *keys, int32_t count, const char *key)
{
    int32_t i;
    for (i = 0; i < count; i++) {
        if (strcmp(keys[i], key) == 0) {
            return i;
        }
    }
    return -2;
}

/* 필드 표에서 key 를 찾아 entry(구조체) 안의 그 위치에 값을 씁니다 */
static int apply_field(void *entry, const FieldDef *fields, int32_t field_count, const char *key, const char *value)
{
    int32_t i;
    for (i = 0; i < field_count; i++) {
        const FieldDef *field = &fields[i];
        int32_t parsed;
        if (strcmp(field->name, key) != 0) {
            continue;
        }
        switch (field->kind) {
        case FIELD_RESOURCE:
            parsed = defs_resource_from_key(value);
            break;
        case FIELD_PLACEMENT:
            parsed = key_index(PLACEMENT_KEYS, COUNT_OF(PLACEMENT_KEYS), value);
            break;
        case FIELD_CATEGORY:
            parsed = key_index(CATEGORY_KEYS, CORE_CAT_COUNT, value);
            break;
        case FIELD_BEHAVIOR:
            parsed = key_index(BEHAVIOR_KEYS, CORE_BEHAVIOR_COUNT, value);
            break;
        case FIELD_BRANCH:
            parsed = key_index(BRANCH_KEYS, CORE_BRANCH_COUNT, value);
            break;
        case FIELD_EFFECT:
            parsed = key_index(EFFECT_KEYS, CORE_EFFECT_COUNT, value);
            break;
        case FIELD_INT:
        default:
            if (!parse_int(value, &parsed)) {
                return CORE_ERR_FORMAT;
            }
            break;
        }
        if (parsed < field->min || parsed > field->max) {
            return CORE_ERR_FORMAT;
        }
        /* 구조체의 시작 주소 + 필드 위치 = 그 필드의 주소 */
        *(int32_t *)((char *)entry + field->offset) = parsed;
        return CORE_OK;
    }
    return CORE_ERR_FORMAT; /* 모르는 필드 */
}

/* ---- 섹션 처리 -------------------------------------------------------- */

typedef struct ParseState {
    Defs *defs;      /* 고치고 있는 임시 사본 */
    void *entry;     /* 지금 섹션의 구조체 (BuildingDef*, TraitDef*, EnemyDef*, ResearchDef*, RankDef*) */
    int32_t section; /* SECTION_PARSERS 번호, -1 = 없음 */
} ParseState;

typedef int (*SectionBeginFn)(ParseState *state, const char *id);
typedef int (*SectionFieldFn)(ParseState *state, const char *key, const char *value);

typedef struct SectionParser {
    const char *keyword; /* [keyword id] */
    int32_t file_id;     /* 이 섹션이 들어갈 수 있는 파일 */
    SectionBeginFn begin;
    SectionFieldFn field;
} SectionParser;

static int begin_building(ParseState *state, const char *id)
{
    int32_t type;
    for (type = 0; type < CORE_BLD_COUNT; type++) {
        if (strcmp(state->defs->buildings[type].key, id) == 0) {
            state->entry = &state->defs->buildings[type];
            return CORE_OK;
        }
    }
    return CORE_ERR_FORMAT; /* 건물 종류는 정해져 있어서 새로 만들 수 없음 */
}

static int field_building(ParseState *state, const char *key, const char *value)
{
    return apply_field(state->entry, BUILDING_FIELDS, COUNT_OF(BUILDING_FIELDS), key, value);
}

static int begin_trait(ParseState *state, const char *id)
{
    int32_t index = defs_find_trait(state->defs, id);
    TraitDef fresh;
    int rc;
    if (index < 0) {
        /* 새 특성: 0 으로 채워 배열 끝에 붙입니다 (개수를 미리 모르므로 realloc 으로 키움) */
        memset(&fresh, 0, sizeof(fresh));
        if (!text_copy(fresh.key, sizeof(fresh.key), id)) {
            return CORE_ERR_FORMAT;
        }
        rc = add_trait(state->defs, &fresh);
        if (rc != CORE_OK) {
            return rc;
        }
        index = state->defs->trait_count - 1;
    }
    state->entry = &state->defs->traits[index];
    return CORE_OK;
}

static int field_trait(ParseState *state, const char *key, const char *value)
{
    return apply_field(state->entry, TRAIT_FIELDS, COUNT_OF(TRAIT_FIELDS), key, value);
}

static int begin_enemy(ParseState *state, const char *id)
{
    int32_t type;
    for (type = 0; type < DEFS_ENEMY_COUNT; type++) {
        if (strcmp(state->defs->enemies[type].key, id) == 0) {
            state->entry = &state->defs->enemies[type];
            return CORE_OK;
        }
    }
    return CORE_ERR_FORMAT; /* 적 종류도 정해져 있음 (그림과 습격 구성이 종류마다 있음) */
}

static int field_enemy(ParseState *state, const char *key, const char *value)
{
    return apply_field(state->entry, ENEMY_FIELDS, COUNT_OF(ENEMY_FIELDS), key, value);
}

/* 연구: 파일이 목록 전체를 정하므로(읽기 전에 비움) 섹션마다 끝에 새로 붙입니다 */
static int begin_research(ParseState *state, const char *id)
{
    ResearchDef fresh;
    int rc;
    if (defs_find_research(state->defs, id) >= 0) {
        return CORE_ERR_FORMAT; /* 같은 키가 두 번 */
    }
    memset(&fresh, 0, sizeof(fresh));
    if (!text_copy(fresh.key, sizeof(fresh.key), id)) {
        return CORE_ERR_FORMAT;
    }
    fresh.requires = -1;
    fresh.points = 1;
    rc = add_research(state->defs, &fresh);
    if (rc != CORE_OK) {
        return rc;
    }
    state->entry = &state->defs->research[state->defs->research_count - 1];
    return CORE_OK;
}

static int field_research(ParseState *state, const char *key, const char *value)
{
    ResearchDef *research = (ResearchDef *)state->entry;
    if (strcmp(key, "requires") == 0) {
        int32_t index;
        if (strcmp(value, "none") == 0) {
            research->requires = -1;
            return CORE_OK;
        }
        /* 앞선 연구는 파일에서 이 연구보다 먼저 나와야 합니다 (그래서 순환이 생길 수 없음) */
        index = defs_find_research(state->defs, value);
        if (index < 0 || index >= (int32_t)(research - state->defs->research)) {
            return CORE_ERR_FORMAT;
        }
        research->requires = index;
        return CORE_OK;
    }
    return apply_field(state->entry, RESEARCH_FIELDS, COUNT_OF(RESEARCH_FIELDS), key, value);
}

static int begin_rank(ParseState *state, const char *id)
{
    int32_t rank;
    for (rank = 0; rank < CORE_RANK_COUNT; rank++) {
        if (strcmp(state->defs->ranks[rank].key, id) == 0) {
            state->entry = &state->defs->ranks[rank];
            return CORE_OK;
        }
    }
    return CORE_ERR_FORMAT; /* 등급은 다섯 개로 정해져 있음 */
}

static int field_rank(ParseState *state, const char *key, const char *value)
{
    return apply_field(state->entry, RANK_FIELDS, COUNT_OF(RANK_FIELDS), key, value);
}

/* 섹션 종류 이름 → 처리 함수 */
static const SectionParser SECTION_PARSERS[] = {
    {"building", CORE_DATA_BUILDINGS, begin_building, field_building},
    {"trait", CORE_DATA_TRAITS, begin_trait, field_trait},
    {"enemy", CORE_DATA_ENEMIES, begin_enemy, field_enemy},
    {"research", CORE_DATA_RESEARCH, begin_research, field_research},
    {"rank", CORE_DATA_RANKS, begin_rank, field_rank},
};

/* "[keyword id]" 줄 */
static int begin_section(ParseState *state, int32_t file_id, char *line)
{
    size_t length = strlen(line);
    char *inside;
    char *space;
    int32_t i;

    if (length < 3 || line[length - 1] != ']') {
        return CORE_ERR_FORMAT;
    }
    line[length - 1] = '\0';
    inside = text_trim(line + 1);
    space = strchr(inside, ' ');
    if (space == NULL) {
        return CORE_ERR_FORMAT;
    }
    *space = '\0';
    for (i = 0; i < COUNT_OF(SECTION_PARSERS); i++) {
        if (strcmp(SECTION_PARSERS[i].keyword, inside) == 0 && SECTION_PARSERS[i].file_id == file_id) {
            state->section = i;
            return SECTION_PARSERS[i].begin(state, text_trim(space + 1));
        }
    }
    return CORE_ERR_FORMAT;
}

/* text[*pos..] 에서 한 줄을 line 으로 꺼냅니다. 1 = 꺼냄, 0 = 끝, -1 = 너무 김 */
static int next_line(const char *text, size_t length, size_t *pos, char *line, size_t line_size)
{
    size_t start = *pos;
    size_t end = start;
    size_t count;
    if (start >= length) {
        return 0;
    }
    while (end < length && text[end] != '\n') {
        end++;
    }
    *pos = end < length ? end + 1 : end;
    count = end - start;
    if (count >= line_size || memchr(text + start, '\0', count) != NULL) {
        return -1;
    }
    memcpy(line, text + start, count);
    line[count] = '\0';
    return 1;
}

/* 섹션 파일 하나 (buildings.def, traits.def) */
static int parse_sections(Defs *defs, int32_t file_id, const char *text, size_t length, int32_t *out_line)
{
    ParseState state;
    char buffer[DEFS_LINE_MAX];
    size_t pos = 0;
    int got;

    memset(&state, 0, sizeof(state));
    state.defs = defs;
    state.section = -1;
    *out_line = 0;
    while ((got = next_line(text, length, &pos, buffer, sizeof(buffer))) != 0) {
        char *line;
        char *equals;
        int rc;
        (*out_line)++;
        if (got < 0) {
            return CORE_ERR_FORMAT;
        }
        line = text_trim(buffer);
        if (line[0] == '\0' || line[0] == '#') {
            continue;
        }
        if (line[0] == '[') {
            rc = begin_section(&state, file_id, line);
        } else {
            equals = strchr(line, '=');
            if (equals == NULL || state.section < 0) {
                return CORE_ERR_FORMAT;
            }
            *equals = '\0';
            rc = SECTION_PARSERS[state.section].field(&state, text_trim(line), text_trim(equals + 1));
        }
        if (rc != CORE_OK) {
            return rc;
        }
    }
    *out_line = 0;
    return CORE_OK;
}

/* 이름 파일: 한 줄에 이름 하나. 목록 전체를 바꿉니다 */
static int parse_names(Defs *defs, const char *text, size_t length, int32_t *out_line)
{
    char buffer[DEFS_LINE_MAX];
    size_t pos = 0;
    int got;
    int rc;

    defs->name_count = 0;
    *out_line = 0;
    while ((got = next_line(text, length, &pos, buffer, sizeof(buffer))) != 0) {
        char *line;
        (*out_line)++;
        if (got < 0) {
            return CORE_ERR_FORMAT;
        }
        line = text_trim(buffer);
        if (line[0] == '\0' || line[0] == '#') {
            continue;
        }
        if (!text_is_valid_name(line, CORE_NAME_BYTES)) {
            return CORE_ERR_FORMAT;
        }
        rc = add_name(defs, line);
        if (rc != CORE_OK) {
            return rc;
        }
    }
    if (defs->name_count < DEFS_MIN_NAMES) {
        return CORE_ERR_FORMAT; /* 이름이 너무 적으면 같은 이름이 금방 겹침 */
    }
    *out_line = 0;
    return CORE_OK;
}

/* 읽은 뒤 서로 맞지 않는 값이 없는지 확인 (예: 성채는 3×3, 줄 배치 건물은 1×1) */
static int check_consistency(const Defs *defs)
{
    int32_t type;
    const BuildingDef *keep = &defs->buildings[CORE_BLD_KEEP];
    if (keep->w != 3 || keep->h != 3) {
        return CORE_ERR_FORMAT;
    }
    for (type = 0; type < CORE_BLD_COUNT; type++) {
        const BuildingDef *def = &defs->buildings[type];
        if (def->line && (def->w != 1 || def->h != 1)) {
            return CORE_ERR_FORMAT;
        }
        if ((type == CORE_BLD_TOWER || type == CORE_BLD_GATE) && (def->w != 1 || def->h != 1)) {
            return CORE_ERR_FORMAT;
        }
        if (def->ideal_terrain < 1) {
            return CORE_ERR_FORMAT;
        }
    }
    if (defs->research_count < 1) {
        return CORE_ERR_FORMAT; /* 연구가 하나도 없으면 연구 창과 승급 조건이 의미가 없음 */
    }
    /* 등급이 요구하는 연구 수가 있는 연구보다 많아도 거절하지 않습니다: 파일을 읽는 순서(연구 → 등급)와
     * 상관없이 고칠 수 있게, 승급할 때 "있는 연구 수"까지만 요구합니다 (rank.c) */
    return CORE_OK;
}

static void add_warning(Defs *defs, int32_t file_id, int32_t line)
{
    if (defs->warning_count < DEFS_MAX_WARNINGS) {
        defs->warnings[defs->warning_count].file_id = file_id;
        defs->warnings[defs->warning_count].line = line;
        defs->warning_count++;
    }
}

int defs_load_text(Defs *defs, int32_t file_id, const char *text, size_t length, int32_t *out_line)
{
    Defs scratch;
    int rc;

    *out_line = 0;
    if (file_id < 0 || file_id >= CORE_DATA_COUNT) {
        return CORE_ERR_INVALID_ARG;
    }
    /* 임시 사본에 적용해 보고, 끝까지 성공했을 때만 바꿉니다 */
    rc = defs_copy(&scratch, defs);
    if (rc != CORE_OK) {
        return rc;
    }
    if (file_id == CORE_DATA_NAMES) {
        rc = parse_names(&scratch, text, length, out_line);
    } else {
        if (file_id == CORE_DATA_RESEARCH) {
            scratch.research_count = 0; /* 연구 파일은 목록 전체를 정합니다 */
        }
        rc = parse_sections(&scratch, file_id, text, length, out_line);
    }
    if (rc == CORE_OK) {
        rc = check_consistency(&scratch);
    }
    if (rc != CORE_OK) {
        defs_free(&scratch);
        if (rc == CORE_ERR_FORMAT) {
            add_warning(defs, file_id, *out_line);
        }
        return rc;
    }
    defs_free(defs);
    *defs = scratch; /* 구조체 통째로 복사: 동적 배열의 주인이 scratch 에서 defs 로 넘어감 */
    return CORE_OK;
}

/* ---- 쓰기 (기본값으로 정의 파일 만들기) ----------------------------------- */

static void write_fields(FILE *file, const void *entry, const FieldDef *fields, int32_t field_count)
{
    int32_t i;
    for (i = 0; i < field_count; i++) {
        const FieldDef *field = &fields[i];
        int32_t value = *(const int32_t *)((const char *)entry + field->offset);
        switch (field->kind) {
        case FIELD_RESOURCE:
            fprintf(file, "%s = %s\n", field->name, defs_resource_key(value));
            break;
        case FIELD_PLACEMENT:
            fprintf(file, "%s = %s\n", field->name, PLACEMENT_KEYS[value]);
            break;
        case FIELD_CATEGORY:
            fprintf(file, "%s = %s\n", field->name, CATEGORY_KEYS[value]);
            break;
        case FIELD_BEHAVIOR:
            fprintf(file, "%s = %s\n", field->name, BEHAVIOR_KEYS[value]);
            break;
        case FIELD_BRANCH:
            fprintf(file, "%s = %s\n", field->name, BRANCH_KEYS[value]);
            break;
        case FIELD_EFFECT:
            fprintf(file, "%s = %s\n", field->name, EFFECT_KEYS[value]);
            break;
        case FIELD_INT:
        default:
            fprintf(file, "%s = %d\n", field->name, (int)value);
            break;
        }
    }
}

int defs_write_file(const Defs *defs, int32_t file_id, const char *path)
{
    FILE *file = platform_fopen_utf8(path, "wb");
    int32_t i;
    int rc;
    if (file == NULL) {
        return CORE_ERR_FILE_OPEN;
    }
    if (file_id == CORE_DATA_BUILDINGS) {
        fputs("# 건물 정의. 값을 바꾸면 게임 밸런스가 바뀝니다. 문법이 틀리면 게임이 기본값을 쓰고 경고합니다.\n"
              "# produces/secondary: none gold mana food wood stone herbs\n"
              "# placement: any near_forest adjacent_rock on_mana\n"
              "# rate: 일꾼 한 명이 일하는 한 틱(10분)에 만드는 양 × 1000 (기분 50, 생산성 100% 기준)\n",
              file);
        for (i = 0; i < CORE_BLD_COUNT; i++) {
            fprintf(file, "\n[building %s]\n", defs->buildings[i].key);
            write_fields(file, &defs->buildings[i], BUILDING_FIELDS, COUNT_OF(BUILDING_FIELDS));
        }
    } else if (file_id == CORE_DATA_TRAITS) {
        fputs("# 주민 특성. work: 일 속도 %, food: 배고파지는 속도 %, mood: 기분, bravery: 용기, magic: 연구 속도 %\n",
              file);
        for (i = 0; i < defs->trait_count; i++) {
            fprintf(file, "\n[trait %s]\n", defs->traits[i].key);
            write_fields(file, &defs->traits[i], TRAIT_FIELDS, COUNT_OF(TRAIT_FIELDS));
        }
    } else if (file_id == CORE_DATA_ENEMIES) {
        fputs("# 적 정의. hp, damage(한 번 공격), cooldown(공격 사이, 게임 속 분), speed(한 틱에 걷는 칸 × 1000),\n"
              "# range(공격 거리, 칸), behavior: thief(훔쳐 달아남) raider(건물을 부숨) monster(주민을 노림),\n"
              "# loot/loot_amount: 저장소에 닿으면 훔치는 자원과 양, mana_reward: 쓰러뜨리면 얻는 마정석\n",
              file);
        for (i = 0; i < DEFS_ENEMY_COUNT; i++) {
            fprintf(file, "\n[enemy %s]\n", defs->enemies[i].key);
            write_fields(file, &defs->enemies[i], ENEMY_FIELDS, COUNT_OF(ENEMY_FIELDS));
        }
    } else if (file_id == CORE_DATA_RESEARCH) {
        fputs("# 연구 나무. 이 파일이 목록 전체를 정합니다 (최대 32개). 문법이 틀리면 게임이 기본값을 쓰고 경고합니다.\n"
              "# branch: economy(경제) defense(방어) magic(마법)\n"
              "# requires: 먼저 끝내야 하는 연구의 키 (이 파일에서 더 앞에 있어야 함) 또는 none\n"
              "# required_rank: 시작하려면 필요한 등급 (0 촌락, 1 마을, 2 요새, 3 성, 4 왕성)\n"
              "# cost_gold/cost_mana: 시작할 때 내는 값, points: 마탑의 마법사가 모아야 하는 연구 점수\n"
              "# effect: farm_yield(농장 +%) extract_yield(벌목장·채석장 +%) storage(저장 한도 +%) market(시장 차이 -%p)\n"
              "#         tower_damage(감시탑 +) wall_hp(성벽·성문 체력 +%) guard_damage(경비병 +) keep_damage(성채 망루 +)\n"
              "#         mana_yield(마정석 광산 +%) healing(앓는 시간 -%) heating(난방 목재 -) ward(습격 건물 피해 -%)\n",
              file);
        for (i = 0; i < defs->research_count; i++) {
            const ResearchDef *research = &defs->research[i];
            fprintf(file, "\n[research %s]\n", research->key);
            fprintf(file, "requires = %s\n", research->requires >= 0 ? defs->research[research->requires].key : "none");
            write_fields(file, research, RESEARCH_FIELDS, COUNT_OF(RESEARCH_FIELDS));
        }
    } else if (file_id == CORE_DATA_RANKS) {
        fputs("# 등급 (촌락 → 마을 → 요새 → 성 → 왕성). 왕성이 되면 승리입니다.\n"
              "# 다음 등급으로 오르려면: population(인구), mood(평균 기분), defense(방어 점수), research(끝낸 연구 수)를\n"
              "# 모두 채우고 공물 tribute_gold, tribute_mana 를 냅니다 (난이도에 따라 공물 %가 다름).\n"
              "# storage: 그 등급이 되면 늘어나는 저장 한도. 촌락은 시작 등급이라 조건을 쓰지 않습니다.\n",
              file);
        for (i = 0; i < CORE_RANK_COUNT; i++) {
            fprintf(file, "\n[rank %s]\n", defs->ranks[i].key);
            write_fields(file, &defs->ranks[i], RANK_FIELDS, COUNT_OF(RANK_FIELDS));
        }
    } else if (file_id == CORE_DATA_NAMES) {
        fputs("# 주민 이름. 한 줄에 하나 (UTF-8). 10개 이상이어야 합니다.\n", file);
        for (i = 0; i < defs->name_count; i++) {
            fprintf(file, "%s\n", defs->names[i].text);
        }
    }
    rc = ferror(file) ? CORE_ERR_FILE_IO : CORE_OK;
    if (fclose(file) != 0) {
        rc = CORE_ERR_FILE_IO;
    }
    return rc;
}
