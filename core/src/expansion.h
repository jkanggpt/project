/* expansion.h — 통합 확장 규칙의 내부 상태와 모듈 경계 */
#ifndef CASTLE_EXPANSION_H
#define CASTLE_EXPANSION_H
#include "castle_expansion.h"
#include "world.h"
#include <stdio.h>

typedef struct ExpansionBuildingSpec {
    const char *key, *name, *description;
    int32_t w, h, rank, workers, residents, hours, upkeep, category, field, flags;
    int32_t radius, sight, attack, range, cooldown, hp, annual_yield, heating_milli, housing_mood;
    int32_t cost[CORE_RES_COUNT], input[CORE_RES_COUNT], output[CORE_RES_COUNT], fuel, research;
} ExpansionBuildingSpec;
typedef struct ExpansionState {
    CoreExpansionTileView *tiles;
    uint8_t *visible, *static_visible, *work_visible, *protected_cells, *connected;
    uint64_t work_visibility_signature;
    int32_t *distance, *queue, *water_area;
    int32_t cells, water_count, dirty, alarm, tax, tax_day, fuel_milli, magic_logs_milli, error;
    int32_t fractional[CORE_RES_COUNT], trade_stock[CORE_RES_COUNT], trade_used;
    int32_t starter_waves, immigration_day, disaster_day, next_person_id, candidate_day;
    int32_t effective_roads, protected_area, protected_population, water_served;
    CoreAnimalView animals[CORE_EXP_MAX_ANIMALS];
    int32_t animal_count, next_animal_id;
    CoreCandidateView candidates[CORE_EXP_MAX_CANDIDATES], archive[64];
    int32_t candidate_count, archive_count;
    int32_t loaded_cells, loaded_animals, loaded_candidates, loaded_buildings, loaded_villagers;
    int32_t loaded_archive, load_mask;
    int32_t last_arrival_day, refill_day, arrived_today, arrival_type, departures_today, blocked_floor8;
    int32_t pending_arrivals, pending_total, pending_type;
    int32_t promotion_state, promotion_target, promotion_remaining;
    int32_t trade_pressure[15], trade_remainder[30], trade_volume[15];
    int32_t hub_visits, manor_visits, saved_travel_ticks;
    int32_t travel_minutes, farm_minutes, last_travel_minutes, last_farm_minutes;
    int32_t report_arrivals, report_arrival_type, report_departures, report_floor8, report_deaths, death_mark;
    int32_t region_count, region_hunt_days[30][12], region_pressure_until[30];
    CoreHabitatView regions[30];
    int32_t region_capacity[30];
    int32_t spawn_attempts, spawn_capacity_rejected, spawn_tile_rejected, spawn_species_rejected;
    uint8_t *water_cold, *region_map;
    int32_t *route_distance, *route_heap, *route_heap_pos;
    int32_t phantom_next_day[6], next_raid_tick;
    int32_t plague_days, plague_cooldown, economic_event, economic_days, economic_cooldown;
    int32_t trade_capacity_milli, trade_capacity_carry;
    int32_t raid_drops[CORE_RES_COUNT], summons_total, deferred_summons;
    int32_t next_type_serial[CORE_BLD_COUNT], last_immigration_edge, market_next_restock_day;
    int32_t phantom_second_day[6], loaded_version;
} ExpansionState;

static inline int exp_day(const World *w) { return w->clock.tick / 144 + 1; }
static inline int exp_minute(const World *w) { return (w->clock.tick % 144) * 10; }
static inline int exp_season(const World *w) { return ((exp_day(w) - 1) / 12) % 4; }
static inline int exp_year(const World *w) { return (exp_day(w) - 1) / 48 + 1; }
static inline int exp_clamp(int value, int low, int high) {
    return value < low ? low : (value > high ? high : value);
}
static inline int exp_dist(int x, int y, int xx, int yy) {
    int dx = x - xx, dy = y - yy;
    return (dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy);
}

const ExpansionBuildingSpec *expansion_spec(int type);
void expansion_fill_definitions(Defs *defs, int expanded);
int expansion_allocate(World *world);
int expansion_resize(World *world);
void expansion_free(World *world);
int expansion_new_world(World *world);
void expansion_tick(World *world);
void expansion_initialize_villager(World *world, Villager *v);
void expansion_add_xp(Villager *villager, int stat, int amount);
void expansion_assign(World *world);
void expansion_water_supply(World *world);
int expansion_available_housing(World *world);
void expansion_candidate_price(const World *world, CoreCandidateView *candidate);
int expansion_edge(World *world,int near_x,int near_y,int *x,int *y);
int expansion_work_percent(const World *world, const Villager *v, int type);
int expansion_qualified(const World *world, const Building *b);
void expansion_rebuild(World *world);
void expansion_capacity(World *world);
void expansion_visibility(World *world);
/* 불러오기는 파생 시야만 복구하며 저장된 탐험 기록을 늘리지 않습니다. */
void expansion_visibility_cache(World *world,int discover);
void expansion_water_bodies(World *world);
void expansion_daily(World *world);
void expansion_produce(World *world);
void expansion_collect_harvest(World *world);
void expansion_growth(World *world);
int expansion_validate(const World *world);
uint64_t expansion_hash(const World *world, uint64_t hash);
void expansion_place_check(const World *world, int type, int x, int y, int rotation, int ignore,
                           CorePlacementView *out);
