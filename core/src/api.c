/*
 * api.c — 공개 API (castle_core.h) 의 구현
 *
 * 여기는 "접착제"만 있습니다: 인자를 검사하고, 알맞은 모듈 함수를 부르고, 결과를 뷰 구조체로 복사합니다.
 * 게임 규칙은 각 모듈(world, placement, villager_ai, jobs, …)에 있습니다.
 * 바깥(C 그래픽 프런트엔드, CLI)에서 무엇을 넘기든 죽지 않고 오류 코드를 돌려주는 것이 이 파일의 책임입니다.
 */
#include <string.h>

#include "castle_core.h"
#include "debug.h"
#include "jobs.h"
#include "market.h"
#include "mem.h"
#include "nature.h"
#include "placement.h"
#include "platform.h"
#include "save.h"
#include "difficulty.h"
#include "rank.h"
#include "research.h"
#include "threat.h"
#include "tutorial.h"
#include "validate.h"
#include "villager_ai.h"
#include "world.h"
#include "expansion.h"

/* 공개 헤더의 상수와 내부 상수가 어긋나면 컴파일 단계에서 막습니다 */
_Static_assert(CORE_RES_COUNT == 15, u8"확장 자원 키와 뷰 크기를 맞춥니다");
_Static_assert(sizeof(CoreSummaryView) % 4 == 0, u8"views must stay 4-byte aligned for 외부 호출자");
_Static_assert(sizeof(CoreNewGameParams) == CORE_NAME_BYTES + 12, "unexpected padding in CoreNewGameParams");
_Static_assert(CORE_MOOD_TERM_COUNT == 10, u8"mood terms changed: update 외부 호출자 strings");
_Static_assert(CORE_LEDGER_COUNT == 14, u8"ledger categories changed: update save format and 외부 호출자 strings");
_Static_assert(CORE_MAX_RESEARCH <= 32, "research done flags are a 32-bit mask");
_Static_assert(CORE_TUTORIAL_COUNT <= 31, "tutorial flags are a bit mask");
_Static_assert(CORE_NATURE_COUNT == 8, u8"nature events changed: update 외부 호출자 strings");
_Static_assert(CORE_THREAT_COUNT == 4, u8"threat types changed: update 외부 호출자 strings");
_Static_assert(sizeof(CoreSaveInfoView) == 13 * 4 + CORE_NAME_BYTES, "unexpected padding in CoreSaveInfoView");

#define DEFS_MAX_FILE_BYTES (1024 * 1024) /* 정의 파일 하나의 최대 크기 */

CORE_API int32_t core_abi_version(void)
{
    return CORE_ABI_VERSION;
}

CORE_API int32_t core_get_struct_sizes(CoreStructSizes *out, int32_t out_size)
{
    if (out == NULL) {
        return CORE_ERR_NULL_ARG;
    }
    if (out_size != (int32_t)sizeof(CoreStructSizes)) {
        return CORE_ERR_STRUCT_SIZE;
    }
    out->new_game_params = (int32_t)sizeof(CoreNewGameParams);
    out->config_view = (int32_t)sizeof(CoreConfigView);
    out->summary_view = (int32_t)sizeof(CoreSummaryView);
    out->event_view = (int32_t)sizeof(CoreEventView);
    out->villager_view = (int32_t)sizeof(CoreVillagerView);
    out->building_view = (int32_t)sizeof(CoreBuildingView);
    out->building_def_view = (int32_t)sizeof(CoreBuildingDefView);
    out->trait_def_view = (int32_t)sizeof(CoreTraitDefView);
    out->placement_view = (int32_t)sizeof(CorePlacementView);
    out->mood_term_view = (int32_t)sizeof(CoreMoodTermView);
    out->ledger_view = (int32_t)sizeof(CoreLedgerView);
    out->market_view = (int32_t)sizeof(CoreMarketView);
    out->merchant_view = (int32_t)sizeof(CoreMerchantView);
    out->enemy_def_view = (int32_t)sizeof(CoreEnemyDefView);
    out->threat_view = (int32_t)sizeof(CoreThreatView);
    out->raid_view = (int32_t)sizeof(CoreRaidView);
    out->enemy_view = (int32_t)sizeof(CoreEnemyView);
    out->research_view = (int32_t)sizeof(CoreResearchView);
    out->rank_view = (int32_t)sizeof(CoreRankView);
    out->run_stats_view = (int32_t)sizeof(CoreRunStatsView);
    out->difficulty_view = (int32_t)sizeof(CoreDifficultyView);
    out->save_info_view = (int32_t)sizeof(CoreSaveInfoView);
    return CORE_OK;
}

CORE_API int32_t core_get_config(CoreConfigView *out)
{
    if (out == NULL) {
        return CORE_ERR_NULL_ARG;
    }
    memset(out, 0, sizeof(*out));
    out->abi_version = CORE_ABI_VERSION;
    out->minutes_per_tick = CLOCK_MINUTES_PER_TICK;
    out->ticks_per_day = CLOCK_TICKS_PER_DAY;
    out->days_per_season = CLOCK_DAYS_PER_SEASON;
    out->seasons_per_year = CLOCK_SEASONS_PER_YEAR;
    out->real_ms_per_tick = CLOCK_REAL_MS_PER_TICK;
    out->max_speed = CLOCK_MAX_SPEED;
    out->max_ticks_per_call = CLOCK_TICKS_PER_DAY * 10;
    out->night_start_minute = CLOCK_NIGHT_START_MIN;
    out->night_end_minute = CLOCK_NIGHT_END_MIN;
    out->resource_count = CORE_RES_COUNT;
    out->terrain_count = CORE_TERRAIN_COUNT;
    out->building_type_count = CORE_BLD_COUNT;
    out->villager_state_count = CORE_VS_COUNT;
    out->walk_milli_tiles_per_tick = VIL_BASE_WALK_MILLI;
    out->work_start_minute = WORLD_WORK_START_MIN;
    out->work_end_minute = WORLD_WORK_END_MIN;
    return CORE_OK;
}

