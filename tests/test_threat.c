/*
 * test_threat.c — 마일스톤 4: 위협 예고, 습격 시작(피신·경비), 보고 적용(피해·처치·약탈·달아남),
 *                 시간 초과, 무너진 성벽, 적 정의 파일, 저장
 */
#include <stdio.h>
#include <string.h>

#include "placement.h"
#include "platform.h"
#include "test_town.h"
#include "test_util.h"
#include "threat.h"

static int32_t count_events(CoreWorld *world, int32_t type)
{
    CoreEventView event;
    int32_t count = 0;
    while (core_poll_event(world, &event) == 1) {
        count += event.type == type;
    }
    return count;
}

/* 다음 위협이 도착할 때까지 (도착시키며) 진행합니다 */
static int run_until_raid(CoreWorld *world)
{
    int32_t guard = 0;
    while (world->threat.raid.id == 0 && guard++ < 40 * CLOCK_TICKS_PER_DAY) {
        if (guard % CLOCK_TICKS_PER_DAY == 1) {
            test_keep_alive(world); /* 습격을 기다리는 동안 굶어서 모두 떠나지 않게 */
        }
        if (!test_run(world, 1)) {
            return 0;
        }
    }
    return world->threat.raid.id != 0;
}

/* 첫 위협의 종류를 바꿔 둡니다 (시험용) */
static void force_next(CoreWorld *world, int32_t type, int32_t strength)
{
    world->threat.threats[0].type = type;
    world->threat.threats[0].strength = strength;
}

void test_threat_first_bandits_announced(void)
{
    CoreWorld *world = test_make_world(401, CORE_DIFF_NORMAL);
    CoreSummaryView summary;
    CoreThreatView threats[CORE_MAX_THREATS];
    CoreEventView event;
    int32_t count = 0;
    int announced = 0;
    CHECK(world != NULL);
    /* 새 게임에서 바로 예고: 5일째 13:00 에 작은 도적 무리 */
    while (core_poll_event(world, &event) == 1) {
        if (event.type == CORE_EVT_THREAT_SCHEDULED) {
            announced = 1;
            CHECK_EQ(event.a, CORE_THREAT_BANDITS);
            CHECK(event.c > 0);
            CHECK(event.d >= 1);
        }
    }
    CHECK(announced);
    CHECK_EQ(core_copy_threats(world, threats, CORE_MAX_THREATS, &count), CORE_OK);
    CHECK_EQ(count, 1);
    CHECK_EQ(threats[0].type, CORE_THREAT_BANDITS);
    CHECK_EQ(threats[0].eta_tick, (THREAT_FIRST_DAY - 1) * CLOCK_TICKS_PER_DAY + 13 * 6);
    CHECK(threats[0].direction >= 0 && threats[0].direction < CORE_DIR_COUNT);
    CHECK_EQ(core_get_summary(world, &summary), CORE_OK);
    CHECK_EQ(summary.threat_type, CORE_THREAT_BANDITS);
    CHECK_EQ(summary.threat_ticks_left, threats[0].eta_tick - world->clock.tick);
    CHECK_EQ(summary.raid_id, 0);
    CHECK_EQ(core_copy_threats(world, threats, 0, &count), CORE_ERR_BUFFER_TOO_SMALL);
    /* 도착 반나절 전에 경보 */
    while (world->clock.tick < threats[0].eta_tick - THREAT_NEAR_TICKS) {
        CHECK_EQ(core_tick(world, 1), CORE_OK);
    }
    CHECK_EQ(count_events(world, CORE_EVT_THREAT_NEAR), 1);
    CHECK_EQ(test_validate_verbose(world), 0);
    core_destroy(world);
}

