/*
 * validate.c — 불변 조건 검사기 (설명은 validate.h)
 */
#include "validate.h"

#include <stddef.h>

#include "placement.h"

#include "text.h"

static int text_ok(const char *name)
{
    return text_is_valid_name(name, CORE_NAME_BYTES);
}

/* 검사 하나를 실행하고, 틀렸으면 개수를 올리고 알립니다 */
typedef struct Checker {
    int32_t violations;
    ValidateReportFn report;
    void *context;
} Checker;

static void expect(Checker *checker, int condition, const char *what)
{
    if (!condition) {
        checker->violations++;
        if (checker->report != NULL) {
            checker->report(checker->context, what);
        }
    }
}

static void check_grid(Checker *checker, const World *world)
{
    const Grid *grid = &world->grid;
    int32_t count;
    int32_t i;
    int32_t bad_terrain = 0;
    int32_t bad_cost = 0;
    int32_t dx;
    int32_t dy;

    expect(checker, grid->width >= GRID_MIN_SIZE && grid->width <= GRID_MAX_SIZE, "grid width out of range");
    expect(checker, grid->height >= GRID_MIN_SIZE && grid->height <= GRID_MAX_SIZE, "grid height out of range");
    expect(checker, grid->terrain != NULL && grid->cost != NULL, "grid arrays missing");
    if (grid->terrain == NULL || grid->cost == NULL || grid->width <= 0 || grid->height <= 0) {
        return;
    }
    count = grid->width * grid->height;
    for (i = 0; i < count; i++) {
        if (grid->terrain[i] >= CORE_TERRAIN_COUNT || grid->road[i] > 1 || grid->block[i] > GRID_BLOCK_GATE) {
            bad_terrain++;
        } else if (grid->cost[i] != grid_expected_cost(grid, i)) {
            bad_cost++;
        }
    }
    expect(checker, bad_terrain == 0, "terrain/road/block value out of range");
    expect(checker, bad_cost == 0, "cost grid does not match terrain, roads and buildings");
    expect(checker, grid->cost_rev >= 1, "cost grid revision not set");

    /* 성채 자리와 그 둘레는 비어 있어야 합니다 */
    expect(checker,
           world->keep_x - GRID_KEEP_CLEAR >= 0 && world->keep_y - GRID_KEEP_CLEAR >= 0 &&
               world->keep_x + GRID_KEEP_CLEAR < grid->width && world->keep_y + GRID_KEEP_CLEAR < grid->height,
           "keep out of bounds");
    if (world->keep_x - GRID_KEEP_CLEAR < 0 || world->keep_y - GRID_KEEP_CLEAR < 0 ||
        world->keep_x + GRID_KEEP_CLEAR >= grid->width || world->keep_y + GRID_KEEP_CLEAR >= grid->height) {
        return;
    }
    for (dy = -GRID_KEEP_CLEAR; dy <= GRID_KEEP_CLEAR; dy++) {
        for (dx = -GRID_KEEP_CLEAR; dx <= GRID_KEEP_CLEAR; dx++) {
            if (grid_terrain_at(grid, world->keep_x + dx, world->keep_y + dy) != CORE_TERRAIN_GRASS) {
                expect(checker, 0, "keep area is not clear grass");
                return;
            }
        }
    }
}

static void check_bank(Checker *checker, const ResourceBank *bank)
{
    int32_t i;
    expect(checker, bank->capacity > 0, "storage capacity must be positive");
    for (i = 0; i < CORE_RES_COUNT; i++) {
        expect(checker, bank->amount[i] >= 0, "resource below zero");
        expect(checker, bank->amount[i] <= eco_bank_limit(bank, i), "resource above its limit");
    }
}

static void check_events(Checker *checker, const EventQueue *queue)
{
    expect(checker, queue->items != NULL, "event queue not allocated");
    expect(checker, queue->capacity >= 1 && queue->capacity <= EVENTS_MAX_CAPACITY, "event capacity out of range");
    expect(checker, queue->count >= 0 && queue->count <= queue->capacity, "event count out of range");
    expect(checker, queue->head >= 0 && queue->head < queue->capacity, "event head out of range");
    expect(checker, queue->dropped >= 0, "negative dropped count");
}

/* 포인터가 월드의 건물 목록에 들어 있는가 (포인터를 따라가지 않고 비교만 함: 잘못된 포인터여도 안전) */
static int building_listed(const World *world, const Building *building)
{
    int32_t i;
    for (i = 0; i < world->building_count; i++) {
        if (world->buildings[i] == building) {
            return 1;
        }
    }
    return 0;
}

static int contains(Villager *const *array, int32_t count, const Villager *villager)
{
    int32_t i;
    for (i = 0; i < count; i++) {
        if (array[i] == villager) {
            return 1;
        }
    }
    return 0;
}

