/* integration — 현행 기능별 회귀 사례 */
#include "fixtures/labor.h"
#include "fixtures/lifecycle.h"
#include "fixtures/settlement.h"

void test_settlement_long_happiness(void) {
    World *w = fx_settlement_make_world();
    if (!w)
        return;
    Villager *v = w->villagers[0];
    v->long_needs[0] = v->long_needs[1] = v->long_needs[2] = 100000;
    v->mood = 0;
    v->needs.hunger = 30;
    int before = expansion_mood(w, v);
    v->needs.hunger = 100;
    CHECK_EQ(expansion_mood(w, v), before);
    v->mood = -200;
    CHECK_EQ(expansion_mood(w, v), exp_clamp(before - 36, 0, 100));
    v->mood = 0;
    v->need_samples = 24;
    v->need_sums[0] = 24 * 20;
    v->need_sums[1] = 24 * 60;
    v->need_sums[2] = 24 * 80;
    expansion_wellbeing_daily(w, v);
    CHECK_EQ(v->long_needs[0], 80000);
    CHECK_EQ(v->long_needs[1], 90000);
    CHECK_EQ(v->long_needs[2], 95000);
    CHECK_EQ(v->need_samples, 0);
    v->health = 20;
    w->clock.tick = 100;
    expansion_hurt(w, v, 4, 8);
    CHECK_EQ(v->mood, -14);
    expansion_hurt(w, v, 4, 8);
    CHECK_EQ(v->mood, -14);
    w->clock.tick = 112;
    expansion_hurt(w, v, 4, 8);
    CHECK_EQ(v->mood, -19);
    core_destroy(w);
}

void test_settlement_manor_promotion_save(void) {
    World *w = fx_settlement_make_world(), *loaded = NULL;
    if (!w)
        return;
    int32_t why = 0, line = 0;
    Building *keep = world_keep(w);
    CHECK_EQ(keep->w, 4);
    CHECK_EQ(keep->h, 4);
    CHECK_EQ(core_rotate_building(w, keep->id, 1, &why), CORE_ERR_REJECTED);
    CHECK_EQ(keep->rotation, 0);
    fx_settlement_add_people(w, 4);
    for (int i = 0; i < w->villager_count; i++)
        w->villagers[i]->happiness = 90;
    w->expansion->effective_roads = 10;
    int gold = w->bank.amount[CORE_RES_GOLD];
    CHECK_EQ(core_rank_up(w, &why), CORE_OK);
    CHECK_EQ(w->bank.amount[CORE_RES_GOLD], gold - 20);
    CHECK_EQ(core_rank_up(w, &why), CORE_ERR_REJECTED);
    CHECK_EQ(w->bank.amount[CORE_RES_GOLD], gold - 20);
    /* 공사 완료는 시작 때의 조건을 다시 검사하지 않습니다. */
    w->expansion->effective_roads = 0;
    for (int i = 0; i < 72; i++)
        expansion_promotion_tick(w);
    CHECK_EQ(w->rank, 0);
    CHECK_EQ(w->expansion->promotion_remaining, 72);
    CHECK_EQ(core_save(w, "settlement_promotion.sav"), CORE_OK);
    CHECK_EQ(core_load("settlement_promotion.sav", NULL, &loaded, &line), CORE_OK);
    if (loaded) {
        CHECK_EQ(loaded->expansion->promotion_remaining, 72);
        for (int i = 0; i < 72; i++)
            expansion_promotion_tick(loaded);
        CHECK_EQ(loaded->rank, 1);
        CHECK_EQ(loaded->expansion->promotion_state, 0);
        core_destroy(loaded);
    }
    core_destroy(w);
    remove("settlement_promotion.sav");
    remove("settlement_promotion.sav.bak");
}

