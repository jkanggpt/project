/* expansion_v123_rules.c — 공용 입지·유한 재고·급수·진료·추방 질의 */
#include "expansion.h"
#include "villager_ai.h"
#include <stdlib.h>
#include <string.h>
void expansion_notice(World *w, int code, int a, int b, const char *message) {
    char text[CORE_NAME_BYTES] = {0};
    size_t n = strlen(message);
    if (n >= sizeof(text))
        n = sizeof(text) - 1;
    while (n && ((unsigned char)message[n] & 0xc0) == 0x80)
        n--;
    memcpy(text, message, n);
    events_push_progress(&w->events, CORE_EVT_EXPANSION_NOTICE, code, a, b, text);
}
int expansion_water_capacity(int type) {
    return type == CORE_BLD_WELL ? 18 : type == CORE_BLD_WATER_TOWER ? 60 : 0;
}
int expansion_water_radius(int type) {
    return type == CORE_BLD_WELL ? 8 : type == CORE_BLD_WATER_TOWER ? 14 : 0;
}
CORE_API int32_t core_expansion_ui_color_group(int32_t type) {
    const ExpansionBuildingSpec *s = expansion_spec(type);
    if (!s)
        return -1;
    if (type == CORE_BLD_WELL || type == CORE_BLD_WATER_TOWER || s->category == CORE_CAT_HOUSING ||
        s->category == CORE_CAT_ROAD)
        return 0;
    if (type == CORE_BLD_KEEP || type == CORE_BLD_TAVERN || type == CORE_BLD_THEATRE ||
        type == CORE_BLD_CHURCH || type == CORE_BLD_ART_MUSEUM || s->field == 9)
        return 5;
    if (s->field == 0 || s->field == 1 || s->field == 3 || s->field == 5)
        return 1;
    if (s->field == 2 || s->field == 4 || s->field == 6 || s->category == CORE_CAT_STORAGE)
        return 2;
    if (s->field == 7 || s->category == CORE_CAT_DEFENSE)
        return 3;
    if (s->field == 8 || s->field == 10 || s->field == 11)
        return 4;
    return 0;
}
int expansion_adjacent_water_id(const World *w, const Building *b) {
    int best = 0, best_contacts = 0, best_area = 0;
    for (int y = b->y - 1; y <= b->y + b->h; y++)
        for (int x = b->x - 1; x <= b->x + b->w; x++) {
            int dx = x < b->x ? b->x - x : x >= b->x + b->w ? x - b->x - b->w + 1 : 0;
            int dy = y < b->y ? b->y - y : y >= b->y + b->h ? y - b->y - b->h + 1 : 0;
            if (dx + dy != 1 || !grid_in_bounds(&w->grid, x, y))
                continue;
            int id = w->expansion->tiles[grid_index(&w->grid, x, y)].water_id;
            if (id < 1 || id > w->expansion->water_count)
                continue;
            int contacts = 0;
            for (int yy = b->y - 1; yy <= b->y + b->h; yy++)
                for (int xx = b->x - 1; xx <= b->x + b->w; xx++) {
                    int ax = xx < b->x ? b->x - xx : xx >= b->x + b->w ? xx - b->x - b->w + 1 : 0;
                    int ay = yy < b->y ? b->y - yy : yy >= b->y + b->h ? yy - b->y - b->h + 1 : 0;
                    if (ax + ay == 1 && grid_in_bounds(&w->grid, xx, yy) &&
                        w->grid.terrain[grid_index(&w->grid, xx, yy)] == CORE_TERRAIN_WATER &&
                        w->expansion->tiles[grid_index(&w->grid, xx, yy)].water_id == id)
                        contacts++;
                }
            int area = w->expansion->water_area[id - 1];
            if (contacts > best_contacts ||
                (contacts == best_contacts && (area > best_area || (area == best_area && id < best)))) {
                best = id;
                best_contacts = contacts;
                best_area = area;
            }
        }
    return best;
}
CORE_API int32_t core_expansion_adjacent_water(const CoreWorld *w, int32_t type, int32_t x, int32_t y,
                                               int32_t rotation) {
    if (!w || !w->expansion)
        return 0;
    const ExpansionBuildingSpec *s = expansion_spec(type);
    if (!s || rotation < 0 || rotation > 3)
        return 0;
    Building b = {0};
    b.x = x;
    b.y = y;
    b.w = rotation & 1 ? s->h : s->w;
    b.h = rotation & 1 ? s->w : s->h;
    return expansion_adjacent_water_id(w, &b);
}
CORE_API int32_t core_expansion_bridge_mask(const CoreWorld *w, int32_t x, int32_t y) {
    if (!w || !w->expansion || !grid_in_bounds(&w->grid, x, y))
        return -1;
    int p = grid_index(&w->grid, x, y);
    if (w->grid.road[p] != 3 || w->grid.terrain[p] != CORE_TERRAIN_WATER)
        return 0;
    static const int dx[4] = {0, 1, 0, -1}, dy[4] = {-1, 0, 1, 0};
    int connection = 0;
    for (int d = 0; d < 4; d++) {
        int xx = x + dx[d], yy = y + dy[d];
        if (!grid_in_bounds(&w->grid, xx, yy))
            continue;
        int q = grid_index(&w->grid, xx, yy);
        if (w->grid.road[q] == 3 || (w->grid.terrain[q] != CORE_TERRAIN_WATER && w->grid.cost[q] &&
                                     w->grid.block[q] != GRID_BLOCK_WALL && !w->grid.occupant[q]))
            connection |= 1 << d;
    }
    return (~connection) & 15;
}
int expansion_clay_take(World *w, const Building *b, int radius, int requested) {
    ExpansionState *e = w->expansion;
    int count = 0, total = 0,
        room = eco_bank_limit(&w->bank, CORE_RES_CLAY) * 1000 - expansion_bank_have(w, CORE_RES_CLAY);
    if (requested > room)
        requested = room;
    if (requested <= 0)
        return 0;
    int *cells = e->queue;
    for (int y = b->y - radius; y < b->y + b->h + radius; y++)
        for (int x = b->x - radius; x < b->x + b->w + radius; x++)
            if (grid_in_bounds(&w->grid, x, y) && expansion_center_distance(b, x, y) <= radius) {
                int p = grid_index(&w->grid, x, y);
                if (w->grid.terrain[p] != CORE_TERRAIN_WATER && e->tiles[p].clay_milli > 0)
                    cells[count++] = p;
            }
    while (count > 0 && requested > 0) {
        int share = requested / count, remainder = requested % count, active = 0,
            start = (b->id + exp_day(w)) % count;
        for (int k = 0; k < count; k++) {
            int p = cells[(start + k) % count], take = share + (k < remainder);
            if (take > e->tiles[p].clay_milli)
                take = e->tiles[p].clay_milli;
            take = expansion_bank_add(w, CORE_RES_CLAY, take, CORE_LEDGER_PRODUCTION);
            e->tiles[p].clay_milli -= take;
            requested -= take;
            total += take;
        }
        for (int k = 0; k < count; k++)
            if (e->tiles[cells[k]].clay_milli > 0)
                cells[active++] = cells[k];
        count = active;
    }
    return total;
}
int expansion_heal_herbs_required(const World *w, const Villager *v, const Building *b) {
    return (b->type == CORE_BLD_HOSPITAL ? 3 : 2) + (v->health <= 35 || v->critical) +
           (v->sick_ticks > 0 && expansion_event_active(w, CORE_NATURE_PLAGUE));
}
CORE_API int32_t core_expansion_healing_herbs(const CoreWorld *w, int32_t id, int32_t clinic) {
    if (!w || !w->expansion)
        return -1;
    const Villager *v = entity_find_villager(w, id);
    const Building *b = entity_find_building(w, clinic);
    if (!v || !b || !expansion_is_medical(b->type))
        return -1;
    return expansion_heal_herbs_required(w, v, b);
}
CORE_API int32_t core_expansion_water_provider(const CoreWorld *w, int32_t id, int32_t *radius,
                                               int32_t *capacity, int32_t *served) {
    if (!w || !radius || !capacity || !served)
        return CORE_ERR_NULL_ARG;
    const Building *b = entity_find_building(w, id);
    if (!b || !w->expansion)
        return CORE_ERR_NOT_FOUND;
    *radius = expansion_water_radius(b->type);
    *capacity = b->state == CORE_BSTATE_ACTIVE ? expansion_water_capacity(b->type) : 0;
    *served = b->water_served;
    return CORE_OK;
}
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
int expansion_v123_migrate(World *w) {
    ExpansionState *e = w->expansion;
    if (e->loaded_version >= 11)
        return CORE_OK;
    expansion_clay_initialize(w);
    for (int i = 0; i < w->villager_count; i++) {
        Villager *v = w->villagers[i];
        if (v->state == CORE_VS_LEAVING)
            v->departure_reason = v->contract_end_tick ? CORE_DEPART_CONTRACT : CORE_DEPART_VOLUNTARY;
    }
    for (int i = 0; i < e->animal_count; i++) {
        CoreAnimalView *a = &e->animals[i];
        if (!a->alive || a->kind != 28)
            continue;
        int p = grid_index(&w->grid, a->x_milli / 1000, a->y_milli / 1000);
        if (w->grid.terrain[p] != CORE_TERRAIN_WATER)
            continue;
        int best = -1, distance = INT32_MAX;
        for (int q = 0; q < e->cells; q++)
            if (w->grid.terrain[q] != CORE_TERRAIN_WATER && !w->grid.occupant[q] && w->grid.cost[q] &&
                w->grid.block[q] != GRID_BLOCK_WALL) {
                int d = exp_dist(q % w->grid.width, q / w->grid.width, a->x_milli / 1000, a->y_milli / 1000);
                if (d < distance) {
                    distance = d;
                    best = q;
                }
            }
        if (best < 0)
            return CORE_ERR_FORMAT;
        a->x_milli = (best % w->grid.width) * 1000 + 500;
        a->y_milli = (best / w->grid.width) * 1000 + 500;
        expansion_notice(w, 161, a->id, best, u8"구세이브 히드라를 가까운 육상으로 안전 이관");
    }
    e->loaded_version = 11;
    return CORE_OK;
}