void test_raid_start_flee_and_guards(void)
{
    CoreWorld *world = test_make_world(402, CORE_DIFF_NORMAL);
    Building *barracks;
    CoreRaidView raid;
    CoreEnemyView enemies[THREAT_MAX_ENEMIES];
    CoreSummaryView summary;
    int32_t count = 0;
    int32_t safety;
    int32_t i;
    CHECK(world != NULL);
    test_give(world, 500);
    test_flatten(world, world->keep_x + 4, world->keep_y - 4, 8, 8);
    barracks = test_build_now(world, CORE_BLD_BARRACKS, world->keep_x + 5, world->keep_y - 3);
    CHECK(barracks != NULL);
    CHECK(barracks->worker_count > 0);
    CHECK(run_until_raid(world));
    CHECK_EQ(count_events(world, CORE_EVT_RAID_STARTED), 1);

    CHECK_EQ(core_get_raid(world, &raid), CORE_OK);
    CHECK(raid.id > 0);
    CHECK_EQ(raid.type, CORE_THREAT_BANDITS);
    CHECK(raid.enemy_count >= 1);
    CHECK_EQ(raid.alive, raid.enemy_count);
    CHECK(raid.spawn_x == 0 || raid.spawn_y == 0 || raid.spawn_x == world->grid.width - 1 ||
          raid.spawn_y == world->grid.height - 1);
    CHECK_EQ(core_copy_enemies(world, enemies, THREAT_MAX_ENEMIES, &count), CORE_OK);
    CHECK_EQ(count, raid.enemy_count);
    for (i = 0; i < count; i++) {
        CHECK_EQ(enemies[i].id, i + 1);
        CHECK_EQ(enemies[i].state, CORE_ENEMY_ALIVE);
        CHECK(enemies[i].type == ENEMY_BANDIT || enemies[i].type == ENEMY_BRUTE);
    }

    /* 경비병은 병영 문 앞으로, 나머지는 성채로 */
    CHECK(test_run(world, 1));
    for (i = 0; i < world->villager_count; i++) {
        Villager *v = world->villagers[i];
        if (v->workplace == barracks) {
            CHECK_EQ(v->state, CORE_VS_GUARD_POST);
            CHECK(v->target == barracks);
        } else if (v->state != CORE_VS_LEAVING) {
            CHECK_EQ(v->state, CORE_VS_FLEE);
            CHECK(v->target == world_keep(world));
        }
    }
    CHECK_EQ(core_get_summary(world, &summary), CORE_OK);
    CHECK_EQ(summary.raid_id, raid.id);
    CHECK_EQ(summary.raid_enemies_left, raid.enemy_count);
    CHECK_EQ(summary.guards, barracks->worker_count);
    CHECK_EQ(summary.threat_type, CORE_THREAT_NONE); /* 다음 위협은 습격이 끝난 뒤에 예고 */

    /* 밖에 있는 경비병은 안전 욕구가 줄고, 습격 중에는 저장할 수 없음 */
    safety = barracks->workers[0]->needs.safety;
    CHECK(test_run(world, 3));
    CHECK(barracks->workers[0]->needs.safety < safety);
    CHECK_EQ(core_save(world, "raid.sav"), CORE_ERR_REJECTED);
    CHECK(!platform_file_exists("raid.sav"));
    CHECK_EQ(test_validate_verbose(world), 0);
    core_destroy(world);
}

