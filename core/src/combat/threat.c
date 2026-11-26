/*
 * threat.c — 위협과 습격 (설명은 threat.h)
 */
#include "threat.h"

#include <string.h>

#include "difficulty.h"
#include "expansion.h"
#include "mem.h"
#include "placement.h"
#include "population.h"
#include "rank.h"
#include "villager_ai.h"
#include "world.h"

/* 난이도별 세기(%)와 위협 사이 간격(%) — 평화, 보통, 혹독 */

/* 성능 검사의 고정 무리 구성 */

typedef int (*ComposeFn)(World *world, Raid *raid, int32_t strength);




/* 도적: 날이 갈수록, 그리고 금화가 많을수록 (부자 마을을 노림) */






/* 적 목록을 count 명 이상 담을 수 있게 키웁니다 (임시 포인터로 받아 실패해도 원래 배열 유지) */
static int reserve_enemies(Raid *raid, int32_t needed) {
    Enemy *grown;
    int32_t capacity;
    if (needed <= raid->enemy_capacity) {
        return CORE_OK;
    }
    capacity = raid->enemy_capacity > 0 ? raid->enemy_capacity : 8;
    while (capacity < needed) {
        capacity *= 2;
    }
    grown = (Enemy *)mem_realloc(raid->enemies, (size_t)capacity * sizeof(Enemy));
    if (grown == NULL) {
        return CORE_ERR_OUT_OF_MEMORY;
    }
    raid->enemies = grown;
    raid->enemy_capacity = capacity;
    return CORE_OK;
}

static int add_enemies(Raid *raid, int32_t type, int32_t count) {
    int32_t i;
    int rc = reserve_enemies(raid, raid->enemy_count + count);
    if (rc != CORE_OK) {
        return rc;
    }
    for (i = 0; i < count; i++) {
        Enemy *enemy = &raid->enemies[raid->enemy_count];
        memset(enemy, 0, sizeof(*enemy));
        enemy->id = raid->enemy_count + 1; /* 습격 안에서 1 부터 */
        enemy->type = type;
        enemy->state = CORE_ENEMY_ALIVE;
        enemy->looted = 0;
        enemy->summoner_id = 0;
        raid->enemy_count++;
    }
    return CORE_OK;
}

/* 도적 다섯에 하나는 두목 */
static int debug_compose_bandits(World *world, Raid *raid, int32_t strength) {
    int32_t brutes = strength / 5;
    int rc = add_enemies(raid, ENEMY_BRUTE, brutes);
    (void)world;
    return rc != CORE_OK ? rc : add_enemies(raid, ENEMY_BANDIT, strength - brutes);
}

/* 마물: 스무 날이 지나면 여섯에 하나는 오우거 */
static int debug_compose_monsters(World *world, Raid *raid, int32_t strength) {
    int32_t ogres = clock_day(&world->clock) >= 20 ? strength / 6 : 0;
    int rc = add_enemies(raid, ENEMY_OGRE, ogres);
    return rc != CORE_OK ? rc : add_enemies(raid, ENEMY_GOBLIN, strength - ogres);
}

/* 군대: 셋에 하나는 궁수 */
static int debug_compose_army(World *world, Raid *raid, int32_t strength) {
    int32_t archers = strength / 3;
    int rc = add_enemies(raid, ENEMY_ARCHER, archers);
    (void)world;
    return rc != CORE_OK ? rc : add_enemies(raid, ENEMY_SOLDIER, strength - archers);
}

static const ComposeFn DEBUG_COMPOSERS[CORE_THREAT_COUNT] = {NULL,debug_compose_bandits,debug_compose_monsters,debug_compose_army};

/* 성능 fixture의 구성은 기존 재현 결과를 유지합니다. 일반 습격 생성은 raids.c만 담당합니다. */

void threat_init(ThreatState *state) {
    memset(state, 0, sizeof(*state));
    state->next_raid_id = 1;
}

void threat_free(ThreatState *state) {
    mem_free(state->raid.enemies);
    state->raid.enemies = NULL;
    state->raid.enemy_capacity = 0;
    state->raid.enemy_count = 0;
}



