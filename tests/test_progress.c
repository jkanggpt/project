/*
 * test_progress.c — 마일스톤 5: 연구(마탑, 효과 표, 저장), 등급(조건, 공물, 잠금 해제), 승패, 튜토리얼, 난이도
 */
#include <stdio.h>
#include <string.h>

#include "placement.h"
#include "platform.h"
#include "rank.h"
#include "research.h"
#include "test_town.h"
#include "test_util.h"
#include "threat.h"
#include "tutorial.h"

static int32_t count_events(CoreWorld *world, int32_t type)
{
    CoreEventView event;
    int32_t count = 0;
    while (core_poll_event(world, &event) == 1) {
        count += event.type == type;
    }
    return count;
}

/* 마법사가 일하도록 시간을 진행해 지금 연구가 끝날 때까지 (최대 days 일) */
static int run_until_research_done(CoreWorld *world, int32_t days)
{
    int32_t tick;
    for (tick = 0; tick < days * CLOCK_TICKS_PER_DAY && world->research.active >= 0; tick++) {
        if (tick % CLOCK_TICKS_PER_DAY == 0) {
            test_keep_alive(world);
        }
        if (!test_run(world, 1)) {
            return 0;
        }
    }
    return world->research.active < 0;
}

static Building *build_mage_tower(CoreWorld *world)
{
    test_flatten(world, world->keep_x + 4, world->keep_y - 4, 8, 8);
    return test_build_now(world, CORE_BLD_MAGE_TOWER, world->keep_x + 5, world->keep_y - 3);
}

void test_research_flow(void)
{
    CoreWorld *world = test_make_world(501, CORE_DIFF_NORMAL);
    Building *tower;
    CoreResearchView view;
    int32_t reason = -1;
    int32_t gold;
    int32_t mana;
    int32_t farm_rate;
    CHECK(world != NULL);
    test_give(world, 1000);
    farm_rate = world_building_def(world, CORE_BLD_FARM)->rate;

    /* 등급이 모자라면 잠김 (윤작은 요새부터) */
    CHECK_EQ(core_get_research(world, 0, &view), CORE_OK);
    CHECK_EQ(view.state, CORE_RSTATE_LOCKED);
    CHECK_EQ(core_start_research(world, 0, &reason), CORE_ERR_REJECTED);
    CHECK_EQ(reason, CORE_DENY_RESEARCH_LOCKED);
    CHECK_EQ(core_start_research(world, 999, &reason), CORE_ERR_NOT_FOUND);
    CHECK_EQ(core_get_research(world, world->defs.research_count, &view), CORE_ERR_NOT_FOUND);

    tower = build_mage_tower(world); /* 마탑은 요새부터: 도우미가 등급을 올려 둠 */
    CHECK(tower != NULL);
    CHECK_EQ(world->rank, CORE_RANK_FORTRESS);
    CHECK_EQ(tower->worker_count, 2);
    CHECK_EQ(core_get_research(world, 0, &view), CORE_OK);
    CHECK_EQ(view.state, CORE_RSTATE_AVAILABLE);
    /* 앞선 연구가 끝나지 않으면 잠김 */
    CHECK_EQ(core_get_research(world, 1, &view), CORE_OK);
    CHECK_EQ(view.requires, 0);
    CHECK_EQ(view.state, CORE_RSTATE_LOCKED);

    /* 금화가 모자라면 거절, 있으면 비용을 내고 시작 (장부: 연구) */
    world->bank.amount[CORE_RES_GOLD] = 1;
    CHECK_EQ(core_start_research(world, 0, &reason), CORE_ERR_REJECTED);
    CHECK_EQ(reason, CORE_DENY_NO_GOLD);
    world->bank.amount[CORE_RES_GOLD] = 500;
    world->bank.amount[CORE_RES_MANA] = 0;
    CHECK_EQ(core_start_research(world, 0, &reason), CORE_ERR_REJECTED);
    CHECK_EQ(reason, CORE_DENY_NO_MANA);
    world->bank.amount[CORE_RES_MANA] = 100;
    gold = world->bank.amount[CORE_RES_GOLD];
    mana = world->bank.amount[CORE_RES_MANA];
    (void)count_events(world, CORE_EVT_NONE);
    CHECK_EQ(core_start_research(world, 0, &reason), CORE_OK);
    CHECK_EQ(world->bank.amount[CORE_RES_GOLD], gold - world->defs.research[0].cost_gold);
    CHECK_EQ(world->bank.amount[CORE_RES_MANA], mana - world->defs.research[0].cost_mana);
    CHECK_EQ(world->ledger.today[CORE_RES_GOLD][CORE_LEDGER_RESEARCH], -world->defs.research[0].cost_gold);
    CHECK_EQ(count_events(world, CORE_EVT_RESEARCH_STARTED), 1);
    CHECK_EQ(core_start_research(world, 4, &reason), CORE_ERR_REJECTED);
    CHECK_EQ(reason, CORE_DENY_RESEARCH_BUSY);

    /* 마법사가 일하면 점수가 모이고, 다 차면 끝나고 효과(농장 생산률)가 적용됨 */
    CHECK(run_until_research_done(world, 10));
    CHECK_EQ(world->research.done_count, 1);
    CHECK_EQ(count_events(world, CORE_EVT_RESEARCH_DONE), 1);
    CHECK_EQ(world_building_def(world, CORE_BLD_FARM)->rate,
             farm_rate * (100 + world->defs.research[0].value) / 100);
    CHECK_EQ(core_get_research(world, 0, &view), CORE_OK);
    CHECK_EQ(view.state, CORE_RSTATE_DONE);
    CHECK_EQ(core_start_research(world, 0, &reason), CORE_ERR_REJECTED);
    CHECK_EQ(reason, CORE_DENY_RESEARCH_DONE);
    CHECK_EQ(core_get_research(world, 1, &view), CORE_OK);
    CHECK_EQ(view.state, CORE_RSTATE_AVAILABLE);
    CHECK_EQ(test_validate_verbose(world), 0);
    core_destroy(world);
}

