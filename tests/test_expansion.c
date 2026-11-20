/* test_expansion.c — 확장 규칙의 저장, 회전, 공유 자원과 경제 보존 검사 */
#include "expansion.h"
#include "mem.h"
#include "test_util.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static World *expanded(void) {
    CoreNewGameParams p = {0};
    World *w = NULL;
    strcpy(p.castle_name, u8"확장 시험");
    p.seed = 7;
    p.difficulty = 1;
    p.ruleset = 1;
    if (core_create(&p, NULL, &w) != CORE_OK) {
        test_fail(__FILE__, __LINE__, "expanded world creation");
        return NULL;
    }
    return w;
}
static Building *complete(World *w, int type, int x, int y, int rot) {
    Building *b = entity_add_building(w, type);
    const ExpansionBuildingSpec *s = expansion_spec(type);
    if (!b) {
        test_fail(__FILE__, __LINE__, "building allocation");
        return NULL;
    }
    b->x = x;
    b->y = y;
    b->w = (rot & 1) ? s->h : s->w;
    b->h = (rot & 1) ? s->w : s->h;
    b->rotation = rot;
    b->state = CORE_BSTATE_ACTIVE;
    b->hp = s->hp;
    b->door_x = x + b->w / 2;
    b->door_y = y + b->h;
    b->auto_assign = 0;
    b->completed_day = 1;
    return b;
}
void test_expansion_world_and_save(void) {
    World *w = expanded(), *loaded = NULL;
    CoreSummaryView summary;
    uint64_t before, after;
    int32_t line = 0;
    int i;
    CHECK(w != NULL);
    if (!w)
        return;
    CHECK_EQ(core_get_summary(w, &summary), CORE_OK);
    CHECK_EQ(summary.map_width, 192);
    CHECK_EQ(summary.population, 4);
    CHECK_EQ(summary.resources[CORE_RES_LOGS], 65);
    CHECK_EQ(summary.resources[CORE_RES_FOOD], 125);
    CHECK_EQ(w->defs.research_count, 24);
    CHECK_EQ(core_validate(w), 0);
    for (i = 0; i < 50; i++) {
        int j;
        for (j = 0; j < w->villager_count; j++)
            CHECK_EQ(core_villager_arrived(w, w->villagers[j]->id, w->villagers[j]->intent_rev), CORE_OK);
        CHECK_EQ(core_tick(w, 1), CORE_OK);
    }
    CHECK_EQ(core_validate(w), 0);
    CHECK_EQ(core_state_hash(w, &before), CORE_OK);
    CHECK_EQ(core_save(w, "expansion_roundtrip.sav"), CORE_OK);
    CHECK_EQ(core_load("expansion_roundtrip.sav", NULL, &loaded, &line), CORE_OK);
    if (loaded) {
        CHECK_EQ(core_state_hash(loaded, &after), CORE_OK);
        CHECK_EQ(before, after);
        CHECK_EQ(core_validate(loaded), 0);
        core_destroy(loaded);
    }
    core_destroy(w);
    (void)remove("expansion_roundtrip.sav");
    (void)remove("expansion_roundtrip.sav.bak");
}
void test_expansion_fuel_and_recipe(void) {
    World *w = expanded();
    Building *kiln;
    CHECK(w != NULL);
    if (!w)
        return;
    w->bank.amount[CORE_RES_COAL] = 1;
    w->bank.amount[CORE_RES_LOGS] = 10;
    w->bank.amount[CORE_RES_WOOD] = 10;
    CHECK_EQ(expansion_fuel(w, 500, 1), 500);
    CHECK_EQ(w->bank.amount[CORE_RES_COAL], 0);
    CHECK_EQ(w->expansion->fuel_milli, 1500);
    CHECK_EQ(w->bank.amount[CORE_RES_LOGS], 10);
    CHECK_EQ(expansion_fuel(w, 1500, 1), 1500);
    CHECK_EQ(w->expansion->fuel_milli, 0);
    CHECK_EQ(w->bank.amount[CORE_RES_WOOD], 10);
    kiln = complete(w, CORE_BLD_KILN, 90, 100, 0);
    kiln->daily_work_milli = 2 * 540000;
    w->bank.amount[CORE_RES_CLAY] = 6;
    w->bank.amount[CORE_RES_BRICKS] = 100;
    expansion_rebuild(w);
    w->grid.road[grid_index(&w->grid, kiln->door_x, kiln->door_y)] = 1;
    w->expansion->distance[grid_index(&w->grid, kiln->door_x, kiln->door_y)] = 1;
    /* 일꾼이 없거나 산출 저장소가 가득 찬 가마는 원료를 태우지 않습니다. */
    expansion_daily(w);
    CHECK_EQ(w->bank.amount[CORE_RES_CLAY], 6);
    CHECK_EQ(w->bank.amount[CORE_RES_BRICKS], 100);
    core_destroy(w);
}
void test_expansion_harvest_once(void) {
    World *w = expanded();
    Building *field;
    CHECK(w != NULL);
    if (!w)
        return;
    field = complete(w, CORE_BLD_CROPLAND, 90, 100, 0);
    field->crop_year = 1;
    field->crop_labor_milli = 2 * 540 * 24 * 1000;
    w->clock.tick = 24 * 144 + 48;
    expansion_produce(w);
    CHECK_EQ(field->pending_harvest_milli, 360000);
    CHECK_EQ(field->crop_labor_milli, 0);
    expansion_produce(w);
    CHECK_EQ(field->pending_harvest_milli, 360000);
    field->crop_year = 1;
    field->crop_labor_milli = 2 * 540 * 24 * 1000;
    field->crop_drought = 1;
    field->pending_harvest_milli = 0;
    expansion_produce(w);
    CHECK_EQ(field->pending_harvest_milli, 360000);
    core_destroy(w);
}
void test_expansion_rotation_and_road_guard(void) {
    World *w = expanded();
    Building *b;
    CorePlacementView preview;
    int i, p;
    CHECK(w != NULL);
    if (!w)
        return;
    w->rank = 5;
    for (i = 0; i < w->expansion->cells; i++) {
        w->expansion->tiles[i].explored = 1;
        w->expansion->visible[i] = 1;
    }
    CHECK_EQ(core_can_place_rotated(w, CORE_BLD_PASTURE, 80, 90, 1, &preview), CORE_OK);
    CHECK_EQ(preview.w, 5);
    CHECK_EQ(preview.h, 6);
    CHECK_EQ(preview.door_x, 85);
    CHECK_EQ(preview.door_y, 93);
    /* 시작 성채 남문에서 두 칸 도로로 이어진 주거 시설을 만듭니다. */
    b = complete(w, CORE_BLD_COTTAGE, 101, 100, 0);
    b->door_x = 101;
    b->door_y = 99;
    for (i = 96; i <= 101; i++)
        w->grid.road[grid_index(&w->grid, i, 98)] = 1;
    w->grid.road[grid_index(&w->grid, 101, 99)] = 1;
    expansion_rebuild(w);
    p = grid_index(&w->grid, 100, 98);
    CHECK(w->expansion->connected[grid_index(&w->grid, 101, 99)]);
    CHECK_EQ(expansion_remove_road(w, 100, 98), CORE_ERR_REJECTED);
    CHECK_EQ(w->grid.road[p], 1);
    core_destroy(w);
}
void test_expansion_medical_and_growth(void) {
    World *w = expanded();
    Building *clinic, *hospital;
    Villager *a, *b;
    CHECK(w != NULL);
    if (!w)
        return;
    a = w->villagers[0];
    b = w->villagers[1];
    clinic = complete(w, CORE_BLD_HEALER, 80, 80, 0);
    hospital = complete(w, CORE_BLD_HOSPITAL, 84, 80, 0);
    a->stats.intellect = 3;
    CHECK_EQ(entity_link_worker(clinic, a, 1), CORE_OK);
    CHECK(!expansion_qualified(w, clinic));
    a->stats.intellect = 4;
    CHECK(expansion_qualified(w, clinic));
    entity_unlink_worker(a);
    a->stats.intellect = 6;
    b->stats.intellect = 4;
    CHECK_EQ(entity_link_worker(hospital, a, 3), CORE_OK);
    CHECK(!expansion_qualified(w, hospital));
    CHECK_EQ(entity_link_worker(hospital, b, 3), CORE_OK);
    CHECK(expansion_qualified(w, hospital));
    CHECK(expansion_work_percent(w, a, CORE_BLD_HOSPITAL) >= 25);
    CHECK(expansion_work_percent(w, a, CORE_BLD_HOSPITAL) <= 400);
    core_destroy(w);
}
void test_expansion_alert_and_departure(void) {
    World *w = expanded();
    Villager *v;
    if (!w)
        return;
    v = w->villagers[0];
    w->expansion->alarm = 1;
    CHECK_EQ(core_tick(w, 1), CORE_OK);
    CHECK_EQ(w->expansion->alarm, 0);
    v->state = CORE_VS_LEAVING;
    v->home = NULL;
    v->target_x = 0;
    v->target_y = w->keep_y;
    v->at_target = 0;
    CHECK_EQ(core_tick(w, 1), CORE_OK);
    CHECK(v->state != CORE_VS_LEAVING);
    CHECK_EQ(expansion_permanent(w), 4);
    expansion_assign(w);
    core_destroy(w);
}
void test_expansion_shared_logging(void) {
    World *w = expanded();
    Building *a, *b;
    CoreExpansionTileView *tree;
    int i;
    CHECK(w != NULL);
    for (i = 0; i < w->expansion->cells; i++) {
        w->expansion->tiles[i].tree_kind = 0;
        w->expansion->tiles[i].tree_logs_milli = 0;
    }
    a = complete(w, CORE_BLD_LOGGING_SITE, 80, 80, 0);
    b = complete(w, CORE_BLD_LOGGING_SITE, 84, 80, 0);
    CHECK_EQ(entity_link_worker(a, w->villagers[0], 2), CORE_OK);
    CHECK_EQ(entity_link_worker(b, w->villagers[1], 2), CORE_OK);
    a->daily_work_milli = b->daily_work_milli = 1080000;
    tree = &w->expansion->tiles[grid_index(&w->grid, 83, 81)];
    tree->tree_kind = 2;
    tree->tree_age = 48000;
    tree->tree_logs_milli = 28000;
    w->bank.amount[CORE_RES_LOGS] = 0;
    expansion_daily(w);
    CHECK_EQ(expansion_bank_have(w, CORE_RES_LOGS), 10000);
    CHECK_EQ(tree->tree_used_milli, 10000);
    CHECK_EQ(tree->tree_logs_milli, 18000);
    core_destroy(w);
}
void test_expansion_protected_gate(void) {
    World *w = expanded();
    Building *gate = NULL;
    int x, y;
    CHECK(w != NULL);
    for (y = 75; y <= 81; y++)
        for (x = 75; x <= 81; x++)
            if (x == 75 || x == 81 || y == 75 || y == 81) {
                Building *b =
                    complete(w, x == 78 && y == 81 ? CORE_BLD_PALISADE_GATE : CORE_BLD_PALISADE, x, y, 0);
                if (x == 78 && y == 81)
                    gate = b;
            }
    expansion_rebuild(w);
    CHECK(w->expansion->protected_cells[grid_index(&w->grid, 78, 78)]);
    CHECK_EQ(w->grid.block[grid_index(&w->grid, 78, 81)], GRID_BLOCK_GATE);
    gate->state = CORE_BSTATE_CONSTRUCTING;
    expansion_rebuild(w);
    CHECK(!w->expansion->protected_cells[grid_index(&w->grid, 78, 78)]);
    gate->state = CORE_BSTATE_RUINED;
    expansion_rebuild(w);
    CHECK(!w->expansion->protected_cells[grid_index(&w->grid, 78, 78)]);
    core_destroy(w);
}
void test_expansion_candidates_and_housing(void) {
    World *w = expanded();
    Building *home, *well;
    CoreCandidateView saved[15];
    CHECK(w != NULL);
    memcpy(saved, w->expansion->candidates, sizeof(saved));
    home = complete(w, CORE_BLD_SHACK, 80, 80, 0);
    well = complete(w, CORE_BLD_WELL, 83, 80, 0);
    expansion_rebuild(w);
    expansion_water_supply(w);
    CHECK_EQ(expansion_available_housing(w), 1);
    well->state = CORE_BSTATE_RUINED;
    expansion_rebuild(w);
    CHECK_EQ(expansion_available_housing(w), 0);
    home->state = CORE_BSTATE_RUINED;
    expansion_rebuild(w);
    expansion_candidates(w);
    CHECK_EQ(memcmp(saved, w->expansion->candidates, sizeof(saved)), 0);
    core_destroy(w);
}
void test_expansion_feed_before_output(void) {
    World *w = expanded();
    Building *a, *b;
    int32_t rc;
    CHECK(w != NULL);
    a = complete(w, CORE_BLD_RANCH, 80, 80, 0);
    b = complete(w, CORE_BLD_RANCH, 84, 80, 0);
    w->bank.amount[CORE_RES_GOLD] = 1000;
    CHECK_EQ(core_expansion_buy_animal(w, a->id, 7), CORE_OK);
    CHECK_EQ(core_expansion_buy_animal(w, b->id, 7), CORE_OK);
    CHECK_EQ(entity_link_worker(a, w->villagers[0], 3), CORE_OK);
    CHECK_EQ(entity_link_worker(b, w->villagers[1], 3), CORE_OK);
    a->daily_work_milli = b->daily_work_milli = w->defs.buildings[CORE_BLD_RANCH].worker_slots * 540000;
    w->expansion->animals[w->expansion->animal_count-1].age = 24;
    w->expansion->animals[w->expansion->animal_count-2].age = 24;
    w->bank.amount[CORE_RES_FOOD] = 0;
    w->expansion->fractional[CORE_RES_FOOD] = 600;
    expansion_animal_daily(w);
    /* 첫 소의 산출물을 두 번째 소의 당일 사료로 쓸 수 없습니다. */
    CHECK_EQ(w->expansion->animals[w->expansion->animal_count - 1].starve_days, 1);
    CHECK_EQ(expansion_bank_have(w, CORE_RES_FOOD), 1015);
    rc = core_expansion_damage_animal(w, w->expansion->animals[w->expansion->animal_count - 1].id, 50);
    CHECK_EQ(rc, CORE_OK);
    w->expansion->animals[w->expansion->animal_count - 1].alive = 0;
    w->bank.amount[CORE_RES_FOOD] = 0;
    w->expansion->fractional[CORE_RES_FOOD] = 300;
    expansion_animal_daily(w);
    CHECK_EQ(expansion_bank_have(w, CORE_RES_FOOD), 515);
    core_destroy(w);
}
void test_expansion_xp_and_stock(void) {
    World *w = expanded();
    Villager *v;
    int initial;
    CHECK(w != NULL);
    v = w->villagers[0];
    initial = v->initial_stats[0];
    expansion_add_xp(v, 0, 1000000);
    CHECK_EQ(v->stats.strength, initial + 3 > 10 ? 10 : initial + 3);
    w->bank.amount[CORE_RES_FOOD] = 500;
    CHECK_EQ(expansion_bank_add(w, CORE_RES_FOOD, -1200, CORE_LEDGER_CONSUMPTION), -1200);
    CHECK_EQ(expansion_bank_have(w, CORE_RES_FOOD), 498800);
    CHECK_EQ(expansion_bank_add(w, CORE_RES_FOOD, 5000, CORE_LEDGER_PRODUCTION), 0);
    core_destroy(w);
}
void test_expansion_allocation_failures(void) {
    CoreNewGameParams p = {0};
    long limit, start = mem_test_live_blocks();
    int success = 0;
    strcpy(p.castle_name, u8"할당 시험");
    p.seed = 7;
    p.difficulty = 1;
    p.ruleset = 1;
    for (limit = 0; limit < 4096; limit++) {
        World *w = NULL;
        int rc;
        mem_test_fail_after(limit);
        rc = core_create(&p, NULL, &w);
        mem_test_fail_after(-1);
        if (rc == CORE_OK) {
            core_destroy(w);
            success = 1;
            break;
        }
        CHECK_EQ(rc, CORE_ERR_OUT_OF_MEMORY);
        CHECK(w == NULL);
        CHECK_EQ(mem_test_live_blocks(), start);
    }
    CHECK(success);
    CHECK_EQ(mem_test_live_blocks(), start);
}
void test_expansion_validation_bounds(void) {
    World *w = expanded();
    int count;
    CHECK(w != NULL);
    count = w->expansion->candidate_count;
    w->expansion->candidate_count = 100000;
    CHECK(expansion_validate(w) > 0);
    w->expansion->candidate_count = count;
    w->expansion->candidates[0].stats[0] = 11;
    CHECK(expansion_validate(w) > 0);
    core_destroy(w);
}
void test_expansion_contract_deadline(void) {
    World *w = expanded();
    Villager *v;
    int id, person;
    CHECK(w != NULL);
    v = w->villagers[0];
    id = v->id;
    person = v->personal_id;
    v->contract_end_tick = w->clock.tick + 20;
    v->state = CORE_VS_LEAVING;
    v->target = NULL;
    v->target_x = 0;
    v->target_y = 96;
    v->at_target = 0;
    CHECK_EQ(core_villager_arrived(w, id, v->intent_rev), CORE_OK);
    CHECK(entity_find_villager(w, id) != NULL);
    CHECK(v->intent_flags & CORE_INTENT_HOLD);
    CHECK_EQ(core_tick(w, 19), CORE_OK);
    CHECK(entity_find_villager(w, id) != NULL);
    CHECK_EQ(core_tick(w, 1), CORE_OK);
    CHECK(entity_find_villager(w, id) == NULL);
    CHECK_EQ(w->expansion->archive_count, 1);
    CHECK_EQ(w->expansion->archive[0].person_id, person);
    CHECK_EQ(core_validate(w), 0);
    core_destroy(w);
}
void test_expansion_magic_consumption(void) {
    World *w = expanded();
    Building *tower, *keep;
    int hp;
    CHECK(w != NULL);
    tower = complete(w, CORE_BLD_ARCANE_TOWER, 80, 80, 0);
    keep = world_keep(w);
    CHECK_EQ(entity_link_worker(tower, w->villagers[0], 3), CORE_OK);
    Villager *worker = w->villagers[0];
    worker->state=CORE_VS_WORKING;worker->target=tower;worker->at_target=1;
    worker->current_x_milli=tower->door_x*1000+500;worker->current_y_milli=tower->door_y*1000+500;
    w->research.done = (1u << 21) | (1u << 22);
    w->research.done_count = 2;
    w->bank.amount[CORE_RES_MANA] = 0;
    w->expansion->fractional[CORE_RES_MANA] = 900;
    CHECK_EQ(core_expansion_fire(w, tower->id), CORE_OK);
    CHECK_EQ(expansion_bank_have(w, CORE_RES_MANA), 0);
    tower->state = CORE_BSTATE_RUINED;
    CHECK_EQ(core_expansion_fire(w, tower->id), CORE_ERR_REJECTED);
    CHECK_EQ(threat_debug_raid(w, CORE_THREAT_BANDITS, CORE_DIR_NORTH, 3), CORE_OK);
    w->expansion->protected_cells[grid_index(&w->grid, keep->x, keep->y)] = 1;
    w->expansion->fractional[CORE_RES_MANA] = 900;
    hp = keep->hp;
    CHECK_EQ(core_report_building_damage(w, w->threat.raid.id, keep->id, 100), CORE_OK);
    CHECK_EQ(keep->hp, hp - 85);
    CHECK_EQ(expansion_bank_have(w, CORE_RES_MANA), 0);
    core_destroy(w);
}
void test_expansion_gate_connectivity(void) {
    World *w = expanded();
    Building *gate, *home;
    int x;
    int32_t amount;
    CHECK(w != NULL);
    gate = complete(w, CORE_BLD_PALISADE_GATE, 98, 100, 0);
    home = complete(w, CORE_BLD_COTTAGE, 100, 101, 2);
    home->door_x = 100;
    home->door_y = 100;
    w->grid.road[grid_index(&w->grid, 96, 98)] = 1;
    w->grid.road[grid_index(&w->grid, 96, 99)] = 1;
    for (x = 96; x <= 100; x++)
        if (x != 98)
            w->grid.road[grid_index(&w->grid, x, 100)] = 1;
    expansion_rebuild(w);
    CHECK(w->expansion->connected[grid_index(&w->grid, 100, 100)]);
    amount = w->bank.amount[CORE_RES_LOGS];
    CHECK_EQ(core_demolish(w, gate->id), CORE_ERR_REJECTED);
    CHECK_EQ(w->bank.amount[CORE_RES_LOGS], amount);
    CHECK(entity_find_building(w, gate->id) == gate);
    home->state = CORE_BSTATE_RUINED;
    expansion_rebuild(w);
    CHECK_EQ(expansion_remove_road(w, 97, 100), CORE_OK);
    w->grid.road[grid_index(&w->grid,97,100)] = 1;
    home->state = CORE_BSTATE_ACTIVE;
    for (x = 97; x <= 99; x++)
        w->grid.road[grid_index(&w->grid, x, 99)] = 1;
    expansion_rebuild(w);
    CHECK_EQ(core_demolish(w, gate->id), CORE_OK);
    CHECK(w->expansion->connected[grid_index(&w->grid, 100, 100)]);
    core_destroy(w);
}
void test_expansion_nearest_refuge(void) {
    World *w = expanded();
    Building *near_gate = NULL;
    Villager *v, *other;
    int x, y, ring;
    CHECK(w != NULL);
    for (y = 70; y <= 90; y++)
        for (x = 70; x <= 120; x++)
            w->grid.terrain[grid_index(&w->grid, x, y)] = CORE_TERRAIN_GRASS;
    for (ring = 0; ring < 2; ring++) {
        int left = 75 + ring * 36;
        for (y = 75; y <= 81; y++)
            for (x = left; x <= left + 6; x++)
                if (x == left || x == left + 6 || y == 75 || y == 81) {
                    Building *wall = complete(
                        w, x == left + 6 && y == 78 ? CORE_BLD_PALISADE_GATE : CORE_BLD_PALISADE, x, y, 0);
                    if (!ring && x == left + 6 && y == 78)
                        near_gate = wall;
                }
    }
    expansion_rebuild(w);
    v = w->villagers[0];
    other = w->villagers[1];
    v->current_x_milli = 84500;
    v->current_y_milli = 78500;
    v->state = CORE_VS_IDLE;
    other->current_x_milli = 84500;
    other->current_y_milli = 78500;
    w->expansion->alarm = 1;
    w->clock.tick = 72;
    expansion_people_tick(w);
    CHECK_EQ(v->state, CORE_VS_FLEE);
    CHECK_EQ(v->target_x, 80);
    CHECK_EQ(v->target_y, 78);
    CHECK(v->target == NULL);
    v->current_x_milli = 80500;
    v->current_y_milli = 78500;
    v->needs.safety = other->needs.safety = 50;
    expansion_people_tick(w);
    CHECK(v->needs.safety > other->needs.safety);
    near_gate->state = CORE_BSTATE_RUINED;
    expansion_rebuild(w);
    expansion_people_tick(w);
    CHECK(v->target_x > 100);
    CHECK(w->expansion->protected_cells[grid_index(&w->grid, v->target_x, v->target_y)]);
    core_destroy(w);
}
void test_expansion_guaranteed_starter_waves(void) {
    World *w = expanded();
    int day, last = 4, first = 0, second = 0, i;
    CHECK(w != NULL);
    w->buildings[1]->state = CORE_BSTATE_RUINED;
    expansion_rebuild(w);
    for (day = 1; day <= 14; day++) {
        w->clock.tick = (day - 1) * 144;
        w->expansion->disaster_day = day;
        w->bank.amount[CORE_RES_FOOD] = 0;
        for (i = 0; i < w->villager_count; i++) {
            Villager *v = w->villagers[i];
            v->mood = 0;
            v->low_mood_ticks = 0;
            v->needs.hunger = 100;
        }
        expansion_people_daily(w);
        for (i = 0; i < w->villager_count; i++)
            if (w->villagers[i]->incoming)
                expansion_arrival_join(w, w->villagers[i]);
        if (w->villager_count != last) {
            CHECK_EQ(w->villager_count, last + 2);
            if (!first)
                first = day;
            else
                second = day;
            last = w->villager_count;
        }
    }
    CHECK(first >= 5 && first <= 8);
    CHECK(second >= 10 && second <= 13);
    CHECK_EQ(w->villager_count, 8);
    CHECK_EQ(w->expansion->starter_waves, 2);
    CHECK_EQ(core_validate(w), 0);
    core_destroy(w);
}
void test_expansion_repair_protection(void) {
    World *w = expanded();
    Building *broken = NULL;
    int x, y;
    int32_t why = 0;
    uint8_t *mask;
    CHECK(w != NULL);
    for (y = 75; y <= 81; y++)
        for (x = 75; x <= 81; x++) {
            w->grid.terrain[grid_index(&w->grid, x, y)] = CORE_TERRAIN_GRASS;
            if (x == 75 || x == 81 || y == 75 || y == 81) {
                Building *b = complete(w, CORE_BLD_PALISADE, x, y, 0);
                if (x == 75 && y == 78)
                    broken = b;
            }
        }
    expansion_rebuild(w);
    CHECK_EQ(w->expansion->protected_area, 25);
    broken->state = CORE_BSTATE_RUINED;
    broken->hp = 0;
    expansion_rebuild(w);
    CHECK_EQ(w->expansion->protected_area, 0);
    w->bank.amount[CORE_RES_LOGS] = 2;
    w->expansion->fractional[CORE_RES_LOGS] = 400;
    w->expansion->magic_logs_milli = 2400;
    CHECK_EQ(core_repair(w, broken->id, &why), CORE_OK);
    CHECK_EQ(w->expansion->protected_area, 25);
    CHECK_EQ(w->grid.cost[grid_index(&w->grid, 75, 78)], CORE_COST_WALL);
    CHECK_EQ(w->expansion->magic_logs_milli, 1400);
    mask = (uint8_t *)malloc((size_t)w->expansion->cells);
    CHECK(mask != NULL);
    CHECK_EQ(core_expansion_copy_protection(w, NULL, 0), CORE_ERR_NULL_ARG);
    CHECK_EQ(core_expansion_copy_protection(w, mask, 1), CORE_ERR_BUFFER_TOO_SMALL);
    CHECK_EQ(core_expansion_copy_protection(w, mask, w->expansion->cells), CORE_OK);
    CHECK_EQ(mask[grid_index(&w->grid, 78, 78)], 1);
    free(mask);
    CHECK_EQ(core_validate(w), 0);
    core_destroy(w);
}
