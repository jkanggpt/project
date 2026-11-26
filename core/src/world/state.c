/*
 * world.c — 월드 생성·해제와 틱 진행 (설명은 world.h)
 */
#include "expansion.h"
#include "world.h"

#include <string.h>

#include "jobs.h"
#include "mem.h"
#include "placement.h"
#include "population.h"
#include "rank.h"
#include "text.h"
#include "tutorial.h"
#include "villager_ai.h"

World *world_alloc(const Defs *defs) {
    World *world = (World *)mem_calloc(1, sizeof(World));
    int rc;
    if (world == NULL) {
        return NULL;
    }
    if (events_init(&world->events, EVENTS_INITIAL_CAPACITY) != CORE_OK) {
        mem_free(world);
        return NULL;
    }
    /* 정의는 복사해서 가집니다: 호출한 쪽이 원본을 해제해도 월드는 안전 */
    rc = defs != NULL ? defs_copy(&world->defs, defs) : defs_init_builtin(&world->defs);
    if (rc != CORE_OK) {
        events_free(&world->events);
        mem_free(world);
        return NULL;
    }
    world->next_villager_id = 1;
    world->next_building_id = 1;
    memcpy(world->base_buildings, world->defs.buildings, sizeof(world->base_buildings));
    research_init(&world->research);
    nature_init(&world->nature);
    market_init(&world->market);
    merchant_init(&world->merchant);
    threat_init(&world->threat);
    return world;
}

void world_destroy(World *world) {
    if (world == NULL) {
        return;
    }
    expansion_free(world);
    entity_free_all(world);
    threat_free(&world->threat);
    grid_free(&world->grid);
    events_free(&world->events);
    defs_free(&world->defs);
    mem_free(world);
}

int world_name_is_valid(const char *name, int32_t max_bytes) { return text_is_valid_name(name, max_bytes); }

int world_set_name(World *world, const char *name) {
    if (!world_name_is_valid(name, CORE_NAME_BYTES)) {
        return CORE_ERR_INVALID_ARG;
    }
    memset(world->castle_name, 0, sizeof(world->castle_name));
    strcpy(world->castle_name, name); /* 길이는 위에서 확인했습니다 */
    return CORE_OK;
}

const BuildingDef *world_building_def(const World *world, int32_t type) {
    return defs_building(&world->defs, type);
}

Building *world_keep(const World *world) { return entity_find_building(world, world->keep_id); }

/* 성채를 지도 가운데에 세웁니다 (다 지은 상태로) */
static int place_keep(World *world) {
    const BuildingDef *def = world_building_def(world, CORE_BLD_KEEP);
    Building *keep = entity_add_building(world, CORE_BLD_KEEP);
    if (keep == NULL) {
        return CORE_ERR_OUT_OF_MEMORY;
    }
    keep->x = world->keep_x - def->w / 2;
    keep->y = world->keep_y - def->h / 2;
    keep->w = def->w;
    keep->h = def->h;
    keep->state = CORE_BSTATE_ACTIVE;
    keep->hp = def->max_hp;
    keep->auto_assign = 0;
    /* 성채 문은 아래쪽 가운데 (지형 생성이 성채 둘레를 비워 두었으므로 늘 비어 있음) */
    keep->door_x = world->keep_x;
    keep->door_y = keep->y + keep->h;
    world->keep_id = keep->id;
    placement_rebuild_layers(world);
    eco_set_capacity(&world->bank, &world->ledger, def->storage);
    return CORE_OK;
}