/* 효과는 건물 정의를 고치고(감시탑·성벽), 성벽의 체력은 새 최대 체력에 맞춰 늘어남. 불러와도 같은 값 */
void test_research_effects_and_save(void)
{
    const char *path = "research.sav";
    CoreWorld *world = test_make_world(502, CORE_DIFF_NORMAL);
    CoreWorld *loaded = NULL;
    CoreBuildingDefView tower_def;
    Building *wall;
    int32_t reason = -1;
    int32_t placed = 0;
    int32_t wall_hp;
    int32_t line = 0;
    uint64_t a = 0;
    uint64_t b = 0;
    int32_t i;
    CHECK(world != NULL);
    test_give(world, 1000);
    CHECK(build_mage_tower(world) != NULL);
    test_flatten(world, world->keep_x - 8, world->keep_y + 4, 3, 3);
    CHECK_EQ(core_place_line(world, CORE_BLD_WALL, world->keep_x - 7, world->keep_y + 5, world->keep_x - 7,
                             world->keep_y + 5, &placed, &reason),
             CORE_OK);
    CHECK_EQ(placed, 1);
    CHECK(test_run(world, 12));
    wall = NULL;
    for (i = 0; i < world->building_count; i++) {
        if (world->buildings[i]->type == CORE_BLD_WALL) {
            wall = world->buildings[i];
        }
    }
    CHECK(wall != NULL && wall->state == CORE_BSTATE_ACTIVE);
    wall_hp = wall->hp;

    /* 날카로운 화살 → 돌 성벽 (방어 갈래) */
    CHECK_EQ(core_start_research(world, 4, &reason), CORE_OK);
    CHECK(run_until_research_done(world, 10));
    CHECK_EQ(core_get_building_def(world, CORE_BLD_TOWER, &tower_def), CORE_OK);
    CHECK_EQ(tower_def.attack, world->base_buildings[CORE_BLD_TOWER].attack + world->defs.research[4].value);
    test_give(world, 1000);
    CHECK_EQ(core_start_research(world, 5, &reason), CORE_OK);
    CHECK(run_until_research_done(world, 10));
    CHECK_EQ(world_building_def(world, CORE_BLD_WALL)->max_hp,
             world->base_buildings[CORE_BLD_WALL].max_hp * (100 + world->defs.research[5].value) / 100);
    CHECK_EQ(wall->hp, wall_hp * (100 + world->defs.research[5].value) / 100);

    /* 하나 더 시작해 둔 채로 저장 → 불러오기: 해시, 효과, 연구 중인 것까지 같음 */
    test_give(world, 1000);
    CHECK_EQ(core_start_research(world, 8, &reason), CORE_OK);
    CHECK(test_run(world, 30));
    test_keep_alive(world);
    (void)platform_remove_file(path);
    CHECK_EQ(core_save(world, path), CORE_OK);
    CHECK_EQ(core_load(path, NULL, &loaded, &line), CORE_OK);
    (void)core_state_hash(world, &a);
    (void)core_state_hash(loaded, &b);
    CHECK(a == b);
    CHECK_EQ(loaded->research.active, 8);
    CHECK_EQ(loaded->research.progress_milli, world->research.progress_milli);
    CHECK(memcmp(loaded->defs.buildings, world->defs.buildings, sizeof(world->defs.buildings)) == 0);
    CHECK_EQ(test_validate_verbose(loaded), 0);
    core_destroy(world);
    core_destroy(loaded);
    (void)platform_remove_file(path);
    {
        char bak[64];
        snprintf(bak, sizeof(bak), "%s.bak", path);
        (void)platform_remove_file(bak);
    }
}

