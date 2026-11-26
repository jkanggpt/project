/*
 * nature.c — 날씨, 자연 사건, 난방, 병 (설명은 nature.h)
 */
#include "expansion.h"
#include "nature.h"

static int season_now(const World *world) { return exp_season(world); }
static int day_in_season(const World *world) { return (exp_day(world) - 1) % 12 + 1; }

#include <stdlib.h>

#include "difficulty.h"
#include "placement.h"
#include "population.h"
#include "world.h"

/* 계절별 농장 생산 % (봄, 여름, 가을, 겨울). 겨울에는 농장이 멈춥니다 → 가을에 비축 */
static const int32_t FARM_SEASON_PERCENT[CORE_SEASON_COUNT] = {100, 120, 110, 0};
/* 계절별 집 한 채의 하룻밤 난방 목재 (겨울은 두 배) */
static const int32_t HEATING_BY_SEASON[CORE_SEASON_COUNT] = {1, 0, 1, 2};
/* 계절별 비(겨울은 눈)가 올 확률 (천분율) */
static const int32_t WET_CHANCE[CORE_SEASON_COUNT] = {350, 150, 400, 550};
/* 난이도별 사건 확률·피해 배율 (%) — 평화, 보통, 혹독 */

/* 현재 사건 일정은 services/events.c에서 처리합니다. */

static const int32_t EVENT_WEATHER[CORE_NATURE_COUNT] = {-1,
                                                         CORE_WEATHER_CLEAR,
                                                         -1,
                                                         CORE_WEATHER_STORM,
                                                         CORE_WEATHER_SNOW,
                                                         -1,
                                                         CORE_WEATHER_CLEAR,
                                                         CORE_WEATHER_CLEAR};

void nature_init(Nature *nature) {
    nature->weather = CORE_WEATHER_CLEAR;
    nature->event = CORE_NATURE_NONE;
    nature->event_days = 0;
    nature->forecast = CORE_NATURE_NONE;
    nature->forecast_days = 0;
    nature->calm_days = 0;
    nature->cold_houses = 0;
}

/* ---- 효과 질문 -------------------------------------------------------- */

int32_t nature_farm_percent(const World *world) {
    int32_t percent = FARM_SEASON_PERCENT[season_now(world)];
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

int32_t nature_walk_percent(const World *world) {
    if (world->nature.event == CORE_NATURE_STORM)
        return 90;
    switch (world->nature.weather) {
    case CORE_WEATHER_SNOW:
        return 85;
    case CORE_WEATHER_STORM:
        return 80;
    default:
        return 100;
    }
}

int32_t nature_heating_per_house(const World *world) {
    int32_t wood = HEATING_BY_SEASON[season_now(world)];
    if (world->nature.event == CORE_NATURE_COLD_SNAP) {
        wood += 1;
    }
    wood -= world->research.heating_minus; /* 연구 "따뜻한 불씨" */
    return wood > 0 ? wood : 0;
}

/* 난방이 필요한 집 (다 지었고 누가 사는 집) */
static int needs_heating(const Building *building) {
    return building->type == CORE_BLD_HOUSE && building->state == CORE_BSTATE_ACTIVE &&
           building->resident_count > 0;
}

void nature_winter_needs(const World *world, int32_t *out_food, int32_t *out_wood) {
    int32_t season = season_now(world);
    int32_t days;
    int32_t houses = 0;
    int32_t i;
    *out_food = 0;
    *out_wood = 0;
    if (season == CORE_SEASON_AUTUMN) {
        days = CLOCK_DAYS_PER_SEASON; /* 겨울 내내 */
    } else if (season == CORE_SEASON_WINTER) {
        days = CLOCK_DAYS_PER_SEASON - day_in_season(world) + 1; /* 남은 겨울 (오늘 포함) */
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

static void set_weather(World *world, int32_t weather) {
    if (world->nature.weather != weather) {
        world->nature.weather = weather;
        events_push_count(&world->events, CORE_EVT_WEATHER_CHANGED, weather);
    }
}

static void roll_weather(World *world) {
    int32_t season = season_now(world);
    int32_t event_weather = EVENT_WEATHER[world->nature.event];
    int32_t weather = CORE_WEATHER_CLEAR;
    if (world->nature.event != CORE_NATURE_NONE && event_weather >= 0) {
        weather = event_weather;
    } else if (rng_chance_permille(&world->rng, WET_CHANCE[season])) {
        weather = season == CORE_SEASON_WINTER ? CORE_WEATHER_SNOW : CORE_WEATHER_RAIN;
    }
    set_weather(world, weather);
}

/* 지난밤 난방: 집마다 목재를 쓰고, 모자란 집은 춥습니다 (그 집 주민의 기분이 떨어짐) */

/* 진행 중인 사건을 하루 진행하고, 예보된 사건을 시작합니다 */

/* 내일의 사건을 고릅니다: 내일 계절에 일어날 수 있는 사건들의 확률을 차례로 더해 한 번 굴립니다 */

/* 가을 첫날과 닷새째: 겨울을 나기에 모자라면 미리 알립니다 */

void nature_daily(World *world) {
    expansion_advance_events(world);
    roll_weather(world);
}

/* ---- 병 -------------------------------------------------------------- */

int nature_make_sick(World *world, Villager *villager) {
    if (villager->sick_ticks > 0 || villager->state == CORE_VS_LEAVING) {
        return 0;
    }
    /* 체력이 좋을수록 빨리 낫습니다: 체력 1 → 1.4배, 5 → 1배, 10 → 0.5배 */
    villager->sick_ticks = NATURE_SICK_TICKS * (15 - villager->stats.vitality) / 10;
    villager->sick_ticks =
        villager->sick_ticks * (100 - world->research.healing_pct) / 100; /* 연구 "약초 마법" */
    if (villager->sick_ticks < 1) {
        villager->sick_ticks = 1;
    }
    villager->heal_ticks = 0;
    villager->flags |= CORE_VF_SICK;
    events_push_villager(&world->events, CORE_EVT_VILLAGER_SICK, villager->id, 0, 0, villager->name);
    return 1;
}

void nature_cure(World *world, Villager *villager, int by_healer) {
    villager->sick_ticks = 0;
    villager->heal_ticks = 0;
    villager->flags &= ~CORE_VF_SICK;
    if (by_healer) {
        villager->health = villager->health + 20 > 100 ? 100 : villager->health + 20;
    }
    events_push_villager(&world->events, CORE_EVT_VILLAGER_CURED, villager->id, by_healer, 0, villager->name);
}

void nature_sick_tick(World *world, Villager *villager) {
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

Building *nature_open_healer(const World *world, const Villager *villager) {
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