void test_raid_reports_apply(void)
{
    CoreWorld *world = test_make_world(403, CORE_DIFF_NORMAL);
    Building *house;
    Building *keep;
    Villager *victim;
    int32_t raid_id;
    int32_t mana;
    int32_t gold;
    int32_t amount = 0;
    int32_t victim_id;
    int32_t population;
    int32_t i;
    CHECK(world != NULL);
    test_give(world, 500);
    test_flatten(world, world->keep_x + 4, world->keep_y - 4, 8, 8);
    house = test_build_now(world, CORE_BLD_HOUSE, world->keep_x + 5, world->keep_y - 3);
    CHECK(house != NULL);
    keep = world_keep(world);
    force_next(world, CORE_THREAT_MONSTERS, 4);
    CHECK(run_until_raid(world));
    raid_id = world->threat.raid.id;
    CHECK_EQ(world->threat.raid.enemy_count, 4);
    (void)count_events(world, CORE_EVT_NONE);

    /* 건물 피해: 집은 무너지고, 성채도 체력이 줄어듦 (0 이 되면 패배: keep_falls_defeat) */
    CHECK_EQ(core_report_building_damage(world, raid_id, house->id, 30), CORE_OK);
    CHECK_EQ(house->hp, 200 - 30);
    CHECK_EQ(core_report_building_damage(world, raid_id, house->id, 9999), CORE_OK);
    CHECK_EQ(house->state, CORE_BSTATE_RUINED);
    CHECK_EQ(core_report_building_damage(world, raid_id, keep->id, 500), CORE_OK);
    CHECK_EQ(keep->hp, world_building_def(world, CORE_BLD_KEEP)->max_hp - 500);
    CHECK_EQ(keep->state, CORE_BSTATE_ACTIVE);
    CHECK_EQ(core_report_building_damage(world, raid_id, 77777, 5), CORE_ERR_NOT_FOUND);
    CHECK_EQ(core_report_building_damage(world, raid_id, keep->id, -1), CORE_ERR_INVALID_ARG);

    /* 주민 피해: 쓰러지면 죽음(습격)과 슬픔 */
    victim = world->villagers[0];
    victim_id = victim->id;
    population = world->villager_count;
    CHECK_EQ(core_report_villager_damage(world, raid_id, victim_id, 30), CORE_OK);
    CHECK_EQ(victim->health, 100 - 30);
    CHECK_EQ(core_report_villager_damage(world, raid_id, victim_id, 500), CORE_OK);
    CHECK_EQ(world->villager_count, population - 1);
    CHECK(entity_find_villager(world, victim_id) == NULL);
    CHECK(world->mourning > 0);
    CHECK_EQ(world->threat.raid.fallen, 1);

    /* 처치: 마물은 마정석을 떨어뜨림 (장부: 약탈·전리품) */
    mana = world->bank.amount[CORE_RES_MANA];
    CHECK_EQ(core_report_enemy_killed(world, raid_id, 1), CORE_OK);
    CHECK_EQ(world->bank.amount[CORE_RES_MANA],
             mana + defs_enemy(&world->defs, world->threat.raid.enemies[0].type)->mana_reward);
    CHECK(world->ledger.today[CORE_RES_MANA][CORE_LEDGER_RAID] > 0);
    CHECK_EQ(core_report_enemy_killed(world, raid_id, 1), CORE_IGNORED); /* 이미 쓰러짐 */
    CHECK_EQ(core_report_enemy_killed(world, raid_id, 99), CORE_ERR_NOT_FOUND);

    /* 약탈: 저장소에서만, 한 명이 한 번만 */
    CHECK_EQ(core_report_loot(world, raid_id, 2, house->id, &amount), CORE_ERR_REJECTED);
    world->bank.amount[CORE_RES_FOOD] = 100;
    CHECK_EQ(core_report_loot(world, raid_id, 2, keep->id, &amount), CORE_OK);
    CHECK(amount > 0);
    CHECK_EQ(world->bank.amount[CORE_RES_FOOD], 100 - amount);
    CHECK_EQ(world->ledger.today[CORE_RES_FOOD][CORE_LEDGER_RAID], -amount);
    CHECK_EQ(core_report_loot(world, raid_id, 2, keep->id, &amount), CORE_IGNORED);
    CHECK_EQ(count_events(world, CORE_EVT_LOOT_STOLEN), 1);

    /* 늦게 온 보고(다른 습격 id)는 무시 */
    gold = world->bank.amount[CORE_RES_GOLD];
    CHECK_EQ(core_report_loot(world, raid_id + 5, 3, keep->id, &amount), CORE_IGNORED);
    CHECK_EQ(world->bank.amount[CORE_RES_GOLD], gold);

    /* 남은 적이 모두 달아나면 습격이 저절로 끝나고 다음 위협이 예고됨 */
    for (i = 2; i <= 4; i++) {
        CHECK_EQ(core_report_enemy_escaped(world, raid_id, i), CORE_OK);
    }
    CHECK_EQ(world->threat.raid.id, 0);
    CHECK_EQ(world->threat.threat_count, 1);
    CHECK_EQ(count_events(world, CORE_EVT_RAID_ENDED), 1);
    CHECK_EQ(core_report_enemy_killed(world, raid_id, 3), CORE_IGNORED);
    CHECK_EQ(core_report_raid_ended(world, raid_id), CORE_IGNORED);
    CHECK_EQ(world->threat.raids_done, 1);
    CHECK_EQ(test_validate_verbose(world), 0);
    core_destroy(world);
}