void test_rank_up_rules(void)
{
    CoreWorld *world = test_make_world(503, CORE_DIFF_NORMAL);
    CoreRankView rank;
    CorePlacementView place;
    int32_t reason = -1;
    int32_t capacity;
    int32_t gold;
    CHECK(world != NULL);
    CHECK_EQ(core_get_rank(world, &rank), CORE_OK);
    CHECK_EQ(rank.rank, CORE_RANK_HAMLET);
    CHECK_EQ(rank.next_rank, CORE_RANK_VILLAGE);
    CHECK(rank.unlocks & (1 << CORE_BLD_BARRACKS));
    CHECK(!rank.ready);
    /* 마을이 되기 전에는 병영을 지을 수 없음 */
    core_can_place(world, CORE_BLD_BARRACKS, world->keep_x + 6, world->keep_y + 6, &place);
    CHECK_EQ(place.result, CORE_PLACE_LOCKED);
    CHECK_EQ(core_rank_up(world, &reason), CORE_ERR_REJECTED);
    CHECK_EQ(reason, CORE_DENY_REQUIREMENTS);

    /* 조건을 채우면(여기서는 정의를 낮춤) 공물이 모자랄 때만 거절 */
    world->defs.ranks[CORE_RANK_VILLAGE].population = 1;
    world->defs.ranks[CORE_RANK_VILLAGE].mood = 0;
    world->defs.ranks[CORE_RANK_VILLAGE].defense = 0;
    world->bank.amount[CORE_RES_GOLD] = 0;
    CHECK_EQ(core_rank_up(world, &reason), CORE_ERR_REJECTED);
    CHECK_EQ(reason, CORE_DENY_NO_GOLD);
    world->bank.amount[CORE_RES_GOLD] = 500;
    world->bank.amount[CORE_RES_MANA] = 0;
    CHECK_EQ(core_rank_up(world, &reason), CORE_ERR_REJECTED);
    CHECK_EQ(reason, CORE_DENY_NO_MANA);
    world->bank.amount[CORE_RES_MANA] = 100;
    CHECK(rank_is_ready(world));
    /* 한 시간 지나면 "승급할 수 있습니다" 알림 (한 번만) */
    (void)count_events(world, CORE_EVT_NONE);
    CHECK(test_run(world, 12));
    CHECK_EQ(count_events(world, CORE_EVT_RANK_READY), 1);
    CHECK(test_run(world, 12));
    CHECK_EQ(count_events(world, CORE_EVT_RANK_READY), 0);

    capacity = world->bank.capacity;
    gold = world->bank.amount[CORE_RES_GOLD];
    CHECK_EQ(core_get_rank(world, &rank), CORE_OK);
    CHECK_EQ(core_rank_up(world, &reason), CORE_OK);
    CHECK_EQ(world->rank, CORE_RANK_VILLAGE);
    CHECK_EQ(world->bank.amount[CORE_RES_GOLD], gold - rank.tribute_gold);
    CHECK_EQ(world->ledger.today[CORE_RES_GOLD][CORE_LEDGER_TRIBUTE], -rank.tribute_gold);
    CHECK_EQ(world->bank.capacity, capacity + world->defs.ranks[CORE_RANK_VILLAGE].storage);
    CHECK_EQ(count_events(world, CORE_EVT_RANK_UP), 1);
    core_can_place(world, CORE_BLD_BARRACKS, world->keep_x + 6, world->keep_y + 6, &place);
    CHECK(place.result != CORE_PLACE_LOCKED);

    /* 습격 중에는 오르지 않음 */
    world->defs.ranks[CORE_RANK_FORTRESS].population = 0;
    world->defs.ranks[CORE_RANK_FORTRESS].mood = 0;
    world->defs.ranks[CORE_RANK_FORTRESS].defense = 0;
    world->defs.ranks[CORE_RANK_FORTRESS].research = 0;
    world->threat.raid.id = 99; /* 습격 중인 척 (적이 없으니 다음 틱에 끝남) */
    CHECK_EQ(core_rank_up(world, &reason), CORE_ERR_REJECTED);
    CHECK_EQ(reason, CORE_DENY_RAID);
    world->threat.raid.id = 0;
    CHECK_EQ(test_validate_verbose(world), 0);
    core_destroy(world);
}

