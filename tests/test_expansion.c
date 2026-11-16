/* test_expansion.c — 확장 규칙의 저장, 회전, 공유 자원과 경제 보존 검사 */
#include "expansion.h"
#include "mem.h"
#include "test_util.h"
#include <stdio.h>
#include <string.h>

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
    CHECK_EQ(field->pending_harvest_milli, 198000);
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
    for (i = 0; i < w->expansion->cells; i++)
        w->expansion->tiles[i].explored = 1;
    CHECK_EQ(core_can_place_rotated(w, CORE_BLD_PASTURE, 80, 90, 1, &preview), CORE_OK);
    CHECK_EQ(preview.w, 4);
    CHECK_EQ(preview.h, 6);
    CHECK_EQ(preview.door_x, 84);
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
    CHECK_EQ(v->state, CORE_VS_LEAVING);
    expansion_assign(w);
    CHECK(v->home == NULL);
    core_destroy(w);
}