int expansion_place(World *world, int type, int x, int y, int rotation, int *id, int *reason);
int expansion_remove_road(World *world, int x, int y);
int expansion_demolish(World *world, Building *b);
int expansion_rank_up(World *world, int *reason);
int expansion_start_research(World *world, int index, int *reason);
int expansion_trade(World *world, int resource, int units, int *gold, int *reason);
int expansion_save(FILE *file, const World *world);
int expansion_load_line(World *world, const char *key, char *value);
int expansion_finish_load(World *world);
int expansion_bank_add(World *world, int resource, int amount_milli, int category);
int expansion_bank_have(const World *world, int resource);
int expansion_fuel(World *world, int requested_milli, int consume);
int expansion_has_research(const World *world, int index);
void expansion_notice(World *world, int code, int a, int b, const char *text);
void expansion_read_summary(const World *world, CoreSummaryView *out);
void expansion_read_market(const World *world, CoreMarketView *out);
void expansion_read_building(const World *world, const Building *b, CoreBuildingView *out);
void expansion_read_villager(const World *world, const Villager *v, CoreVillagerView *out);
void expansion_people_tick(World *world);
Building *expansion_refuge(World *world, const Villager *villager, int *x, int *y);
int expansion_person_protected(const World *world, const Villager *villager);
void expansion_people_daily(World *world);
void expansion_candidates(World *world);
int expansion_setup_research(World *world);
void expansion_apply_research(World *world);
int expansion_mood(const World *world, const Villager *v);
void expansion_animals_tick(World *world);
int expansion_wild_kind(World *world, int x, int y);
void expansion_animal_daily(World *world);
int expansion_market_level(const World *world);
int expansion_permanent(const World *world);
int expansion_departure_allowed(World *world, const Villager *v, int reserve);
void expansion_wellbeing_sample(World *world, Villager *v);
void expansion_wellbeing_daily(World *world, Villager *v);
void expansion_hurt(World *world, Villager *v, int damage, int raid);
Building *expansion_hub(World *world, Villager *v, int food, Building *job);
void expansion_ecology_init(World *world);
void expansion_ecology_daily(World *world);
void expansion_ecology_recount(World *world);
int expansion_animal_spawn(World *world, int region, int initial);
int expansion_animal_habitat(const World *world, int kind, int x, int y);
void expansion_terrain_costs(World *world);
void expansion_promotion_tick(World *world);
void expansion_market_daily(World *world);
void expansion_arrival_join(World *world, Villager *villager);
extern const CoreAnimalInfo EXP_ANIMALS[CORE_EXP_ANIMALS];
int expansion_center_distance(const Building *building, int x, int y);
int expansion_centers_distance(const Building *a, const Building *b);
int expansion_working_count(const World *world, const Building *building);
int expansion_working_present(const World *world, const Building *building, const Villager *villager);
int expansion_labor_milli(const World *world, const Building *building);
int expansion_sqrt_milli(int value);
int expansion_service_base(const Building *building, int workers);
int expansion_begin_work(World *world, Villager *villager, Building *building);
int expansion_agriculture_event(const World *world, int type);
int expansion_event_active(const World *world, int event);
int expansion_service_shift(int type);
int expansion_trade_limit(const World *world);
void expansion_choose_identity(World *world, Villager *villager);
void expansion_raids_tick(World *world);
int expansion_debug_town(World *world, int villagers, int enemies);
void expansion_raider_definitions(Defs *defs);
void expansion_raider_drops(World *w,int type);
void expansion_v122_generate(World *w);
void expansion_v122_migrate(World *w);
void expansion_v122_numbers(World *w);
void expansion_mood_event(World *w,Villager *v,int kind,int amount,int source,int cooldown,int daily_cap);
int expansion_schedule(const World *w,const Villager *v,CoreWorkSchedule *out);
int expansion_leisure(World *w,Villager *v);
void expansion_leisure_reward(World *w,Villager *v,Building *b,int day);
int expansion_military(int type);
int expansion_tower(int type);
int expansion_duty(World *w,Villager *v,Building *b);
void expansion_v122_life(World *w,Villager *v);
#endif
