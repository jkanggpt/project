/*
 * tutorial.c — 튜토리얼 목표 (설명은 tutorial.h)
 */
#include "tutorial.h"

#include "world.h"

#define TUTORIAL_FLAG_ASSIGNED 1

static int has_active(const World *world, int32_t type)
{
    int32_t i;
    for (i = 0; i < world->building_count; i++) {
        if (world->buildings[i]->type == type && world->buildings[i]->state == CORE_BSTATE_ACTIVE) {
            return 1;
        }
    }
    return 0;
}

static int built_farm(const World *world)
{
    return has_active(world, CORE_BLD_FARM);
}

static int built_house(const World *world)
{
    return has_active(world, CORE_BLD_HOUSE);
}

static int assigned_worker(const World *world)
{
    return (world->tutorial_flags & TUTORIAL_FLAG_ASSIGNED) != 0;
}

static int built_wall(const World *world)
{
    return has_active(world, CORE_BLD_WALL);
}

static int survived_raid(const World *world)
{
    return world->threat.raids_done >= 1 && world->outcome != CORE_OUTCOME_LOST;
}

typedef int (*TutorialCheckFn)(const World *world);

/* 목표 번호(CoreTutorialStep) → 확인 함수 */
static const TutorialCheckFn TUTORIAL_CHECKS[CORE_TUTORIAL_COUNT] = {
    built_farm,      /* CORE_TUTORIAL_FARM */
    built_house,     /* CORE_TUTORIAL_HOUSE */
    assigned_worker, /* CORE_TUTORIAL_ASSIGN */
    built_wall,      /* CORE_TUTORIAL_WALL */
    survived_raid,   /* CORE_TUTORIAL_RAID */
};

void tutorial_note_assign(World *world)
{
    world->tutorial_flags |= TUTORIAL_FLAG_ASSIGNED;
}

static int32_t count_bits(int32_t mask)
{
    int32_t count = 0;
    while (mask != 0) {
        count += mask & 1;
        mask >>= 1;
    }
    return count;
}

void tutorial_hourly(World *world)
{
    int32_t step;
    if (world->tutorial_done == TUTORIAL_ALL_DONE) {
        return;
    }
    for (step = 0; step < CORE_TUTORIAL_COUNT; step++) {
        int32_t bit = 1 << step;
        if ((world->tutorial_done & bit) || !TUTORIAL_CHECKS[step](world)) {
            continue;
        }
        world->tutorial_done |= bit;
        events_push_progress(&world->events, CORE_EVT_TUTORIAL_STEP, step, count_bits(world->tutorial_done),
                             CORE_TUTORIAL_COUNT, NULL);
    }
    if (world->tutorial_done == TUTORIAL_ALL_DONE) {
        (void)eco_change(&world->bank, &world->ledger, CORE_RES_GOLD, TUTORIAL_REWARD_GOLD, CORE_LEDGER_OTHER);
        (void)eco_change(&world->bank, &world->ledger, CORE_RES_MANA, TUTORIAL_REWARD_MANA, CORE_LEDGER_OTHER);
        events_push_progress(&world->events, CORE_EVT_TUTORIAL_DONE, TUTORIAL_REWARD_GOLD, TUTORIAL_REWARD_MANA, 0, NULL);
    }
}