void test_settlement_weighted_hubs(void) {
    World *w = fx_settlement_make_world();
    if (!w)
        return;
    Building *food = fx_settlement_add_building(w, CORE_BLD_GRANARY, 110, 100),
             *all = fx_settlement_add_building(w, CORE_BLD_WAREHOUSE, 116, 100);
    Villager *v = w->villagers[0];
    for (int y = 98; y < 109; y++)
        for (int x = 106; x < 124; x++) {
            int p = grid_index(&w->grid, x, y);
            w->grid.terrain[p] = CORE_TERRAIN_GRASS;
            w->expansion->tiles[p].biome = 0;
        }
    expansion_rebuild(w);
    w->expansion->connected[grid_index(&w->grid, food->door_x, food->door_y)] = 1;
    w->expansion->connected[grid_index(&w->grid, all->door_x, all->door_y)] = 1;
    v->current_x_milli = food->door_x * 1000 + 500;
    v->current_y_milli = food->door_y * 1000 + 500;
    v->hub_revision = -1;
    CHECK(expansion_hub(w, v, 1, NULL) == food);
    v->hub_revision = -1;
    CHECK(expansion_hub(w, v, 0, NULL) == all);
    v->hub_revision = -1;
    w->clock.tick = 42;
    v->meal_day = 0;
    v->needs.hunger = 20;
    v->target = food;
    v->target_x = food->door_x;
    v->target_y = food->door_y;
    v->at_target = 1;
    int before = expansion_bank_have(w, CORE_RES_FOOD);
    expansion_people_tick(w);
    CHECK_EQ(expansion_bank_have(w, CORE_RES_FOOD), before - 1000);
    expansion_people_tick(w);
    CHECK_EQ(expansion_bank_have(w, CORE_RES_FOOD), before - 1000);
    v->hub_revision = -1;
    w->expansion->alarm = 1;
    CHECK(expansion_hub(w, v, 1, NULL) == world_keep(w));
    core_destroy(w);
}

void test_settlement_water_temperature_and_fishing(void) {
    World *w = fx_settlement_make_world();
    if (!w)
        return;
    int boundary[512], land = 0;
    for (int p = 0; p < w->expansion->cells; p++) {
        w->grid.terrain[p] = CORE_TERRAIN_GRASS;
        w->expansion->tiles[p].biome = 0;
    }
    for (int y = 20; y < 30; y++)
        for (int x = 20; x < 40; x++) {
            int p = grid_index(&w->grid, x, y);
            w->grid.terrain[p] = CORE_TERRAIN_WATER;
            w->expansion->tiles[p].biome = CORE_BIOME_LAKE;
        }
    for (int y = 18; y <= 31; y++)
        for (int x = 18; x <= 41; x++) {
            int dx = x < 20 ? 20 - x : x > 39 ? x - 39 : 0, dy = y < 20 ? 20 - y : y > 29 ? y - 29 : 0;
            if (dx + dy > 0 && dx + dy <= 2)
                boundary[land++] = grid_index(&w->grid, x, y);
        }
    int threshold = (land * 6 + 9) / 10;
    for (int i = 0; i < (threshold - 1) / 2; i++)
        w->expansion->tiles[boundary[i]].biome = CORE_BIOME_SNOW;
    if ((threshold - 1) & 1)
        w->expansion->tiles[boundary[(threshold - 1) / 2]].biome = CORE_BIOME_TAIGA;
    expansion_water_bodies(w);
    CoreWaterView water;
    core_expansion_water(w, 1, &water);
    CHECK_EQ(water.area, 200);
    CHECK_EQ(water.cold, 0);
    int used = (threshold - 1) / 2;
    if ((threshold - 1) & 1)
        w->expansion->tiles[boundary[used]].biome = CORE_BIOME_SNOW;
    else
        w->expansion->tiles[boundary[used]].biome = CORE_BIOME_TAIGA;
    expansion_water_bodies(w);
    core_expansion_water(w, 1, &water);
    CHECK_EQ(water.cold, 1);
    CHECK_EQ(water.capacity_milli, 44100);
    Building *a = fx_settlement_add_building(w, CORE_BLD_PORT, 18, 20),
             *b = fx_settlement_add_building(w, CORE_BLD_PORT, 40, 24);
    CHECK_EQ(entity_link_worker(a, w->villagers[0], 8), CORE_OK);
    CHECK_EQ(entity_link_worker(b, w->villagers[1], 8), CORE_OK);
    expansion_rebuild(w);
    expansion_capacity(w);
    w->expansion->connected[grid_index(&w->grid, a->door_x, a->door_y)] = 1;
    w->expansion->connected[grid_index(&w->grid, b->door_x, b->door_y)] = 1;
    w->expansion->distance[grid_index(&w->grid, a->door_x, a->door_y)] = 0;
    w->expansion->distance[grid_index(&w->grid, b->door_x, b->door_y)] = 0;
    a->daily_work_milli = b->daily_work_milli = w->defs.buildings[CORE_BLD_PORT].worker_slots * 540000;
    w->bank.amount[CORE_RES_FOOD] = 0;
    w->expansion->fractional[CORE_RES_FOOD] = 0;
    expansion_daily(w);
    CHECK_EQ(expansion_bank_have(w, CORE_RES_FOOD), 30000);
    w->clock.tick = 36 * 144;
    w->nature.event = CORE_NATURE_NONE;
    a->daily_work_milli = b->daily_work_milli = w->defs.buildings[CORE_BLD_PORT].worker_slots * 540000;
    w->bank.amount[CORE_RES_FOOD] = 0;
    w->expansion->fractional[CORE_RES_FOOD] = 0;
    expansion_daily(w);
    CHECK_EQ(expansion_bank_have(w, CORE_RES_FOOD), 30000);
    core_destroy(w);
}

