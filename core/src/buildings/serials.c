/* serials — 현재 기능의 단일 구현 */
#include "expansion.h"
#include "placement.h"
#include "rank.h"
#include "villager_ai.h"
#include <stdlib.h>
#include <string.h>

void expansion_ensure_building_serials(World *w) {
    ExpansionState *e = w->expansion;
    for (int i = 0; i < w->building_count; i++) {
        Building *b = w->buildings[i];
        if (b->type_serial > e->next_type_serial[b->type])
            e->next_type_serial[b->type] = b->type_serial;
        if (!b->type_serial)
            b->type_serial = ++e->next_type_serial[b->type];
    }
}