void test_victory_at_royal_castle(void)
{
    CoreWorld *world = test_make_world(504, CORE_DIFF_NORMAL);
    CoreRunStatsView stats;
    int32_t reason = -1;
    int32_t rank;
    CHECK(world != NULL);
    for (rank = 1; rank < CORE_RANK_COUNT; rank++) {
        RankDef *def = &world->defs.ranks[rank];
        def->population = 0;
        def->mood = 0;
        def->defense = 0;
        def->research = 0;
    }
    test_give(world, 2000);
    (void)count_events(world, CORE_EVT_NONE);
    for (rank = 1; rank < CORE_RANK_COUNT; rank++) {
        CHECK_EQ(core_rank_up(world, &reason), CORE_OK);
        world->bank.amount[CORE_RES_GOLD] = 2000;
        world->bank.amount[CORE_RES_MANA] = 2000;
    }
    CHECK_EQ(world->rank, CORE_RANK_ROYAL);
    CHECK_EQ(world->outcome, CORE_OUTCOME_WON);
    CHECK_EQ(count_events(world, CORE_EVT_VICTORY), 1);
    CHECK_EQ(core_rank_up(world, &reason), CORE_ERR_REJECTED);
    CHECK_EQ(reason, CORE_DENY_MAX_RANK);
    CHECK_EQ(core_get_run_stats(world, &stats), CORE_OK);
    CHECK_EQ(stats.outcome, CORE_OUTCOME_WON);
    CHECK_EQ(stats.victory_day, 1);
    CHECK(stats.peak_population >= 5);
    /* 이긴 뒤에도 계속할 수 있음 */
    CHECK(test_run(world, 10));
    CHECK_EQ(test_validate_verbose(world), 0);
    core_destroy(world);
}

