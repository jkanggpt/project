/* 습격 접근·발견·공격·퇴각과 보고의 현재 계약 */
#include "fixtures/town.h"
#include "threat.h"
#include "platform.h"

static int start_test_raid(World *w) {
    w->expansion->next_raid_tick = w->clock.tick;
    expansion_raids_tick(w);
    return w->threat.raid.id;
}
void test_threat_first_bandits_announced(void) {
    World *w = fx_town_world(401);
    CHECK(w);
    expansion_raids_tick(w);
    int next = w->expansion->next_raid_tick;
    CHECK(next > 17 * 144 && next < 22 * 144);
    CHECK(!w->threat.raid.id);
    w->clock.tick = next;
    CHECK(start_test_raid(w));
    CHECK_EQ(w->threat.raid.phase, CORE_RAID_APPROACH);
    CHECK(!core_expansion_build_locked(w));
    CHECK_EQ(fx_town_events(w, CORE_EVT_RAID_STARTED), 0);
    CHECK_EQ(core_expansion_engaged(w, 1), CORE_OK);
    CHECK_EQ(fx_town_events(w, CORE_EVT_RAID_STARTED), 1);
    CHECK(core_expansion_build_locked(w));
    core_destroy(w);
}
void test_raid_start_flee_and_guards(void) {
    World *w = fx_town_world(402);
    CHECK(w);
    Building *b = fx_town_building(w, CORE_BLD_BARRACKS, 0);
    CHECK(b);
    Villager *v = w->villagers[0], *civilian = w->villagers[1];
    CHECK_EQ(core_assign_worker(w, v->id, b->id), CORE_OK);
    CHECK(threat_is_guard(w, v));
    CHECK(start_test_raid(w));
    CHECK_EQ(core_expansion_engaged(w, 1), CORE_OK);
    expansion_people_tick(w);
    CHECK_EQ(civilian->state, CORE_VS_FLEE);
    CHECK(w->expansion->alarm);
    CHECK_EQ(w->threat.raid.assault_end_tick - w->threat.raid.started_tick, 288);
    core_destroy(w);
}
void test_raid_reports_apply(void) {
    World *w = fx_town_world(403);
    CHECK(w);
    int raid = start_test_raid(w);
    CHECK(raid);
    Building *b = world_keep(w);
    int hp = b->hp;
    CHECK_EQ(core_report_building_damage(w, raid - 1, b->id, 7), CORE_IGNORED);
    CHECK_EQ(b->hp, hp);
    CHECK_EQ(core_report_building_damage(w, raid, b->id, 7), CORE_OK);
    CHECK_EQ(b->hp, hp - 7);
    CHECK(w->threat.raid.first_discovered);
    Villager *v = w->villagers[0];
    hp = v->health;
    v->inside_building = 0;
    CHECK_EQ(core_report_villager_damage(w, raid, v->id, 3), CORE_OK);
    CHECK(v->health < hp);
    CHECK_EQ(core_report_enemy_killed(w, raid, 1), CORE_OK);
    CHECK_EQ(core_report_enemy_killed(w, raid, 1), CORE_IGNORED);
    CHECK_EQ(w->threat.raid.killed, 1);
    core_destroy(w);
}
void test_raid_times_out(void) {
    World *w = fx_town_world(404);
    CHECK(w);
    CHECK(start_test_raid(w));
    int start = w->clock.tick;
    w->clock.tick += 400;
    expansion_raids_tick(w);
    CHECK_EQ(w->threat.raid.phase, CORE_RAID_APPROACH);
    CHECK_EQ(core_expansion_engaged(w, 1), CORE_OK);
    CHECK_EQ(w->threat.raid.started_tick, start + 400);
    w->clock.tick += 287;
    expansion_raids_tick(w);
    CHECK_EQ(w->threat.raid.phase, CORE_RAID_ASSAULT);
    w->clock.tick++;
    expansion_raids_tick(w);
    CHECK_EQ(w->threat.raid.phase, CORE_RAID_RETREAT);
    CHECK(w->threat.raid.id);
    int id = w->threat.raid.id;
    CHECK_EQ(core_report_raid_ended(w, id), CORE_OK);
    CHECK(!w->threat.raid.id);
    CHECK_EQ(core_report_raid_ended(w, id), CORE_IGNORED);
    core_destroy(w);
}
void test_ruined_wall_is_passable(void) {
    World *w = fx_town_world(405);
    CHECK(w);
    Building *b = fx_town_building(w, CORE_BLD_PALISADE, 0);
    CHECK(b);
    int p = grid_index(&w->grid, b->x, b->y);
    expansion_rebuild(w);
    CHECK_EQ(w->grid.block[p], GRID_BLOCK_WALL);
    CHECK(!w->grid.cost[p] || w->grid.cost[p] == CORE_COST_WALL);
    placement_damage(w, b, b->hp);
    CHECK_EQ(b->state, CORE_BSTATE_RUINED);
    expansion_rebuild(w);
    CHECK_EQ(w->grid.block[p], GRID_BLOCK_NONE);
    CHECK(w->grid.cost[p] > 0);
    core_destroy(w);
}
void test_sheltered_villagers_eat(void) {
    World *w = fx_town_world(406);
    CHECK(w);
    CHECK(start_test_raid(w));
    CHECK_EQ(core_expansion_engaged(w, 1), CORE_OK);
    Villager *v = w->villagers[0];
    v->meal_day = 0;
    v->needs.hunger = 0;
    expansion_people_tick(w);
    CHECK_EQ(v->state, CORE_VS_FLEE);
    CHECK_EQ(core_villager_arrived(w, v->id, v->intent_rev), CORE_OK);
    int food = expansion_bank_have(w, CORE_RES_FOOD);
    expansion_people_tick(w);
    CHECK_EQ(v->meal_day, exp_day(w));
    CHECK_EQ(v->needs.hunger, 100);
    CHECK(expansion_bank_have(w, CORE_RES_FOOD) < food);
    core_destroy(w);
}
void test_no_repair_during_raid(void) {
    World *w = fx_town_world(407);
    CHECK(w);
    Building *b = world_keep(w);
    b->hp -= 10;
    int reason = 0;
    CHECK(start_test_raid(w));
    CHECK_EQ(core_expansion_engaged(w, 1), CORE_OK);
    CHECK_EQ(core_repair(w, b->id, &reason), CORE_ERR_REJECTED);
    CHECK_EQ(reason, CORE_DENY_RAID);
    CHECK_EQ(core_report_raid_ended(w, w->threat.raid.id), CORE_OK);
    CHECK_EQ(core_repair(w, b->id, &reason), CORE_OK);
    core_destroy(w);
}
void test_enemy_defs_and_composition(void) {
    World *w = fx_town_world(408);
    CHECK(w);
    for (int i = 0; i < 28; i++) {
        CoreEnemyDefView d;
        CHECK_EQ(core_get_enemy_def(w, i, &d), CORE_OK);
        CHECK(d.hp > 0 && d.damage > 0 && d.speed > 0);
    }
    CHECK(start_test_raid(w));
    Raid *r = &w->threat.raid;
    CHECK(r->enemy_count >= 3 && r->enemy_count <= 5);
    int count = 0;
    for (int i = 0; i < 28; i++)
        count += r->initial_types[i];
    CHECK_EQ(count, r->initial_count);
    for (int i = 0; i < r->enemy_count; i++)
        CHECK(r->enemies[i].type >= 0 && r->enemies[i].type < 28);
    core_destroy(w);
}
void test_threat_save_roundtrip(void) {
    World *w = fx_town_world(409);
    CHECK(w);
    expansion_raids_tick(w);
    CHECK(w->expansion->next_raid_tick > w->clock.tick);
    fx_content_current_roundtrip(w);
    CHECK(start_test_raid(w));
    CHECK_EQ(w->threat.raid.phase, CORE_RAID_APPROACH);
    fx_content_current_roundtrip(w);
    CHECK_EQ(core_expansion_engaged(w, 1), CORE_OK);
    fx_content_current_roundtrip(w);
    core_destroy(w);
}
