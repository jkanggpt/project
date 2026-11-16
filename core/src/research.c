/*
 * research.c — 연구 (설명은 research.h)
 */
#include "research.h"
#include "expansion.h"

#include <string.h>

#include "economy.h"
#include "jobs.h"
#include "placement.h"
#include "world.h"

void research_init(ResearchState *state)
{
    memset(state, 0, sizeof(*state));
    state->active = -1;
}

static int is_done(const World *world, int32_t index)
{
    return index >= 0 && index < 32 && (world->research.done & (1u << index)) != 0;
}

int32_t research_state_of(const World *world, int32_t index)
{
    const ResearchDef *def = defs_research(&world->defs, index);
    if (def == NULL) {
        return CORE_RSTATE_LOCKED;
    }
    if (is_done(world, index)) {
        return CORE_RSTATE_DONE;
    }
    if (world->research.active == index) {
        return CORE_RSTATE_ACTIVE;
    }
    if (world->rank < def->required_rank || (def->requires >= 0 && !is_done(world, def->requires))) {
        return CORE_RSTATE_LOCKED;
    }
    return CORE_RSTATE_AVAILABLE;
}

int research_start(World *world, int32_t index, int32_t *out_reason)
{
    const ResearchDef *def = defs_research(&world->defs, index);
    int32_t cost[CORE_RES_COUNT];
    int32_t state;
    *out_reason = CORE_DENY_NONE;
    if (world->outcome == CORE_OUTCOME_LOST) {
        *out_reason = CORE_DENY_GAME_OVER;
        return CORE_ERR_REJECTED;
    }
    state = research_state_of(world, index);
    if (state == CORE_RSTATE_DONE) {
        *out_reason = CORE_DENY_RESEARCH_DONE;
        return CORE_ERR_REJECTED;
    }
    if (world->research.active >= 0) {
        *out_reason = CORE_DENY_RESEARCH_BUSY;
        return CORE_ERR_REJECTED;
    }
    if (state == CORE_RSTATE_LOCKED) {
        *out_reason = CORE_DENY_RESEARCH_LOCKED;
        return CORE_ERR_REJECTED;
    }
    memset(cost, 0, sizeof(cost));
    cost[CORE_RES_GOLD] = def->cost_gold;
    cost[CORE_RES_MANA] = def->cost_mana;
    if (world->bank.amount[CORE_RES_GOLD] < def->cost_gold) {
        *out_reason = CORE_DENY_NO_GOLD;
        return CORE_ERR_REJECTED;
    }
    if (world->bank.amount[CORE_RES_MANA] < def->cost_mana) {
        *out_reason = CORE_DENY_NO_MANA;
        return CORE_ERR_REJECTED;
    }
    (void)eco_pay(&world->bank, &world->ledger, cost, CORE_LEDGER_RESEARCH);
    world->research.active = index;
    world->research.progress_milli = 0;
    events_push_progress(&world->events, CORE_EVT_RESEARCH_STARTED, index, 0, 0, def->key);
    return CORE_OK;
}

/* ---- 효과 함수 (함수 포인터 표) --------------------------------------- */

typedef void (*ResearchEffectFn)(World *world, int32_t value);

/* 정의의 값 하나를 value % 만큼 키웁니다 */
static void scale(int32_t *field, int32_t value)
{
    *field = (int32_t)((int64_t)*field * (100 + value) / 100);
}

static BuildingDef *bdef(World *world, int32_t type)
{
    return &world->defs.buildings[type];
}

static void effect_farm_yield(World *world, int32_t value)
{
    scale(&bdef(world, CORE_BLD_FARM)->rate, value);
}

static void effect_extract_yield(World *world, int32_t value)
{
    scale(&bdef(world, CORE_BLD_LUMBER)->rate, value);
    scale(&bdef(world, CORE_BLD_QUARRY)->rate, value);
}

static void effect_storage(World *world, int32_t value)
{
    scale(&bdef(world, CORE_BLD_KEEP)->storage, value);
    scale(&bdef(world, CORE_BLD_WAREHOUSE)->storage, value);
}

static void effect_market(World *world, int32_t value)
{
    world->research.market_spread += value;
}

static void effect_tower_damage(World *world, int32_t value)
{
    bdef(world, CORE_BLD_TOWER)->attack += value;
}

static void effect_wall_hp(World *world, int32_t value)
{
    scale(&bdef(world, CORE_BLD_WALL)->max_hp, value);
    scale(&bdef(world, CORE_BLD_GATE)->max_hp, value);
}

static void effect_guard_damage(World *world, int32_t value)
{
    bdef(world, CORE_BLD_BARRACKS)->attack += value;
}

static void effect_keep_damage(World *world, int32_t value)
{
    bdef(world, CORE_BLD_KEEP)->attack += value;
}

static void effect_mana_yield(World *world, int32_t value)
{
    scale(&bdef(world, CORE_BLD_MANA_MINE)->rate, value);
}

static void effect_healing(World *world, int32_t value)
{
    world->research.healing_pct += value;
}

static void effect_heating(World *world, int32_t value)
{
    world->research.heating_minus += value;
}

