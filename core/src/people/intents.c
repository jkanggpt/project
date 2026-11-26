/* intents.c — 실제 이동 요청·도착 확인·차단 복구와 욕구 조회 */
#include "expansion.h"
#include "villager_ai.h"
#include "difficulty.h"
#include "nature.h"
#include "population.h"
#include <stdlib.h>

static int32_t clamp100(int32_t value) { return value < 0 ? 0 : (value > 100 ? 100 : value); }

int32_t vil_walk_speed(const World *world, const Villager *villager) {
    int32_t speed = VIL_BASE_WALK_MILLI * (90 + villager->stats.vitality * 2) / 100;
    if (villager->needs.hunger == 0 || villager->sick_ticks > 0) {
        speed = speed * 70 / 100; /* 굶거나 아프면 느려짐 */
    }
    if (world->expansion && villager->slow_until_tick > world->clock.tick)
        speed = speed * 80 / 100;
    return speed * nature_walk_percent(world) / 100; /* 눈·폭풍 */
}

void vil_set_intent(Villager *villager, int32_t state, Building *target, int32_t x, int32_t y,
                    int32_t flags) {
    int same_place = villager->target == target && villager->target_x == x && villager->target_y == y;
    if (state == villager->state && same_place && flags == villager->intent_flags) {
        return; /* 바뀐 것이 없으면 rev 도 그대로 */
    }
    villager->state = state;
    villager->target = target;
    villager->target_x = x;
    villager->target_y = y;
    villager->intent_flags = flags;
    villager->intent_rev++;
    villager->state_ticks = 0;
    if (flags & CORE_INTENT_HOLD) {
        villager->at_target = 1; /* 제자리에 있으라는 뜻이므로 이미 도착한 것 */
    } else if (!same_place) {
        villager->at_target = 0;
    }
}

static int walkable(const World *world, int32_t x, int32_t y) {
    const Grid *grid = &world->grid;
    int32_t index;
    uint8_t cost;
    if (!grid_in_bounds(grid, x, y)) {
        return 0;
    }
    index = grid_index(grid, x, y);
    cost = grid->cost[index];
    return cost != CORE_COST_BLOCKED && cost != CORE_COST_WALL;
}

void vil_edge_tile(const World *world, int32_t near_x, int32_t near_y, int32_t *out_x, int32_t *out_y) {
    const Grid *grid = &world->grid;
    int32_t best_distance = -1;
    int32_t i;
    const Building *keep = world_keep(world);
    *out_x = keep != NULL ? keep->door_x : world->keep_x;
    *out_y = keep != NULL ? keep->door_y : world->keep_y;
    for (i = 0; i < grid->width * 2 + grid->height * 2; i++) {
        int32_t x;
        int32_t y;
        int32_t distance;
        if (i < grid->width) {
            x = i;
            y = 0;
        } else if (i < grid->width * 2) {
            x = i - grid->width;
            y = grid->height - 1;
        } else if (i < grid->width * 2 + grid->height) {
            x = 0;
            y = i - grid->width * 2;
        } else {
            x = grid->width - 1;
            y = i - grid->width * 2 - grid->height;
        }
        if (!walkable(world, x, y)) {
            continue;
        }
        distance = abs(x - near_x) + abs(y - near_y);
        if (best_distance < 0 || distance < best_distance) {
            best_distance = distance;
            *out_x = x;
            *out_y = y;
        }
    }
}