void test_settlement_migrant_fog_and_save(void) {
    World *w = fx_settlement_make_world(), *loaded = NULL;
    if (!w)
        return;
    Villager *v = w->villagers[0];
    int p = grid_index(&w->grid, 10, 10);
    uint64_t before = 0, after = 0;
    v->current_x_milli = v->current_y_milli = 10500;
    v->incoming = 1;
    memset(w->expansion->static_visible, 0, (size_t)w->expansion->cells);
    w->expansion->tiles[p].explored = 0;
    expansion_visibility(w);
    CHECK_EQ(w->expansion->visible[p], 0);
    v->incoming = 0;
    v->state = CORE_VS_LEAVING;
    expansion_visibility(w);
    CHECK_EQ(w->expansion->visible[p], 0);
    v->state = CORE_VS_IDLE;
    expansion_visibility(w);
    CHECK_EQ(w->expansion->visible[p], 1);
    v->mood = -123;
    v->happiness = 24;
    v->incoming = 1;
    w->expansion->trade_pressure[2] = 123;
    w->expansion->trade_remainder[2] = 829232;
    w->expansion->refill_day = 16;
    CHECK_EQ(core_state_hash(w, &before), CORE_OK);
    CHECK_EQ(core_save(w, "settlement_state.sav"), CORE_OK);
    int32_t line = 0;
    CHECK_EQ(core_load("settlement_state.sav", NULL, &loaded, &line), CORE_OK);
    if (loaded) {
        core_state_hash(loaded, &after);
        CHECK_EQ(before, after);
        CHECK_EQ(loaded->villagers[0]->mood, -123);
        core_destroy(loaded);
    }
    core_destroy(w);
    remove("settlement_state.sav");
    remove("settlement_state.sav.bak");
}