int expansion_hunter_threat(World *w, Villager *v, int animal_id) {
    Building *b = v->workplace;
    if (!b || (b->type != CORE_BLD_HUNTER_CAMP && b->type != CORE_BLD_HUNTING_LODGE))
        return 0;
    CoreWorkSchedule schedule;
    if (!expansion_schedule(w, v, &schedule) || core_expansion_outfit(w, v->id) == 0)
        return 0;
    for (int i = 0; i < w->expansion->animal_count; i++)
        if (w->expansion->animals[i].id == animal_id) {
            const CoreAnimalView *a = &w->expansion->animals[i];
            const CoreAnimalInfo *info = &EXP_ANIMALS[a->kind];
            if (v->health <= 35 || (b->type == CORE_BLD_HUNTER_CAMP && info->biomass >= 5)) {
                v->hunt_retreat_reason = v->health <= 35 ? 1 : 2;
                v->hold_ticks = 0;
                v->mission = CORE_MISSION_RETURN;

                vil_set_intent(v, CORE_VS_GUARD_POST, b, b->door_x, b->door_y, 0);
                return 1;
            }
            int radius = b->type == CORE_BLD_HUNTER_CAMP ? 8 : 15;
            if (!a->owner && expansion_center_distance(b, a->x_milli / 1000, a->y_milli / 1000) <= radius) {
                v->hold_ticks = 0;
                v->mission = CORE_MISSION_HUNT;

                return 1;
            }
        }
    return 0;
}
