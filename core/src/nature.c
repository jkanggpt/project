/*
 * nature.c — 날씨, 자연 사건, 난방, 병 (설명은 nature.h)
 */
#include "nature.h"

#include <stdlib.h>

#include "placement.h"
#include "population.h"
#include "world.h"

#define SEASON_BIT(season) (1 << (season))
#define ALL_BUT_WINTER (SEASON_BIT(CORE_SEASON_SPRING) | SEASON_BIT(CORE_SEASON_SUMMER) | SEASON_BIT(CORE_SEASON_AUTUMN))

/* 계절별 농장 생산 % (봄, 여름, 가을, 겨울). 겨울에는 농장이 멈춥니다 → 가을에 비축 */
static const int32_t FARM_SEASON_PERCENT[CORE_SEASON_COUNT] = {100, 120, 110, 0};
/* 계절별 집 한 채의 하룻밤 난방 목재 (겨울은 두 배) */
static const int32_t HEATING_BY_SEASON[CORE_SEASON_COUNT] = {1, 0, 1, 2};
/* 계절별 비(겨울은 눈)가 올 확률 (천분율) */
static const int32_t WET_CHANCE[CORE_SEASON_COUNT] = {350, 150, 400, 550};
/* 난이도별 사건 확률·피해 배율 (%) — 평화, 보통, 혹독 */
static const int32_t SEVERITY_PCT[CORE_DIFF_COUNT] = {60, 100, 140};

/* ---- 사건 처리 함수 (NATURE_EVENTS 표) ---------------------------------- */

typedef void (*NatureHandler)(World *world);

typedef struct NatureEventDef {
    int32_t season_mask;   /* 일어날 수 있는 계절 (비트) */
    int32_t chance;        /* 하루에 예보될 확률 (천분율, 보통 난이도) */
    int32_t min_days;
    int32_t max_days;
    int32_t weather;       /* 사건 동안의 날씨 (-1 = 평소대로) */
    NatureHandler on_start; /* 시작하는 날 아침 */
    NatureHandler on_day;   /* 이어지는 날마다 아침 (시작한 날 포함) */
    NatureHandler on_end;   /* 끝나는 날 아침 */
} NatureEventDef;

/* 역병: 처음에 주민 여덟 명 중 한 명꼴이 앓습니다 */
static void plague_start(World *world)
{
    int32_t count = world->villager_count / 8 + 1;
    int32_t tries = world->villager_count * 2;
    while (count > 0 && tries-- > 0 && world->villager_count > 0) {
        Villager *villager = world->villagers[rng_range(&world->rng, 0, world->villager_count - 1)];
        if (nature_make_sick(world, villager)) {
            count--;
        }
    }
}

/* 역병이 도는 동안: 아픈 주민마다 하루에 한 번, 다른 주민에게 옮길 수 있습니다 */
static void plague_day(World *world)
{
    int32_t chance = 300 * SEVERITY_PCT[world->difficulty] / 100;
    int32_t sick = 0;
    int32_t i;
    for (i = 0; i < world->villager_count; i++) {
        sick += world->villagers[i]->sick_ticks > 0;
    }
    for (i = 0; i < sick && world->villager_count > 0; i++) {
        if (rng_chance_permille(&world->rng, chance)) {
            Villager *villager = world->villagers[rng_range(&world->rng, 0, world->villager_count - 1)];
            (void)nature_make_sick(world, villager);
        }
    }
}

/* 폭풍: 건물 몇 채가 부서집니다 (성채, 성벽, 성문은 튼튼해서 버팀) */
static void storm_start(World *world)
{
    int32_t hits = 1 + world->building_count / 10;
    int32_t tries = world->building_count * 2;
    if (hits > 4) {
        hits = 4;
    }
    while (hits > 0 && tries-- > 0 && world->building_count > 0) {
        Building *building = world->buildings[rng_range(&world->rng, 0, world->building_count - 1)];
        const BuildingDef *def = world_building_def(world, building->type);
        int32_t damage;
        if (building->type == CORE_BLD_KEEP || building->type == CORE_BLD_WALL || building->type == CORE_BLD_GATE ||
            building->state != CORE_BSTATE_ACTIVE) {
            continue;
        }
        damage = def->max_hp * rng_range(&world->rng, 25, 60) / 100 * SEVERITY_PCT[world->difficulty] / 100;
        placement_damage(world, building, damage);
        hits--;
    }
}