void test_raid_times_out(void)
{
    CoreWorld *world = test_make_world(404, CORE_DIFF_NORMAL);
    int32_t started;
    CHECK(world != NULL);
    CHECK(run_until_raid(world));
    started = world->clock.tick;
    /* 아무 보고도 없으면 하루 뒤 C 가 스스로 끝냄 (남은 적은 물러남) */
    while (world->threat.raid.id != 0) {
        CHECK(test_run(world, 1));
        CHECK(world->clock.tick - started <= THREAT_RAID_TIMEOUT);
    }
    CHECK_EQ(world->clock.tick - started, THREAT_RAID_TIMEOUT);
    CHECK_EQ(world->threat.raids_done, 1);
    /* 피신했던 주민은 다시 일상으로 */
    CHECK(test_run(world, 2));
    {
        int32_t i;
        for (i = 0; i < world->villager_count; i++) {
            CHECK(world->villagers[i]->state != CORE_VS_FLEE);
        }
    }
    CHECK_EQ(test_validate_verbose(world), 0);
    core_destroy(world);
}

void test_ruined_wall_is_passable(void)
{
    CoreWorld *world = test_make_world(405, CORE_DIFF_NORMAL);
    Building *wall = NULL;
    int32_t placed = 0;
    int32_t reason = 0;
    int32_t x;
    int32_t y;
    int32_t rev;
    int32_t i;
    CHECK(world != NULL);
    test_give(world, 500);
    x = world->keep_x + 6;
    y = world->keep_y + 6;
    test_flatten(world, x - 2, y - 2, 5, 5);
    CHECK_EQ(core_place_line(world, CORE_BLD_WALL, x, y, x, y, &placed, &reason), CORE_OK);
    CHECK_EQ(placed, 1);
    CHECK(test_run(world, 10));
    for (i = 0; i < world->building_count; i++) {
        if (world->buildings[i]->type == CORE_BLD_WALL) {
            wall = world->buildings[i];
        }
    }
    CHECK(wall != NULL && wall->state == CORE_BSTATE_ACTIVE);
    CHECK_EQ(world->grid.cost[grid_index(&world->grid, x, y)], CORE_COST_WALL);
    rev = world->grid.cost_rev;
    placement_damage(world, wall, 10000);
    CHECK_EQ(wall->state, CORE_BSTATE_RUINED);
    CHECK(world->grid.cost[grid_index(&world->grid, x, y)] != CORE_COST_WALL);
    CHECK(world->grid.cost[grid_index(&world->grid, x, y)] != CORE_COST_BLOCKED);
    CHECK(world->grid.cost_rev != rev);
    CHECK_EQ(core_repair(world, wall->id, &reason), CORE_OK);
    CHECK_EQ(world->grid.cost[grid_index(&world->grid, x, y)], CORE_COST_WALL);
    CHECK_EQ(test_validate_verbose(world), 0);
    core_destroy(world);
}