static void effect_ward(World *world, int32_t value)
{
    world->research.ward_pct += value;
}

/* 효과 번호(CoreResearchEffect, 정의 파일의 effect 키) → 효과 함수 */
static const ResearchEffectFn RESEARCH_EFFECTS[CORE_EFFECT_COUNT] = {
    effect_farm_yield,    /* CORE_EFFECT_FARM_YIELD */
    effect_extract_yield, /* CORE_EFFECT_EXTRACT_YIELD */
    effect_storage,       /* CORE_EFFECT_STORAGE */
    effect_market,        /* CORE_EFFECT_MARKET */
    effect_tower_damage,  /* CORE_EFFECT_TOWER_DAMAGE */
    effect_wall_hp,       /* CORE_EFFECT_WALL_HP */
    effect_guard_damage,  /* CORE_EFFECT_GUARD_DAMAGE */
    effect_keep_damage,   /* CORE_EFFECT_KEEP_DAMAGE */
    effect_mana_yield,    /* CORE_EFFECT_MANA_YIELD */
    effect_healing,       /* CORE_EFFECT_HEALING */
    effect_heating,       /* CORE_EFFECT_HEATING */
    effect_ward,          /* CORE_EFFECT_WARD */
};

void research_reapply(World *world)
{
    if(world->expansion){expansion_apply_research(world);return;}
    ResearchState *state = &world->research;
    int32_t i;
    memcpy(world->defs.buildings, world->base_buildings, sizeof(world->defs.buildings));
    state->market_spread = 0;
    state->heating_minus = 0;
    state->healing_pct = 0;
    state->ward_pct = 0;
    state->done_count = 0;
    for (i = 0; i < world->defs.research_count && i < 32; i++) {
        const ResearchDef *def = &world->defs.research[i];
        if (!is_done(world, i)) {
            continue;
        }
        state->done_count++;
        if (def->effect >= 0 && def->effect < CORE_EFFECT_COUNT) {
            RESEARCH_EFFECTS[def->effect](world, def->value);
        }
    }
    world->market.spread_pct = MARKET_SPREAD_PCT - state->market_spread;
    if (world->market.spread_pct < MARKET_MIN_SPREAD_PCT) {
        world->market.spread_pct = MARKET_MIN_SPREAD_PCT;
    }
}

/* 연구가 끝남: 효과를 다시 계산하고, 성벽·성문의 체력을 새 최대 체력에 맞춰 키우고, 저장 한도를 다시 셈 */
static void complete(World *world, int32_t index)
{
    int32_t old_max[2];
    int32_t i;
    old_max[0] = world->defs.buildings[CORE_BLD_WALL].max_hp;
    old_max[1] = world->defs.buildings[CORE_BLD_GATE].max_hp;
    world->research.done |= 1u << index;
    world->research.active = -1;
    world->research.progress_milli = 0;
    research_reapply(world);
    for (i = 0; i < world->building_count; i++) {
        Building *building = world->buildings[i];
        int32_t old;
        if (building->state != CORE_BSTATE_ACTIVE ||
            (building->type != CORE_BLD_WALL && building->type != CORE_BLD_GATE)) {
            continue;
        }
        old = old_max[building->type == CORE_BLD_GATE];
        if (old > 0) {
            building->hp = (int32_t)((int64_t)building->hp * world->defs.buildings[building->type].max_hp / old);
        }
    }
    placement_refresh_capacity(world);
    events_push_progress(&world->events, CORE_EVT_RESEARCH_DONE, index, world->research.done_count, 0,
                         world->defs.research[index].key);
}

void research_work(World *world, Building *tower, Villager *mage)
{
    const ResearchDef *def = defs_research(&world->defs, world->research.active);
    const BuildingDef *tower_def = world_building_def(world, tower->type);
    int32_t magic = 0;
    int64_t amount;
    int32_t i;
    if (def == NULL || world->outcome == CORE_OUTCOME_LOST) {
        return; /* 연구 중인 것이 없으면 마법사는 책만 읽음 */
    }
    for (i = 0; i < 2; i++) {
        int32_t trait = mage->traits[i];
        if (trait >= 0 && trait < world->defs.trait_count) {
            magic += world->defs.traits[trait].magic_pct;
        }
    }
    amount = (int64_t)tower_def->rate * jobs_work_percent(world, mage, tower->type) / 100;
    amount = amount * (100 + magic) / 100;
    if (amount < 0) {
        amount = 0;
    }
    world->research.progress_milli += (int32_t)amount;
    if ((int64_t)world->research.progress_milli >= (int64_t)def->points * 1000) {
        complete(world, world->research.active);
    }
}

int32_t research_mages(const World *world)
{
    int32_t count = 0;
    int32_t i;
    for (i = 0; i < world->villager_count; i++) {
        const Villager *villager = world->villagers[i];
        count += villager->state == CORE_VS_WORKING && villager->at_target && villager->workplace != NULL &&
                 villager->workplace->type == CORE_BLD_MAGE_TOWER &&
                 villager->workplace->state == CORE_BSTATE_ACTIVE;
    }
    return count;
}
