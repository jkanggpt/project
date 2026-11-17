/* castle_expansion.h — 통합 확장 v1의 평면 뷰와 명령 */
#ifndef CASTLE_EXPANSION_API_H
#define CASTLE_EXPANSION_API_H
#include "castle_core.h"

#define CORE_RULES_CLASSIC 0
#define CORE_RULES_EXPANSION 1
#define CORE_LEGACY_BLD_COUNT 16
#define CORE_LEGACY_RANK_COUNT 5
#define CORE_EXP_SKILLS 12
#define CORE_EXP_STATS 5
#define CORE_EXP_ANIMALS 16
#define CORE_EXP_MAX_ANIMALS 300
#define CORE_EXP_MAX_CANDIDATES 15
#define CORE_EXP_OUTDOOR 1
#define CORE_EXP_PLOT 2
#define CORE_EXP_ROAD 4
#define CORE_EXP_WALL 8
#define CORE_EXP_GATE 16

typedef struct CoreExpansionBuildingInfo {
    int32_t type, w, h, rank, workers, residents, build_hours, upkeep, flags, job_field, category;
    int32_t radius, sight, attack, range, cooldown, hp, annual_yield, heating_milli, housing_mood;
    int32_t cost[CORE_RES_COUNT], input_milli[CORE_RES_COUNT], output_milli[CORE_RES_COUNT], fuel_milli,
        research_milli;
    char key[CORE_KEY_BYTES], name[CORE_NAME_BYTES], description[256];
} CoreExpansionBuildingInfo;
typedef struct CoreExpansionTileView {
    int32_t biome, tree_kind, tree_age, tree_logs_milli, tree_used_milli, regrow_milli;
    int32_t ore_kind, ore_milli, water_id, explored;
} CoreExpansionTileView;
typedef struct CoreAnimalView {
    int32_t id, kind, x_milli, y_milli, hp, age, owner, breed_day, starve_days, alive;
} CoreAnimalView;
typedef struct CoreCandidateView {
    int32_t person_id, fee, field, age, stats[5], initial[5], xp[5], skills[12], traits[2], hired;
    char name[CORE_NAME_BYTES];
} CoreCandidateView;
typedef struct CoreExpansionRankView {
    int32_t target, population, mood, roads, area, protected_pct, gold, mana, gold_bars, research;
    int32_t ready, reason;
    uint64_t unlocks[2];
    char name[CORE_NAME_BYTES], requirements[256];
} CoreExpansionRankView;

CORE_API int32_t core_expansion_building_info(int32_t type, CoreExpansionBuildingInfo *out);
CORE_API int32_t core_defs_expand(CoreDefs *defs);
CORE_API const char *core_expansion_building_name(int32_t type);
CORE_API const char *core_expansion_rank_name(int32_t rank);
CORE_API const char *core_expansion_research_name(int32_t index);
CORE_API const char *core_expansion_goal_name(int32_t index);
CORE_API const char *core_expansion_research_effect(int32_t index);
CORE_API int32_t core_expansion_market_stock(const CoreWorld *world, int32_t resource);
CORE_API const char *core_expansion_resource_key(int32_t resource);
CORE_API int32_t core_can_place_rotated(const CoreWorld *world, int32_t type, int32_t x, int32_t y,
                                        int32_t rotation, CorePlacementView *out);
CORE_API int32_t core_place_rotated(CoreWorld *world, int32_t type, int32_t x, int32_t y, int32_t rotation,
                                    int32_t *id, int32_t *reason);
CORE_API int32_t core_rotate_building(CoreWorld *world, int32_t id, int32_t rotation, int32_t *reason);
CORE_API int32_t core_place_line_rotated(CoreWorld *world, int32_t type, int32_t x0, int32_t y0, int32_t x1,
                                         int32_t y1, int32_t rotation, int32_t *placed, int32_t *reason);
CORE_API int32_t core_expansion_copy_tiles(const CoreWorld *world, CoreExpansionTileView *out,
                                           int32_t capacity);
CORE_API int32_t core_expansion_copy_visibility(const CoreWorld *world, uint8_t *out, int32_t capacity);
/* 지도 크기만큼 행 우선으로 복사합니다. 정상 벽·문에 닫힌 내부 칸은1, 나머지는0입니다. */
CORE_API int32_t core_expansion_copy_protection(const CoreWorld *world, uint8_t *out, int32_t capacity);
CORE_API int32_t core_expansion_copy_animals(const CoreWorld *world, CoreAnimalView *out, int32_t capacity,
                                             int32_t *count);
CORE_API int32_t core_expansion_copy_candidates(const CoreWorld *world, CoreCandidateView *out,
                                                int32_t capacity, int32_t *count);
CORE_API int32_t core_expansion_set_tax(CoreWorld *world, int32_t tax);
CORE_API int32_t core_expansion_set_smelter(CoreWorld *world, int32_t id, int32_t mode);
CORE_API int32_t core_expansion_hire(CoreWorld *world, int32_t person_id, int32_t *villager_id);
CORE_API int32_t core_expansion_buy_animal(CoreWorld *world, int32_t building_id, int32_t kind);
CORE_API int32_t core_expansion_damage_animal(CoreWorld *world, int32_t animal_id, int32_t damage);
CORE_API int32_t core_expansion_rank(const CoreWorld *world, CoreExpansionRankView *out);
CORE_API int32_t core_expansion_spotted(CoreWorld *world, int32_t x, int32_t y);
CORE_API int32_t core_expansion_fire(CoreWorld *world, int32_t building_id);
CORE_API int32_t core_expansion_report_position(CoreWorld *world, int32_t villager_id, int32_t x_milli,
                                                int32_t y_milli);
#endif
