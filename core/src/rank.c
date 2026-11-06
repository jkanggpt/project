/*
 * rank.c — 등급과 승패 (설명은 rank.h)
 */
#include "rank.h"

#include <string.h>

#include "difficulty.h"
#include "placement.h"
#include "threat.h"
#include "world.h"

int32_t rank_defense(const World *world)
{
    int32_t total = 0;
    int32_t i;
    for (i = 0; i < world->building_count; i++) {
        const Building *building = world->buildings[i];
        if (building->state == CORE_BSTATE_ACTIVE) {
            total += world_building_def(world, building->type)->defense;
        }
    }
    for (i = 0; i < world->villager_count; i++) {
        total += threat_is_guard(world, world->villagers[i]);
    }
    return total;
}

static int32_t average_mood(const World *world)
{
    int32_t total = 0;
    int32_t i;
    for (i = 0; i < world->villager_count; i++) {
        total += world->villagers[i]->mood;
    }
    return world->villager_count > 0 ? total / world->villager_count : 0;
}

/* 공물 = 정의의 값 × 난이도 % (올림) */
static int32_t tribute(const World *world, int32_t base)
{
    int32_t pct = difficulty_preset(world->difficulty)->tribute_pct;
    return (int32_t)(((int64_t)base * pct + 99) / 100);
}

void rank_fill_view(const World *world, CoreRankView *out)
{
    const RankDef *next;
    int32_t type;
    int32_t i;
    memset(out, 0, sizeof(*out));
    out->rank = world->rank;
    out->have_population = world->villager_count;
    out->have_mood = average_mood(world);
    out->have_defense = rank_defense(world);
    out->have_research = world->research.done_count;
    if (world->rank >= CORE_RANK_COUNT - 1) {
        out->next_rank = -1;
        return;
    }
    out->next_rank = world->rank + 1;
    next = &world->defs.ranks[out->next_rank];
    out->need_population = next->population;
    out->need_mood = next->mood;
    out->need_defense = next->defense;
    /* 연구 정의가 그보다 적으면 있는 만큼만 요구 (정의 파일을 고치다 오를 수 없게 되지 않도록) */
    out->need_research = next->research < world->defs.research_count ? next->research : world->defs.research_count;
    out->tribute_gold = tribute(world, next->tribute_gold);
    out->tribute_mana = tribute(world, next->tribute_mana);
    out->storage_bonus = next->storage;
    for (type = 0; type < CORE_BLD_COUNT; type++) {
        if (world->defs.buildings[type].required_rank == out->next_rank && type != CORE_BLD_KEEP) {
            out->unlocks |= 1 << type;
        }
    }
    for (i = 0; i < world->defs.research_count; i++) {
        out->research_unlocks += world->defs.research[i].required_rank == out->next_rank;
    }
    out->ready = out->have_population >= out->need_population && out->have_mood >= out->need_mood &&
                 out->have_defense >= out->need_defense && out->have_research >= out->need_research &&
                 world->bank.amount[CORE_RES_GOLD] >= out->tribute_gold &&
                 world->bank.amount[CORE_RES_MANA] >= out->tribute_mana && world->outcome != CORE_OUTCOME_LOST;
}

int rank_is_ready(const World *world)
{
    CoreRankView view;
    rank_fill_view(world, &view);
    return view.ready;
}

int rank_up(World *world, int32_t *out_reason)
{
    CoreRankView view;
    int32_t cost[CORE_RES_COUNT];
    *out_reason = CORE_DENY_NONE;
    rank_fill_view(world, &view);
    if (world->outcome == CORE_OUTCOME_LOST) {
        *out_reason = CORE_DENY_GAME_OVER;
        return CORE_ERR_REJECTED;
    }
    if (view.next_rank < 0) {
        *out_reason = CORE_DENY_MAX_RANK;
        return CORE_ERR_REJECTED;
    }
    if (threat_raid_active(world)) {
        *out_reason = CORE_DENY_RAID; /* 싸우는 중에는 즉위식을 하지 않습니다 */
        return CORE_ERR_REJECTED;
    }
    if (view.have_population < view.need_population || view.have_mood < view.need_mood ||
        view.have_defense < view.need_defense || view.have_research < view.need_research) {
        *out_reason = CORE_DENY_REQUIREMENTS;
        return CORE_ERR_REJECTED;
    }
    if (world->bank.amount[CORE_RES_GOLD] < view.tribute_gold) {
        *out_reason = CORE_DENY_NO_GOLD;
        return CORE_ERR_REJECTED;
    }
    if (world->bank.amount[CORE_RES_MANA] < view.tribute_mana) {
        *out_reason = CORE_DENY_NO_MANA;
        return CORE_ERR_REJECTED;
    }
    memset(cost, 0, sizeof(cost));
    cost[CORE_RES_GOLD] = view.tribute_gold;
    cost[CORE_RES_MANA] = view.tribute_mana;
    (void)eco_pay(&world->bank, &world->ledger, cost, CORE_LEDGER_TRIBUTE);
    world->rank = view.next_rank;
    placement_refresh_capacity(world);
    events_push_progress(&world->events, CORE_EVT_RANK_UP, world->rank, 0, 0, NULL);
    if (world->rank == CORE_RANK_ROYAL && world->outcome == CORE_OUTCOME_NONE) {
        world->outcome = CORE_OUTCOME_WON;
        world->stats.victory_day = clock_day(&world->clock);
        events_push_progress(&world->events, CORE_EVT_VICTORY, world->stats.victory_day, 0, 0, NULL);
    }
    return CORE_OK;
}

int32_t rank_storage_bonus(const World *world)
{
    int32_t total = 0;
    int32_t rank;
    for (rank = 1; rank <= world->rank && rank < CORE_RANK_COUNT; rank++) {
        total += world->defs.ranks[rank].storage;
    }
    return total;
}

void rank_lose(World *world, int32_t reason)
{
    if (world->outcome == CORE_OUTCOME_LOST) {
        return;
    }
    world->outcome = CORE_OUTCOME_LOST;
    world->lose_reason = reason;
    threat_abort_raid(world);
    events_push_progress(&world->events, CORE_EVT_DEFEAT, reason, clock_day(&world->clock), 0, NULL);
}

void rank_hourly(World *world)
{
    Building *keep;
    if (world->outcome == CORE_OUTCOME_LOST) {
        return;
    }
    keep = world_keep(world);
    if (keep == NULL || keep->state == CORE_BSTATE_RUINED) {
        rank_lose(world, CORE_LOSE_KEEP);
        return;
    }
    if (world->villager_count == 0) {
        rank_lose(world, CORE_LOSE_POPULATION);
        return;
    }
    /* 조건을 처음 채우면 한 번 알립니다 (같은 등급을 두 번 알리지 않음) */
    if (world->rank_ready_announced <= world->rank && world->rank < CORE_RANK_COUNT - 1 && rank_is_ready(world)) {
        world->rank_ready_announced = world->rank + 1;
        events_push_progress(&world->events, CORE_EVT_RANK_READY, world->rank + 1, 0, 0, NULL);
    }
}