int world_create(const CoreNewGameParams *params, const Defs *defs, World **out) {
    World *world;
    int rc;
    int32_t i;

    *out = NULL;
    if (params->ruleset < 0 || params->ruleset > 1 || params->seed < 0 || params->difficulty < 0 ||
        params->difficulty >= CORE_DIFF_COUNT || !world_name_is_valid(params->castle_name, CORE_NAME_BYTES)) {
        return CORE_ERR_INVALID_ARG;
    }

    world = world_alloc(defs);
    if (world == NULL) {
        return CORE_ERR_OUT_OF_MEMORY;
    }
    (void)world_set_name(world, params->castle_name);
    world->difficulty = params->difficulty;
    world->seed = params->seed;
    world->clock.tick = CLOCK_START_TICK;
    /* 지도 생성과 게임 진행이 같은 난수 스트림을 쓰도록 시드로 초기화합니다 */
    rng_seed(&world->rng, (uint64_t)(uint32_t)params->seed, 0x4D43u);
    eco_bank_init(&world->bank, params->difficulty);
    world->ruleset = 1;
    rc = expansion_allocate(world);
    if (rc == CORE_OK)
        rc = grid_alloc(&world->grid, 160, 160);
    if (rc == CORE_OK)
        rc = expansion_resize(world);
    if (rc == CORE_OK) {
        world->keep_x = world->grid.width / 2;
        world->keep_y = world->grid.height / 2;
        rc = grid_generate(&world->grid, (uint32_t)params->seed, &world->rng, world->keep_x, world->keep_y);
    }
    if (rc == CORE_OK) {
        rc = place_keep(world);
    }
    if (rc == CORE_OK) {
        rc = expansion_new_world(world);
    }
    if (rc == CORE_OK) {
        rc = population_spawn_initial(world);
    }
    if (rc == CORE_OK) {
        expansion_assign(world);
        expansion_visibility(world);
        world->stats.peak_population = world->villager_count;
    }
    if (rc != CORE_OK) {
        world_destroy(world);
        return rc;
    }
    /* 망가진 정의 파일이 있었다면 알립니다 (기본값을 쓰는 중) */
    for (i = 0; i < world->defs.warning_count; i++) {
        events_push_warning(&world->events, CORE_EVT_DEF_WARNING, world->defs.warnings[i].file_id,
                            world->defs.warnings[i].line);
    }
    *out = world;
    return CORE_OK;
}

/* 매일 06:00: 장부 넘기기, 날씨·자연 사건·난방, 임금과 세금, 이주민, 시장·상인, 배정, 굶주림 알림 */

/* 한 틱(게임 속 10분)을 진행합니다 */
static void world_step(World *world) {
    world->clock.tick++;
    expansion_tick(world);
}

int world_tick(World *world, int32_t ticks) {
    int32_t i;
    if (ticks < 1) {
        return CORE_ERR_INVALID_ARG;
    }
    if (world->outcome == CORE_OUTCOME_LOST) {
        return CORE_ERR_REJECTED; /* 패배한 뒤에는 시간이 흐르지 않습니다 */
    }
    if (world->clock.tick > CLOCK_MAX_TICK - ticks) {
        return CORE_ERR_INVALID_ARG; /* 시계가 int32 를 넘지 않게 막습니다 */
    }
    for (i = 0; i < ticks && world->outcome != CORE_OUTCOME_LOST; i++) {
        world_step(world);
    }
    return world->expansion && world->expansion->error ? world->expansion->error : CORE_OK;
}

/* ---- 상태 해시 (FNV-1a) ----------------------------------------------- */

#define FNV_OFFSET 1469598103934665603ULL
#define FNV_PRIME 1099511628211ULL

static uint64_t hash_bytes(uint64_t hash, const void *data, size_t size) {
    const unsigned char *bytes = (const unsigned char *)data;
    size_t i;
    for (i = 0; i < size; i++) {
        hash ^= bytes[i];
        hash *= FNV_PRIME;
    }
    return hash;
}

/* 정수는 바이트 순서와 상관없이 같은 해시가 나오도록 한 바이트씩 넣습니다 */
static uint64_t hash_u64(uint64_t hash, uint64_t value) {
    int i;
    for (i = 0; i < 8; i++) {
        unsigned char b = (unsigned char)(value >> (i * 8));
        hash = hash_bytes(hash, &b, 1);
    }
    return hash;
}