void test_labor_event_labor_and_services(void) {
    World *w = fx_labor_world121();
    if (!w)
        return;
    Building *b = fx_labor_building121(w, CORE_BLD_CROPLAND, 80, 80);
    Villager *v = w->villagers[0];
    entity_link_worker(b, v, 2);
    fx_labor_present121(v, b);
    v->traits[0] = v->traits[1] = -1;
    int e = expansion_work_percent(w, v, b->type) * 100;
    w->nature.event = CORE_NATURE_DROUGHT;
    CHECK_EQ(expansion_begin_work(w, v, b), 10);
    CHECK_EQ(b->crop_labor_milli, e / 4);
    w->nature.event = CORE_NATURE_BUMPER;
    expansion_begin_work(w, v, b);
    CHECK_EQ(b->crop_labor_milli, e / 4 + e * 24 / 10);
    w->research.done |= 1u << 17;
    w->nature.event = CORE_NATURE_NONE;
    expansion_begin_work(w, v, b);
    CHECK_EQ(b->crop_labor_milli, e / 4 + e * 24 / 10 + e * 108 / 100);
    CHECK_EQ(expansion_agriculture_event(w, CORE_BLD_IRRIGATED_FARM), 1000);
    w->nature.event = CORE_NATURE_DROUGHT;
    CHECK_EQ(expansion_agriculture_event(w, CORE_BLD_IRRIGATED_FARM), 650);
    w->research.done |= 1u << 2;
    CHECK_EQ(expansion_agriculture_event(w, CORE_BLD_IRRIGATED_FARM), 750);
    Building *school = fx_labor_building121(w, CORE_BLD_SCHOOL, 70, 70);
    entity_link_worker(school, w->villagers[1], 3);
    w->villagers[1]->stats.intellect = 5;
    CHECK(!expansion_qualified(w, school));
    entity_link_worker(school, w->villagers[2], 3);
    w->villagers[2]->stats.intellect = 2;
    CHECK(expansion_qualified(w, school));
    core_destroy(w);
}

void test_labor_visibility_trade_and_centers(void) {
    World *w = fx_labor_world121();
    if (!w)
        return;
    Building *b = fx_labor_building121(w, CORE_BLD_WATCHTOWER, 40, 40);
    Villager *v = w->villagers[0];
    entity_link_worker(b, v, 2);
    expansion_rebuild(w);
    expansion_visibility(w);
    int p = grid_index(&w->grid, 40, 35);
    CHECK(!w->expansion->visible[p]);
    fx_labor_present121(v, b);
    expansion_visibility(w);
    CHECK(w->expansion->visible[p]);
    v->at_target = 0;
    expansion_visibility(w);
    CHECK(!w->expansion->visible[p]);
    CHECK(w->expansion->tiles[p].explored);
    CHECK_EQ(expansion_center_distance(b, 40, 40), 0);
    CHECK_EQ(expansion_center_distance(b, 40, 35), 5);
    Building dummy = {0};
    dummy.x = 10;
    dummy.y = 20;
    dummy.w = 4;
    dummy.h = 2;
    CHECK_EQ(expansion_center_distance(&dummy, 11, 20), 0);
    CHECK_EQ(expansion_center_distance(&dummy, 12, 21), 0);
    CHECK_EQ(expansion_center_distance(&dummy, 9, 19), 3);
    Building *trade = fx_labor_building121(w, CORE_BLD_STALL, 60, 60);
    trade->state = CORE_BSTATE_CONSTRUCTING;
    CorePlacementView result;
    expansion_place_check(w, CORE_BLD_MARKET, 65, 65, 0, 0, &result);
    CHECK_EQ(result.result, CORE_PLACE_TRADE_EXISTS);
    entity_remove_building(w, trade);
    w->rank = 5;
    expansion_place_check(w, CORE_BLD_MARKET, 65, 65, 0, 0, &result);
    CHECK_EQ(result.result, CORE_PLACE_NOT_VISIBLE);
    core_destroy(w);
}