/* 사건 종류 → 처리 함수와 수치. 효과(농장 %, 난방)는 아래 질문 함수들이 event 번호를 보고 정합니다 */
static const NatureEventDef NATURE_EVENTS[CORE_NATURE_COUNT] = {
    /* 없음 */ {0, 0, 0, 0, -1, NULL, NULL, NULL},
    /* 가뭄 */ {SEASON_BIT(CORE_SEASON_SUMMER), 90, 3, 5, CORE_WEATHER_CLEAR, NULL, NULL, NULL},
    /* 역병 */ {ALL_BUT_WINTER, 35, 4, 6, -1, plague_start, plague_day, NULL},
    /* 폭풍 */ {ALL_BUT_WINTER, 50, 1, 1, CORE_WEATHER_STORM, storm_start, NULL, NULL},
    /* 혹한 */ {SEASON_BIT(CORE_SEASON_WINTER), 120, 2, 4, CORE_WEATHER_SNOW, NULL, NULL, NULL},
    /* 풍년 */ {SEASON_BIT(CORE_SEASON_SUMMER) | SEASON_BIT(CORE_SEASON_AUTUMN), 50, 3, 4, -1, NULL, NULL, NULL},
};

void nature_init(Nature *nature)
{
    nature->weather = CORE_WEATHER_CLEAR;
    nature->event = CORE_NATURE_NONE;
    nature->event_days = 0;
    nature->forecast = CORE_NATURE_NONE;
    nature->forecast_days = 0;
    nature->calm_days = 0;
    nature->cold_houses = 0;
}

/* ---- 효과 질문 -------------------------------------------------------- */

int32_t nature_farm_percent(const World *world)
{
    int32_t percent = FARM_SEASON_PERCENT[clock_season(&world->clock)];
    if (world->nature.weather == CORE_WEATHER_RAIN) {
        percent = percent * 110 / 100;
    }
    if (world->nature.event == CORE_NATURE_DROUGHT) {
        percent = percent * 50 / 100;
    } else if (world->nature.event == CORE_NATURE_BUMPER) {
        percent = percent * 150 / 100;
    }
    return percent;
}

int32_t nature_walk_percent(const World *world)
{
    switch (world->nature.weather) {
    case CORE_WEATHER_SNOW:
        return 85;
    case CORE_WEATHER_STORM:
        return 80;
    default:
        return 100;
    }
}

int32_t nature_heating_per_house(const World *world)
{
    int32_t wood = HEATING_BY_SEASON[clock_season(&world->clock)];
    if (world->nature.event == CORE_NATURE_COLD_SNAP) {
        wood += 1;
    }
    return wood;
}

/* 난방이 필요한 집 (다 지었고 누가 사는 집) */
static int needs_heating(const Building *building)
{
    return building->type == CORE_BLD_HOUSE && building->state == CORE_BSTATE_ACTIVE && building->resident_count > 0;
}

void nature_winter_needs(const World *world, int32_t *out_food, int32_t *out_wood)
{
    int32_t season = clock_season(&world->clock);
    int32_t days;
    int32_t houses = 0;
    int32_t i;
    *out_food = 0;
    *out_wood = 0;
    if (season == CORE_SEASON_AUTUMN) {
        days = CLOCK_DAYS_PER_SEASON; /* 겨울 내내 */
    } else if (season == CORE_SEASON_WINTER) {
        days = CLOCK_DAYS_PER_SEASON - clock_day_of_season(&world->clock) + 1; /* 남은 겨울 (오늘 포함) */
    } else {
        return;
    }
    for (i = 0; i < world->building_count; i++) {
        houses += needs_heating(world->buildings[i]);
    }
    *out_food = (world->villager_count * NATURE_FOOD_PER_DAY_X10 * days + 9) / 10;
    *out_wood = houses * HEATING_BY_SEASON[CORE_SEASON_WINTER] * days;
}