static uint64_t hash_i32(uint64_t hash, int32_t value) { return hash_u64(hash, (uint64_t)(uint32_t)value); }

static uint64_t hash_villager(uint64_t hash, const Villager *v) {
    hash = hash_i32(hash, v->id);
    hash = hash_bytes(hash, v->name, strlen(v->name));
    hash = hash_i32(hash, v->age);
    hash = hash_i32(hash, v->stats.strength);
    hash = hash_i32(hash, v->stats.intellect);
    hash = hash_i32(hash, v->stats.dexterity);
    hash = hash_i32(hash, v->stats.vitality);
    hash = hash_i32(hash, v->needs.hunger);
    hash = hash_i32(hash, v->needs.rest);
    hash = hash_i32(hash, v->needs.safety);
    hash = hash_i32(hash, v->needs.housing);
    hash = hash_i32(hash, v->traits[0]);
    hash = hash_i32(hash, v->traits[1]);
    hash = hash_i32(hash, v->mood);
    hash = hash_i32(hash, v->health);
    hash = hash_i32(hash, v->flags);
    hash = hash_i32(hash, v->home != NULL ? v->home->id : 0);
    hash = hash_i32(hash, v->workplace != NULL ? v->workplace->id : 0);
    hash = hash_i32(hash, v->state);
    hash = hash_i32(hash, v->intent_rev);
    hash = hash_i32(hash, v->intent_flags);
    hash = hash_i32(hash, v->target != NULL ? v->target->id : 0);
    hash = hash_i32(hash, v->target_x);
    hash = hash_i32(hash, v->target_y);
    hash = hash_i32(hash, v->at_target);
    hash = hash_i32(hash, v->loc_x);
    hash = hash_i32(hash, v->loc_y);
    hash = hash_i32(hash, v->state_ticks);
    hash = hash_i32(hash, v->hold_ticks);
    hash = hash_i32(hash, v->low_mood_ticks);
    hash = hash_i32(hash, v->hunger_acc);
    hash = hash_i32(hash, v->rest_acc);
    hash = hash_i32(hash, v->story_flags);
    hash = hash_i32(hash, v->blocked_day);
    hash = hash_i32(hash, v->sick_ticks);
    hash = hash_i32(hash, v->heal_ticks);
    hash = hash_i32(hash, v->sex);
    hash = hash_i32(hash, v->appearance);
    return hash;
}

static uint64_t hash_building(uint64_t hash, const Building *b) {
    hash = hash_i32(hash, b->id);
    hash = hash_i32(hash, b->type);
    hash = hash_i32(hash, b->x);
    hash = hash_i32(hash, b->y);
    hash = hash_i32(hash, b->w);
    hash = hash_i32(hash, b->h);
    hash = hash_i32(hash, b->state);
    hash = hash_i32(hash, b->hp);
    hash = hash_i32(hash, b->build_progress);
    hash = hash_i32(hash, b->door_x);
    hash = hash_i32(hash, b->door_y);
    hash = hash_i32(hash, b->auto_assign);
    hash = hash_i32(hash, b->work_milli[0]);
    hash = hash_i32(hash, b->work_milli[1]);
    hash = hash_i32(hash, b->flags);
    return hash;
}

