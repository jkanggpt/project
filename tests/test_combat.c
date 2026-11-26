/* combat — 현행 기능별 회귀 사례 */
#include "fixtures/labor.h"
#include "fixtures/lifecycle.h"
#include "fixtures/settlement.h"

void test_settlement_attack_impact_and_save(void) {
    World *w = fx_settlement_make_world(), *loaded = NULL;
    if (!w)
        return;
    Villager *v = w->villagers[0];
    CoreAnimalView *a = &w->expansion->animals[0];
    v->current_x_milli = v->current_y_milli = 75500;
    v->inside_building = v->incoming = 0;
    v->state = CORE_VS_IDLE;
    v->health = 100;
    a->kind = 11;
    a->hp = 36;
    a->alive = 1;
    a->owner = 0;
    a->x_milli = a->y_milli = 75500;
    a->target_id = v->id;
    a->attack_tick = w->clock.tick;
    a->attack_remaining_milli = 2400;
    expansion_ecology_recount(w);
    CHECK_EQ(core_expansion_advance_animal_attacks(w, 2399), CORE_OK);
    CHECK_EQ(v->health, 100);
    CHECK_EQ(core_expansion_advance_animal_attacks(w, 1), CORE_OK);
    CHECK_EQ(v->health, 93);
    CHECK_EQ(core_expansion_advance_animal_attacks(w, 2400), CORE_OK);
    CHECK_EQ(v->health, 93);
    a->attack_remaining_milli = 2400;
    core_expansion_advance_animal_attacks(w, 1200);
    CHECK_EQ(core_save(w, "settlement_attack.sav"), CORE_OK);
    int32_t line = 0;
    CHECK_EQ(core_load("settlement_attack.sav", NULL, &loaded, &line), CORE_OK);
    if (loaded) {
        CHECK_EQ(loaded->expansion->animals[0].attack_remaining_milli, 1200);
        core_expansion_advance_animal_attacks(loaded, 1199);
        CHECK_EQ(loaded->villagers[0]->health, 93);
        core_expansion_advance_animal_attacks(loaded, 1);
        CHECK_EQ(loaded->villagers[0]->health, 86);
        core_destroy(loaded);
    }
    core_destroy(w);
    remove("settlement_attack.sav");
    remove("settlement_attack.sav.bak");
}

void test_labor_raids_summons_and_drops(void) {
    World *w = fx_labor_world121();
    if (!w)
        return;
    CoreRaiderInfo info;
    for (int i = 0; i < 28; i++) {
        CHECK_EQ(core_expansion_raider_info(i, &info), CORE_OK);
        CHECK(info.cost > 0);
        CHECK(w->defs.enemies[i].hp > 0);
    }
    w->clock.tick = 120 * 144 + 40;
    w->expansion->next_raid_tick = w->clock.tick;
    expansion_raids_tick(w);
    Raid *r = &w->threat.raid;
    CHECK(r->id);
    CHECK(r->enemy_count >= 3 && r->enemy_count <= 5);
    CHECK(expansion_center_distance(world_keep(w), r->spawn_x, r->spawn_y) >= 25);
    CHECK(r->spawn_x == 0 || r->spawn_x == w->grid.width - 1 || r->spawn_y == 0 ||
          r->spawn_y == w->grid.height - 1);
    CHECK(!w->expansion->protected_cells[grid_index(&w->grid, r->spawn_x, r->spawn_y)]);
    r->enemies[0].type = 13;
    for (int i = 0; i < 80; i++) {
        int32_t id = 0;
        CHECK_EQ(core_expansion_summon(w, 1, &id), CORE_OK);
        CHECK(id > 0);
    }
    CHECK(r->enemy_count > 64);
    CHECK_EQ(core_validate(w), 0);
    CHECK_EQ(threat_report_enemy_killed(w, r->id, 1), CORE_OK);
    for (int i = 0; i < r->enemy_count; i++)
        if (r->enemies[i].summoner_id == 1)
            CHECK_EQ(r->enemies[i].state, CORE_ENEMY_ESCAPED);
    int gold = w->bank.amount[CORE_RES_GOLD];
    CHECK_EQ(threat_report_enemy_killed(w, r->id, 1), CORE_IGNORED);
    CHECK_EQ(w->bank.amount[CORE_RES_GOLD], gold);
    core_destroy(w);
}