/* 성채에 피신한 주민은 배가 고프면 성채 창고에서 먹음 (습격이 하루 내내 이어져도 굶지 않게) */
void test_sheltered_villagers_eat(void)
{
    CoreWorld *world = test_make_world(407, CORE_DIFF_NORMAL);
    int32_t food;
    int32_t sheltered = 0;
    int32_t i;
    CHECK(world != NULL);
    test_give(world, 200);
    CHECK(run_until_raid(world));
    CHECK(test_run(world, 1));
    test_arrive_all(world);
    for (i = 0; i < world->villager_count; i++) {
        Villager *v = world->villagers[i];
        if (v->state == CORE_VS_FLEE && v->at_target) {
            v->needs.hunger = 5;
            sheltered++;
        }
    }
    CHECK(sheltered > 0);
    food = world->bank.amount[CORE_RES_FOOD];
    CHECK(test_run(world, 1));
    CHECK(world->bank.amount[CORE_RES_FOOD] < food);
    for (i = 0; i < world->villager_count; i++) {
        Villager *v = world->villagers[i];
        if (v->state == CORE_VS_FLEE && v->at_target) {
            CHECK(v->needs.hunger > 5);
        }
    }
    CHECK_EQ(test_validate_verbose(world), 0);
    core_destroy(world);
}

/* 습격 중에는 고칠 수 없음 (맞는 동안 고치면 성벽이 무너지지 않으므로). 끝나면 다시 고칠 수 있음 */
void test_no_repair_during_raid(void)
{
    CoreWorld *world = test_make_world(406, CORE_DIFF_NORMAL);
    Building *keep;
    int32_t reason = -1;
    CHECK(world != NULL);
    test_give(world, 500);
    keep = entity_find_building(world, world->keep_id);
    CHECK(keep != NULL);
    CHECK(run_until_raid(world));
    keep->hp -= 50;
    CHECK_EQ(core_repair(world, keep->id, &reason), CORE_ERR_REJECTED);
    CHECK_EQ(reason, CORE_DENY_RAID);
    CHECK_EQ(core_report_raid_ended(world, world->threat.raid.id), CORE_OK);
    CHECK_EQ(core_repair(world, keep->id, &reason), CORE_OK);
    CHECK_EQ(reason, CORE_DENY_NONE);
    CHECK_EQ(test_validate_verbose(world), 0);
    core_destroy(world);
}