void test_keep_falls_defeat(void)
{
    const char *path = "defeat.sav";
    CoreWorld *world = test_make_world(505, CORE_DIFF_NORMAL);
    CoreWorld *loaded = NULL;
    CoreSummaryView summary;
    int32_t reason = -1;
    int32_t raid_id;
    int32_t line = 0;
    CHECK(world != NULL);
    test_give(world, 500);
    while (world->threat.raid.id == 0) {
        test_keep_alive(world);
        CHECK(test_run(world, 1));
    }
    raid_id = world->threat.raid.id;
    (void)count_events(world, CORE_EVT_NONE);
    CHECK_EQ(core_report_building_damage(world, raid_id, world->keep_id, 99999), CORE_OK);
    CHECK_EQ(world_keep(world)->state, CORE_BSTATE_RUINED);
    CHECK_EQ(world->outcome, CORE_OUTCOME_LOST);
    CHECK_EQ(world->lose_reason, CORE_LOSE_KEEP);
    CHECK_EQ(world->threat.raid.id, 0); /* 습격도 끝남 */
    CHECK_EQ(count_events(world, CORE_EVT_DEFEAT), 1);
    CHECK_EQ(core_get_summary(world, &summary), CORE_OK);
    CHECK_EQ(summary.outcome, CORE_OUTCOME_LOST);
    /* 진 뒤에는 시간이 흐르지 않고 연구·승급도 안 됨 */
    CHECK_EQ(core_tick(world, 1), CORE_ERR_REJECTED);
    CHECK_EQ(core_start_research(world, 0, &reason), CORE_ERR_REJECTED);
    CHECK_EQ(reason, CORE_DENY_GAME_OVER);
    CHECK_EQ(core_rank_up(world, &reason), CORE_ERR_REJECTED);
    CHECK_EQ(reason, CORE_DENY_GAME_OVER);
    /* 저장하고 불러와도 진 상태 */
    (void)platform_remove_file(path);
    CHECK_EQ(core_save(world, path), CORE_OK);
    CHECK_EQ(core_load(path, NULL, &loaded, &line), CORE_OK);
    CHECK_EQ(loaded->outcome, CORE_OUTCOME_LOST);
    CHECK_EQ(core_tick(loaded, 1), CORE_ERR_REJECTED);
    CHECK_EQ(test_validate_verbose(world), 0);
    core_destroy(world);
    core_destroy(loaded);
    (void)platform_remove_file(path);
}

void test_population_zero_defeat(void)
{
    CoreWorld *world = test_make_world(506, CORE_DIFF_HARSH);
    CoreRunStatsView stats;
    int32_t rc = CORE_OK;
    int32_t day;
    CHECK(world != NULL);
    /* 아무도 도착을 알리지 않으면 아무도 먹지 못해 굶다가 모두 떠남 → 패배 */
    for (day = 0; day < 30 && rc == CORE_OK; day++) {
        rc = core_tick(world, CLOCK_TICKS_PER_DAY);
    }
    CHECK_EQ(rc, CORE_ERR_REJECTED);
    CHECK_EQ(world->villager_count, 0);
    CHECK_EQ(world->outcome, CORE_OUTCOME_LOST);
    CHECK_EQ(world->lose_reason, CORE_LOSE_POPULATION);
    CHECK_EQ(core_get_run_stats(world, &stats), CORE_OK);
    CHECK(stats.villagers_left >= 5);
    CHECK_EQ(stats.peak_population, 5);
    CHECK_EQ(test_validate_verbose(world), 0);
    core_destroy(world);
}

