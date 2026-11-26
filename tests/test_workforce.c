/* workforce — 현행 기능별 회귀 사례 */
#include "fixtures/labor.h"
#include "fixtures/lifecycle.h"

void test_labor_actual_labor_and_xp(void) {
    World *w = fx_labor_world121();
    if (!w)
        return;
    Building *b = fx_labor_building121(w, CORE_BLD_GARDEN, 80, 80);
    Villager *v = w->villagers[0];
    CHECK_EQ(entity_link_worker(b, v, 2), CORE_OK);
    v->meal_day = 1;
    v->traits[0] = v->traits[1] = -1;
    for (int i = 0; i < 36; i++) {
        w->clock.tick = 48 + i;
        fx_labor_present121(v, b);
        v->target_x = b->x + v->id % b->w;
        v->target_y = b->y + (v->id / b->w) % b->h;
        expansion_people_tick(w);
    }
    CHECK_EQ(v->skill_minutes[0], 360);
    for (int i = 0; i < 5; i++)
        CHECK_EQ(v->stat_xp[i], ((int[]){2100, 0, 1800, 1200, 450}[i]));
    CHECK_EQ(v->xp_work_minutes[0], 0);
    CHECK_EQ(v->work_day_minutes, 360);
    CHECK(expansion_labor_milli(w, b) > 0);
    v->at_target = 0;
    CHECK_EQ(expansion_working_count(w, b), 0);
    b->daily_work_milli = 7500 * b->worker_count * 540;
    CHECK(expansion_labor_milli(w, b) <= 7500);
    core_destroy(w);
}

void test_workforce_economy_and_vision(void) {
    int32_t base, mult, quantity, weights[5];
    CHECK_EQ(core_expansion_restock(1, 25, &base, &mult, &quantity), CORE_OK);
    CHECK_EQ(base, 30);
    CHECK_EQ(mult, 9000);
    CHECK_EQ(quantity, 25);
    core_expansion_restock(2, 100, &base, &mult, &quantity);
    CHECK_EQ(base, 120);
    CHECK_EQ(mult, 10000);
    CHECK_EQ(quantity, 120);
    core_expansion_restock(3, 750, &base, &mult, &quantity);
    CHECK_EQ(base, 390);
    CHECK_EQ(mult, 13000);
    CHECK_EQ(quantity, 505);
    core_expansion_restock(2, 416, &base, &mult, &quantity);
    CHECK_EQ(quantity, 140);
    core_expansion_restock(3, 725, &base, &mult, &quantity);
    CHECK_EQ(quantity, 505);
    int previous = 0;
    for (int e = 25; e <= 750; e++) {
        core_expansion_restock(3, e, &base, &mult, &quantity);
        CHECK(mult >= previous);
        CHECK_EQ(quantity % 5, 0);
        previous = mult;
    }
    CHECK_EQ(core_expansion_skill_threshold(1), 6912);
    CHECK_EQ(core_expansion_skill_threshold(2), 48 * 540);
    CHECK_EQ(core_expansion_skill_threshold(3), 120 * 540);
    CHECK_EQ(core_expansion_weights(CORE_BLD_BARRACKS, weights), CORE_OK);
    CHECK_EQ(weights[0], 35);
    CHECK_EQ(weights[4], 0);
    core_expansion_weights(CORE_BLD_TOWER, weights);
    CHECK_EQ(weights[1], 40);
    CHECK_EQ(weights[3], 40);
    CoreNewGameParams p = {0};
    World *w = NULL;
    strcpy(p.castle_name, u8"교역 식량 실내 시야");
    p.seed = 42;
    p.ruleset = 1;
    CHECK_EQ(test_create_settlement(&p, &w), CORE_OK);
    if (!w)
        return;
    CHECK_EQ(w->expansion->market_next_restock_day, 5);
    w->clock.tick = 4 * 144 + 36;
    expansion_candidates(w);
    CHECK_EQ(w->expansion->trade_stock[CORE_RES_FOOD], 30);
    Building *shop = entity_add_building(w, CORE_BLD_MARKET);
    CHECK(shop != NULL);
    if (!shop) {
        core_destroy(w);
        return;
    }
    shop->state = CORE_BSTATE_ACTIVE;
    shop->x = 40;
    shop->y = 40;
    shop->w = shop->h = 2;
    shop->door_x = 41;
    shop->door_y = 42;
    shop->hp = 200;
    CHECK_EQ(entity_link_worker(shop, w->villagers[0], 2), CORE_OK);
    CHECK(core_expansion_tradable(w, CORE_RES_FOOD));
    CHECK(!core_expansion_tradable(w, CORE_RES_GOLD_ORE));
    CHECK(!core_expansion_tradable(w, CORE_RES_MANA));
    w->expansion->market_next_restock_day = 5;
    w->clock.tick = 4 * 144 + 36;
    w->expansion->market_staff_day = 4;
    w->expansion->market_staff_efficiency = 750;
    expansion_candidates(w);
    CHECK_EQ(w->expansion->trade_stock[CORE_RES_FOOD], 155);
    CHECK_EQ(w->expansion->market_next_restock_day, 7);
    Villager *v = w->villagers[0];
    v->incoming = 0;
    v->inside_building = shop->id;
    v->current_x_milli = 41500;
    v->current_y_milli = 42500;
    expansion_visibility(w);
    CHECK(w->expansion->visible[grid_index(&w->grid, 45, 42)]);
    v->incoming = 1;
    expansion_visibility(w);
    CHECK(!w->expansion->visible[grid_index(&w->grid, 45, 42)]);
    v->incoming = 0;
    v->state = CORE_VS_LEAVING;
    expansion_visibility(w);
    CHECK(!w->expansion->visible[grid_index(&w->grid, 45, 42)]);
    v->state = CORE_VS_WORKING;
    CHECK_EQ(entity_link_worker(shop, v, 2), CORE_OK);
    v->target = shop;
    v->target_x = 41;
    v->target_y = 42;
    v->at_target = 1;
    w->clock.tick = 48;
    v->needs.hunger = 10;
    v->health = 100;
    v->meal_day = 0;
    w->bank.amount[CORE_RES_FOOD] = 0;
    for (int i = 1; i < w->villager_count; i++)
        w->villagers[i]->incoming = 1;
    expansion_people_tick(w);
    CHECK_EQ(v->state, CORE_VS_WORKING);
    CHECK(v->skill_minutes[9] > 0);
    CHECK(w->expansion->market_staff_efficiency >= 25);
    CHECK_EQ(w->expansion->market_staff_day, 1);
    int32_t requirements[CORE_RES_COUNT];
    core_expansion_daily_requirements(w, requirements);
    CHECK_EQ(requirements[CORE_RES_FOOD], 1000);
    w->bank.amount[CORE_RES_FOOD] = 50;
    v->meal_day = 0;
    v->target = world_keep(w);
    v->target_x = v->target->door_x;
    v->target_y = v->target->door_y;
    v->at_target = 1;
    v->current_x_milli = v->target_x * 1000 + 500;
    v->current_y_milli = v->target_y * 1000 + 500;
    v->hub_revision = -1;
    expansion_people_tick(w);
    CHECK_EQ(expansion_bank_have(w, CORE_RES_FOOD), 49000);
    w->clock.tick = 60;
    v->needs.hunger = 1;
    expansion_people_tick(w);
    CHECK_EQ(expansion_bank_have(w, CORE_RES_FOOD), 49000);
    core_destroy(w);
}