void test_enemy_defs_and_composition(void)
{
    static const char GOOD[] = "[enemy goblin]\nhp = 55\nmana_reward = 3\n";
    static const char BAD[] = "[enemy goblin]\nhp = 55\nbehavior = dancer\n";
    static const char UNKNOWN[] = "[enemy dragon]\nhp = 9000\n";
    CoreDefs *defs = NULL;
    CoreWorld *world = NULL;
    CoreEnemyDefView view;
    CoreNewGameParams params;
    int32_t line = 0;
    int32_t ogres = 0;
    int32_t archers = 0;
    int32_t i;
    CHECK_EQ(core_defs_create(&defs), CORE_OK);
    CHECK_EQ(core_defs_load_text(defs, CORE_DATA_ENEMIES, GOOD, (int32_t)strlen(GOOD), &line), CORE_OK);
    CHECK_EQ(core_defs_load_text(defs, CORE_DATA_ENEMIES, BAD, (int32_t)strlen(BAD), &line), CORE_ERR_FORMAT);
    CHECK_EQ(line, 3);
    CHECK_EQ(core_defs_load_text(defs, CORE_DATA_ENEMIES, UNKNOWN, (int32_t)strlen(UNKNOWN), &line), CORE_ERR_FORMAT);
    memset(&params, 0, sizeof(params));
    strcpy(params.castle_name, "Test");
    params.seed = 406;
    params.difficulty = CORE_DIFF_NORMAL;
    CHECK_EQ(core_create(&params, defs, &world), CORE_OK);
    core_defs_destroy(defs);
    CHECK_EQ(core_get_enemy_def(world, ENEMY_GOBLIN, &view), CORE_OK);
    CHECK_EQ(view.hp, 55); /* 좋은 파일은 적용, 틀린 파일은 통째로 버림 */
    CHECK_EQ(view.mana_reward, 3);
    CHECK_EQ(view.behavior, CORE_BEHAVIOR_MONSTER);
    CHECK_EQ(strcmp(view.key, "goblin"), 0);
    CHECK_EQ(core_get_enemy_def(world, DEFS_ENEMY_COUNT, &view), CORE_ERR_NOT_FOUND);
    CHECK_EQ(count_events(world, CORE_EVT_DEF_WARNING), 2);

    /* 구성: 스무 날 뒤 마물 무리에는 오우거, 군대에는 궁수 셋에 하나 */
    world->clock.tick = 30 * CLOCK_TICKS_PER_DAY;
    world->threat.threats[0].eta_tick = world->clock.tick + 1;
    force_next(world, CORE_THREAT_MONSTERS, 12);
    CHECK_EQ(core_tick(world, 1), CORE_OK);
    CHECK_EQ(world->threat.raid.enemy_count, 12);
    for (i = 0; i < world->threat.raid.enemy_count; i++) {
        ogres += world->threat.raid.enemies[i].type == ENEMY_OGRE;
    }
    CHECK_EQ(ogres, 2);
    CHECK_EQ(core_report_raid_ended(world, world->threat.raid.id), CORE_OK);
    world->threat.threats[0].eta_tick = world->clock.tick + 1;
    force_next(world, CORE_THREAT_ARMY, 9);
    CHECK_EQ(core_tick(world, 1), CORE_OK);
    for (i = 0; i < world->threat.raid.enemy_count; i++) {
        archers += world->threat.raid.enemies[i].type == ENEMY_ARCHER;
    }
    CHECK_EQ(archers, 3);
    CHECK_EQ(test_validate_verbose(world), 0);
    core_destroy(world);
}

void test_threat_save_roundtrip(void)
{
    const char *path = "threat.sav";
    CoreWorld *world = test_make_world(407, CORE_DIFF_HARSH);
    CoreWorld *loaded = NULL;
    uint64_t a = 0;
    uint64_t b = 1;
    CHECK(world != NULL);
    /* 한 번 습격을 치르고, 다음 위협이 예고된 상태로 저장 */
    CHECK(run_until_raid(world));
    CHECK_EQ(core_report_raid_ended(world, world->threat.raid.id), CORE_OK);
    CHECK(test_run(world, 20));
    CHECK_EQ(world->threat.threat_count, 1);
    CHECK_EQ(core_save(world, path), CORE_OK);
    CHECK_EQ(core_load(path, NULL, &loaded, NULL), CORE_OK);
    (void)core_state_hash(world, &a);
    (void)core_state_hash(loaded, &b);
    CHECK(a == b);
    CHECK_EQ(loaded->threat.threat_count, 1);
    CHECK_EQ(loaded->threat.threats[0].eta_tick, world->threat.threats[0].eta_tick);
    CHECK_EQ(loaded->threat.next_raid_id, world->threat.next_raid_id);
    CHECK_EQ(loaded->threat.raids_done, 1);
    /* 같은 미래: 다음 습격도 같은 때 같은 구성으로 */
    CHECK(run_until_raid(world));
    CHECK(run_until_raid(loaded));
    CHECK_EQ(world->clock.tick, loaded->clock.tick);
    CHECK_EQ(world->threat.raid.enemy_count, loaded->threat.raid.enemy_count);
    CHECK_EQ(test_validate_verbose(loaded), 0);
    core_destroy(world);
    core_destroy(loaded);
    (void)platform_remove_file(path);
    (void)platform_remove_file("threat.sav.bak");
}