CORE_API const char *core_result_name(int32_t code)
{
    switch (code) {
    case CORE_IGNORED:
        return "CORE_IGNORED";
    case CORE_OK:
        return "CORE_OK";
    case CORE_ERR_NULL_ARG:
        return "CORE_ERR_NULL_ARG";
    case CORE_ERR_INVALID_ARG:
        return "CORE_ERR_INVALID_ARG";
    case CORE_ERR_OUT_OF_MEMORY:
        return "CORE_ERR_OUT_OF_MEMORY";
    case CORE_ERR_BUFFER_TOO_SMALL:
        return "CORE_ERR_BUFFER_TOO_SMALL";
    case CORE_ERR_FILE_OPEN:
        return "CORE_ERR_FILE_OPEN";
    case CORE_ERR_FILE_IO:
        return "CORE_ERR_FILE_IO";
    case CORE_ERR_FORMAT:
        return "CORE_ERR_FORMAT";
    case CORE_ERR_VERSION:
        return "CORE_ERR_VERSION";
    case CORE_ERR_STRUCT_SIZE:
        return "CORE_ERR_STRUCT_SIZE";
    case CORE_ERR_REJECTED:
        return "CORE_ERR_REJECTED";
    case CORE_ERR_NOT_FOUND:
        return "CORE_ERR_NOT_FOUND";
    default:
        return "CORE_ERR_UNKNOWN";
    }
}

/* ---- 정의 ------------------------------------------------------------ */

CORE_API int32_t core_defs_create(CoreDefs **out_defs)
{
    CoreDefs *defs;
    if (out_defs == NULL) {
        return CORE_ERR_NULL_ARG;
    }
    *out_defs = NULL;
    defs = (CoreDefs *)mem_calloc(1, sizeof(CoreDefs));
    if (defs == NULL) {
        return CORE_ERR_OUT_OF_MEMORY;
    }
    if (defs_init_builtin(&defs->defs) != CORE_OK) {
        mem_free(defs);
        return CORE_ERR_OUT_OF_MEMORY;
    }
    *out_defs = defs;
    return CORE_OK;
}

CORE_API void core_defs_destroy(CoreDefs *defs)
{
    if (defs != NULL) {
        defs_free(&defs->defs);
        mem_free(defs);
    }
}

CORE_API int32_t core_defs_load_text(CoreDefs *defs, int32_t file_id, const char *text, int32_t length,
                                     int32_t *out_line)
{
    int32_t line = 0;
    int rc;
    if (out_line != NULL) {
        *out_line = 0;
    }
    if (defs == NULL || text == NULL) {
        return CORE_ERR_NULL_ARG;
    }
    if (length < 0 || length > DEFS_MAX_FILE_BYTES || file_id < 0 || file_id >= CORE_DATA_COUNT) {
        return CORE_ERR_INVALID_ARG;
    }
    rc = defs_load_text(&defs->defs, file_id, text, (size_t)length, &line);
    if (out_line != NULL) {
        *out_line = line;
    }
    return rc;
}

CORE_API int32_t core_defs_load_file(CoreDefs *defs, int32_t file_id, const char *path_utf8, int32_t *out_line)
{
    FILE *file;
    char *text;
    size_t length = 0;
    size_t got;
    int32_t rc;
    if (out_line != NULL) {
        *out_line = 0;
    }
    if (defs == NULL || path_utf8 == NULL) {
        return CORE_ERR_NULL_ARG;
    }
    file = platform_fopen_utf8(path_utf8, "rb");
    if (file == NULL) {
        return CORE_ERR_FILE_OPEN;
    }
    /* 크기를 미리 모르므로 상한만큼 할당하고 읽은 만큼만 씁니다 */
    text = (char *)mem_malloc(DEFS_MAX_FILE_BYTES + 1);
    if (text == NULL) {
        fclose(file);
        return CORE_ERR_OUT_OF_MEMORY;
    }
    while ((got = fread(text + length, 1, DEFS_MAX_FILE_BYTES + 1 - length, file)) > 0) {
        length += got;
        if (length > DEFS_MAX_FILE_BYTES) {
            break;
        }
    }
    if (ferror(file)) {
        rc = CORE_ERR_FILE_IO;
    } else if (length > DEFS_MAX_FILE_BYTES) {
        rc = CORE_ERR_FORMAT;
    } else {
        rc = core_defs_load_text(defs, file_id, text, (int32_t)length, out_line);
    }
    fclose(file);
    mem_free(text);
    return rc;
}

CORE_API int32_t core_defs_save_file(const CoreDefs *defs, int32_t file_id, const char *path_utf8)
{
    if (defs == NULL || path_utf8 == NULL) {
        return CORE_ERR_NULL_ARG;
    }
    if (file_id < 0 || file_id >= CORE_DATA_COUNT || path_utf8[0] == '\0') {
        return CORE_ERR_INVALID_ARG;
    }
    return defs_write_file(&defs->defs, file_id, path_utf8);
}

/* ---- 생성, 해제, 시간 --------------------------------------------------- */

CORE_API int32_t core_create(const CoreNewGameParams *params, const CoreDefs *defs, CoreWorld **out_world)
{
    if (out_world == NULL) {
        return CORE_ERR_NULL_ARG;
    }
    *out_world = NULL;
    if (params == NULL) {
        return CORE_ERR_NULL_ARG;
    }
    return world_create(params, defs != NULL ? &defs->defs : NULL, out_world);
}

CORE_API void core_destroy(CoreWorld *world)
{
    world_destroy(world);
}

CORE_API int32_t core_tick(CoreWorld *world, int32_t ticks)
{
    if (world == NULL) {
        return CORE_ERR_NULL_ARG;
    }
    if (ticks < 1 || ticks > CLOCK_TICKS_PER_DAY * 10) {
        return CORE_ERR_INVALID_ARG;
    }
    return world_tick(world, ticks);
}

/* ---- 읽기 ------------------------------------------------------------ */