static void check_building(Checker *checker, const World *world, const Building *b, int32_t *storage_total)
{
    const Grid *grid = &world->grid;
    const BuildingDef *def = world_building_def(world, b->type);
    int32_t cx;
    int32_t cy;
    int32_t i;
    int footprint_ok = 1;

    expect(checker, b->id >= 1 && b->id < world->next_building_id, "building id out of range");
    expect(checker, def != NULL && b->type != CORE_BLD_ROAD, "building type invalid");
    if (def == NULL) {
        return;
    }
    expect(checker, b->state >= CORE_BSTATE_CONSTRUCTING && b->state <= CORE_BSTATE_RUINED,
           "building state invalid");
    expect(checker, b->hp >= 0 && b->hp <= def->max_hp, "building hp out of range");
    expect(checker, b->build_progress >= 0, "building progress negative");
    expect(checker, b->w >= 1 && b->w <= 4 && b->h >= 1 && b->h <= 4, "building size invalid");
    expect(checker, b->x >= 0 && b->y >= 0 && b->x + b->w <= grid->width && b->y + b->h <= grid->height,
           "building out of bounds");
    if (b->x < 0 || b->y < 0 || b->x + b->w > grid->width || b->y + b->h > grid->height) {
        return;
    }
    for (cy = b->y; cy < b->y + b->h; cy++) {
        for (cx = b->x; cx < b->x + b->w; cx++) {
            if (grid->occupant[grid_index(grid, cx, cy)] != b->id) {
                footprint_ok = 0;
            }
        }
    }
    expect(checker, footprint_ok, "occupancy grid does not match building footprint");
    if (placement_needs_door(def, b->type)) {
        expect(checker, grid_in_bounds(grid, b->door_x, b->door_y), "building door out of bounds");
        if (grid_in_bounds(grid, b->door_x, b->door_y)) {
            int32_t door = grid_index(grid, b->door_x, b->door_y);
            expect(checker, grid->occupant[door] == 0 && grid->door_count[door] > 0, "building door blocked");
        }
    }
    expect(checker, b->worker_count >= 0 && b->worker_count <= def->worker_slots, "too many workers");
    expect(checker, b->resident_count >= 0 && b->resident_count <= def->resident_slots, "too many residents");
    for (i = 0; i < b->worker_count; i++) {
        expect(checker, b->workers[i]->workplace == b, "worker does not point back to workplace");
        expect(checker, i == 0 || b->workers[i - 1]->id < b->workers[i]->id, "worker list not sorted");
    }
    for (i = 0; i < b->resident_count; i++) {
        expect(checker, b->residents[i]->home == b, "resident does not point back to home");
    }
    expect(checker, b->work_milli[0] >= 0 && b->work_milli[1] >= 0, "work progress negative");
    expect(checker, (b->flags & ~CORE_BF_COLD) == 0, "building flags invalid");
    expect(checker, b->flags == 0 || (b->type == CORE_BLD_HOUSE && b->state == CORE_BSTATE_ACTIVE),
           "only lived-in houses can be cold");
    if (b->state == CORE_BSTATE_RUINED) {
        expect(checker, b->hp == 0 && b->worker_count == 0 && b->resident_count == 0,
               "ruined building still has hp or people");
    } else if (b->state == CORE_BSTATE_ACTIVE) {
        expect(checker, b->hp > 0, "active building has no hp");
    }
    if (b->state == CORE_BSTATE_ACTIVE) {
        *storage_total += def->storage;
    }
}

static void check_villager(Checker *checker, const World *world, const Villager *v)
{
    expect(checker, v->id >= 1 && v->id < world->next_villager_id, "villager id out of range");
    expect(checker, text_ok(v->name), "villager name invalid");
    expect(checker, v->needs.hunger >= 0 && v->needs.hunger <= 100 && v->needs.rest >= 0 && v->needs.rest <= 100 &&
                        v->needs.safety >= 0 && v->needs.safety <= 100 && v->needs.housing >= 0 &&
                        v->needs.housing <= 100,
           "villager needs out of range");
    expect(checker, v->mood >= 0 && v->mood <= 100, "villager mood out of range");
    expect(checker, v->health > 0 && v->health <= 100, "villager health out of range");
    expect(checker, v->state >= 0 && v->state < CORE_VS_COUNT, "villager state invalid");
    expect(checker, grid_in_bounds(&world->grid, v->target_x, v->target_y), "villager target off map");
    expect(checker, grid_in_bounds(&world->grid, v->loc_x, v->loc_y), "villager location off map");
    expect(checker, v->traits[0] >= -1 && v->traits[0] < world->defs.trait_count && v->traits[1] >= -1 &&
                        v->traits[1] < world->defs.trait_count,
           "villager trait out of range");
    if (v->home != NULL) {
        expect(checker, building_listed(world, v->home), "villager home is not a building");
        if (building_listed(world, v->home)) {
            expect(checker, contains(v->home->residents, v->home->resident_count, v), "home does not list resident");
        }
    }
    if (v->workplace != NULL) {
        expect(checker, building_listed(world, v->workplace), "villager workplace is not a building");
        if (building_listed(world, v->workplace)) {
            expect(checker, contains(v->workplace->workers, v->workplace->worker_count, v),
                   "workplace does not list worker");
        }
    }
    if (v->target != NULL) {
        expect(checker, building_listed(world, v->target), "villager target is not a building");
    }
    expect(checker, v->sick_ticks >= 0 && v->sick_ticks <= NATURE_SICK_TICKS * 2, "sickness out of range");
    expect(checker, ((v->flags & CORE_VF_SICK) != 0) == (v->sick_ticks > 0), "sick flag does not match sickness");
    expect(checker, v->heal_ticks >= 0 && v->heal_ticks <= NATURE_HEAL_TICKS, "healing progress out of range");
}

