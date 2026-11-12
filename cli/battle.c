/*
 * battle.c — CLI 의 숫자 전투 (설명은 battle.h)
 */
#include "battle.h"

#include <stdlib.h>
#include <string.h>

#define BATTLE_MAX_ROUNDS 12
#define BATTLE_LOOT_ROUND 5
#define BATTLE_ENEMY_SPEED 2000 /* 걸어오는 시간 계산용 (한 틱에 2칸) */

/* 이 습격 전용 작은 난수 (xorshift). 월드의 난수를 쓰면 CLI 가 게임 결과를 바꾸게 되므로 따로 둡니다 */
static uint32_t next_random(uint32_t *state)
{
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

void battle_init(Battle *battle)
{
    memset(battle, 0, sizeof(*battle));
}

/* 주민·건물·적 뷰를 한꺼번에 복사해 둡니다 (한 판 동안 씀) */
typedef struct Snapshot {
    CoreSummaryView summary;
    CoreBuildingView *buildings;
    int32_t building_count;
    CoreVillagerView *villagers;
    int32_t villager_count;
    CoreEnemyView *enemies;
    int32_t enemy_count;
} Snapshot;

static void snapshot_free(Snapshot *snap)
{
    free(snap->buildings);
    free(snap->villagers);
    free(snap->enemies);
}

static int snapshot_take(Snapshot *snap, CoreWorld *world, int32_t enemy_capacity)
{
    memset(snap, 0, sizeof(*snap));
    if (core_get_summary(world, &snap->summary) != CORE_OK) {
        return 0;
    }
    snap->buildings = (CoreBuildingView *)malloc((size_t)(snap->summary.building_count_hint + 1) * sizeof(CoreBuildingView));
    snap->villagers = (CoreVillagerView *)malloc((size_t)(snap->summary.villager_count_hint + 1) * sizeof(CoreVillagerView));
    snap->enemies = (CoreEnemyView *)malloc((size_t)(enemy_capacity + 1) * sizeof(CoreEnemyView));
    if (snap->buildings == NULL || snap->villagers == NULL || snap->enemies == NULL ||
        core_copy_buildings(world, snap->buildings, snap->summary.building_count_hint + 1, &snap->building_count) !=
            CORE_OK ||
        core_copy_villagers(world, snap->villagers, snap->summary.villager_count_hint + 1, &snap->villager_count) !=
            CORE_OK ||
        core_copy_enemies(world, snap->enemies, enemy_capacity + 1, &snap->enemy_count) != CORE_OK) {
        snapshot_free(snap);
        return 0;
    }
    return 1;
}

/* 한 판(10분)의 공격량: 피해 × 10분 ÷ 공격 사이 */
static int32_t per_round(int32_t damage, int32_t cooldown)
{
    return cooldown > 0 ? damage * 10 / cooldown : 0;
}

/* 감시탑 + 성채 망루 + 경비병의 한 판 피해 */
static int32_t defense_power(CoreWorld *world, const Snapshot *snap)
{
    CoreBuildingDefView def;
    int32_t total = 0;
    int32_t i;
    for (i = 0; i < snap->building_count; i++) {
        const CoreBuildingView *b = &snap->buildings[i];
        if (b->state != CORE_BSTATE_ACTIVE || (b->type != CORE_BLD_TOWER && b->type != CORE_BLD_KEEP)) {
            continue;
        }
        if (core_get_building_def(world, b->type, &def) == CORE_OK) {
            total += per_round(def.attack, def.attack_cooldown);
        }
    }
    if (core_get_building_def(world, CORE_BLD_BARRACKS, &def) == CORE_OK) {
        total += snap->summary.guards * per_round(def.attack, def.attack_cooldown);
    }
    return total;
}

static int32_t pick_wall(const Snapshot *snap, uint32_t *random)
{
    int32_t walls = 0;
    int32_t i;
    int32_t pick;
    for (i = 0; i < snap->building_count; i++) {
        walls += snap->buildings[i].type == CORE_BLD_WALL && snap->buildings[i].state == CORE_BSTATE_ACTIVE;
    }
    if (walls == 0) {
        return 0;
    }
    pick = (int32_t)(next_random(random) % (uint32_t)walls);
    for (i = 0; i < snap->building_count; i++) {
        if (snap->buildings[i].type == CORE_BLD_WALL && snap->buildings[i].state == CORE_BSTATE_ACTIVE && pick-- == 0) {
            return snap->buildings[i].id;
        }
    }
    return 0;
}

static int wall_standing(const Snapshot *snap, int32_t wall_id)
{
    int32_t i;
    for (i = 0; i < snap->building_count; i++) {
        if (snap->buildings[i].id == wall_id) {
            return snap->buildings[i].state == CORE_BSTATE_ACTIVE;
        }
    }
    return 0;
}

/* 방어 쪽이 쏜 만큼 앞에서부터 적을 쓰러뜨립니다 */
static void defend(Battle *battle, CoreWorld *world, const Snapshot *snap)
{
    int32_t i;
    battle->pool += defense_power(world, snap);
    for (i = 0; i < snap->enemy_count && battle->pool > 0; i++) {
        CoreEnemyDefView def;
        const CoreEnemyView *enemy = &snap->enemies[i];
        int32_t hp;
        if (enemy->state != CORE_ENEMY_ALIVE || core_get_enemy_def(world, enemy->type, &def) != CORE_OK) {
            continue;
        }
        hp = battle->enemy_hp > 0 ? battle->enemy_hp : def.hp;
        if (battle->pool >= hp) {
            battle->pool -= hp;
            battle->enemy_hp = 0;
            (void)core_report_enemy_killed(world, battle->raid_id, enemy->id);
        } else {
            battle->enemy_hp = hp - battle->pool; /* 맨 앞 적이 다친 채로 다음 판 */
            battle->pool = 0;
        }
    }
}

/* 살아 있는 적이 성벽, 경비병, 건물에 피해를 줍니다 */
static void attack(Battle *battle, CoreWorld *world, const Snapshot *snap)
{
    int32_t damage = 0;
    int32_t candidates = 0;
    int32_t i;
    for (i = 0; i < snap->enemy_count; i++) {
        CoreEnemyDefView def;
        if (snap->enemies[i].state == CORE_ENEMY_ALIVE &&
            core_get_enemy_def(world, snap->enemies[i].type, &def) == CORE_OK) {
            damage += per_round(def.damage, def.cooldown);
        }
    }
    if (damage <= 0) {
        return;
    }
    if (battle->wall_id != 0 && wall_standing(snap, battle->wall_id)) {
        (void)core_report_building_damage(world, battle->raid_id, battle->wall_id, damage);
        return; /* 성벽을 부수는 동안에는 안으로 못 들어옴 */
    }
    for (i = 0; i < snap->villager_count; i++) {
        if (snap->villagers[i].state == CORE_VS_GUARD_POST) {
            (void)core_report_villager_damage(world, battle->raid_id, snap->villagers[i].id, damage / 3);
            damage -= damage / 3;
            break;
        }
    }
    for (i = 0; i < snap->building_count; i++) {
        candidates += snap->buildings[i].type != CORE_BLD_WALL && snap->buildings[i].state != CORE_BSTATE_RUINED;
    }
    if (candidates > 0) {
        int32_t pick = (int32_t)(next_random(&battle->random) % (uint32_t)candidates);
        for (i = 0; i < snap->building_count; i++) {
            const CoreBuildingView *b = &snap->buildings[i];
            if (b->type != CORE_BLD_WALL && b->state != CORE_BSTATE_RUINED && pick-- == 0) {
                (void)core_report_building_damage(world, battle->raid_id, b->id, damage);
                break;
            }
        }
    }
}

/* 도적·마물: 살아남은 자는 성채에서 훔치고 달아남 */
static void loot(Battle *battle, CoreWorld *world, const Snapshot *snap)
{
    int32_t keep = 0;
    int32_t i;
    for (i = 0; i < snap->building_count; i++) {
        if (snap->buildings[i].type == CORE_BLD_KEEP) {
            keep = snap->buildings[i].id;
        }
    }
    for (i = 0; i < snap->enemy_count; i++) {
        CoreEnemyDefView def;
        int32_t amount;
        const CoreEnemyView *enemy = &snap->enemies[i];
        if (enemy->state != CORE_ENEMY_ALIVE || core_get_enemy_def(world, enemy->type, &def) != CORE_OK ||
            def.behavior == CORE_BEHAVIOR_RAIDER) {
            continue;
        }
        (void)core_report_loot(world, battle->raid_id, enemy->id, keep, &amount);
        (void)core_report_enemy_escaped(world, battle->raid_id, enemy->id);
    }
    battle->enemy_hp = 0; /* 맨 앞 적이 달아났을 수 있음 */
}

int32_t battle_tick(Battle *battle, CoreWorld *world)
{
    CoreRaidView raid;
    CoreSummaryView summary;
    Snapshot snap;
    int32_t rc = core_get_raid(world, &raid);
    if (rc != CORE_OK) {
        return rc;
    }
    if (raid.id == 0) {
        battle->raid_id = 0;
        return CORE_OK;
    }
    (void)core_get_summary(world, &summary);
    if (raid.id != battle->raid_id) {
        /* 새 습격: 적이 가장자리에서 성채까지 걸어오는 시간 */
        int32_t distance = abs(raid.spawn_x - summary.keep_x) + abs(raid.spawn_y - summary.keep_y);
        battle_init(battle);
        battle->raid_id = raid.id;
        battle->start_tick = summary.tick + distance * 1000 / BATTLE_ENEMY_SPEED;
        battle->random = (uint32_t)summary.seed * 2654435761u + (uint32_t)raid.id * 40503u + 1u;
        return CORE_OK;
    }
    if (summary.tick < battle->start_tick) {
        return CORE_OK;
    }
    if (!snapshot_take(&snap, world, raid.enemy_count)) {
        return CORE_ERR_OUT_OF_MEMORY;
    }
    if (battle->round == 0) {
        battle->wall_id = pick_wall(&snap, &battle->random);
    }
    battle->round++;
    defend(battle, world, &snap);
    attack(battle, world, &snap);
    snapshot_free(&snap);
    if (!battle->looted && battle->round >= BATTLE_LOOT_ROUND && snapshot_take(&snap, world, raid.enemy_count)) {
        if (battle->wall_id == 0 || !wall_standing(&snap, battle->wall_id)) {
            loot(battle, world, &snap);
            battle->looted = 1;
        }
        snapshot_free(&snap);
    }
    if (battle->round >= BATTLE_MAX_ROUNDS) {
        (void)core_report_raid_ended(world, battle->raid_id);
    }
    return CORE_OK;
}