CORE_API int32_t core_get_summary(const CoreWorld *world, CoreSummaryView *out)
{
    int32_t mood_total = 0;
    int32_t i;
    if (world == NULL || out == NULL) {
        return CORE_ERR_NULL_ARG;
    }
    memset(out, 0, sizeof(*out));
    out->tick = world->clock.tick;
    out->day = clock_day(&world->clock);
    out->minute_of_day = clock_minute_of_day(&world->clock);
    out->season = clock_season(&world->clock);
    out->day_of_season = clock_day_of_season(&world->clock);
    out->year = clock_year(&world->clock);
    out->is_night = clock_is_night(&world->clock);
    out->difficulty = world->difficulty;
    out->seed = world->seed;
    out->map_width = world->grid.width;
    out->map_height = world->grid.height;
    out->keep_x = world->keep_x;
    out->keep_y = world->keep_y;
    out->keep_size = GRID_KEEP_SIZE;
    out->cost_grid_rev = world->grid.cost_rev;
    out->population = world->villager_count;
    out->rank = world->rank;
    out->building_count = world->building_count;
    out->villager_count_hint = world->villager_count;
    out->building_count_hint = world->building_count;
    for (i = 0; i < world->building_count; i++) {
        const Building *b = world->buildings[i];
        const BuildingDef *def = world_building_def(world, b->type);
        if (b->state == CORE_BSTATE_ACTIVE) {
            out->housing_capacity += def->resident_slots;
            out->job_slots += def->worker_slots;
        }
    }
    for (i = 0; i < world->villager_count; i++) {
        const Villager *v = world->villagers[i];
        mood_total += v->mood;
        out->homeless += v->home == NULL;
        out->employed += v->workplace != NULL;
    }
    out->average_mood = world->villager_count > 0 ? mood_total / world->villager_count : 0;
    out->weather = world->nature.weather;
    out->nature_event = world->nature.event;
    out->nature_days_left = world->nature.event_days;
    out->forecast_event = world->nature.forecast;
    out->merchant_days_left = world->merchant.days_left;
    out->market_open = market_is_open(world, &i);
    for (i = 0; i < world->villager_count; i++) {
        out->sick_count += world->villagers[i]->sick_ticks > 0;
    }
    out->cold_houses = world->nature.cold_houses;
    out->heating_per_house = nature_heating_per_house(world);
    out->farm_percent = nature_farm_percent(world);
    nature_winter_needs(world, &out->winter_food_need, &out->winter_wood_need);
    if (world->threat.threat_count > 0) {
        const Threat *next = &world->threat.threats[0];
        out->threat_type = next->type;
        out->threat_direction = next->direction;
        out->threat_ticks_left = next->eta_tick - world->clock.tick;
        out->threat_strength = next->strength;
    }
    out->raid_id = world->threat.raid.id;
    out->raid_type = world->threat.raid.id != 0 ? world->threat.raid.type : CORE_THREAT_NONE;
    for (i = 0; i < world->threat.raid.enemy_count && world->threat.raid.id != 0; i++) {
        out->raid_enemies_left += world->threat.raid.enemies[i].state == CORE_ENEMY_ALIVE;
    }
    for (i = 0; i < world->villager_count; i++) {
        out->guards += threat_is_guard(world, world->villagers[i]);
    }
    out->defense = rank_defense(world);
    out->rank_ready = rank_is_ready(world);
    out->research_active = world->research.active;
    if (world->research.active >= 0) {
        out->research_progress = world->research.progress_milli / 1000;
        out->research_points = defs_research(&world->defs, world->research.active)->points;
    }
    out->research_mages = research_mages(world);
    out->research_done = world->research.done_count;
    out->outcome = world->outcome;
    out->lose_reason = world->lose_reason;
    out->tutorial_done = world->tutorial_done;
    for (i = 0; i < CORE_RES_COUNT; i++) {
        out->resources[i] = world->bank.amount[i];
        out->resource_capacity[i] = eco_is_stored_good(i) ? world->bank.capacity : 0;
        out->resource_delta[i] = eco_ledger_net(&world->ledger, i);
    }
    /* 이름 버퍼는 크기가 같고 항상 '\0' 으로 끝나므로 통째로 복사합니다 */
    memcpy(out->castle_name, world->castle_name, CORE_NAME_BYTES);
    out->castle_name[CORE_NAME_BYTES - 1] = '\0';
    if(world->expansion)expansion_read_summary(world,out);
    return CORE_OK;
}

CORE_API int32_t core_get_map_size(const CoreWorld *world, int32_t *out_width, int32_t *out_height)
{
    if (world == NULL || out_width == NULL || out_height == NULL) {
        return CORE_ERR_NULL_ARG;
    }
    *out_width = world->grid.width;
    *out_height = world->grid.height;
    return CORE_OK;
}

/* 격자 층 하나를 복사하는 공통 부분 */
static int32_t copy_layer(const CoreWorld *world, const uint8_t *layer, uint8_t *out, int32_t capacity)
{
    int32_t count;
    if (world == NULL || out == NULL) {
        return CORE_ERR_NULL_ARG;
    }
    count = world->grid.width * world->grid.height;
    if (capacity < count) {
        return CORE_ERR_BUFFER_TOO_SMALL;
    }
    memcpy(out, layer, (size_t)count);
    return CORE_OK;
}

CORE_API int32_t core_copy_terrain(const CoreWorld *world, uint8_t *out, int32_t capacity)
{
    return copy_layer(world, world != NULL ? world->grid.terrain : NULL, out, capacity);
}

CORE_API int32_t core_copy_roads(const CoreWorld *world, uint8_t *out, int32_t capacity)
{
    return copy_layer(world, world != NULL ? world->grid.road : NULL, out, capacity);
}

CORE_API int32_t core_copy_cost_grid(const CoreWorld *world, uint8_t *out, int32_t capacity, int32_t *out_rev)
{
    int32_t rc = copy_layer(world, world != NULL ? world->grid.cost : NULL, out, capacity);
    if (rc == CORE_OK && out_rev != NULL) {
        *out_rev = world->grid.cost_rev;
    }
    return rc;
}

static void villager_to_view(const World *world, const Villager *v, CoreVillagerView *out)
{
    memset(out, 0, sizeof(*out));
    out->id = v->id;
    out->state = v->state;
    out->intent_rev = v->intent_rev;
    out->intent_flags = v->intent_flags;
    out->target_building = v->target != NULL ? v->target->id : 0;
    out->target_x = v->target_x;
    out->target_y = v->target_y;
    out->at_target = v->at_target;
    out->loc_x = v->loc_x;
    out->loc_y = v->loc_y;
    out->home_id = v->home != NULL ? v->home->id : 0;
    out->workplace_id = v->workplace != NULL ? v->workplace->id : 0;
    out->job = v->workplace != NULL ? v->workplace->type : -1;
    out->mood = v->mood;
    {
        int32_t terms[CORE_MOOD_TERM_COUNT];
        out->mood_target = vil_mood_terms(world, v, terms);
    }
    out->health = v->health;
    out->hunger = v->needs.hunger;
    out->rest = v->needs.rest;
    out->safety = v->needs.safety;
    out->housing = v->needs.housing;
    out->strength = v->stats.strength;
    out->intellect = v->stats.intellect;
    out->dexterity = v->stats.dexterity;
    out->vitality = v->stats.vitality;
    out->trait_a = v->traits[0];
    out->trait_b = v->traits[1];
    out->age = v->age;
    out->flags = v->flags;
    out->walk_speed = vil_walk_speed(world, v);
    out->sex = v->sex;
    out->appearance = v->appearance;
    out->work_efficiency = v->workplace && world->expansion ? expansion_work_percent(world, v, v->workplace->type) : 100;
    out->combat_max_hp = v->workplace && v->workplace->type == CORE_BLD_BARRACKS ?
        100 * exp_clamp(100 + 8 * (v->stats.vitality - 4), 76, 148) / 100 : 100;
    memcpy(out->name, v->name, CORE_NAME_BYTES);
    out->name[CORE_NAME_BYTES - 1] = '\0';
    if(world->expansion)expansion_read_villager(world,v,out);
}

