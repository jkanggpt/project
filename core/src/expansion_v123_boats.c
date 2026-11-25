/* expansion_v123_boats.c — 생산과 수역 항로를 분리한 2인승 어선 */
#include "expansion.h"
#include "villager_ai.h"
#include <stdlib.h>
#include <string.h>
static const int DX[4] = {0, 1, 0, -1}, DY[4] = {-1, 0, 1, 0};
static int water_ok(const World *w, int x, int y, int water) {
    if (!grid_in_bounds(&w->grid, x, y))
        return 0;
    int p = grid_index(&w->grid, x, y);
    return w->grid.terrain[p] == CORE_TERRAIN_WATER && w->expansion->tiles[p].water_id == water &&
           !w->grid.road[p] && !w->grid.occupant[p] && w->grid.block[p] != GRID_BLOCK_WALL;
}
static int launch(const World *w, const Building *b, int water, int *x, int *y) {
    for (int yy = b->y - 1; yy <= b->y + b->h; yy++)
        for (int xx = b->x - 1; xx <= b->x + b->w; xx++) {
            int d = (xx < b->x           ? b->x - xx
                     : xx >= b->x + b->w ? xx - b->x - b->w + 1
                                         : 0) +
                    (yy < b->y           ? b->y - yy
                     : yy >= b->y + b->h ? yy - b->y - b->h + 1
                                         : 0);
            if (d == 1 && water_ok(w, xx, yy, water)) {
                *x = xx;
                *y = yy;
                return 1;
            }
        }
    return 0;
}
static int next_water(World *w, int sx, int sy, int tx, int ty, int water, int *nx, int *ny) {
    ExpansionState *e = w->expansion;
    Grid *g = &w->grid;
    if (!water_ok(w, tx, ty, water))
        return 0;
    for (int p = 0; p < e->cells; p++)
        e->route_distance[p] = -1;
    int head = 0, tail = 0, target = grid_index(g, tx, ty), start = grid_index(g, sx, sy);
    e->queue[tail++] = target;
    e->route_distance[target] = 0;
    while (head < tail && e->route_distance[start] < 0) {
        int p = e->queue[head++], x = p % g->width, y = p / g->width;
        for (int d = 0; d < 4; d++) {
            int xx = x + DX[d], yy = y + DY[d];
            if (!water_ok(w, xx, yy, water))
                continue;
            int q = grid_index(g, xx, yy);
            if (e->route_distance[q] < 0) {
                e->route_distance[q] = e->route_distance[p] + 1;
                e->queue[tail++] = q;
            }
        }
    }
    if (e->route_distance[start] < 0)
        return 0;
    *nx = sx;
    *ny = sy;
    if (start == target)
        return 1;
    for (int d = 0; d < 4; d++) {
        int xx = sx + DX[d], yy = sy + DY[d];
        if (water_ok(w, xx, yy, water) &&
            e->route_distance[grid_index(g, xx, yy)] == e->route_distance[start] - 1) {
            *nx = xx;
            *ny = yy;
            return 1;
        }
    }
    return 0;
}
static void disembark(World *w, CoreBoatView *boat, const Building *port) {
    int x = port ? port->door_x : -1, y = port ? port->door_y : -1;
    if (!port) {
        int distance = INT32_MAX;
        for (int p = 0; p < w->expansion->cells; p++)
            if (w->grid.cost[p] && w->grid.terrain[p] != CORE_TERRAIN_WATER && !w->grid.occupant[p] &&
                w->grid.block[p] != GRID_BLOCK_WALL) {
                int d = exp_dist(p % w->grid.width, p / w->grid.width, boat->x_milli / 1000,
                                 boat->y_milli / 1000);
                if (d < distance) {
                    distance = d;
                    x = p % w->grid.width;
                    y = p / w->grid.width;
                }
            }
    }
    if (x < 0)
        return;
    for (int k = 0; k < boat->crew_count; k++) {
        Villager *v = entity_find_villager(w, boat->crew[k]);
        if (!v)
            continue;
        v->boat_port_id = 0;
        v->current_x_milli = x * 1000 + 500;
        v->current_y_milli = y * 1000 + 500;
        v->loc_x = x;
        v->loc_y = y;
        v->inside_building = port ? port->id : 0;
        if (v->state != CORE_VS_LEAVING) {
            CoreWorkSchedule schedule;
            if (port && v->workplace == port && !v->sick_ticks && v->health >= 50 &&
                expansion_schedule(w, v, &schedule)) {
                vil_set_intent(v, CORE_VS_WORKING, (Building *)port, x, y, 0);
                v->at_target = 1;
            } else
                vil_set_intent(v, CORE_VS_IDLE, NULL, x, y, 0);
        }
        v->intent_rev++;
    }
    boat->crew_count = 0;
    boat->crew[0] = boat->crew[1] = 0;
    boat->state = 0;
    boat->returning = 0;
}
int expansion_boat_hold(World *w, Villager *v) {
    if (!v->boat_port_id)
        return 0;
    expansion_v122_life(w, v);
    expansion_wellbeing_sample(w, v);
    if (v->meal_day < exp_day(w) && expansion_bank_have(w, CORE_RES_FOOD) >= 1000) {
        expansion_bank_add(w, CORE_RES_FOOD, -1000, CORE_LEDGER_CONSUMPTION);
        v->meal_day = exp_day(w);
        v->needs.hunger = 100;
    }
    if (v->sick_ticks > 0) {
        v->sick_ticks--;
        if (!v->sick_ticks)
            v->flags &= ~CORE_VF_SICK;
    }
    Building *b = entity_find_building(w, v->boat_port_id);
    CoreWorkSchedule schedule;
    if (b && v->workplace == b && v->state != CORE_VS_LEAVING && !v->sick_ticks && v->health >= 50 &&
        expansion_schedule(w, v, &schedule)) {
        v->inside_building = 0;
        v->state = CORE_VS_WORKING;
        expansion_gain_xp(v, 4, b->type, expansion_begin_work(w, v, b));
    }
    return 1;
}
static int move_boat(World *w, CoreBoatView *boat) {
    int budget = 550;
    while (budget > 0) {
        int x = boat->x_milli / 1000, y = boat->y_milli / 1000;
        if (boat->returning && boat->x_milli == boat->home_x * 1000 + 500 &&
            boat->y_milli == boat->home_y * 1000 + 500)
            return 1;
        if (boat->x_milli == x * 1000 + 500 && boat->y_milli == y * 1000 + 500) {
            int tx = boat->home_x, ty = boat->home_y, nx = x, ny = y;
            if (!boat->returning) {
                int points[289], count = 0;
                for (int yy = boat->home_y - 8; yy <= boat->home_y + 8; yy++)
                    for (int xx = boat->home_x - 8; xx <= boat->home_x + 8; xx++)
                        if (water_ok(w, xx, yy, boat->water_id))
                            points[count++] = grid_index(&w->grid, xx, yy);
                for (int retry = 0; retry < 6; retry++) {
                    if (count && boat->waypoint % 6) {
                        int p = points[expansion_map_hash(boat->port_id, boat->waypoint % 6, boat->slot) %
                                       ((unsigned)count)];
                        tx = p % w->grid.width;
                        ty = p / w->grid.width;
                    } else {
                        tx = boat->home_x;
                        ty = boat->home_y;
                    }
                    if (x != tx || y != ty)
                        break;
                    boat->waypoint++;
                }
            }
            if (!next_water(w, x, y, tx, ty, boat->water_id, &nx, &ny)) {
                boat->waypoint++;
                return 0;
            }
            boat->next_x = nx;
            boat->next_y = ny;
        }
        if (!water_ok(w, boat->next_x, boat->next_y, boat->water_id)) {
            boat->x_milli = x * 1000 + 500;
            boat->y_milli = y * 1000 + 500;
            return 0;
        }
        int dx = boat->next_x * 1000 + 500 - boat->x_milli, dy = boat->next_y * 1000 + 500 - boat->y_milli;
        if (!dx && !dy)
            return 1;
        boat->facing = dx > 0 ? 1 : dx < 0 ? 3 : dy < 0 ? 0 : 2;
        int step = abs(dx ? dx : dy);
        if (step > budget)
            step = budget;
        if (dx)
            boat->x_milli += dx > 0 ? step : -step;
        else
            boat->y_milli += dy > 0 ? step : -step;
        budget -= step;
    }
    return 1;
}
void expansion_boats_tick(World *w) {
    ExpansionState *e = w->expansion;
    for (int i = 0; i < e->boat_count; i++) {
        CoreBoatView *boat = &e->boats[i];
        Building *port = entity_find_building(w, boat->port_id);
        if (port && port->state != CORE_BSTATE_ACTIVE)
            port = NULL;
        if (!boat->crew_count)
            continue;
        if (!port) {
            disembark(w, boat, NULL);
            continue;
        }
        for (int k = 0; k < boat->crew_count; k++) {
            Villager *v = entity_find_villager(w, boat->crew[k]);
            CoreWorkSchedule schedule;
            if (!v || v->workplace != port || v->sick_ticks || v->health < 50 ||
                v->state == CORE_VS_LEAVING || !expansion_schedule(w, v, &schedule))
                boat->returning = 1;
        }
        int x = boat->x_milli / 1000, y = boat->y_milli / 1000;
        if (!water_ok(w, x, y, boat->water_id)) {
            int found = 0;
            for (int d = 0; d < 4; d++)
                if (water_ok(w, x + DX[d], y + DY[d], boat->water_id)) {
                    x += DX[d];
                    y += DY[d];
                    boat->x_milli = x * 1000 + 500;
                    boat->y_milli = y * 1000 + 500;
                    boat->next_x = x;
                    boat->next_y = y;
                    found = 1;
                    break;
                }
            if (!found) {
                boat->blocked = 1;
                boat->state = 3;
                continue;
            }
        }
        if (boat->returning && !water_ok(w, boat->home_x, boat->home_y, boat->water_id)) {
            if (!launch(w, port, boat->water_id, &boat->home_x, &boat->home_y)) {
                boat->blocked = 1;
                boat->state = 3;
                continue;
            }
        }
        if (boat->returning && x == boat->home_x && y == boat->home_y) {
            disembark(w, boat, port);
            continue;
        }
        if (!move_boat(w, boat)) {
            boat->blocked = 1;
            boat->state = 3;
            continue;
        }
        if (boat->returning && boat->x_milli == boat->home_x * 1000 + 500 &&
            boat->y_milli == boat->home_y * 1000 + 500) {
            disembark(w, boat, port);
            continue;
        }
        boat->blocked = 0;
        boat->state = boat->returning ? 2 : 1;
        for (int k = 0; k < boat->crew_count; k++) {
            Villager *v = entity_find_villager(w, boat->crew[k]);
            if (v) {
                v->current_x_milli = boat->x_milli;
                v->current_y_milli = boat->y_milli;
                v->inside_building = 0;
            }
        }
    }
    for (int i = 0; i < w->building_count; i++) {
        Building *b = w->buildings[i];
        if (b->type != CORE_BLD_PORT || b->state != CORE_BSTATE_ACTIVE)
            continue;
        int water = expansion_adjacent_water_id(w, b), x, y;
        if (!water || !launch(w, b, water, &x, &y))
            continue;
        int waiting = 0, active = 0, singles = 0, reforming = 0;
        for (int k = 0; k < b->worker_count; k++)
            waiting += !b->workers[k]->boat_port_id && expansion_working_present(w, b, b->workers[k]);
        for (int k = 0; k < e->boat_count; k++)
            if (e->boats[k].port_id == b->id && e->boats[k].crew_count) {
                active += e->boats[k].crew_count;
                singles += e->boats[k].crew_count == 1;
                reforming |= e->boats[k].returning == 2;
            }
        if ((active == 1 && waiting > 0) || (active == 2 && singles == 2)) {
            for (int k = 0; k < e->boat_count; k++)
                if (e->boats[k].port_id == b->id && e->boats[k].crew_count)
                    e->boats[k].returning = 2;
            reforming = 1;
        }
        if (reforming)
            continue;
        for (int slot = 0; slot < 2; slot++) {
            CoreBoatView *boat = NULL;
            for (int j = 0; j < e->boat_count; j++)
                if (e->boats[j].port_id == b->id && e->boats[j].slot == slot)
                    boat = &e->boats[j];
            if (boat && boat->crew_count)
                continue;
            Villager *crew[2] = {0};
            int count = 0;
            for (int j = 0; j < b->worker_count && count < 2; j++) {
                Villager *v = b->workers[j];
                if (!v->boat_port_id && !v->sick_ticks && v->health >= 50 &&
                    expansion_working_present(w, b, v))
                    crew[count++] = v;
            }
            if (!count)
                continue;
            if (!boat) {
                for (int j = 0; j < e->boat_count; j++)
                    if (!e->boats[j].crew_count && !entity_find_building(w, e->boats[j].port_id)) {
                        boat = &e->boats[j];
                        break;
                    }
                if (!boat && e->boat_count < CORE_EXP_MAX_BOATS)
                    boat = &e->boats[e->boat_count++];
            }
            if (!boat)
                break;
            *boat = (CoreBoatView){0};
            boat->port_id = b->id;
            boat->slot = slot;
            boat->state = 1;
            boat->water_id = water;
            int lx = x, ly = y;
            if (slot) {
                int best = -1;
                for (int yy = b->y - 1; yy <= b->y + b->h; yy++)
                    for (int xx = b->x - 1; xx <= b->x + b->w; xx++) {
                        int d = (xx < b->x           ? b->x - xx
                                 : xx >= b->x + b->w ? xx - b->x - b->w + 1
                                                     : 0) +
                                (yy < b->y           ? b->y - yy
                                 : yy >= b->y + b->h ? yy - b->y - b->h + 1
                                                     : 0);
                        if (d == 1 && water_ok(w, xx, yy, water)) {
                            int score = exp_dist(xx, yy, x, y);
                            if (score > best) {
                                best = score;
                                lx = xx;
                                ly = yy;
                            }
                        }
                    }
            }
            x = lx;
            y = ly;
            boat->home_x = boat->next_x = x;
            boat->home_y = boat->next_y = y;
            boat->x_milli = x * 1000 + 500;
            boat->y_milli = y * 1000 + 500;
            boat->crew_count = count;
            for (int k = 0; k < count; k++) {
                boat->crew[k] = crew[k]->id;
                crew[k]->boat_port_id = b->id;
                crew[k]->inside_building = 0;
                crew[k]->current_x_milli = boat->x_milli;
                crew[k]->current_y_milli = boat->y_milli;
            }
        }
    }
}
CORE_API int32_t core_expansion_copy_boats(const CoreWorld *w, CoreBoatView *out, int32_t capacity,
                                           int32_t *count) {
    if (!w || !count || capacity < 0 || (!out && capacity))
        return CORE_ERR_NULL_ARG;
    if (!w->expansion)
        return CORE_ERR_INVALID_ARG;
    *count = 0;
    for (int i = 0; i < w->expansion->boat_count; i++)
        if (w->expansion->boats[i].crew_count)
            (*count)++;
    if (capacity < *count)
        return CORE_ERR_BUFFER_TOO_SMALL;
    int n = 0;
    for (int i = 0; i < w->expansion->boat_count; i++)
        if (w->expansion->boats[i].crew_count)
            out[n++] = w->expansion->boats[i];
    return CORE_OK;
}