/* ---- 하루 결산 -------------------------------------------------------- */

static void set_weather(World *world, int32_t weather)
{
    if (world->nature.weather != weather) {
        world->nature.weather = weather;
        events_push_count(&world->events, CORE_EVT_WEATHER_CHANGED, weather);
    }
}

static void roll_weather(World *world)
{
    int32_t season = clock_season(&world->clock);
    const NatureEventDef *def = &NATURE_EVENTS[world->nature.event];
    int32_t weather = CORE_WEATHER_CLEAR;
    if (world->nature.event != CORE_NATURE_NONE && def->weather >= 0) {
        weather = def->weather;
    } else if (rng_chance_permille(&world->rng, WET_CHANCE[season])) {
        weather = season == CORE_SEASON_WINTER ? CORE_WEATHER_SNOW : CORE_WEATHER_RAIN;
    }
    set_weather(world, weather);
}

/* 지난밤 난방: 집마다 목재를 쓰고, 모자란 집은 춥습니다 (그 집 주민의 기분이 떨어짐) */
static void heat_houses(World *world)
{
    int32_t per_house = nature_heating_per_house(world);
    int32_t cold = 0;
    int32_t i;
    for (i = 0; i < world->building_count; i++) {
        Building *building = world->buildings[i];
        building->flags &= ~CORE_BF_COLD;
        if (!needs_heating(building) || per_house <= 0) {
            continue;
        }
        if (world->bank.amount[CORE_RES_WOOD] >= per_house) {
            (void)eco_change(&world->bank, &world->ledger, CORE_RES_WOOD, -per_house, CORE_LEDGER_HEATING);
        } else {
            building->flags |= CORE_BF_COLD;
            cold++;
        }
    }
    world->nature.cold_houses = cold;
    if (cold > 0) {
        events_push_count(&world->events, CORE_EVT_HOUSES_COLD, cold);
    }
}

/* 진행 중인 사건을 하루 진행하고, 예보된 사건을 시작합니다 */
static void advance_events(World *world)
{
    Nature *nature = &world->nature;
    if (nature->event != CORE_NATURE_NONE) {
        const NatureEventDef *def = &NATURE_EVENTS[nature->event];
        nature->event_days--;
        /* 계절이 바뀌어 그 사건이 있을 수 없는 계절이 되면 끝냅니다 (겨울의 풍년 같은 일이 없게) */
        if (!(def->season_mask & SEASON_BIT(clock_season(&world->clock)))) {
            nature->event_days = 0;
        }
        if (nature->event_days <= 0) {
            int32_t ended = nature->event;
            if (def->on_end != NULL) {
                def->on_end(world);
            }
            nature->event = CORE_NATURE_NONE;
            nature->event_days = 0;
            nature->calm_days = 0;
            events_push_count(&world->events, CORE_EVT_NATURE_ENDED, ended);
        } else if (def->on_day != NULL) {
            def->on_day(world);
        }
    } else {
        nature->calm_days++;
    }
    if (nature->event == CORE_NATURE_NONE && nature->forecast != CORE_NATURE_NONE) {
        const NatureEventDef *def = &NATURE_EVENTS[nature->forecast];
        nature->event = nature->forecast;
        nature->event_days = nature->forecast_days;
        nature->forecast = CORE_NATURE_NONE;
        nature->forecast_days = 0;
        events_push_pair(&world->events, CORE_EVT_NATURE_STARTED, nature->event, nature->event_days);
        if (def->on_start != NULL) {
            def->on_start(world);
        }
        if (def->on_day != NULL) {
            def->on_day(world);
        }
    }
}