void test_tutorial_objectives(void)
{
    CoreWorld *world = test_make_world(507, CORE_DIFF_NORMAL);
    CoreEventView event;
    Building *farm;
    int32_t steps = 0;
    int32_t done = 0;
    int32_t reason = -1;
    int32_t placed = 0;
    int32_t gold;
    int32_t i;
    CHECK(world != NULL);
    test_give(world, 500);
    test_flatten(world, world->keep_x + 4, world->keep_y - 4, 10, 10);
    farm = test_build_now(world, CORE_BLD_FARM, world->keep_x + 5, world->keep_y - 3);
    CHECK(farm != NULL);
    CHECK(test_build_now(world, CORE_BLD_HOUSE, world->keep_x + 9, world->keep_y - 3) != NULL);
    CHECK(test_run(world, 6));
    CHECK_EQ(world->tutorial_done, (1 << CORE_TUTORIAL_FARM) | (1 << CORE_TUTORIAL_HOUSE));
    /* 직접 배치 (자동 배치로 들어간 것은 세지 않음) */
    for (i = 0; i < world->villager_count; i++) {
        if (world->villagers[i]->workplace == NULL) {
            CHECK_EQ(core_assign_worker(world, world->villagers[i]->id, farm->id), CORE_ERR_REJECTED);
            break;
        }
    }
    CHECK_EQ(core_unassign_worker(world, farm->workers[0]->id), CORE_OK);
    CHECK_EQ(core_assign_worker(world, world->villagers[0]->id == farm->workers[0]->id ? world->villagers[1]->id
                                                                                      : world->villagers[0]->id,
                                farm->id),
             CORE_OK);
    test_flatten(world, world->keep_x - 8, world->keep_y + 4, 3, 3);
    CHECK_EQ(core_place_line(world, CORE_BLD_WALL, world->keep_x - 7, world->keep_y + 5, world->keep_x - 7,
                             world->keep_y + 5, &placed, &reason),
             CORE_OK);
    CHECK(test_run(world, 12));
    CHECK_EQ(world->tutorial_done, TUTORIAL_ALL_DONE & ~(1 << CORE_TUTORIAL_RAID));
    /* 첫 도적 습격을 버티면 끝, 보상 */
    while (world->threat.raid.id == 0) {
        test_keep_alive(world);
        CHECK(test_run(world, 1));
    }
    CHECK_EQ(core_report_raid_ended(world, world->threat.raid.id), CORE_OK);
    gold = world->bank.amount[CORE_RES_GOLD];
    CHECK(test_run(world, 6));
    CHECK_EQ(world->tutorial_done, TUTORIAL_ALL_DONE);
    CHECK_EQ(world->bank.amount[CORE_RES_GOLD] >= gold + TUTORIAL_REWARD_GOLD - 5, 1);
    while (core_poll_event(world, &event) == 1) {
        steps += event.type == CORE_EVT_TUTORIAL_STEP;
        done += event.type == CORE_EVT_TUTORIAL_DONE;
    }
    CHECK_EQ(steps, CORE_TUTORIAL_COUNT);
    CHECK_EQ(done, 1);
    CHECK(test_run(world, 24));
    CHECK_EQ(count_events(world, CORE_EVT_TUTORIAL_DONE), 0); /* 한 번만 */
    CHECK_EQ(test_validate_verbose(world), 0);
    core_destroy(world);
}

void test_difficulty_presets(void)
{
    CoreDifficultyView view[CORE_DIFF_COUNT];
    int32_t d;
    for (d = 0; d < CORE_DIFF_COUNT; d++) {
        CoreWorld *world = test_make_world(508, d);
        CHECK_EQ(core_get_difficulty(d, &view[d]), CORE_OK);
        CHECK(world != NULL);
        CHECK_EQ(world->villager_count, view[d].start_villagers);
        CHECK_EQ(world->bank.amount[CORE_RES_GOLD], view[d].start_resources[CORE_RES_GOLD]);
        core_destroy(world);
    }
    /* 평화 → 혹독: 주민·자원은 줄고, 습격은 세지고 잦아지고, 사건은 세지고, 공물은 늘어남 */
    for (d = 1; d < CORE_DIFF_COUNT; d++) {
        CHECK(view[d].start_villagers < view[d - 1].start_villagers);
        CHECK(view[d].start_resources[CORE_RES_FOOD] < view[d - 1].start_resources[CORE_RES_FOOD]);
        CHECK(view[d].raid_strength_pct > view[d - 1].raid_strength_pct);
        CHECK(view[d].raid_interval_pct < view[d - 1].raid_interval_pct);
        CHECK(view[d].nature_pct > view[d - 1].nature_pct);
        CHECK(view[d].tribute_pct > view[d - 1].tribute_pct);
    }
    CHECK_EQ(core_get_difficulty(CORE_DIFF_COUNT, &view[0]), CORE_ERR_INVALID_ARG);
    CHECK_EQ(core_get_difficulty(0, NULL), CORE_ERR_NULL_ARG);
}

