/*
 * population.c — 첫 주민, 이주, 떠남, 죽음 (설명은 population.h)
 */
#include "population.h"

#include <string.h>

#include "jobs.h"
#include "villager_ai.h"

/* 난이도별 첫 주민 수 (평화, 보통, 혹독) */
static const int32_t INITIAL_VILLAGERS[CORE_DIFF_COUNT] = {7, 6, 5};

/* 지금 살아 있는 주민 중에 같은 이름이 있으면 1 */
static int name_in_use(const World *world, const char *name)
{
    int32_t i;
    for (i = 0; i < world->villager_count; i++) {
        if (strcmp(world->villagers[i]->name, name) == 0) {
            return 1;
        }
    }
    return 0;
}

/* 이름 목록에서 아직 쓰지 않은 이름을 고릅니다 (몇 번 시도해서 없으면 겹쳐도 씀) */
static const char *pick_name(World *world)
{
    const Defs *defs = &world->defs;
    const char *name = defs->names[rng_range(&world->rng, 0, defs->name_count - 1)].text;
    int attempt;
    for (attempt = 0; attempt < 12 && name_in_use(world, name); attempt++) {
        name = defs->names[rng_range(&world->rng, 0, defs->name_count - 1)].text;
    }
    return name;
}

Villager *population_create(World *world, int32_t x, int32_t y)
{
    Villager *villager = entity_add_villager(world);
    if (villager == NULL) {
        return NULL;
    }
    (void)strcpy(villager->name, pick_name(world)); /* 이름 목록은 길이 검사를 마친 값 */
    villager->age = rng_range(&world->rng, 16, 55);
    villager->stats.strength = rng_range(&world->rng, 2, 9);
    villager->stats.intellect = rng_range(&world->rng, 2, 9);
    villager->stats.dexterity = rng_range(&world->rng, 2, 9);
    villager->stats.vitality = rng_range(&world->rng, 2, 9);
    if (world->defs.trait_count > 0) {
        villager->traits[0] = rng_range(&world->rng, 0, world->defs.trait_count - 1);
        if (world->defs.trait_count > 1) {
            /* 두 번째 특성은 첫 번째와 다르게 */
            villager->traits[1] = (villager->traits[0] + rng_range(&world->rng, 1, world->defs.trait_count - 1)) %
                                  world->defs.trait_count;
        }
    }
    villager->needs.hunger = 80;
    villager->needs.rest = 80;
    villager->needs.safety = 100;
    villager->needs.housing = 25;
    villager->mood = 60;
    villager->health = 100;
    villager->state = CORE_VS_IDLE;
    villager->loc_x = x;
    villager->loc_y = y;
    villager->target_x = x;
    villager->target_y = y;
    villager->at_target = 1;
    villager->intent_rev = 1;
    return villager;
}

int population_spawn_initial(World *world)
{
    const Building *keep = world_keep(world);
    int32_t count = INITIAL_VILLAGERS[world->difficulty];
    int32_t i;
    if (keep == NULL) {
        return CORE_ERR_INVALID_ARG;
    }
    for (i = 0; i < count; i++) {
        if (population_create(world, keep->door_x, keep->door_y) == NULL) {
            return CORE_ERR_OUT_OF_MEMORY;
        }
    }
    return CORE_OK;
}

/* 다 지은 집의 빈자리 수 */
static int32_t free_housing(const World *world)
{
    int32_t free_slots = 0;
    int32_t i;
    for (i = 0; i < world->building_count; i++) {
        const Building *building = world->buildings[i];
        if (building->state == CORE_BSTATE_ACTIVE) {
            free_slots += world_building_def(world, building->type)->resident_slots - building->resident_count;
        }
    }
    return free_slots;
}

static int32_t average_mood(const World *world)
{
    int32_t total = 0;
    int32_t i;
    if (world->villager_count == 0) {
        return 0;
    }
    for (i = 0; i < world->villager_count; i++) {
        total += world->villagers[i]->mood;
    }
    return total / world->villager_count;
}

void population_daily(World *world)
{
    int32_t free_slots = free_housing(world);
    int32_t mood = average_mood(world);
    int32_t arrivals;
    int32_t i;

    /* 빈 집이 있고, 마을 분위기가 좋고, 먹을 것이 넉넉해야 이주민이 옵니다. 겨울에는 길이 험해 오지 않습니다 */
    if (free_slots <= 0 || mood < POP_IMMIGRATION_MOOD || clock_season(&world->clock) == CORE_SEASON_WINTER ||
        world->bank.amount[CORE_RES_FOOD] < world->villager_count * POP_FOOD_DAYS_FOR_IMMIGRANTS) {
        return;
    }
    arrivals = 1 + (mood - POP_IMMIGRATION_MOOD) / 15;
    if (arrivals > free_slots) {
        arrivals = free_slots;
    }
    if (arrivals > POP_MAX_IMMIGRANTS) {
        arrivals = POP_MAX_IMMIGRANTS;
    }
    for (i = 0; i < arrivals; i++) {
        /* 아무 변의 가운데쯤에서 걸어 들어옵니다 */
        static const int32_t SIDE_X[4] = {1, 2, 1, 0};
        static const int32_t SIDE_Y[4] = {0, 1, 2, 1};
        int32_t side = rng_range(&world->rng, 0, 3);
        int32_t ex;
        int32_t ey;
        Villager *villager;
        vil_edge_tile(world, SIDE_X[side] * (world->grid.width - 1) / 2, SIDE_Y[side] * (world->grid.height - 1) / 2,
                      &ex, &ey);
        villager = population_create(world, ex, ey);
        if (villager == NULL) {
            return; /* 메모리 부족: 오늘은 이주민 없음 (게임은 계속) */
        }
        villager->mood = 65;
        villager->intent_flags = CORE_INTENT_SPAWN_EDGE;
        events_push_villager(&world->events, CORE_EVT_IMMIGRANT, villager->id, 0, 0, villager->name);
    }
    jobs_on_buildings_changed(world);
}

void population_begin_leaving(World *world, Villager *villager)
{
    int32_t ex;
    int32_t ey;
    entity_unlink_worker(villager);
    entity_unlink_resident(villager);
    villager->hold_ticks = 0;
    vil_edge_tile(world, villager->loc_x, villager->loc_y, &ex, &ey);
    vil_set_intent(villager, CORE_VS_LEAVING, NULL, ex, ey, 0);
    events_push_villager(&world->events, CORE_EVT_VILLAGER_LEAVING, villager->id, 0, 0, villager->name);
    jobs_on_buildings_changed(world); /* 빈 일자리를 다른 주민으로 채움 */
}

void population_remove_departed(World *world, Villager *villager)
{
    events_push_villager(&world->events, CORE_EVT_VILLAGER_LEFT, villager->id, 0, 0, villager->name);
    entity_remove_villager(world, villager);
}

void population_kill(World *world, Villager *villager, int32_t cause)
{
    events_push_villager(&world->events, CORE_EVT_VILLAGER_DIED, villager->id, cause, 0, villager->name);
    world->mourning += POP_MOURNING_PER_DEATH;
    if (world->mourning > POP_MOURNING_MAX) {
        world->mourning = POP_MOURNING_MAX;
    }
    entity_remove_villager(world, villager);
    jobs_on_buildings_changed(world);
}