void test_lifecycle_discovery_timer_and_shift(void) {
    CoreNewGameParams p = {0};
    World *w = NULL, *copy = NULL;
    int32_t line = 0;
    strcpy(p.castle_name, u8"발견 시점과 최초 근무조");
    p.seed = 42;
    p.ruleset = 1;
    CHECK_EQ(test_create_settlement(&p, &w), CORE_OK);
    if (!w)
        return;
    CHECK_EQ(threat_debug_raid(w, CORE_THREAT_BANDITS, CORE_DIR_EAST, 2), CORE_OK);
    expansion_raids_tick(w);
    Raid *r = &w->threat.raid;
    CHECK_EQ(r->phase, CORE_RAID_APPROACH);
    CHECK_EQ(r->assault_end_tick, 0);
    CHECK(!core_expansion_build_locked(w));
    CHECK(!threat_raid_active(w));
    w->clock.tick += 500;
    expansion_raids_tick(w);
    CHECK_EQ(r->phase, CORE_RAID_APPROACH);
    CHECK_EQ(r->assault_end_tick, 0);
    int started = w->clock.tick;
    CHECK_EQ(core_expansion_building_hit(w, w->keep_id, 1, 1, 1), CORE_OK);
    CHECK_EQ(r->phase, CORE_RAID_ASSAULT);
    CHECK_EQ(r->started_tick, started);
    CHECK_EQ(r->assault_end_tick, started + 288);
    CHECK(core_expansion_build_locked(w));
    w->clock.tick++;
    expansion_raid_discover(w);
    CHECK_EQ(r->assault_end_tick, started + 288);
    CHECK_EQ(core_save(w, "lifecycle_discovery.sav"), CORE_OK);
    CHECK_EQ(core_load("lifecycle_discovery.sav", NULL, &copy, &line), CORE_OK);
    if (copy) {
        uint64_t a, b;
        core_state_hash(w, &a);
        core_state_hash(copy, &b);
        CHECK_EQ(a, b);
        core_destroy(copy);
    }
    w->clock.tick = started + 287;
    expansion_raids_tick(w);
    CHECK_EQ(r->phase, CORE_RAID_ASSAULT);
    w->clock.tick++;
    expansion_raids_tick(w);
    CHECK_EQ(r->phase, CORE_RAID_RETREAT);
    CHECK(core_expansion_build_locked(w));
    CHECK_EQ(core_report_enemy_escaped(w, r->id, 1), CORE_OK);
    CHECK_EQ(core_report_enemy_escaped(w, r->id, 2), CORE_OK);
    CHECK(!core_expansion_build_locked(w));
    /* 다음 적 배열을 재사용할 때 이전 HP·철수·공격 상태를 물려주지 않습니다. */
    r->enemies[0].runtime.valid = 1;
    r->enemies[0].runtime.fleeing = 1;
    r->enemies[0].runtime.hp = 17;
    CHECK_EQ(threat_debug_raid(w, CORE_THREAT_BANDITS, CORE_DIR_EAST, 1), CORE_OK);
    CHECK_EQ(r->enemies[0].runtime.valid, 0);
    CHECK_EQ(r->enemies[0].runtime.fleeing, 0);
    CHECK_EQ(core_expansion_engaged(w, 1), CORE_OK);
    CHECK_EQ(r->assault_end_tick, w->clock.tick + 288);
    CHECK_EQ(core_report_enemy_escaped(w, r->id, 1), CORE_OK);
    CHECK_EQ(threat_debug_raid(w, CORE_THREAT_BANDITS, CORE_DIR_EAST, 1), CORE_OK);
    CHECK_EQ(core_expansion_spotted(w, w->keep_x, w->keep_y), CORE_OK);
    CHECK_EQ(r->first_discovered, 1);
    CHECK_EQ(r->assault_end_tick, w->clock.tick + 288);
    Building *post = entity_add_building(w, CORE_BLD_OUTPOST);
    CHECK(post != NULL);
    if (post) {
        post->x = 90;
        post->y = 90;
        post->w = post->h = 1;
        post->door_x = 90;
        post->door_y = 91;
        post->hp = 110;
        post->state = CORE_BSTATE_ACTIVE;
        Villager *v = w->villagers[0], *other = w->villagers[1];
        CHECK_EQ(core_expansion_assign_shift(w, v->id, post->id, 1), CORE_OK);
        CHECK_EQ(v->shift_kind, 1);
        Building *previous = other->workplace;
        CHECK_EQ(core_expansion_assign_shift(w, other->id, post->id, 1), CORE_ERR_REJECTED);
        CHECK(other->workplace == previous);
        CHECK_EQ(core_expansion_assign_shift(w, other->id, post->id, 0), CORE_OK);
        CHECK_EQ(other->shift_kind, 0);
    }
    core_destroy(w);
    remove("lifecycle_discovery.sav");
    remove("lifecycle_discovery.sav.bak");
}