int threat_raid_active(const World *world) {
    return world->threat.raid.id != 0 && (world->threat.raid.first_discovered ||
                                          world->threat.raid.phase == CORE_RAID_RETREAT);
}

/* day 일 minute 분의 틱 */




/* 다음 위협을 고릅니다: 첫 위협은 작은 도적 무리, 그 뒤로는 날짜에 맞는 종류 중 무작위 */




/* 지도 가장자리, 방향 쪽 가운데에 가장 가까운 걸을 수 있는 칸 */
static void spawn_tile(const World *world, int32_t direction, int32_t *out_x, int32_t *out_y) {
    int32_t w = world->grid.width;
    int32_t h = world->grid.height;
    static const int32_t FX[CORE_DIR_COUNT] = {1, 2, 1, 0}; /* × (w-1)/2 */
    static const int32_t FY[CORE_DIR_COUNT] = {0, 1, 2, 1};
    vil_edge_tile(world, FX[direction] * (w - 1) / 2, FY[direction] * (h - 1) / 2, out_x, out_y);
}

static void debug_start_raid(World *world, const Threat *threat) {
    ThreatState *state = &world->threat;
    Raid *raid = &state->raid;
    raid->enemy_count = 0;
    if (DEBUG_COMPOSERS[threat->type](world, raid, threat->strength) != CORE_OK) {
        raid->enemy_count = 0; /* 메모리가 모자라면 이번 습격은 오지 않음 (게임은 계속) */
        return;
    }
    raid->id = state->next_raid_id++;
    raid->type = threat->type;
    raid->direction = threat->direction;
    raid->started_tick = world->clock.tick;
    raid->killed = 0;
    raid->escaped = 0;
    raid->stolen = 0;
    raid->fallen = 0;
    spawn_tile(world, threat->direction, &raid->spawn_x, &raid->spawn_y);
    if (world->expansion) {
        raid->phase = CORE_RAID_APPROACH;
        raid->first_discovered = 0;
        raid->assault_end_tick = 0;
        raid->initial_count = raid->enemy_count;
        memset(raid->initial_types, 0, sizeof(raid->initial_types));
        for (int i = 0; i < raid->enemy_count; i++)
            if (raid->enemies[i].type < 28)
                raid->initial_types[raid->enemies[i].type]++;
    }
}

int threat_debug_raid(World *world, int32_t type, int32_t direction, int32_t count) {
    Threat threat;
    if (world->threat.raid.id != 0) {
        return CORE_ERR_REJECTED;
    }
    memset(&threat, 0, sizeof(threat));
    threat.type = type;
    threat.direction = direction;
    threat.eta_tick = world->clock.tick;
    threat.strength = count;
    debug_start_raid(world, &threat);
    return world->threat.raid.id != 0 ? CORE_OK : CORE_ERR_OUT_OF_MEMORY;
}

static void end_raid(World *world) {
    ThreatState *state = &world->threat;
    Raid *raid = &state->raid;
    int32_t i;
    for (i = 0; i < raid->enemy_count; i++) {
        if (raid->enemies[i].state == CORE_ENEMY_ALIVE) {
            raid->enemies[i].state = CORE_ENEMY_ESCAPED; /* 남은 적은 물러남 */
            raid->escaped++;
        }
    }
    if (world->expansion && world->outcome != CORE_OUTCOME_LOST && raid->first_discovered)
        for (i = 0; i < world->villager_count; i++)
            expansion_mood_event(world, world->villagers[i], 20, 3, raid->id, 1000000000, 0);
    events_push_raid(&world->events, CORE_EVT_RAID_ENDED, raid->killed, raid->enemy_count, raid->stolen,
                     raid->fallen);
    raid->id = 0;
    raid->phase = CORE_RAID_COMPLETE;
    raid->enemy_count = 0; /* 배열은 다음 습격에 다시 씀 */
    state->raids_done++;
}