/* 날씨·자연 사건·시장·상인 */
static void check_pressure(Checker *checker, const World *world)
{
    const Nature *nature = &world->nature;
    const Merchant *merchant = &world->merchant;
    int32_t i;
    expect(checker, nature->weather >= 0 && nature->weather < CORE_WEATHER_COUNT, "weather invalid");
    expect(checker, nature->event >= 0 && nature->event < CORE_NATURE_COUNT, "nature event invalid");
    expect(checker, (nature->event == CORE_NATURE_NONE) == (nature->event_days == 0), "event days mismatch");
    expect(checker, nature->forecast >= 0 && nature->forecast < CORE_NATURE_COUNT, "forecast invalid");
    expect(checker, (nature->forecast == CORE_NATURE_NONE) == (nature->forecast_days == 0), "forecast days mismatch");
    expect(checker, nature->event_days >= 0 && nature->event_days <= 30 && nature->forecast_days <= 30 &&
                        nature->calm_days >= 0 && nature->cold_houses >= 0,
           "nature counters out of range");
    for (i = 0; i < CORE_RES_COUNT; i++) {
        int32_t base = market_base_price(i) * 100;
        if (base == 0) {
            expect(checker, world->market.price[i] == 0, "untradable resource has a price");
        } else {
            expect(checker,
                   world->market.price[i] >= base * MARKET_MIN_PCT / 100 &&
                       world->market.price[i] <= base * MARKET_MAX_PCT / 100,
                   "market price out of range");
        }
    }
    expect(checker, merchant->days_left >= 0 && merchant->days_left <= MERCHANT_STAY_DAYS, "merchant days invalid");
    expect(checker, merchant->deal_count >= 0 && merchant->deal_count <= CORE_MERCHANT_MAX_DEALS,
           "merchant deal count invalid");
    expect(checker, merchant->days_left > 0 || merchant->deal_count == 0, "merchant left but deals remain");
    for (i = 0; i < merchant->deal_count && i < CORE_MERCHANT_MAX_DEALS; i++) {
        const MerchantDeal *deal = &merchant->deals[i];
        expect(checker,
               deal->give_resource >= 0 && deal->give_resource < CORE_RES_COUNT && deal->get_resource >= 0 &&
                   deal->get_resource < CORE_RES_COUNT && deal->give_amount > 0 && deal->get_amount > 0 &&
                   (deal->taken == 0 || deal->taken == 1),
               "merchant deal invalid");
    }
}

static void check_entities(Checker *checker, const World *world)
{
    const Building *keep = world_keep(world);
    int32_t storage_total = 0;
    int32_t i;
    expect(checker, keep != NULL && keep->type == CORE_BLD_KEEP, "keep missing");
    for (i = 0; i < world->building_count; i++) {
        check_building(checker, world, world->buildings[i], &storage_total);
    }
    for (i = 0; i < world->villager_count; i++) {
        check_villager(checker, world, world->villagers[i]);
    }
    expect(checker, world->bank.capacity == (storage_total > 0 ? storage_total : 1),
           "storage capacity does not match buildings");
    expect(checker, world->mourning >= 0 && world->mourning <= 30, "mourning out of range");
}

int32_t validate_world(const World *world, ValidateReportFn report, void *context)
{
    Checker checker;
    checker.violations = 0;
    checker.report = report;
    checker.context = context;

    if (world == NULL) {
        expect(&checker, 0, "world is NULL");
        return checker.violations;
    }
    expect(&checker, world_name_is_valid(world->castle_name, CORE_NAME_BYTES), "castle name invalid");
    expect(&checker, world->difficulty >= 0 && world->difficulty < CORE_DIFF_COUNT, "difficulty out of range");
    expect(&checker, world->seed >= 0, "seed negative");
    expect(&checker, world->clock.tick >= 0 && world->clock.tick <= CLOCK_MAX_TICK, "clock tick out of range");
    expect(&checker, (world->rng.inc & 1u) == 1u, "rng increment must be odd");
    check_bank(&checker, &world->bank);
    check_grid(&checker, world);
    check_events(&checker, &world->events);
    check_pressure(&checker, world);
    if (world->grid.occupant != NULL) {
        check_entities(&checker, world);
    }
    return checker.violations;
}