CORE_API int32_t core_copy_villagers(const CoreWorld *world, CoreVillagerView *out, int32_t capacity,
                                     int32_t *out_count)
{
    int32_t i;
    if (world == NULL || out_count == NULL) {
        return CORE_ERR_NULL_ARG;
    }
    *out_count = world->villager_count;
    if (capacity < world->villager_count) {
        return CORE_ERR_BUFFER_TOO_SMALL;
    }
    if (out == NULL && world->villager_count > 0) {
        return CORE_ERR_NULL_ARG;
    }
    for (i = 0; i < world->villager_count; i++) {
        villager_to_view(world, world->villagers[i], &out[i]);
    }
    return CORE_OK;
}

static void building_to_view(const World *world, const Building *b, CoreBuildingView *out)
{
    const BuildingDef *def = world_building_def(world, b->type);
    int32_t i;
    memset(out, 0, sizeof(*out));
    out->id = b->id;
    out->type = b->type;
    out->x = b->x;
    out->y = b->y;
    out->w = b->w;
    out->h = b->h;
    out->state = b->state;
    out->hp = b->hp;
    out->max_hp = def->max_hp;
    out->build_progress = b->build_progress;
    out->build_total = def->build_ticks;
    out->workers = b->worker_count;
    out->worker_slots = def->worker_slots;
    out->residents = b->resident_count;
    out->resident_slots = def->resident_slots;
    for (i = 0; i < b->worker_count; i++) {
        const Villager *worker = b->workers[i];
        out->working_now += world->expansion ? expansion_working_present(world,b,worker) : worker->state == CORE_VS_WORKING && worker->at_target;
    }
    out->auto_assign = b->auto_assign;
    out->door_x = b->door_x;
    out->door_y = b->door_y;
    out->productivity = b->productivity;
    out->flags = b->flags;
    if(world->expansion)expansion_read_building(world,b,out);
}

CORE_API int32_t core_copy_buildings(const CoreWorld *world, CoreBuildingView *out, int32_t capacity,
                                     int32_t *out_count)
{
    int32_t i;
    if (world == NULL || out_count == NULL) {
        return CORE_ERR_NULL_ARG;
    }
    *out_count = world->building_count;
    if (capacity < world->building_count) {
        return CORE_ERR_BUFFER_TOO_SMALL;
    }
    if (out == NULL && world->building_count > 0) {
        return CORE_ERR_NULL_ARG;
    }
    for (i = 0; i < world->building_count; i++) {
        building_to_view(world, world->buildings[i], &out[i]);
    }
    return CORE_OK;
}

CORE_API int32_t core_get_building_def(const CoreWorld *world, int32_t type, CoreBuildingDefView *out)
{
    const BuildingDef *def;
    int32_t i;
    if (world == NULL || out == NULL) {
        return CORE_ERR_NULL_ARG;
    }
    def = world_building_def(world, type);
    if (def == NULL) {
        return CORE_ERR_NOT_FOUND;
    }
    memset(out, 0, sizeof(*out));
    out->type = type;
    out->w = def->w;
    out->h = def->h;
    for (i = 0; i < CORE_RES_COUNT; i++) {
        out->cost[i] = def->cost[i];
    }
    out->build_ticks = def->build_ticks;
    out->max_hp = def->max_hp;
    out->worker_slots = def->worker_slots;
    out->resident_slots = def->resident_slots;
    out->storage = def->storage;
    out->produces = def->produces;
    out->rate = def->rate;
    out->secondary = def->secondary;
    out->secondary_rate = def->secondary_rate;
    out->work_radius = def->work_radius;
    out->placement = def->placement;
    out->min_terrain = def->min_terrain;
    out->ideal_terrain = def->ideal_terrain;
    out->wage = def->wage;
    out->required_rank = def->required_rank;
    out->category = def->category;
    out->line = def->line;
    out->available = type != CORE_BLD_KEEP && def->required_rank <= world->rank;
    out->attack = def->attack;
    out->attack_range = def->attack_range;
    out->attack_cooldown = def->attack_cooldown;
    out->defense = def->defense;
    out->count_terrain = placement_counted_terrain(def);
    memcpy(out->key, def->key, CORE_KEY_BYTES);
    out->key[CORE_KEY_BYTES - 1] = '\0';
    if(world->expansion){const ExpansionBuildingSpec *s=expansion_spec(type);out->kind_flags=s->flags;out->job_field=s->field;out->upkeep=s->upkeep;strcpy(out->name,s->name);}
    return CORE_OK;
}

CORE_API int32_t core_get_trait_def(const CoreWorld *world, int32_t index, CoreTraitDefView *out)
{
    const TraitDef *trait;
    if (world == NULL || out == NULL) {
        return CORE_ERR_NULL_ARG;
    }
    if (index < 0 || index >= world->defs.trait_count) {
        return CORE_ERR_NOT_FOUND;
    }
    trait = &world->defs.traits[index];
    memset(out, 0, sizeof(*out));
    out->index = index;
    out->work_pct = trait->work_pct;
    out->food_pct = trait->food_pct;
    out->mood = trait->mood;
    out->bravery = trait->bravery;
    out->magic_pct = trait->magic_pct;
    memcpy(out->key, trait->key, CORE_KEY_BYTES);
    out->key[CORE_KEY_BYTES - 1] = '\0';
    return CORE_OK;
}

CORE_API int32_t core_get_mood_terms(const CoreWorld *world, int32_t villager_id, CoreMoodTermView *out,
                                     int32_t capacity, int32_t *out_count)
{
    const Villager *villager;
    int32_t terms[CORE_MOOD_TERM_COUNT];
    int32_t i;
    if (world == NULL || out == NULL || out_count == NULL) {
        return CORE_ERR_NULL_ARG;
    }
    *out_count = CORE_MOOD_TERM_COUNT;
    if (capacity < CORE_MOOD_TERM_COUNT) {
        return CORE_ERR_BUFFER_TOO_SMALL;
    }
    villager = entity_find_villager(world, villager_id);
    if (villager == NULL) {
        return CORE_ERR_NOT_FOUND;
    }
    (void)vil_mood_terms(world, villager, terms);
    for (i = 0; i < CORE_MOOD_TERM_COUNT; i++) {
        out[i].term = i;
        out[i].value = terms[i];
    }
    return CORE_OK;
}