uint64_t world_state_hash(const World *world) {
    uint64_t hash = FNV_OFFSET;
    size_t cells = (size_t)world->grid.width * (size_t)world->grid.height;
    int32_t i;
    int32_t c;

    hash = hash_bytes(hash, world->castle_name, strlen(world->castle_name));
    hash = hash_i32(hash, world->difficulty);
    hash = hash_i32(hash, world->seed);
    hash = hash_i32(hash, world->keep_x);
    hash = hash_i32(hash, world->keep_y);
    hash = hash_i32(hash, world->keep_id);
    hash = hash_i32(hash, world->rank);
    hash = hash_i32(hash, world->rank_ready_announced);
    hash = hash_i32(hash, world->outcome);
    hash = hash_i32(hash, world->lose_reason);
    hash = hash_i32(hash, world->tutorial_done);
    hash = hash_i32(hash, world->tutorial_flags);
    hash = hash_u64(hash, world->research.done);
    hash = hash_i32(hash, world->research.active);
    hash = hash_i32(hash, world->research.progress_milli);
    hash = hash_i32(hash, world->stats.peak_population);
    hash = hash_i32(hash, world->stats.enemies_killed);
    hash = hash_i32(hash, world->stats.villagers_died);
    hash = hash_i32(hash, world->stats.villagers_left);
    hash = hash_i32(hash, world->stats.buildings_built);
    hash = hash_i32(hash, world->stats.victory_day);
    hash = hash_i32(hash, world->next_villager_id);
    hash = hash_i32(hash, world->next_building_id);
    hash = hash_i32(hash, world->mourning);
    hash = hash_i32(hash, world->clock.tick);
    hash = hash_u64(hash, world->rng.state);
    hash = hash_u64(hash, world->rng.inc);
    hash = hash_i32(hash, world->bank.capacity);
    hash = hash_i32(hash, world->nature.weather);
    hash = hash_i32(hash, world->nature.event);
    hash = hash_i32(hash, world->nature.event_days);
    hash = hash_i32(hash, world->nature.forecast);
    hash = hash_i32(hash, world->nature.forecast_days);
    hash = hash_i32(hash, world->nature.calm_days);
    hash = hash_i32(hash, world->nature.cold_houses);
    hash = hash_i32(hash, world->threat.threat_count);
    for (i = 0; i < world->threat.threat_count; i++) {
        const Threat *threat = &world->threat.threats[i];
        hash = hash_i32(hash, threat->type);
        hash = hash_i32(hash, threat->direction);
        hash = hash_i32(hash, threat->eta_tick);
        hash = hash_i32(hash, threat->strength);
        hash = hash_i32(hash, threat->near_warned);
    }
    hash = hash_i32(hash, world->threat.next_raid_id);
    hash = hash_i32(hash, world->threat.raids_done);
    hash = hash_i32(hash, world->threat.raid.id); /* 확장 전투 레코드와 함께 습격 식별자를 검산합니다. */
    hash = hash_i32(hash, world->merchant.days_left);
    hash = hash_i32(hash, world->merchant.next_visit_day);
    hash = hash_i32(hash, world->merchant.deal_count);
    for (i = 0; i < world->merchant.deal_count; i++) {
        const MerchantDeal *deal = &world->merchant.deals[i];
        hash = hash_i32(hash, deal->give_resource);
        hash = hash_i32(hash, deal->give_amount);
        hash = hash_i32(hash, deal->get_resource);
        hash = hash_i32(hash, deal->get_amount);
        hash = hash_i32(hash, deal->taken);
    }
    for (i = 0; i < CORE_RES_COUNT; i++) {
        hash = hash_i32(hash, world->bank.amount[i]);
        hash = hash_i32(hash, world->market.price[i]);
        for (c = 0; c < CORE_LEDGER_COUNT; c++) {
            hash = hash_i32(hash, world->ledger.today[i][c]);
            hash = hash_i32(hash, world->ledger.yesterday[i][c]);
        }
    }
    hash = hash_i32(hash, world->grid.width);
    hash = hash_i32(hash, world->grid.height);
    if (world->grid.terrain != NULL && world->grid.cost != NULL) {
        hash = hash_bytes(hash, world->grid.terrain, cells);
        hash = hash_bytes(hash, world->grid.road, cells);
        hash = hash_bytes(hash, world->grid.cost, cells);
    }
    for (i = 0; i < world->building_count; i++) {
        hash = hash_building(hash, world->buildings[i]);
    }
    for (i = 0; i < world->villager_count; i++) {
        hash = hash_villager(hash, world->villagers[i]);
    }
    if (world->expansion)
        hash = expansion_hash(world, hash);
    return hash;
}