/* 연구·등급 정의 파일: 앞에 없는 연구를 가리키거나, 같은 키가 두 번이거나, 없는 연구 수를 요구하면 거절 */
void test_research_and_rank_files(void)
{
    static const char *const BAD_RESEARCH[] = {
        "[research a]\nrequires = b\n",                          /* 뒤에 나오는(없는) 연구 → 2번째 줄 */
        "[research a]\npoints = 5\n[research a]\n",              /* 같은 키 → 3번째 줄 */
        "[research a]\neffect = flying\n",                       /* 모르는 효과 → 2번째 줄 */
        "",                                                     /* 연구가 하나도 없음 → 끝에서 */
    };
    static const int32_t LINES[] = {2, 3, 2, 0};
    static const char *const GOOD =
        "[research first]\nbranch = magic\ncost_gold = 1\npoints = 2\neffect = ward\nvalue = 10\n"
        "[research second]\nrequires = first\nbranch = defense\npoints = 3\neffect = tower_damage\nvalue = 2\n";
    CoreDefs *defs = NULL;
    int32_t line = -1;
    int32_t i;
    CHECK_EQ(core_defs_create(&defs), CORE_OK);
    for (i = 0; i < (int32_t)(sizeof(BAD_RESEARCH) / sizeof(BAD_RESEARCH[0])); i++) {
        CHECK_EQ(core_defs_load_text(defs, CORE_DATA_RESEARCH, BAD_RESEARCH[i], (int32_t)strlen(BAD_RESEARCH[i]),
                                     &line),
                 CORE_ERR_FORMAT);
        CHECK_EQ(line, LINES[i]);
        CHECK_EQ(defs->defs.research_count, BUILTIN_RESEARCH_COUNT); /* 기본값 그대로 */
    }
    /* 파일이 목록 전체를 정함 */
    CHECK_EQ(core_defs_load_text(defs, CORE_DATA_RESEARCH, GOOD, (int32_t)strlen(GOOD), &line), CORE_OK);
    CHECK_EQ(defs->defs.research_count, 2);
    CHECK_EQ(defs->defs.research[1].requires, 0);
    CHECK_EQ(defs->defs.research[1].effect, CORE_EFFECT_TOWER_DAMAGE);
    /* 등급 파일: 모르는 등급·필드는 거절, 맞으면 바뀜 */
    {
        const char *ranks = "[rank emperor]\n";
        CHECK_EQ(core_defs_load_text(defs, CORE_DATA_RANKS, ranks, (int32_t)strlen(ranks), &line), CORE_ERR_FORMAT);
        CHECK_EQ(line, 1);
    }
    {
        const char *ranks = "[rank village]\npopulation = 3\nglory = 10\n";
        CHECK_EQ(core_defs_load_text(defs, CORE_DATA_RANKS, ranks, (int32_t)strlen(ranks), &line), CORE_ERR_FORMAT);
        CHECK_EQ(line, 3);
    }
    {
        const char *ranks = "[rank village]\npopulation = 3\ntribute_gold = 10\n";
        CHECK_EQ(core_defs_load_text(defs, CORE_DATA_RANKS, ranks, (int32_t)strlen(ranks), &line), CORE_OK);
        CHECK_EQ(defs->defs.ranks[CORE_RANK_VILLAGE].population, 3);
    }
    /* 연구가 2개뿐이면 왕성(연구 7개)도 2개까지만 요구 */
    {
        CoreNewGameParams params;
        CoreWorld *world = NULL;
        memset(&params, 0, sizeof(params));
        strcpy(params.castle_name, "시험성");
        params.seed = 509;
        params.difficulty = CORE_DIFF_NORMAL;
        CHECK_EQ(core_create(&params, defs, &world), CORE_OK);
        world->rank = CORE_RANK_CASTLE;
        {
            CoreRankView rank;
            CHECK_EQ(core_get_rank(world, &rank), CORE_OK);
            CHECK_EQ(rank.need_research, 2);
        }
        core_destroy(world);
    }
    core_defs_destroy(defs);
}
