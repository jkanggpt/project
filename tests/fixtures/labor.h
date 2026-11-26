#ifndef CASTLE_TEST_FIXTURE_LABOR_H
#define CASTLE_TEST_FIXTURE_LABOR_H
/* labor.h — 현재 통합 테스트의 공통 준비 조건 */
#include "expansion.h"
#include "population.h"
#include "test_util.h"
#include <stdio.h>
#include <string.h>

static inline World *fx_labor_world121(void) {
    CoreNewGameParams p = {0};
    World *w = NULL;
    strcpy(p.castle_name, u8"개정 시험");
    p.seed = 42;
    p.ruleset = 1;
    p.difficulty = 1;
    if (core_create(&p, NULL, &w) != CORE_OK)
        test_fail(__FILE__, __LINE__, "labor world");
    return w;
}

static inline Building *fx_labor_building121(World *w, int type, int x, int y) {
    Building *b = entity_add_building(w, type);
    const ExpansionBuildingSpec *s = expansion_spec(type);
    if (!b)
        return NULL;
    b->x = x;
    b->y = y;
    b->w = s->w;
    b->h = s->h;
    b->door_x = x + b->w / 2;
    b->door_y = y + b->h;
    b->hp = s->hp;
    b->state = CORE_BSTATE_ACTIVE;
    b->completed_day = 1;
    b->auto_assign = 0;
    return b;
}

static inline void fx_labor_present121(Villager *v, Building *b) {
    v->target = b;
    v->state = CORE_VS_WORKING;
    v->at_target = 1;
    v->incoming = 0;
    v->target_x = b->door_x;
    v->target_y = b->door_y;
    v->loc_x = b->door_x;
    v->loc_y = b->door_y;
    v->current_x_milli = b->door_x * 1000 + 500;
    v->current_y_milli = b->door_y * 1000 + 500;
}
#endif