void test_lifecycle_rules_and_save(void) {
    CoreNewGameParams p = {0};
    World *w = NULL, *loaded = NULL;
    int32_t line = 0;
    strcpy(p.castle_name, u8"lifecycle 저장 시험");
    p.seed = 7;
    p.difficulty = 1;
    p.ruleset = 1;
    CHECK_EQ(test_create_settlement(&p, &w), CORE_OK);
    if (!w)
        return;
    CHECK_EQ(CORE_BLD_COUNT, 73);
    CHECK_EQ(CORE_SAVE_VERSION, 14);
    CHECK_EQ(core_expansion_tree_mature_age(3), 48000);
    CHECK_EQ(expansion_spec(CORE_BLD_COTTAGE)->w, 2);
    CHECK_EQ(expansion_spec(CORE_BLD_COTTAGE)->h, 3);
    CHECK_EQ(expansion_spec(CORE_BLD_BARRACKS)->rank, 1);
    CHECK_EQ(expansion_spec(CORE_BLD_GARRISON)->workers, 6);
    for (int i = 0; i < w->expansion->cells; i++)
        CHECK(w->expansion->tiles[i].stone_milli > 0);
    CHECK(w->expansion->water_count >= 5);
    int large = 0;
    for (int i = 0; i < w->expansion->water_count; i++)
        large += w->expansion->water_area[i] >= 250;
    CHECK(large > 0);
    CHECK_EQ(core_validate(w), 0);
    CHECK_EQ(core_tick(w, 50), CORE_OK);
    Villager *v = w->villagers[0];
    int baseline[5];
    memcpy(baseline, v->stat_xp, sizeof(baseline));
    expansion_gain_xp(v, 7, CORE_BLD_TOWER, 180);
    int32_t actor[20] = {1,
                         v->current_x_milli,
                         v->current_y_milli,
                         12000,
                         42000,
                         1,
                         0,
                         0,
                         0,
                         0,
                         0,
                         2100,
                         5,
                         0,
                         2,
                         1,
                         3,
                         96,
                         97,
                         1};
    int32_t clock[2] = {1250, 1},
            projectile[13] = {95000, 98000, 96000, 99000, 400, 1600, 0, 0, 1, 1, 6, 50, 1};
    CHECK_EQ(core_expansion_report_runtime(w, CORE_RUNTIME_ACTOR, v->id, actor, 20), CORE_OK);
    CHECK_EQ(core_expansion_report_runtime(w, CORE_RUNTIME_CLOCK, 0, clock, 2), CORE_OK);
    CHECK_EQ(core_expansion_report_runtime(w, CORE_RUNTIME_PROJECTILE, 0, projectile, 13), CORE_OK);
    CHECK_EQ(core_save(w, "lifecycle_roundtrip.sav"), CORE_OK);
    int rc = core_load("lifecycle_roundtrip.sav", NULL, &loaded, &line);
    if (rc != CORE_OK)
        fprintf(stderr, "lifecycle load result=%d line=%d\n", rc, line);
    CHECK_EQ(rc, CORE_OK);
    if (loaded) {
        uint64_t a, b;
        core_state_hash(w, &a);
        core_state_hash(loaded, &b);
        CHECK_EQ(a, b);
        int32_t actual[20] = {0};
        CHECK_EQ(core_expansion_copy_runtime(loaded, CORE_RUNTIME_ACTOR, v->id, actual, 20), CORE_OK);
        CHECK(!memcmp(actor, actual, sizeof(actor)));
        CHECK_EQ(core_expansion_copy_runtime(loaded, CORE_RUNTIME_PROJECTILE, 0, actual, 13), CORE_OK);
        CHECK(!memcmp(projectile, actual, sizeof(projectile)));
        Villager *resumed = loaded->villagers[0];
        expansion_gain_xp(resumed, 7, CORE_BLD_BARRACKS, 180);
        for (int i = 0; i < 5; i++)
            CHECK_EQ(resumed->stat_xp[i] - baseline[i], ((int[]){1050, 2100, 1350, 1500, 0})[i]);
        core_destroy(loaded);
    }
    Building *post = entity_add_building(w, CORE_BLD_OUTPOST);
    CHECK(post != NULL);
    if (post) {
        post->state = CORE_BSTATE_ACTIVE;
        post->x = 90;
        post->y = 90;
        post->w = post->h = 1;
        post->door_x = 90;
        post->door_y = 91;
        post->hp = 110;
        Villager *day = w->villagers[0], *night = w->villagers[1];
        CoreWorkSchedule schedule;
        CHECK_EQ(core_assign_worker(w, day->id, post->id), CORE_OK);
        CHECK_EQ(core_assign_worker(w, night->id, post->id), CORE_OK);
        CHECK_EQ(day->shift_kind, 0);
        CHECK_EQ(night->shift_kind, 1);
        w->clock.tick = 108;
        expansion_schedule(w, day, &schedule);
        CHECK(!schedule.on_duty);
        expansion_schedule(w, night, &schedule);
        CHECK(schedule.on_duty);
        w->clock.tick = 144 + 54;
        CHECK_EQ(core_expansion_set_shift(w, day->id, 1), CORE_OK);
        CHECK_EQ(night->shift_kind, 0);
        CHECK_EQ(core_expansion_set_shift(w, day->id, 0), CORE_ERR_REJECTED);
        w->clock.tick = 144 + 107;
        night->state = CORE_VS_WORKING;
        night->target = post;
        night->at_target = 1;
        night->current_x_milli = 90500;
        night->current_y_milli = 91500;
        CHECK_EQ(core_expansion_fire(w, post->id), CORE_OK);
        w->clock.tick = 144 + 108;
        expansion_schedule(w, night, &schedule);
        CHECK(schedule.on_duty);
        expansion_schedule(w, day, &schedule);
        CHECK(!schedule.on_duty);
        w->clock.tick = 144 + 114;
        expansion_schedule(w, day, &schedule);
        CHECK(schedule.on_duty);
    }
    /* 나무 갱신 뒤에도 재로드와 같은 생물군계 이동 비용을 유지합니다. */
    expansion_rebuild(w);
    expansion_growth(w);
    uint8_t *cost = malloc((size_t)w->expansion->cells);
    CHECK(cost != NULL);
    if (cost) {
        memcpy(cost, w->grid.cost, (size_t)w->expansion->cells);
        expansion_terrain_costs(w);
        CHECK(!memcmp(cost, w->grid.cost, (size_t)w->expansion->cells));
        free(cost);
    }
    w->clock.tick = 400;
    v = w->villagers[0];
    v->mood = 0;
    expansion_mood_event(w, v, 13, -2, 10001, 18, 0);
    expansion_mood_event(w, v, 13, -2, 10001, 18, 0);
    CHECK_EQ(v->mood, -2);
    expansion_mood_event(w, v, 13, -2, 10002, 18, 0);
    CHECK_EQ(v->mood, -4);
    w->clock.tick += 18;
    expansion_mood_event(w, v, 13, -2, 10001, 18, 0);
    CHECK_EQ(v->mood, -6);
    expansion_mood_event(w, v, 17, -5, 10001, 12, 0);
    expansion_mood_event(w, v, 17, -5, 10002, 12, 0);
    CHECK_EQ(v->mood, -11);
    expansion_mood_event(w, v, 15, -3, 77, 1000000000, 0);
    for (int n = 0; n < 40; n++)
        expansion_mood_event(w, v, 13, -1, 20000 + n, 18, 0);
    int once = v->mood;
    expansion_mood_event(w, v, 15, -3, 77, 1000000000, 0);
    CHECK_EQ(v->mood, once);
    Building plan = {0};
    plan.type = CORE_BLD_SMELTER;
    int in[CORE_RES_COUNT], out[CORE_RES_COUNT], fuel = 0;
    CHECK_EQ(expansion_recipe_plan(w, &plan, 1000, 1000, 0, in, out, &fuel), 1000);
    CHECK_EQ(in[CORE_RES_IRON_ORE], 8000);
    CHECK_EQ(out[CORE_RES_IRON], 4000);
    /* 최근 사건과 노동 분수는 새 저장에도 그대로 남습니다. */
    CHECK_EQ(core_save(w, "lifecycle_events.sav"), CORE_OK);
    loaded = NULL;
    CHECK_EQ(core_load("lifecycle_events.sav", NULL, &loaded, &line), CORE_OK);
    if (loaded) {
        uint64_t a, b;
        core_state_hash(w, &a);
        core_state_hash(loaded, &b);
        CHECK_EQ(a, b);
        core_destroy(loaded);
        remove("lifecycle_events.sav");
        remove("lifecycle_events.sav.bak");
    }
    core_destroy(w);
    if (rc == CORE_OK) {
        remove("lifecycle_roundtrip.sav");
        remove("lifecycle_roundtrip.sav.bak");
    }
}