/* 모두 쓰러졌거나 달아났으면 끝 */
void threat_abort_raid(World *world) {
    if (world->threat.raid.id != 0) {
        end_raid(world);
    }
}

static void end_if_done(World *world) {
    const Raid *raid = &world->threat.raid;
    int32_t i;
    for (i = 0; i < raid->enemy_count; i++) {
        if (raid->enemies[i].state == CORE_ENEMY_ALIVE) {
            return;
        }
    }
    end_raid(world);
}



int threat_is_guard(const World *world, const Villager *villager) {
    (void)world;
    return villager->workplace != NULL &&
           (villager->workplace->type == CORE_BLD_BARRACKS ||
            villager->workplace->type == CORE_BLD_GARRISON) &&
           villager->workplace->state == CORE_BSTATE_ACTIVE;
}

/* ---- 보고 ------------------------------------------------------------ */

static Enemy *find_enemy(World *world, int32_t enemy_id) {
    Raid *raid = &world->threat.raid;
    if (enemy_id < 1 || enemy_id > raid->enemy_count) {
        return NULL;
    }
    return &raid->enemies[enemy_id - 1];
}

/* 습격 id 가 지금 습격과 같은가 (다르면 늦게 온 보고) */
static int current(const World *world, int32_t raid_id) {
    return world->threat.raid.id != 0 && world->threat.raid.id == raid_id;
}

int threat_report_building_damage(World *world, int32_t raid_id, int32_t building_id, int32_t damage) {
    Building *building;
    if (!current(world, raid_id)) {
        return CORE_IGNORED;
    }
    building = entity_find_building(world, building_id);
    if (building == NULL) {
        return CORE_ERR_NOT_FOUND;
    }
    if (world->expansion && building->state == CORE_BSTATE_CONSTRUCTING)
        return CORE_IGNORED;
    if (world->expansion && damage > 0)
        expansion_raid_discover(world);
    /* 연구 "마법 결계": 건물이 받는 피해가 줄어듦 */
    if (world->expansion && expansion_has_research(world, 21) &&
        world->expansion->protected_cells[grid_index(&world->grid, building->x, building->y)]) {
        int cost = expansion_has_research(world, 22) ? 900 : 1000;
        if (expansion_bank_have(world, CORE_RES_MANA) >= cost) {
            (void)expansion_bank_add(world, CORE_RES_MANA, -cost, CORE_LEDGER_RAID);
            damage = (int32_t)((int64_t)damage * 85 / 100);
        }
    }
    damage = (int32_t)((int64_t)damage * (100 - world->research.ward_pct) / 100);
    if (world->expansion) {
        building->damage_kind = 1;
        building->damage_tick = world->clock.tick;
    }
    placement_damage(world, building, damage);
    if (building->type == CORE_BLD_KEEP && building->state == CORE_BSTATE_RUINED) {
        rank_lose(world, CORE_LOSE_KEEP); /* 성채가 무너지면 패배 (습격도 여기서 끝남) */
    }
    return CORE_OK;
}

int threat_report_villager_damage(World *world, int32_t raid_id, int32_t villager_id, int32_t damage) {
    Villager *villager;
    if (!current(world, raid_id)) {
        return CORE_IGNORED;
    }
    villager = entity_find_villager(world, villager_id);
    if (villager == NULL) {
        return CORE_ERR_NOT_FOUND;
    }
    if (world->expansion && villager->inside_building)
        return CORE_IGNORED;
    if (world->expansion && damage > 0 && villager->damage_kind != 2)
        expansion_raid_discover(world);
    if (world->expansion && villager->workplace && expansion_military(villager->workplace->type) &&
        expansion_has_research(world, 11))
        damage = (int32_t)((int64_t)damage * 90 / 100);
    if (world->expansion)
        damage = expansion_defend_damage(villager, damage);
    villager->health -= damage;
    if (world->expansion)
        expansion_hurt(world, villager, damage, raid_id);
    if (villager->health <= 0) {
        world->threat.raid.fallen++;
        population_kill(world, villager, CORE_DEATH_RAID);
    }
    return CORE_OK;
}