CORE_API int32_t core_get_enemy_def(const CoreWorld *world, int32_t type, CoreEnemyDefView *out)
{
    const EnemyDef *def;
    if (world == NULL || out == NULL) {
        return CORE_ERR_NULL_ARG;
    }
    def = defs_enemy(&world->defs, type);
    if (def == NULL) {
        return CORE_ERR_NOT_FOUND;
    }
    memset(out, 0, sizeof(*out));
    out->type = type;
    out->hp = def->hp;
    out->damage = def->damage;
    out->cooldown = def->cooldown;
    out->speed = def->speed;
    out->range = def->range;
    out->behavior = def->behavior;
    out->loot_resource = def->loot_resource;
    out->loot_amount = def->loot_amount;
    out->mana_reward = def->mana_reward;
    memcpy(out->key, def->key, CORE_KEY_BYTES);
    out->key[CORE_KEY_BYTES - 1] = '\0';
    return CORE_OK;
}

CORE_API int32_t core_copy_threats(const CoreWorld *world, CoreThreatView *out, int32_t capacity, int32_t *out_count)
{
    int32_t i;
    if (world == NULL || out_count == NULL || (out == NULL && capacity > 0)) {
        return CORE_ERR_NULL_ARG;
    }
    *out_count = world->threat.threat_count;
    if (capacity < world->threat.threat_count) {
        return CORE_ERR_BUFFER_TOO_SMALL;
    }
    for (i = 0; i < world->threat.threat_count; i++) {
        const Threat *threat = &world->threat.threats[i];
        out[i].type = threat->type;
        out[i].direction = threat->direction;
        out[i].eta_tick = threat->eta_tick;
        out[i].ticks_left = threat->eta_tick - world->clock.tick;
        out[i].strength = threat->strength;
    }
    return CORE_OK;
}

CORE_API int32_t core_get_raid(const CoreWorld *world, CoreRaidView *out)
{
    const Raid *raid;
    int32_t i;
    if (world == NULL || out == NULL) {
        return CORE_ERR_NULL_ARG;
    }
    memset(out, 0, sizeof(*out));
    raid = &world->threat.raid;
    if (raid->id == 0) {
        return CORE_OK;
    }
    out->id = raid->id;
    out->type = raid->type;
    out->direction = raid->direction;
    out->spawn_x = raid->spawn_x;
    out->spawn_y = raid->spawn_y;
    out->enemy_count = raid->enemy_count;
    out->started_tick = raid->started_tick;
    out->stolen = raid->stolen;
    for (i = 0; i < raid->enemy_count; i++) {
        out->alive += raid->enemies[i].state == CORE_ENEMY_ALIVE;
    }
    return CORE_OK;
}

CORE_API int32_t core_copy_enemies(const CoreWorld *world, CoreEnemyView *out, int32_t capacity, int32_t *out_count)
{
    const Raid *raid;
    int32_t count;
    int32_t i;
    if (world == NULL || out_count == NULL || (out == NULL && capacity > 0)) {
        return CORE_ERR_NULL_ARG;
    }
    raid = &world->threat.raid;
    count = raid->id != 0 ? raid->enemy_count : 0;
    *out_count = count;
    if (capacity < count) {
        return CORE_ERR_BUFFER_TOO_SMALL;
    }
    for (i = 0; i < count; i++) {
        out[i].id = raid->enemies[i].id;
        out[i].type = raid->enemies[i].type;
        out[i].state = raid->enemies[i].state;
        out[i].summoner_id = raid->enemies[i].summoner_id;
    }
    return CORE_OK;
}

CORE_API int32_t core_get_ledger(const CoreWorld *world, CoreLedgerView *out)
{
    if (world == NULL || out == NULL) {
        return CORE_ERR_NULL_ARG;
    }
    /* 내부 장부와 뷰가 같은 [자원][항목] 모양이므로 통째로 복사합니다 */
    _Static_assert(sizeof(out->today) == sizeof(world->ledger.today), "ledger view shape");
    memcpy(out->today, world->ledger.today, sizeof(out->today));
    memcpy(out->yesterday, world->ledger.yesterday, sizeof(out->yesterday));
    return CORE_OK;
}

CORE_API int32_t core_get_market(const CoreWorld *world, CoreMarketView *out)
{
    int32_t reason;
    int32_t i;
    if (world == NULL || out == NULL) {
        return CORE_ERR_NULL_ARG;
    }
    memset(out, 0, sizeof(*out));
    if(world->expansion){expansion_read_market(world,out);return CORE_OK;}
    out->open = market_is_open(world, &reason);
    out->closed_reason = reason;
    for (i = 0; i < CORE_RES_COUNT; i++) {
        if (!market_is_tradable(i)) {
            continue;
        }
        out->buy_price[i] = market_buy_price(&world->market, i);
        out->sell_price[i] = market_sell_price(&world->market, i);
        out->base_price[i] = market_base_price(i);
        out->trend[i] = world->market.price[i] / market_base_price(i);
    }
    return CORE_OK;
}

CORE_API int32_t core_get_merchant(const CoreWorld *world, CoreMerchantView *out)
{
    int32_t i;
    if (world == NULL || out == NULL) {
        return CORE_ERR_NULL_ARG;
    }
    memset(out, 0, sizeof(*out));
    out->days_left = world->merchant.days_left;
    out->deal_count = world->merchant.deal_count;
    for (i = 0; i < world->merchant.deal_count; i++) {
        const MerchantDeal *deal = &world->merchant.deals[i];
        out->deals[i].give_resource = deal->give_resource;
        out->deals[i].give_amount = deal->give_amount;
        out->deals[i].get_resource = deal->get_resource;
        out->deals[i].get_amount = deal->get_amount;
        out->deals[i].taken = deal->taken;
    }
    return CORE_OK;
}

