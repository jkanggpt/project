/* 연구·승급·승패·튜토리얼의 현재 계약 */
#include "fixtures/town.h"
#include "rank.h"
#include "research.h"
#include "threat.h"
#include "tutorial.h"

static void ready_rank(World *w, int rank) {
    w->rank = rank;
    while (w->villager_count < 100)
        population_create(w, w->keep_x, w->keep_y + 5);
    for (int i = 0; i < w->villager_count; i++) {
        w->villagers[i]->happiness = 100;
        w->villagers[i]->incoming = 0;
        w->villagers[i]->contract_end_tick = 0;
    }
    for (int r = 0; r < CORE_RES_COUNT; r++)
        w->bank.amount[r] = 10000;
    w->research.done = 0x3f;
    w->research.done_count = 6;
    w->expansion->effective_roads = 1000;
    w->expansion->protected_area = 1000;
    w->expansion->protected_population = 100;
}
void test_research_flow(void) {
    World *w = fx_town_world(501);
    CHECK(w);
    int reason = 0;
    CoreResearchView r;
    CHECK_EQ(core_start_research(w, 0, &reason), CORE_ERR_REJECTED);
    CHECK_EQ(reason, CORE_DENY_RESEARCH_LOCKED);
    CHECK_EQ(core_start_research(w, 24, &reason), CORE_ERR_NOT_FOUND);
    Building *b = fx_town_building(w, CORE_BLD_LIBRARY, 0);
    CHECK(b);
    fx_town_worker(w, b, w->villagers[0]);
    w->rank = 3;
    int gold = w->bank.amount[CORE_RES_GOLD];
    CHECK_EQ(core_start_research(w, 0, &reason), CORE_OK);
    CHECK_EQ(w->bank.amount[CORE_RES_GOLD], gold - 70);
    CHECK_EQ(core_start_research(w, 1, &reason), CORE_ERR_REJECTED);
    CHECK_EQ(reason, CORE_DENY_RESEARCH_BUSY);
    w->defs.research[0].points = 1;
    b->daily_work_milli = b->research_work_milli = 540000;
    expansion_daily(w);
    CHECK(expansion_has_research(w, 0));
    CHECK_EQ(w->research.active, -1);
    CHECK_EQ(core_get_research(w, 0, &r), CORE_OK);
    CHECK_EQ(r.state, CORE_RSTATE_DONE);
    CHECK_EQ(fx_town_events(w, CORE_EVT_RESEARCH_DONE), 1);
    CHECK_EQ(core_start_research(w, 0, &reason), CORE_ERR_REJECTED);
    CHECK_EQ(reason, CORE_DENY_RESEARCH_DONE);
    core_destroy(w);
}
void test_research_effects_and_save(void) {
    World *w = fx_town_world(502);
    CHECK(w);
    int before = w->defs.buildings[CORE_BLD_GATE].max_hp;
    w->research.done = UINT32_C(1) << 10;
    w->research.done_count = 1;
    expansion_apply_research(w);
    CHECK_EQ(w->defs.buildings[CORE_BLD_GATE].max_hp, before * 120 / 100);
    expansion_apply_research(w);
    CHECK_EQ(w->defs.buildings[CORE_BLD_GATE].max_hp, before * 120 / 100);
    fx_content_current_roundtrip(w);
    core_destroy(w);
}
void test_rank_up_rules(void) {
    World *w = fx_town_world(503);
    CHECK(w);
    int reason = 0;
    CoreExpansionRankView r;
    CHECK_EQ(core_expansion_rank(w, &r), CORE_OK);
    CHECK(!r.ready);
    CHECK_EQ(core_rank_up(w, &reason), CORE_ERR_REJECTED);
    ready_rank(w, 0);
    CHECK_EQ(core_expansion_rank(w, &r), CORE_OK);
    CHECK(r.ready);
    w->bank.amount[CORE_RES_GOLD] = 0;
    CHECK_EQ(core_rank_up(w, &reason), CORE_ERR_REJECTED);
    CHECK_EQ(reason, CORE_DENY_NO_GOLD);
    w->bank.amount[CORE_RES_GOLD] = 1000;
    CHECK_EQ(core_rank_up(w, &reason), CORE_OK);
    CHECK_EQ(w->rank, 0);
    CHECK_EQ(w->expansion->promotion_remaining, 144);
    for (int i = 0; i < 143; i++)
        expansion_promotion_tick(w);
    CHECK_EQ(w->rank, 0);
    expansion_promotion_tick(w);
    CHECK_EQ(w->rank, 1);
    CHECK_EQ(w->expansion->promotion_state, 0);
    CHECK_EQ(fx_town_events(w, CORE_EVT_RANK_UP), 1);
    core_destroy(w);
}
void test_victory_at_royal_castle(void) {
    World *w = fx_town_world(504);
    CHECK(w);
    ready_rank(w, 4);
    CHECK(fx_town_building(w, CORE_BLD_GARRISON, 0));
    CHECK(fx_town_building(w, CORE_BLD_MAGE_TOWER, 1));
    w->expansion->effective_roads = 1000;
    w->expansion->protected_area = 1000;
    w->expansion->protected_population = 100;
    int reason = 0;
    CHECK_EQ(core_rank_up(w, &reason), CORE_OK);
    for (int i = 0; i < 144; i++)
        expansion_promotion_tick(w);
    CHECK_EQ(w->rank, 5);
    CHECK_EQ(w->outcome, CORE_OUTCOME_WON);
    CHECK_EQ(fx_town_events(w, CORE_EVT_VICTORY), 1);
    CHECK_EQ(core_rank_up(w, &reason), CORE_ERR_REJECTED);
    CHECK_EQ(reason, CORE_DENY_MAX_RANK);
    core_destroy(w);
}
void test_keep_falls_defeat(void) {
    World *w = fx_town_world(505);
    CHECK(w);
    Building *b = world_keep(w);
    placement_damage(w, b, b->hp);
    CHECK_EQ(core_tick(w, 1), CORE_OK);
    CHECK_EQ(w->outcome, CORE_OUTCOME_LOST);
    CHECK_EQ(w->lose_reason, CORE_LOSE_KEEP);
    CHECK_EQ(core_tick(w, 1), CORE_ERR_REJECTED);
    fx_content_current_roundtrip(w);
    core_destroy(w);
}
void test_population_zero_defeat(void) {
    World *w = fx_town_world(506);
    CHECK(w);
    while (w->villager_count)
        population_kill(w, w->villagers[0], CORE_DEATH_STARVATION);
    CHECK_EQ(core_tick(w, 1), CORE_OK);
    CHECK_EQ(w->outcome, CORE_OUTCOME_LOST);
    CHECK_EQ(w->lose_reason, CORE_LOSE_POPULATION);
    CHECK_EQ(core_tick(w, 1), CORE_ERR_REJECTED);
    core_destroy(w);
}
/* 모든 게임의 처음 목표 5개 (world/content.c 의 expansion_goals, 한 시간마다 확인) */
void test_tutorial_objectives(void) {
    World *w = fx_town_world(507);
    CHECK(w);
    CoreSummaryView s;
    Building *garden = fx_town_building(w, CORE_BLD_GARDEN, 0);
    CHECK(garden);
    CHECK(fx_town_building(w, CORE_BLD_SHACK, 1)); /* fx_town_building 은 영주관까지 도로도 깔아 줌 */
    fx_town_worker(w, garden, w->villagers[0]);
    CHECK_EQ(core_tick(w, 6), CORE_OK);
    CHECK_EQ(core_get_summary(w, &s), CORE_OK);
    CHECK_EQ(s.tutorial_done & 15, 15); /* 텃밭, 주거, 직접 배치, 연결 도로 10칸 */
    CHECK(!(s.tutorial_done & 16));
    w->rank = 1;
    CHECK_EQ(core_tick(w, 6), CORE_OK);
    CHECK_EQ(core_get_summary(w, &s), CORE_OK);
    CHECK_EQ(s.tutorial_done, 31); /* 촌락 승급까지 */
    core_destroy(w);
}
void test_difficulty_presets(void) {
    for (int d = 0; d < CORE_DIFF_COUNT; d++) {
        World *w = test_make_world(508, d);
        CHECK(w);
        CoreSummaryView s;
        CHECK_EQ(core_get_summary(w, &s), CORE_OK);
        CHECK_EQ(s.difficulty, d);
        CHECK_EQ(s.population, 4);
        CHECK_EQ(s.map_width, 160);
        expansion_raids_tick(w);
        int day = w->expansion->next_raid_tick / 144 + 1;
        int first[3] = {28, 18, 14};
        CHECK(day >= first[d]);
        CHECK(day <= first[d] + 3);
        core_destroy(w);
    }
    CoreDifficultyView v;
    CHECK_EQ(core_get_difficulty(CORE_DIFF_COUNT, &v), CORE_ERR_INVALID_ARG);
    CHECK_EQ(core_get_difficulty(0, NULL), CORE_ERR_NULL_ARG);
}
void test_research_and_rank_files(void) {
    CoreDefs *defs = NULL;
    CHECK_EQ(core_defs_create(&defs), CORE_OK);
    int32_t line = 0;
    const char *bad = "[research a]\nrequires = missing\n";
    CHECK_EQ(core_defs_load_text(defs, CORE_DATA_RESEARCH, bad, (int32_t)strlen(bad), &line),
             CORE_ERR_FORMAT);
    CHECK_EQ(line, 2);
    CHECK_EQ(core_defs_expand(defs), CORE_OK);
    CHECK_EQ(defs->defs.research_count, 24);
    for (int i = 0; i < 24; i++) {
        CHECK(defs->defs.research[i].points > 0);
        CHECK(defs->defs.research[i].requires < i);
    }
    CHECK_EQ(core_defs_save_file(defs, CORE_DATA_RESEARCH, "research_current.def"), CORE_OK);
    CoreDefs *copy = NULL;
    CHECK_EQ(core_defs_create(&copy), CORE_OK);
    CHECK_EQ(core_defs_expand(copy), CORE_OK);
    CHECK_EQ(core_defs_load_file(copy, CORE_DATA_RESEARCH, "research_current.def", &line), CORE_OK);
    CHECK_EQ(copy->defs.research_count, 24);
    CHECK(!memcmp(defs->defs.research, copy->defs.research, 24 * sizeof(ResearchDef)));
    remove("research_current.def");
    core_defs_destroy(copy);
    core_defs_destroy(defs);
}
void test_building_def_counted_terrain(void) {
    World *w = test_make_world(144, CORE_DIFF_NORMAL);
    CHECK(w);
    for (int type = 0; type < CORE_BLD_COUNT; type++) {
        CoreBuildingDefView d;
        CoreExpansionBuildingInfo s;
        CHECK_EQ(core_get_building_def(w, type, &d), CORE_OK);
        CHECK_EQ(core_expansion_building_info(type, &s), CORE_OK);
        CHECK_EQ(d.w, s.w);
        CHECK_EQ(d.h, s.h);
        CHECK_EQ(d.work_radius, s.radius);
        CHECK_EQ(d.required_rank, s.rank);
    }
    core_destroy(w);
}