int threat_report_enemy_killed(World *world, int32_t raid_id, int32_t enemy_id) {
    Enemy *enemy;
    const EnemyDef *def;
    if (!current(world, raid_id)) {
        return CORE_IGNORED;
    }
    enemy = find_enemy(world, enemy_id);
    if (enemy == NULL) {
        return CORE_ERR_NOT_FOUND;
    }
    if (enemy->state != CORE_ENEMY_ALIVE) {
        return CORE_IGNORED;
    }
    def = defs_enemy(&world->defs, enemy->type);
    enemy->state = CORE_ENEMY_KILLED;
    world->threat.raid.killed++;
    world->stats.enemies_killed++;
    if (world->expansion) {
        if (!enemy->summoner_id)
            expansion_raider_drops(world, enemy->type);
        for (int i = 0; i < world->threat.raid.enemy_count; i++) {
            Enemy *child = &world->threat.raid.enemies[i];
            if (child->state == CORE_ENEMY_ALIVE && child->summoner_id == enemy_id) {
                child->state = CORE_ENEMY_ESCAPED;
                world->threat.raid.escaped++;
            }
        }
    } else if (def->mana_reward > 0) {
        (void)eco_change(&world->bank, &world->ledger, CORE_RES_MANA, def->mana_reward, CORE_LEDGER_RAID);
    }
    events_push_raid(&world->events, CORE_EVT_ENEMY_KILLED, enemy->id, enemy->type, def->mana_reward, 0);
    end_if_done(world);
    return CORE_OK;
}

int threat_report_loot(World *world, int32_t raid_id, int32_t enemy_id, int32_t building_id,
                       int32_t *out_amount) {
    Enemy *enemy;
    const EnemyDef *def;
    Building *building;
    int32_t amount;
    *out_amount = 0;
    if (!current(world, raid_id)) {
        return CORE_IGNORED;
    }
    enemy = find_enemy(world, enemy_id);
    building = entity_find_building(world, building_id);
    if (enemy == NULL || building == NULL) {
        return CORE_ERR_NOT_FOUND;
    }
    if (enemy->state != CORE_ENEMY_ALIVE || enemy->looted) {
        return CORE_IGNORED; /* 한 명이 두 번 훔치지는 못함 */
    }
    if ((building->type != CORE_BLD_KEEP && building->type != CORE_BLD_WAREHOUSE) ||
        building->state != CORE_BSTATE_ACTIVE) {
        return CORE_ERR_REJECTED; /* 저장소가 아님 */
    }
    def = defs_enemy(&world->defs, enemy->type);
    enemy->looted = 1;
    if (def->loot_resource < 0 || def->loot_amount <= 0) {
        return CORE_OK;
    }
    amount = def->loot_amount < world->bank.amount[def->loot_resource]
                 ? def->loot_amount
                 : world->bank.amount[def->loot_resource];
    if (amount > 0) {
        (void)eco_change(&world->bank, &world->ledger, def->loot_resource, -amount, CORE_LEDGER_RAID);
        world->threat.raid.stolen += amount;
        events_push_raid(&world->events, CORE_EVT_LOOT_STOLEN, def->loot_resource, amount, building->id, 0);
    }
    *out_amount = amount;
    return CORE_OK;
}

int threat_report_enemy_escaped(World *world, int32_t raid_id, int32_t enemy_id) {
    Enemy *enemy;
    if (!current(world, raid_id)) {
        return CORE_IGNORED;
    }
    enemy = find_enemy(world, enemy_id);
    if (enemy == NULL) {
        return CORE_ERR_NOT_FOUND;
    }
    if (enemy->state != CORE_ENEMY_ALIVE) {
        return CORE_IGNORED;
    }
    enemy->state = CORE_ENEMY_ESCAPED;
    world->threat.raid.escaped++;
    end_if_done(world);
    return CORE_OK;
}

int threat_report_raid_ended(World *world, int32_t raid_id) {
    if (!current(world, raid_id)) {
        return CORE_IGNORED;
    }
    end_raid(world);
    return CORE_OK;
}