int32_t vil_mood_terms(const World *world, const Villager *villager, int32_t terms[CORE_MOOD_TERM_COUNT]) {
    int32_t total = 0;
    int32_t i;

    for (i = 0; i < CORE_MOOD_TERM_COUNT; i++) {
        terms[i] = 0;
    }
    terms[CORE_MOOD_BASE] = difficulty_preset(world->difficulty)->base_mood;
    terms[CORE_MOOD_HUNGER] = (villager->needs.hunger - 50) / 4 - (villager->needs.hunger == 0 ? 10 : 0);
    terms[CORE_MOOD_REST] = (villager->needs.rest - 50) / 5;
    terms[CORE_MOOD_SAFETY] = (villager->needs.safety - 50) / 5;
    terms[CORE_MOOD_HOUSING] = villager->home != NULL ? 5 : -12;
    for (i = 0; i < 2; i++) {
        int32_t trait = villager->traits[i];
        if (trait >= 0 && trait < world->defs.trait_count) {
            terms[CORE_MOOD_TRAITS] += world->defs.traits[trait].mood;
        }
    }
    if (villager->workplace != NULL) {
        terms[CORE_MOOD_WAGES] = (villager->flags & CORE_VF_UNPAID) ? -15 : 5;
    }
    terms[CORE_MOOD_MOURNING] = -world->mourning;
    if (villager->home != NULL && (villager->home->flags & CORE_BF_COLD)) {
        terms[CORE_MOOD_COLD] = -15; /* 난방을 못 한 집 */
    } else if (villager->home == NULL && clock_season(&world->clock) == CORE_SEASON_WINTER) {
        terms[CORE_MOOD_COLD] = -10; /* 겨울에 한데서 잠 */
    }
    if (villager->sick_ticks > 0) {
        terms[CORE_MOOD_SICKNESS] = -10;
    }
    for (i = 0; i < CORE_MOOD_TERM_COUNT; i++) {
        total += terms[i];
    }
    return clamp100(total);
}

static void fall_back(World *world, Villager *villager) {
    int32_t day = clock_day(&world->clock);
    if (villager->state == CORE_VS_LEAVING) {
        population_remove_departed(world, villager);
        return;
    }
    villager->hold_ticks = VIL_HOLD_TICKS;
    vil_set_intent(villager, CORE_VS_IDLE, NULL, villager->loc_x, villager->loc_y, CORE_INTENT_HOLD);
    if (villager->blocked_day != day) {
        villager->blocked_day = day;
        events_push_villager(&world->events, CORE_EVT_PATH_BLOCKED, villager->id, 0, 0, villager->name);
    }
}

int vil_arrived(World *world, Villager *villager, int32_t intent_rev) {
    if (intent_rev != villager->intent_rev) {
        return CORE_IGNORED; /* 걷는 사이에 의도가 바뀜: 오래된 보고 */
    }
    villager->at_target = 1;
    villager->loc_x = villager->target_x;
    villager->loc_y = villager->target_y;
    villager->intent_flags &= ~CORE_INTENT_SPAWN_EDGE;
    if (world->expansion && villager->incoming && villager->target == world_keep(world))
        expansion_arrival_join(world, villager);
    if (world->expansion && villager->target && villager->state != CORE_VS_GUARD_POST &&
        !(expansion_spec(villager->target->type)->flags & CORE_EXP_OUTDOOR))
        villager->inside_building = villager->target->id;
    switch (villager->state) {
    case CORE_VS_LEAVING:
        if (world->expansion && !villager->departure_approved &&
            villager->contract_end_tick > world->clock.tick)
            villager->intent_flags |= CORE_INTENT_HOLD;
        else
            population_remove_departed(world, villager);
        break;
    default:
        break;
    }
    return CORE_OK;
}

int vil_unreachable(World *world, Villager *villager, int32_t intent_rev) {
    if (intent_rev != villager->intent_rev) {
        return CORE_IGNORED;
    }
    if (world->expansion) {
        villager->hub_revision = -1;
        villager->hub_id = 0;
    }
    if (world->expansion && villager->state == CORE_VS_LEAVING) {
        int x, y;
        if (expansion_person_exit(world, villager, &x, &y))
            vil_set_intent(villager, CORE_VS_LEAVING, NULL, x, y, 0);
        villager->at_target = 0;
        villager->intent_rev++;
        return CORE_OK;
    }
    if (world->expansion && villager->mission == CORE_MISSION_RETURN && villager->workplace) {
        Building *b = villager->workplace;
        if (b->type == CORE_BLD_HUNTER_CAMP || b->type == CORE_BLD_HUNTING_LODGE) {
            if (villager->hunt_retreat_reason != 3)
                expansion_notice(world, 165, villager->id, b->id, u8"수렵시설 접근 불가 · 비상 대피");
            villager->hunt_retreat_reason = 3;
            int x, y;
            b = expansion_refuge(world, villager, &x, &y);
            vil_set_intent(villager, CORE_VS_GUARD_POST, b, b ? b->door_x : x, b ? b->door_y : y, 0);
        } else
            vil_set_intent(villager, CORE_VS_GUARD_POST, b, b->door_x, b->door_y, 0);
        villager->intent_rev++;
        return CORE_OK;
    }
    fall_back(world, villager);
    return CORE_OK;
}
