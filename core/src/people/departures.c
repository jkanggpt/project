/* departures — 현재 기능의 단일 구현 */
#include "expansion.h"
#include "placement.h"
#include "rank.h"
#include "villager_ai.h"
#include <stdlib.h>
#include <string.h>

int expansion_person_exit(World *w, const Villager *v, int *out_x, int *out_y) {
    int x = v->current_x_milli / 1000, y = v->current_y_milli / 1000;
    const Building *inside = entity_find_building(w, v->boat_port_id ? v->boat_port_id : v->inside_building);
    if (inside) {
        x = inside->door_x;
        y = inside->door_y;
    }
    Grid *g = &w->grid;
    if (!grid_in_bounds(g, x, y))
        return 0;
    int head = 0, tail = 0, *queue = g->scratch_queue;
    memset(g->scratch_mark, 0, (size_t)w->expansion->cells);
    int start = grid_index(g, x, y);
    queue[tail++] = start;
    g->scratch_mark[start] = 1;
    static const int dx[4] = {0, 1, 0, -1}, dy[4] = {-1, 0, 1, 0};
    while (head < tail) {
        int p = queue[head++];
        x = p % g->width;
        y = p / g->width;
        if (x == 0 || y == 0 || x == g->width - 1 || y == g->height - 1) {
            *out_x = x;
            *out_y = y;
            return 1;
        }
        for (int d = 0; d < 4; d++) {
            int xx = x + dx[d], yy = y + dy[d];
            if (!grid_in_bounds(g, xx, yy))
                continue;
            int q = grid_index(g, xx, yy);
            if (!g->scratch_mark[q] && g->cost[q] && g->block[q] != GRID_BLOCK_WALL) {
                g->scratch_mark[q] = 1;
                queue[tail++] = q;
            }
        }
    }
    return 0;
}

CORE_API int32_t core_expansion_can_expel(const CoreWorld *w, int32_t id, CoreExileView *out) {
    if (!w || !out)
        return CORE_ERR_NULL_ARG;
    memset(out, 0, sizeof(*out));
    const Villager *v = entity_find_villager(w, id);
    if (!w->expansion || !v) {
        out->reason = CORE_EXILE_NOT_FOUND;
        return CORE_OK;
    }
    out->permanent = expansion_permanent(w);
    out->approved = v->departure_approved;
    out->departure_reason = v->departure_reason;
    out->approved_tick = v->departure_tick;
    for (int i = 0; i < w->villager_count; i++)
        if (!w->villagers[i]->contract_end_tick && w->villagers[i]->state == CORE_VS_LEAVING)
            out->pending++;
    if (v->state == CORE_VS_LEAVING || v->departure_approved)
        out->reason = CORE_EXILE_ALREADY_LEAVING;
    else if (v->incoming || v->health <= 0)
        out->reason = CORE_EXILE_INVALID;
    else if (!v->contract_end_tick && out->permanent - out->pending <= 8)
        out->reason = CORE_EXILE_FLOOR8;
    else {
        int x, y;
        if (!expansion_person_exit((World *)w, v, &x, &y))
            out->reason = CORE_EXILE_NO_PATH;
    }
    return CORE_OK;
}

CORE_API int32_t core_expansion_expel(CoreWorld *w, int32_t id) {
    if (!w)
        return CORE_ERR_NULL_ARG;
    CoreExileView check;
    int rc = core_expansion_can_expel(w, id, &check);
    if (rc != CORE_OK)
        return rc;
    if (check.reason)
        return CORE_ERR_REJECTED;
    Villager *v = entity_find_villager(w, id);
    int x = 0, y = 0;
    if (!expansion_person_exit(w, v, &x, &y))
        return CORE_ERR_REJECTED;
    entity_unlink_worker(v);
    entity_unlink_resident(v);
    v->departure_reason = CORE_DEPART_EXPEL;
    v->departure_approved = 1;
    v->departure_tick = w->clock.tick;
    v->mission = CORE_MISSION_NONE;
    v->mission_target = 0;
    v->hold_ticks = 0;
    v->leisure_building = 0;
    v->leisure_elapsed = 0;
    v->inside_building = 0;
    v->flags |= CORE_VF_MANUAL;
    vil_set_intent(v, CORE_VS_LEAVING, NULL, x, y, 0);
    expansion_notice(w, 160, v->id, expansion_permanent(w), u8"주민 추방 승인 · 실제 도보 이탈 중");
    w->expansion->dirty = 1;
    return CORE_OK;
}

CORE_API int32_t core_expansion_resident_status(const CoreWorld *w, int32_t id, CoreResidentStatus *out) {
    if (!w || !out)
        return CORE_ERR_NULL_ARG;
    const Villager *v = entity_find_villager(w, id);
    if (!v || !w->expansion)
        return CORE_ERR_NOT_FOUND;
    *out = (CoreResidentStatus){v->departure_reason, v->departure_approved, v->departure_tick,
                                v->boat_port_id, v->hunt_retreat_reason};
    return CORE_OK;
}

CORE_API int32_t core_expansion_house_residents(const CoreWorld *w, int32_t id, CoreVillagerView *out,
                                                int32_t capacity, int32_t *count) {
    if (!w || !count || capacity < 0 || (!out && capacity))
        return CORE_ERR_NULL_ARG;
    const Building *b = entity_find_building(w, id);
    if (!b || !w->expansion)
        return CORE_ERR_NOT_FOUND;
    *count = b->resident_count;
    if (capacity < b->resident_count)
        return CORE_ERR_BUFFER_TOO_SMALL;
    for (int i = 0; i < b->resident_count; i++) {
        int rc = core_get_villager(w, b->residents[i]->id, &out[i]);
        if (rc != CORE_OK)
            return rc;
    }
    return CORE_OK;
}
