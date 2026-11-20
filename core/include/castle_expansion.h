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
#define CORE_EXP_ANIMALS 30
#define CORE_EXP_MAX_ANIMALS 512
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
    int32_t region, attack_tick, target_id, retaliate_id, move_acc, attack_remaining_milli;
    int32_t threat_until_tick, attack_cooldown_milli;
} CoreAnimalView;

enum CoreBiome {
    CORE_BIOME_GRASS,
    CORE_BIOME_FOREST,
    CORE_BIOME_TAIGA,
    CORE_BIOME_MOUNTAIN,
    CORE_BIOME_WETLAND,
    CORE_BIOME_SNOW,
    CORE_BIOME_DESERT,
    CORE_BIOME_LAKE,
    CORE_BIOME_SWAMP,
    CORE_BIOME_SAVANNA,
    CORE_BIOME_COUNT
};
typedef struct CoreAnimalInfo {
    int32_t hp, attack, food, leather, biomass, speed_milli, swim_milli, behavior;
    int32_t width_milli, height_milli, livestock;
    int32_t attack_minutes, attack_range, mana_drop;
    char name[CORE_NAME_BYTES];
} CoreAnimalInfo;
typedef struct CoreImmigrationView {
    int32_t status, reasons, permanent, last_day, next_day, required_happiness, happiness;
    int32_t vacant, water_served, road_ok, food_needed, food_available, minimum, maximum;
    int32_t special_waves, refill_day;
} CoreImmigrationView;
enum CoreImmigrationStatus {
    CORE_IMM_WAIT,
    CORE_IMM_BLOCKED,
    CORE_IMM_READY,
    CORE_IMM_ARRIVING,
    CORE_IMM_SPECIAL,
    CORE_IMM_REFILL
};
#define CORE_IMM_COOLDOWN 1
#define CORE_IMM_HOUSING 2
#define CORE_IMM_WATER 4
#define CORE_IMM_ROAD 8
#define CORE_IMM_FOOD 16
#define CORE_IMM_DISASTER 32
#define CORE_IMM_HAPPINESS 64
typedef struct CoreWaterView {
    int32_t id, area, cold, capacity_milli;
} CoreWaterView;
typedef struct CoreHabitatView {
    int32_t id, biome, area, count, biomass, capacity, target, aggressive, pressure_until, count_cap;
} CoreHabitatView;
typedef struct CoreTradeQuote {
    int32_t resource, units, total_gold, unit_cents, season_bp, event_bp, pressure_bp, rate_bp;
    int32_t next_pressure, next_remainder, reason, stock, remaining;
} CoreTradeQuote;
CORE_API int32_t core_expansion_animal_info(int32_t kind, CoreAnimalInfo *out);
CORE_API int32_t core_expansion_immigration(const CoreWorld *world, CoreImmigrationView *out);
CORE_API int32_t core_expansion_water(const CoreWorld *world, int32_t id, CoreWaterView *out);
CORE_API int32_t core_expansion_habitats(const CoreWorld *world, CoreHabitatView *out, int32_t capacity);
CORE_API int32_t core_expansion_quote(const CoreWorld *world, int32_t resource, int32_t units,
                                      CoreTradeQuote *out);
CORE_API int32_t core_expansion_display_resource(int32_t index);
CORE_API int32_t core_expansion_display_building(int32_t index);
/* 성능 검사 전용입니다. 일반 스폰의 야생 180마리 상한과 별개입니다. */
CORE_API int32_t core_expansion_debug_wildlife(CoreWorld *world, int32_t count);
/* 게임 속 1/1000분 단위로 공격 예비 동작을 진행합니다. 세 번째 프레임에서 피해를 확정합니다. */
CORE_API int32_t core_expansion_advance_animal_attacks(CoreWorld *world, int32_t elapsed_milli);
typedef struct CoreCandidateView {
    int32_t person_id, fee, field, age, stats[5], initial[5], xp[5], skills[12], traits[2], hired;
    int32_t sex, appearance;
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
/* v1.2.1 습격 외형과 고유 능력. 기본 적 번호는 세력별 순서입니다. */
typedef struct CoreRaiderInfo { int32_t faction,cost,unlock,width_milli,height_milli,ability; char name[CORE_NAME_BYTES]; } CoreRaiderInfo;
CORE_API int32_t core_expansion_raider_info(int32_t type,CoreRaiderInfo *out);
CORE_API int32_t core_expansion_summon(CoreWorld *world,int32_t parent_id,int32_t *out_id);
CORE_API int32_t core_expansion_slow(CoreWorld *world,int32_t villager_id);
typedef struct CoreEventLayers {int32_t plague_days,economic_event,economic_days;} CoreEventLayers;
CORE_API int32_t core_expansion_events(const CoreWorld *world,CoreEventLayers *out);
CORE_API int32_t core_expansion_efficiency(const CoreWorld *world,int32_t villager_id,int32_t building_type,int32_t *out_percent);

#endif
