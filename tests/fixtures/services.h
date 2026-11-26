#ifndef CASTLE_TEST_FIXTURE_SERVICES_H
#define CASTLE_TEST_FIXTURE_SERVICES_H
/* services.h — 현재 통합 테스트의 공통 준비 조건 */
#include "expansion.h"
#include "population.h"
#include "test_util.h"
#include <stdio.h>
#include <string.h>
/* 수작업 배치 시나리오의 기준점은 (96,96)입니다. 지도는 현재160규칙으로 생성합니다. */

static inline World *fx_services_fresh(int seed) {
    CoreNewGameParams p = {0};
    World *w = NULL;
    strcpy(p.castle_name, u8"시설 검사");
    p.ruleset = 1;
    p.seed = seed;
    if (core_create(&p, NULL, &w) != CORE_OK)
        return NULL;
    return w;
}

static inline Building *fx_services_facility(World *w, int type, int x, int y) {
    Building *b = entity_add_building(w, type);
    if (!b)
        return NULL;
    const ExpansionBuildingSpec *s = expansion_spec(type);
    b->x = x;
    b->y = y;
    b->w = s->w;
    b->h = s->h;
    b->hp = s->hp;
    b->state = CORE_BSTATE_ACTIVE;
    b->door_x = x + s->w / 2;
    b->door_y = y + s->h;
    b->auto_assign = 0;
    return b;
}
#endif
