/*
 * validate.c — 불변 조건 검사기 (설명은 validate.h)
 */
#include "validate.h"

#include <stddef.h>

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
        if (grid->terrain[i] >= CORE_TERRAIN_COUNT) {
            bad_terrain++;
        } else if (grid->cost[i] != grid_terrain_cost(grid->terrain[i])) {
            bad_cost++;
        }
    }
    expect(checker, bad_terrain == 0, "terrain value out of range");
    expect(checker, bad_cost == 0, "cost grid does not match terrain");
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
    return checker.violations;
}