void test_workforce_uniform_route(void) {
    for (int kind = 0; kind < 4; kind++) {
        CoreNewGameParams p = {0};
        World *w = NULL, *loaded = NULL;
        int32_t line = 0;
        strcpy(p.castle_name, u8"일터 환복 동선");
        p.seed = 42;
        p.ruleset = 1;
        CHECK_EQ(test_create_settlement(&p, &w), CORE_OK);
        if (!w)
            return;
        int type =
            (int[]){CORE_BLD_HUNTER_CAMP, CORE_BLD_HUNTING_LODGE, CORE_BLD_BARRACKS, CORE_BLD_GARRISON}[kind];
        Building *b = entity_add_building(w, type);
        CHECK(b != NULL);
        if (!b) {
            core_destroy(w);
            return;
        }
        b->state = CORE_BSTATE_ACTIVE;
        b->x = 80;
        b->y = 80;
        b->w = expansion_spec(type)->w;
        b->h = expansion_spec(type)->h;
        b->door_x = 81;
        b->door_y = 80 + b->h;
        b->hp = w->defs.buildings[type].max_hp;
        expansion_ensure_building_serials(w);
        expansion_rebuild(w);
        Villager *v = w->villagers[0];
        CHECK_EQ(entity_link_worker(b, v, 8), CORE_OK);
        v->needs.hunger = v->needs.rest = 100;
        v->health = 100;
        v->meal_day = 1;
        w->clock.tick = 54;
        expansion_people_tick(w);
        CHECK_EQ(v->mission, CORE_MISSION_EQUIP);
        CHECK_EQ(v->target, b);
        CHECK_EQ(core_expansion_outfit(w, v->id), 0);
        v->at_target = 1;
        v->current_x_milli = b->door_x * 1000 + 500;
        v->current_y_milli = b->door_y * 1000 + 500;
        expansion_people_tick(w);
        CHECK_EQ(v->inside_building, b->id);
        CHECK_EQ(core_expansion_outfit(w, v->id), 0);
        w->clock.tick += 2;
        expansion_people_tick(w);
        CHECK_EQ(core_expansion_outfit(w, v->id), kind + 1);
        CHECK_EQ(v->inside_building, 0);
        v->current_x_milli = 97500;
        v->current_y_milli = 93500;
        v->at_target = 0;
        w->clock.tick = 115;
        expansion_people_tick(w);
        CHECK_EQ(v->mission, CORE_MISSION_UNEQUIP);
        CHECK_EQ(v->target, b);
        CHECK_EQ(core_expansion_outfit(w, v->id), kind + 1);
        v->at_target = 1;
        v->current_x_milli = b->door_x * 1000 + 500;
        v->current_y_milli = b->door_y * 1000 + 500;
        expansion_people_tick(w);
        CHECK_EQ(v->inside_building, b->id);
        CHECK_EQ(core_save(w, "uniform_route.sav"), CORE_OK);
        CHECK_EQ(core_load("uniform_route.sav", NULL, &loaded, &line), CORE_OK);
        if (loaded) {
            uint64_t x, y;
            core_state_hash(w, &x);
            core_state_hash(loaded, &y);
            CHECK_EQ(x, y);
            core_destroy(loaded);
        }
        w->clock.tick += 2;
        expansion_people_tick(w);
        CHECK_EQ(core_expansion_outfit(w, v->id), 0);
        CHECK_EQ(v->mission_target, 0);
        if (kind >= 2) {
            w->expansion->alarm = 1;
            CoreWorkSchedule schedule;
            expansion_schedule(w, v, &schedule);
            CHECK(schedule.on_duty);
        }
        core_destroy(w);
        remove("uniform_route.sav");
        remove("uniform_route.sav.bak");
    }
}