CORE_API int32_t core_get_research(const CoreWorld *world, int32_t index, CoreResearchView *out)
{
    const ResearchDef *def;
    if (world == NULL || out == NULL) {
        return CORE_ERR_NULL_ARG;
    }
    def = defs_research(&world->defs, index);
    if (def == NULL) {
        return CORE_ERR_NOT_FOUND;
    }
    memset(out, 0, sizeof(*out));
    out->index = index;
    out->branch = def->branch;
    out->requires = def->requires;
    out->required_rank = def->required_rank;
    out->cost_gold = def->cost_gold;
    out->cost_mana = def->cost_mana;
    out->points = def->points;
    out->effect = def->effect;
    out->value = def->value;
    out->state = research_state_of(world, index);
    out->progress = world->research.active == index ? world->research.progress_milli / 1000 : 0;
    memcpy(out->key, def->key, CORE_KEY_BYTES);
    out->key[CORE_KEY_BYTES - 1] = '\0';
    return CORE_OK;
}

CORE_API int32_t core_get_rank(const CoreWorld *world, CoreRankView *out)
{
    if (world == NULL || out == NULL) {
        return CORE_ERR_NULL_ARG;
    }
    rank_fill_view(world, out);
    return CORE_OK;
}

CORE_API int32_t core_get_run_stats(const CoreWorld *world, CoreRunStatsView *out)
{
    if (world == NULL || out == NULL) {
        return CORE_ERR_NULL_ARG;
    }
    memset(out, 0, sizeof(*out));
    out->days = clock_day(&world->clock);
    out->peak_population = world->stats.peak_population;
    out->raids_survived = world->threat.raids_done;
    out->enemies_killed = world->stats.enemies_killed;
    out->villagers_died = world->stats.villagers_died;
    out->villagers_left = world->stats.villagers_left;
    out->buildings_built = world->stats.buildings_built;
    out->research_done = world->research.done_count;
    out->rank = world->rank;
    out->outcome = world->outcome;
    out->lose_reason = world->lose_reason;
    out->victory_day = world->stats.victory_day;
    return CORE_OK;
}

CORE_API int32_t core_get_difficulty(int32_t difficulty, CoreDifficultyView *out)
{
    const DifficultyPreset *preset;
    int32_t i;
    if (out == NULL) {
        return CORE_ERR_NULL_ARG;
    }
    if (difficulty < 0 || difficulty >= CORE_DIFF_COUNT) {
        return CORE_ERR_INVALID_ARG;
    }
    preset = difficulty_preset(difficulty);
    memset(out, 0, sizeof(*out));
    out->difficulty = difficulty;
    out->start_villagers = preset->start_villagers;
    for (i = 0; i < CORE_RES_COUNT; i++) {
        out->start_resources[i] = preset->start_resources[i];
    }
    out->base_mood = preset->base_mood;
    out->raid_strength_pct = preset->raid_strength_pct;
    out->raid_interval_pct = preset->raid_interval_pct;
    out->nature_pct = preset->nature_pct;
    out->tribute_pct = preset->tribute_pct;
    return CORE_OK;
}

CORE_API int32_t core_get_repair_cost(const CoreWorld *world, int32_t building_id, int32_t *out_cost, int32_t count)
{
    const Building *building;
    if (world == NULL || out_cost == NULL) {
        return CORE_ERR_NULL_ARG;
    }
    if (count != CORE_RES_COUNT) {
        return CORE_ERR_INVALID_ARG;
    }
    building = entity_find_building(world, building_id);
    if (building == NULL) {
        return CORE_ERR_NOT_FOUND;
    }
    placement_repair_cost(world, building, out_cost);
    return CORE_OK;
}

/* ---- 배치와 명령 ------------------------------------------------------ */

CORE_API int32_t core_can_place(const CoreWorld *world, int32_t type, int32_t x, int32_t y, CorePlacementView *out)
{
    if (world == NULL || out == NULL) {
        return CORE_ERR_NULL_ARG;
    }
    if(world->expansion)expansion_place_check(world,type,x,y,0,0,out);else placement_check(world, type, x, y, out);
    return CORE_OK;
}

CORE_API int32_t core_place_building(CoreWorld *world, int32_t type, int32_t x, int32_t y, int32_t *out_id,
                                     int32_t *out_reason)
{
    int32_t id = 0;
    int32_t reason = CORE_PLACE_OK;
    int rc;
    if (world == NULL) {
        return CORE_ERR_NULL_ARG;
    }
    rc = world->expansion?expansion_place(world,type,x,y,0,&id,&reason):placement_place(world, type, x, y, &id, &reason);
    if (out_id != NULL) {
        *out_id = id;
    }
    if (out_reason != NULL) {
        *out_reason = reason;
    }
    return rc;
}

CORE_API int32_t core_place_line(CoreWorld *world, int32_t type, int32_t x0, int32_t y0, int32_t x1, int32_t y1,
                                 int32_t *out_placed, int32_t *out_reason)
{
    int32_t placed = 0;
    int32_t reason = CORE_PLACE_OK;
    int rc;
    if (world == NULL) {
        return CORE_ERR_NULL_ARG;
    }
    rc = placement_line(world, type, x0, y0, x1, y1, &placed, &reason);
    if (out_placed != NULL) {
        *out_placed = placed;
    }
    if (out_reason != NULL) {
        *out_reason = reason;
    }
    return rc;
}

CORE_API int32_t core_demolish(CoreWorld *world, int32_t building_id)
{
    Building *building;
    if (world == NULL) {
        return CORE_ERR_NULL_ARG;
    }
    building = entity_find_building(world, building_id);
    if (building == NULL) {
        return CORE_ERR_NOT_FOUND;
    }
    return world->expansion?expansion_demolish(world,building):placement_demolish(world, building);
}

CORE_API int32_t core_remove_road(CoreWorld *world, int32_t x, int32_t y)
{
    if (world == NULL) {
        return CORE_ERR_NULL_ARG;
    }
    return world->expansion?expansion_remove_road(world,x,y):placement_remove_road(world, x, y);
}

CORE_API int32_t core_assign_worker(CoreWorld *world, int32_t villager_id, int32_t building_id)
{
    Villager *villager;
    Building *building;
    int rc;
    if (world == NULL) {
        return CORE_ERR_NULL_ARG;
    }
    villager = entity_find_villager(world, villager_id);
    building = entity_find_building(world, building_id);
    if (villager == NULL || building == NULL) {
        return CORE_ERR_NOT_FOUND;
    }
    int update_sight=world->expansion && villager->workplace && expansion_working_present(world,villager->workplace,villager);
    rc = jobs_assign_manual(world, villager, building);
    if (rc == CORE_OK) {
        tutorial_note_assign(world); /* 튜토리얼 목표 "주민을 직접 배치하기" */
        if(update_sight)expansion_visibility(world);
    }
    return rc;
}

