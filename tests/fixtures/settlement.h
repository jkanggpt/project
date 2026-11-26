#ifndef CASTLE_TEST_FIXTURE_SETTLEMENT_H
#define CASTLE_TEST_FIXTURE_SETTLEMENT_H
/* settlement.h — 현재 통합 테스트의 공통 준비 조건 */
#include "expansion.h"
#include "population.h"
#include "test_util.h"
#include "villager_ai.h"
#include <stdio.h>
#include <string.h>

static inline World *fx_settlement_make_world(void) {
    CoreNewGameParams p = {0};
    World *w = NULL;
    strcpy(p.castle_name, u8"정착지 시험");
    p.seed = 42;
    p.difficulty = 1;
    p.ruleset = 1;
    if (test_create_settlement(&p, &w) != CORE_OK)
        test_fail(__FILE__, __LINE__, "settlement world");
    return w;
}

static inline void fx_settlement_add_people(World *w, int count) {
    for (int i = 0; i < count; i++) {
        Villager *v = population_create(w, w->keep_x, w->keep_y + 4);
        if (!v)
            test_fail(__FILE__, __LINE__, "settlement population");
    }
}

static inline Building *fx_settlement_add_building(World *w, int type, int x, int y) {
    Building *b = entity_add_building(w, type);
    const ExpansionBuildingSpec *spec = expansion_spec(type);
    if (!b) {
        test_fail(__FILE__, __LINE__, "settlement building");
        return NULL;
    }
    b->x = x;
    b->y = y;
    b->w = spec->w;
    b->h = spec->h;
    b->door_x = x + b->w / 2;
    b->door_y = y + b->h;
    b->state = CORE_BSTATE_ACTIVE;
    b->hp = spec->hp;
    return b;
}
#endif