/* 내일의 사건을 고릅니다: 내일 계절에 일어날 수 있는 사건들의 확률을 차례로 더해 한 번 굴립니다 */
static void forecast_tomorrow(World *world)
{
    Nature *nature = &world->nature;
    int32_t tomorrow_season;
    int32_t roll;
    int32_t total = 0;
    int32_t event;
    if (nature->event != CORE_NATURE_NONE || nature->forecast != CORE_NATURE_NONE ||
        nature->calm_days < NATURE_CALM_DAYS || clock_day(&world->clock) < NATURE_GRACE_DAYS) {
        return;
    }
    tomorrow_season = clock_day_of_season(&world->clock) == CLOCK_DAYS_PER_SEASON
                          ? (clock_season(&world->clock) + 1) % CORE_SEASON_COUNT
                          : clock_season(&world->clock);
    roll = rng_range(&world->rng, 0, 999);
    for (event = 1; event < CORE_NATURE_COUNT; event++) {
        const NatureEventDef *def = &NATURE_EVENTS[event];
        if (!(def->season_mask & SEASON_BIT(tomorrow_season))) {
            continue;
        }
        total += def->chance * SEVERITY_PCT[world->difficulty] / 100;
        if (roll < total) {
            nature->forecast = event;
            nature->forecast_days = rng_range(&world->rng, def->min_days, def->max_days);
            events_push_count(&world->events, CORE_EVT_NATURE_FORECAST, event);
            return;
        }
    }
}

/* 가을 첫날과 닷새째: 겨울을 나기에 모자라면 미리 알립니다 */
static void warn_winter(World *world)
{
    int32_t food;
    int32_t wood;
    int32_t day = clock_day_of_season(&world->clock);
    if (clock_season(&world->clock) != CORE_SEASON_AUTUMN || (day != 1 && day != 5)) {
        return;
    }
    nature_winter_needs(world, &food, &wood);
    if (world->bank.amount[CORE_RES_FOOD] < food || world->bank.amount[CORE_RES_WOOD] < wood) {
        events_push_pair(&world->events, CORE_EVT_WINTER_WARNING, food, wood);
    }
}

void nature_daily(World *world)
{
    advance_events(world);
    roll_weather(world);
    heat_houses(world);
    warn_winter(world);
    forecast_tomorrow(world);
}

/* ---- 병 -------------------------------------------------------------- */

int nature_make_sick(World *world, Villager *villager)
{
    if (villager->sick_ticks > 0 || villager->state == CORE_VS_LEAVING) {
        return 0;
    }
    /* 체력이 좋을수록 빨리 낫습니다: 체력 1 → 1.4배, 5 → 1배, 10 → 0.5배 */
    villager->sick_ticks = NATURE_SICK_TICKS * (15 - villager->stats.vitality) / 10;
    villager->heal_ticks = 0;
    villager->flags |= CORE_VF_SICK;
    events_push_villager(&world->events, CORE_EVT_VILLAGER_SICK, villager->id, 0, 0, villager->name);
    return 1;
}

void nature_cure(World *world, Villager *villager, int by_healer)
{
    villager->sick_ticks = 0;
    villager->heal_ticks = 0;
    villager->flags &= ~CORE_VF_SICK;
    if (by_healer) {
        villager->health = villager->health + 20 > 100 ? 100 : villager->health + 20;
    }
    events_push_villager(&world->events, CORE_EVT_VILLAGER_CURED, villager->id, by_healer, 0, villager->name);
}

void nature_sick_tick(World *world, Villager *villager)
{
    if (villager->sick_ticks <= 0) {
        return;
    }
    /* 앓는 동안 두 시간마다 건강이 1 줄어듭니다 (사흘이면 약 36) */
    if (world->clock.tick % 12 == 0) {
        villager->health--;
    }
    villager->sick_ticks--;
    if (villager->sick_ticks == 0) {
        nature_cure(world, villager, 0);
    }
}

Building *nature_open_healer(const World *world, const Villager *villager)
{
    Building *best = NULL;
    int32_t best_distance = 0;
    int32_t i;
    if (world->bank.amount[CORE_RES_HERBS] <= 0) {
        return NULL;
    }
    for (i = 0; i < world->building_count; i++) {
        Building *building = world->buildings[i];
        int32_t distance;
        if (building->type != CORE_BLD_HEALER || building->state != CORE_BSTATE_ACTIVE ||
            building->worker_count == 0 || building->door_x < 0) {
            continue;
        }
        distance = abs(building->door_x - villager->loc_x) + abs(building->door_y - villager->loc_y);
        if (best == NULL || distance < best_distance) {
            best = building;
            best_distance = distance;
        }
    }
    return best;
}