CORE_API int32_t core_unassign_worker(CoreWorld *world, int32_t villager_id)
{
    Villager *villager;
    if (world == NULL) {
        return CORE_ERR_NULL_ARG;
    }
    villager = entity_find_villager(world, villager_id);
    if (villager == NULL) {
        return CORE_ERR_NOT_FOUND;
    }
    int update_sight=world->expansion && villager->workplace && expansion_working_present(world,villager->workplace,villager);
    jobs_unassign_manual(world, villager);
    if(update_sight)expansion_visibility(world);
    return CORE_OK;
}

CORE_API int32_t core_release_worker(CoreWorld *world, int32_t villager_id)
{
    Villager *villager;
    if (world == NULL) {
        return CORE_ERR_NULL_ARG;
    }
    villager = entity_find_villager(world, villager_id);
    if (villager == NULL) {
        return CORE_ERR_NOT_FOUND;
    }
    jobs_release_manual(world, villager);
    return CORE_OK;
}

CORE_API int32_t core_set_auto_assign(CoreWorld *world, int32_t building_id, int32_t enabled)
{
    Building *building;
    if (world == NULL) {
        return CORE_ERR_NULL_ARG;
    }
    if (enabled != 0 && enabled != 1) {
        return CORE_ERR_INVALID_ARG;
    }
    building = entity_find_building(world, building_id);
    if (building == NULL) {
        return CORE_ERR_NOT_FOUND;
    }
    building->auto_assign = enabled;
    if (enabled) {
        jobs_auto_assign(world);
    }
    return CORE_OK;
}

CORE_API int32_t core_trade(CoreWorld *world, int32_t resource, int32_t lots, int32_t *out_gold,
                            int32_t *out_reason)
{
    int32_t gold;
    int32_t reason;
    if (out_gold != NULL) {
        *out_gold = 0;
    }
    if (out_reason != NULL) {
        *out_reason = CORE_DENY_NONE;
    }
    if (world == NULL) {
        return CORE_ERR_NULL_ARG;
    }
    if(world->expansion){int a,b;return expansion_trade(world,resource,lots,out_gold?out_gold:&a,out_reason?out_reason:&b);}
    if (resource < 0 || resource >= CORE_RES_COUNT || lots == 0 || lots < -CORE_MARKET_MAX_LOTS ||
        lots > CORE_MARKET_MAX_LOTS) {
        return CORE_ERR_INVALID_ARG;
    }
    {
        int rc = market_trade(world, resource, lots, &gold, &reason);
        if (out_gold != NULL) {
            *out_gold = gold;
        }
        if (out_reason != NULL) {
            *out_reason = reason;
        }
        return rc;
    }
}

CORE_API int32_t core_merchant_accept(CoreWorld *world, int32_t index, int32_t *out_reason)
{
    int32_t reason;
    int rc;
    if (out_reason != NULL) {
        *out_reason = CORE_DENY_NONE;
    }
    if (world == NULL) {
        return CORE_ERR_NULL_ARG;
    }
    if (index < 0 || index >= CORE_MERCHANT_MAX_DEALS) {
        return CORE_ERR_INVALID_ARG;
    }
    rc = merchant_accept(world, index, &reason);
    if (out_reason != NULL) {
        *out_reason = reason;
    }
    return rc;
}

CORE_API int32_t core_repair(CoreWorld *world, int32_t building_id, int32_t *out_reason)
{
    Building *building;
    int32_t reason;
    int rc;
    if (out_reason != NULL) {
        *out_reason = CORE_DENY_NONE;
    }
    if (world == NULL) {
        return CORE_ERR_NULL_ARG;
    }
    building = entity_find_building(world, building_id);
    if (building == NULL) {
        return CORE_ERR_NOT_FOUND;
    }
    rc = placement_repair(world, building, &reason);
    if (out_reason != NULL) {
        *out_reason = reason;
    }
    return rc;
}

CORE_API int32_t core_start_research(CoreWorld *world, int32_t index, int32_t *out_reason)
{
    int32_t reason;
    int rc;
    if (out_reason != NULL) {
        *out_reason = CORE_DENY_NONE;
    }
    if (world == NULL) {
        return CORE_ERR_NULL_ARG;
    }
    if (defs_research(&world->defs, index) == NULL) {
        return CORE_ERR_NOT_FOUND;
    }
    rc = world->expansion?expansion_start_research(world,index,&reason):research_start(world, index, &reason);
    if (out_reason != NULL) {
        *out_reason = reason;
    }
    return rc;
}

CORE_API int32_t core_rank_up(CoreWorld *world, int32_t *out_reason)
{
    int32_t reason;
    int rc;
    if (out_reason != NULL) {
        *out_reason = CORE_DENY_NONE;
    }
    if (world == NULL) {
        return CORE_ERR_NULL_ARG;
    }
    rc = world->expansion?expansion_rank_up(world,&reason):rank_up(world, &reason);
    if (out_reason != NULL) {
        *out_reason = reason;
    }
    return rc;
}

/* ---- 보고 ------------------------------------------------------------ */

CORE_API int32_t core_villager_arrived(CoreWorld *world, int32_t villager_id, int32_t intent_rev)
{
    Villager *villager;
    if (world == NULL) {
        return CORE_ERR_NULL_ARG;
    }
    villager = entity_find_villager(world, villager_id);
    if (villager == NULL) {
        return CORE_ERR_NOT_FOUND;
    }
    return vil_arrived(world, villager, intent_rev);
}

CORE_API int32_t core_villager_unreachable(CoreWorld *world, int32_t villager_id, int32_t intent_rev)
{
    Villager *villager;
    if (world == NULL) {
        return CORE_ERR_NULL_ARG;
    }
    villager = entity_find_villager(world, villager_id);
    if (villager == NULL) {
        return CORE_ERR_NOT_FOUND;
    }
    return vil_unreachable(world, villager, intent_rev);
}

/* ---- 습격 보고 -------------------------------------------------------- */

#define REPORT_MAX_DAMAGE 100000

CORE_API int32_t core_report_building_damage(CoreWorld *world, int32_t raid_id, int32_t building_id, int32_t damage)
{
    if (world == NULL) {
        return CORE_ERR_NULL_ARG;
    }
    if (damage < 0 || damage > REPORT_MAX_DAMAGE) {
        return CORE_ERR_INVALID_ARG;
    }
    return threat_report_building_damage(world, raid_id, building_id, damage);
}

