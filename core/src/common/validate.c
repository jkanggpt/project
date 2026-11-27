/* validate.c — 현재 월드의 공개 경계 불변식 */
#include "validate.h"
#include "expansion.h"
#include "text.h"

typedef struct Checker {
    int32_t violations;
    ValidateReportFn report;
    void *context;
} Checker;
static void expect(Checker *c, int condition, const char *message) {
    if (!condition) {
        c->violations++;
        if (c->report)
            c->report(c->context, message);
    }
}
int32_t validate_world(const World *w, ValidateReportFn report, void *context) {
    Checker c = {0, report, context};
    if (!w) {
        expect(&c, 0, "world is NULL");
        return c.violations;
    }
    expect(&c, world_name_is_valid(w->castle_name, CORE_NAME_BYTES), "castle name invalid");
    expect(&c, w->difficulty >= 0 && w->difficulty < CORE_DIFF_COUNT, "difficulty invalid");
    expect(&c, w->seed >= 0, "seed invalid");
    expect(&c, w->ruleset == 1 && w->expansion, "current world state missing");
    expect(&c, w->clock.tick >= 0 && w->clock.tick <= CLOCK_MAX_TICK, "clock invalid");
    expect(&c, (w->rng.inc & 1u) == 1u, "rng increment must be odd");
    expect(&c, (w->guide.active == 0 || w->guide.active == 1) && w->guide.step >= 0 &&
                   w->guide.step <= GUIDE_STEP_COUNT && (w->guide.active || !w->guide.step) && w->guide.flags == 0,
           "tutorial guide state invalid");
    if (!w->expansion)
        return c.violations;
    expect(&c, expansion_validate(w) == 0, "content invariant failed");
    const Building *keep = world_keep(w);
    expect(&c, keep && keep->type == CORE_BLD_KEEP, "keep missing");
    if (keep) {
        expect(&c, keep->x + keep->w / 2 == w->keep_x && keep->y + keep->h / 2 == w->keep_y,
               "keep center invalid");
        expect(&c, grid_in_bounds(&w->grid, w->keep_x, w->keep_y), "keep outside map");
        if (grid_in_bounds(&w->grid, w->keep_x, w->keep_y))
            expect(&c, w->grid.terrain[grid_index(&w->grid, w->keep_x, w->keep_y)] != CORE_TERRAIN_WATER,
                   "keep in water");
    }
    if (w->grid.width > 0 && w->grid.height > 0 && w->grid.terrain && w->grid.cost) {
        for (int i = 0; i < w->grid.width * w->grid.height; i++) {
            expect(&c, w->grid.terrain[i] < CORE_TERRAIN_COUNT, "terrain invalid");
            if (w->grid.terrain[i] < CORE_TERRAIN_COUNT && w->expansion->tiles[i].biome >= 0 &&
                w->expansion->tiles[i].biome < 10)
                expect(&c, w->grid.cost[i] == expansion_terrain_cost_at(w, i), "cost layer invalid");
        }
    } else
        expect(&c, 0, "grid missing");
    expect(&c,
           w->events.count >= 0 && w->events.count <= w->events.capacity &&
               w->events.capacity <= EVENTS_MAX_CAPACITY,
           "event queue invalid");
    return c.violations;
}