CORE_API int32_t core_report_villager_damage(CoreWorld *world, int32_t raid_id, int32_t villager_id, int32_t damage)
{
    if (world == NULL) {
        return CORE_ERR_NULL_ARG;
    }
    if (damage < 0 || damage > REPORT_MAX_DAMAGE) {
        return CORE_ERR_INVALID_ARG;
    }
    return threat_report_villager_damage(world, raid_id, villager_id, damage);
}

CORE_API int32_t core_report_enemy_killed(CoreWorld *world, int32_t raid_id, int32_t enemy_id)
{
    return world == NULL ? CORE_ERR_NULL_ARG : threat_report_enemy_killed(world, raid_id, enemy_id);
}

CORE_API int32_t core_report_loot(CoreWorld *world, int32_t raid_id, int32_t enemy_id, int32_t building_id,
                                  int32_t *out_amount)
{
    int32_t amount = 0;
    int rc;
    if (out_amount != NULL) {
        *out_amount = 0;
    }
    if (world == NULL) {
        return CORE_ERR_NULL_ARG;
    }
    rc = threat_report_loot(world, raid_id, enemy_id, building_id, &amount);
    if (out_amount != NULL) {
        *out_amount = amount;
    }
    return rc;
}

CORE_API int32_t core_report_enemy_escaped(CoreWorld *world, int32_t raid_id, int32_t enemy_id)
{
    return world == NULL ? CORE_ERR_NULL_ARG : threat_report_enemy_escaped(world, raid_id, enemy_id);
}

CORE_API int32_t core_report_raid_ended(CoreWorld *world, int32_t raid_id)
{
    return world == NULL ? CORE_ERR_NULL_ARG : threat_report_raid_ended(world, raid_id);
}

/* ---- 피드백, 저장, 디버그 ------------------------------------------------ */

CORE_API int32_t core_poll_event(CoreWorld *world, CoreEventView *out)
{
    Event event;
    if (world == NULL || out == NULL) {
        return CORE_ERR_NULL_ARG;
    }
    if (!events_pop(&world->events, &event)) {
        memset(out, 0, sizeof(*out));
        return 0;
    }
    events_to_view(&event, out);
    return 1;
}

CORE_API int32_t core_save(const CoreWorld *world, const char *path_utf8)
{
    if (world == NULL || path_utf8 == NULL) {
        return CORE_ERR_NULL_ARG;
    }
    if (path_utf8[0] == '\0') {
        return CORE_ERR_INVALID_ARG;
    }
    if (threat_raid_active(world)) {
        return CORE_ERR_REJECTED; /* 습격 중에는 저장할 수 없음 (끝난 뒤에) */
    }
    return save_write(world, path_utf8);
}

CORE_API int32_t core_load(const char *path_utf8, const CoreDefs *defs, CoreWorld **out_world,
                           int32_t *out_error_line)
{
    int32_t line = 0;
    int rc;
    if (out_world == NULL) {
        return CORE_ERR_NULL_ARG;
    }
    *out_world = NULL;
    if (out_error_line != NULL) {
        *out_error_line = 0;
    }
    if (path_utf8 == NULL) {
        return CORE_ERR_NULL_ARG;
    }
    if (path_utf8[0] == '\0') {
        return CORE_ERR_INVALID_ARG;
    }
    rc = save_read(path_utf8, defs != NULL ? &defs->defs : NULL, out_world, &line);
    if (out_error_line != NULL) {
        *out_error_line = line;
    }
    return rc;
}

CORE_API int32_t core_peek_save(const char *path_utf8, const CoreDefs *defs, CoreSaveInfoView *out)
{
    World *world = NULL;
    int32_t line = 0;
    int rc;
    if (out == NULL) {
        return CORE_ERR_NULL_ARG;
    }
    memset(out, 0, sizeof(*out));
    if (path_utf8 == NULL) {
        out->result = CORE_ERR_NULL_ARG;
        return CORE_ERR_NULL_ARG;
    }
    if (path_utf8[0] == '\0') {
        out->result = CORE_ERR_INVALID_ARG;
        return CORE_ERR_INVALID_ARG;
    }
    out->version = save_peek_version(path_utf8);
    /* 요약만 보려고 해도 파일 전체를 실제로 불러와 검사합니다: 목록에 "불러올 수 있음"이라고 보인 슬롯은
     * 정말로 불러올 수 있어야 하기 때문입니다. 세이브 하나는 작으므로(수십 KB) 슬롯 넷이면 금방입니다 */
    rc = save_read(path_utf8, defs != NULL ? &defs->defs : NULL, &world, &line);
    out->result = rc;
    out->error_line = line;
    if (rc != CORE_OK) {
        return rc;
    }
    out->seed = world->seed;
    out->difficulty = world->difficulty;
    out->tick = world->clock.tick;
    out->day = clock_day(&world->clock);
    out->year = world->expansion ? exp_year(world) : clock_year(&world->clock);
    out->season = world->expansion ? exp_season(world) : clock_season(&world->clock);
    out->minute_of_day = clock_minute_of_day(&world->clock);
    out->rank = world->rank;
    out->population = world->villager_count;
    out->outcome = world->outcome;
    memcpy(out->castle_name, world->castle_name, CORE_NAME_BYTES);
    out->castle_name[CORE_NAME_BYTES - 1] = '\0';
    world_destroy(world);
    return CORE_OK;
}

CORE_API int32_t core_check_castle_name(const char *name_utf8)
{
    if (name_utf8 == NULL) {
        return CORE_ERR_NULL_ARG;
    }
    return world_name_is_valid(name_utf8, CORE_NAME_BYTES) ? CORE_OK : CORE_ERR_INVALID_ARG;
}

CORE_API int32_t core_debug_stress(CoreWorld *world, int32_t villagers, int32_t enemies)
{
    if (world == NULL) {
        return CORE_ERR_NULL_ARG;
    }
    if (world->outcome == CORE_OUTCOME_LOST) {
        return CORE_ERR_REJECTED;
    }
    return debug_stress(world, villagers, enemies);
}

CORE_API int32_t core_validate(const CoreWorld *world)
{
    if (world == NULL) {
        return CORE_ERR_NULL_ARG;
    }
    return validate_world(world, NULL, NULL);
}

CORE_API int32_t core_state_hash(const CoreWorld *world, uint64_t *out_hash)
{
    if (world == NULL || out_hash == NULL) {
        return CORE_ERR_NULL_ARG;
    }
    *out_hash = world_state_hash(world);
    return CORE_OK;
}
