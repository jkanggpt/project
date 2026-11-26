/* schedule — 현재 기능의 단일 구현 */
#include "expansion.h"
#include "placement.h"
#include "rank.h"
#include "villager_ai.h"
#include <stdlib.h>
#include <string.h>

int expansion_duty(World *w, Villager *v, Building *b) {
    CoreWorkSchedule schedule;
    expansion_schedule(w, v, &schedule);
    if (!schedule.on_duty)
        return 0;
    if (!v->mission && (v->target != b || !v->at_target))
        return 0;
    if (v->duty_day != exp_day(w)) {
        v->duty_day = exp_day(w);
        v->duty_minutes = 0;
    }
    v->duty_minutes += 10;
    if (v->duty_minutes >= (expansion_tower(b->type) ? 720 : 550))
        expansion_mood_event(w, v, 8, expansion_tower(b->type) ? -2 : -1, 0, 144, 1);
    if (expansion_tower(b->type))
        return 0;
    int hunting = b->type == CORE_BLD_HUNTER_CAMP || b->type == CORE_BLD_HUNTING_LODGE;
    if (!hunting && !expansion_military(b->type))
        return 0;
    if (!v->mission && (v->target != b || !v->at_target))
        return 0;
    if (v->health <= 35 || (v->mission == CORE_MISSION_RETURN && v->health < 70)) {
        v->mission = CORE_MISSION_RETURN;
        vil_set_intent(v, CORE_VS_GUARD_POST, b, b->door_x, b->door_y, 0);
        if (v->at_target) {
            v->inside_building = b->id;
            v->health = exp_clamp(v->health + 1, 0, 100);
        }
        return 1;
    }
    v->inside_building = 0;
    if (w->expansion->alarm && !hunting) {
        v->mission = CORE_MISSION_COMBAT;
        return 0;
    }
    int sight = 4;
    int radius = hunting                        ? exp_clamp(expansion_spec(b->type)->radius - sight, 0, 15)
                 : b->type == CORE_BLD_GARRISON ? 4
                                                : 3;
    if (w->clock.tick >= v->mission_next_tick || !v->mission) {
        int x = b->door_x, y = b->door_y;
        for (int attempt = 0; attempt < 40; attempt++) {
            uint32_t n = expansion_map_hash(v->id, v->mission_waypoint++, exp_day(w));
            int xx = b->x + b->w / 2 + (int)(n % (unsigned)(radius * 2 + 1)) - radius;
            int yy = b->y + b->h / 2 + (int)((n >> 8) % (unsigned)(radius * 2 + 1)) - radius;
            if (grid_in_bounds(&w->grid, xx, yy) && expansion_center_distance(b, xx, yy) <= radius &&
                w->grid.cost[grid_index(&w->grid, xx, yy)] &&
                w->grid.block[grid_index(&w->grid, xx, yy)] != GRID_BLOCK_WALL) {
                x = xx;
                y = yy;
                break;
            }
        }
        v->mission = hunting ? CORE_MISSION_SCOUT : CORE_MISSION_PATROL;
        vil_set_intent(v, CORE_VS_GUARD_POST, NULL, x, y, 0);
        v->mission_next_tick = w->clock.tick + 12;
    }
    return 1;
}

int expansion_leisure(World *w, Villager *v) {
    int minute = exp_minute(w), day = exp_day(w);
    if (minute < 420 || minute >= 1320 || v->needs.hunger < 40 || v->needs.rest < 30 || w->expansion->alarm)
        return 0;
    if (v->leisure_building) {
        Building *b = entity_find_building(w, v->leisure_building);
        if (!b || b->state != CORE_BSTATE_ACTIVE || !expansion_working_count(w, b) ||
            !expansion_qualified_present(w, b) || v->target != b) {
            v->leisure_building = 0;
            v->leisure_elapsed = 0;
            v->mission_next_tick = w->clock.tick + 12;
            return 0;
        }
        if (v->at_target) {
            v->inside_building = b->id;
            v->leisure_elapsed += 10;
            if (v->leisure_elapsed >= v->leisure_duration && b->service_credit_milli >= 1000) {
                expansion_leisure_reward(w, v, b, day);
                if (b->type == CORE_BLD_SCHOOL || b->type == CORE_BLD_LIBRARY ||
                    b->type == CORE_BLD_ACADEMY) {
                    v->education_day = day;
                    expansion_mood_event(w, v, 10, 2, b->id, 144, 1);
                } else
                    v->leisure_day = day;
                v->leisure_building = 0;
                v->leisure_elapsed = 0;
                v->mission_next_tick = w->clock.tick + 12;
                return 0;
            }
        }
        return 1;
    }
    if (v->mission == CORE_MISSION_WALK) {
        int x = v->current_x_milli / 1000, y = v->current_y_milli / 1000;
        int p = grid_index(&w->grid, x, y), origin = grid_index(&w->grid, v->walk_origin_x, v->walk_origin_y);
        if (!w->expansion->protected_cells[p] || !w->expansion->protected_cells[origin]) {
            v->mission = CORE_MISSION_NONE;
            return 0;
        }
        if (!v->at_target)
            return 1;
        if (x != v->walk_origin_x || y != v->walk_origin_y) {
            vil_set_intent(v, CORE_VS_IDLE, NULL, v->walk_origin_x, v->walk_origin_y, 0);
            return 1;
        }
        if (w->clock.tick >= v->mission_next_tick) {
            v->mission = CORE_MISSION_NONE;
            if (v->walk_reward_day != day) {
                expansion_mood_event(w, v, 9, 1, 0, 144, 1);
                v->walk_reward_day = day;
            }
            v->mission_next_tick = w->clock.tick + 12;
            return 0;
        }
        static const int dx[4] = {0, 1, 0, -1}, dy[4] = {-1, 0, 1, 0};
        int start = rng_range(&w->rng, 0, 3);
        for (int k = 0; k < 4; k++) {
            int xx = x + dx[(start + k) % 4], yy = y + dy[(start + k) % 4];
            if (!grid_in_bounds(&w->grid, xx, yy))
                continue;
            int q = grid_index(&w->grid, xx, yy);
            if (w->expansion->protected_cells[q] && w->expansion->tiles[q].explored && w->grid.cost[q] &&
                !w->grid.block[q]) {
                vil_set_intent(v, CORE_VS_IDLE, NULL, xx, yy, 0);
                return 1;
            }
        }
        v->mission = CORE_MISSION_NONE;
        return 0;
    }
    if (w->clock.tick < v->mission_next_tick)
        return 0;
    v->mission_next_tick = w->clock.tick + 12;
    int weight_sum = 20, choice = -1;
    for (int i = 0; i < w->building_count; i++) {
        Building *b = w->buildings[i];
        int education =
            b->type == CORE_BLD_SCHOOL || b->type == CORE_BLD_LIBRARY || b->type == CORE_BLD_ACADEMY;
        if (b->type == CORE_BLD_ART_MUSEUM &&
            !((minute >= 540 && minute < 720) || (minute >= 1080 && minute < 1320)))
            continue;
        if (!expansion_service_shift(b->type) || b->state != CORE_BSTATE_ACTIVE ||
            !expansion_working_count(w, b) || !expansion_qualified_present(w, b) ||
            b->service_credit_milli < 1000)
            continue;
        if (education ? v->education_day == day : v->leisure_day == day)
            continue;
        int index = b->type == CORE_BLD_TAVERN      ? 0
                    : b->type == CORE_BLD_BATHHOUSE ? 1
                    : b->type == CORE_BLD_CHURCH    ? 3
                                                    : 2;
        if (!education && day < v->leisure_days[index])
            continue;
        int guests = 0;
        for (int j = 0; j < w->villager_count; j++)
            guests += w->villagers[j]->leisure_building == b->id;
        if (guests >= expansion_service_base(b, expansion_working_count(w, b)))
            continue;
        int distance = exp_dist(v->current_x_milli / 1000, v->current_y_milli / 1000, b->door_x, b->door_y);
        int weight = exp_clamp(70 - v->mood / 5 - distance * 2 - guests * 5, 1, 120);
        if (v->needs.rest < 60)
            weight /= 2;
        weight_sum += weight;
        if (rng_range(&w->rng, 1, weight_sum) <= weight)
            choice = i;
    }
    if (choice >= 0) {
        Building *b = w->buildings[choice];
        v->leisure_building = b->id;
        v->leisure_elapsed = 0;
        v->leisure_duration = b->type == CORE_BLD_TAVERN       ? 60
                              : b->type == CORE_BLD_BATHHOUSE  ? 80
                              : b->type == CORE_BLD_CHURCH     ? 70
                              : b->type == CORE_BLD_ART_MUSEUM ? 30
                              : b->type == CORE_BLD_ACADEMY    ? 110
                                                               : 100;
        vil_set_intent(v, CORE_VS_IDLE, b, b->door_x, b->door_y, 0);
        return 1;
    }
    /* 단일 인접 칸의 경로이므로 보호구역 바깥을 가로지르는 산책이 없습니다. */
    if (minute < 1140 && expansion_person_protected(w, v) && rng_range(&w->rng, 0, 3) == 0) {
        int x = v->current_x_milli / 1000, y = v->current_y_milli / 1000;
        static const int dx[4] = {0, 1, 0, -1}, dy[4] = {-1, 0, 1, 0};
        int start = rng_range(&w->rng, 0, 3);
        for (int k = 0; k < 4; k++) {
            int xx = x + dx[(start + k) % 4], yy = y + dy[(start + k) % 4];
            if (!grid_in_bounds(&w->grid, xx, yy))
                continue;
            int p = grid_index(&w->grid, xx, yy);
            if (w->expansion->protected_cells[p] && w->expansion->tiles[p].explored && w->grid.cost[p] &&
                !w->grid.block[p]) {
                v->walk_origin_x = x;
                v->walk_origin_y = y;
                v->mission = CORE_MISSION_WALK;
                vil_set_intent(v, CORE_VS_IDLE, NULL, xx, yy, 0);
                v->mission_next_tick = w->clock.tick + rng_range(&w->rng, 2, 6);
                return 1;
            }
        }
    }
    return 0;
}

int expansion_schedule(const World *w, const Villager *v, CoreWorkSchedule *out) {
    *out = (CoreWorkSchedule){480, 1020, 0, 0, 1320, 360, 0, 0};
    const Building *b = v->workplace;
    if (!b || b->state != CORE_BSTATE_ACTIVE)
        return 0;
    int type = b->type, field = expansion_spec(type)->field, minute = exp_minute(w);
    if (field == 0 || field == 1 || field == 5) {
        out->start = 420;
        out->end = 960;
    }
    if (field == 3 || type == CORE_BLD_RANCH || type == CORE_BLD_PASTURE) {
        out->start = 360;
        out->end = 900;
    }
    if (field == 6 || field == 8) {
        out->start = 540;
        out->end = 1080;
    }
    if (expansion_service_shift(type)) {
        if (b->worker_count == 1) {
            out->start = 420;
            out->end = 660;
            out->second_start = 1020;
            out->second_end = 1320;
        } else {
            int slot = 0;
            for (int i = 0; i < b->worker_count; i++)
                if (b->workers[i] == v)
                    slot = i;
            out->start = (slot & 1) ? 780 : 420;
            out->end = (slot & 1) ? 1320 : 960;
        }
    }
    if (type == CORE_BLD_ART_MUSEUM) {
        out->start = 540;
        out->end = 720;
        out->second_start = 1080;
        out->second_end = 1320;
    }
    if (type == CORE_BLD_OBSERVATORY) {
        out->start = 480;
        out->end = 1020;
        out->second_start = 1140;
        out->second_end = 1320;
    }
    if (expansion_military(type)) {
        out->start = 480;
        out->end = 1080;
    }
    if (expansion_tower(type)) {
        out->shift = v->shift_kind;
        out->start = out->shift ? 1080 : 360;
        out->end = out->shift ? 360 : 1080;
        out->sleep_start = out->shift ? 480 : 1320;
        out->sleep_end = out->shift ? 960 : 360;
    }
    out->on_duty = out->start < out->end ? minute >= out->start && minute < out->end
                                         : minute >= out->start || minute < out->end;
    out->on_duty |= out->second_end && minute >= out->second_start && minute < out->second_end;
    if (expansion_military(type) && w->expansion->alarm)
        out->on_duty = 1;
    if (expansion_tower(type)) {
        if (b->combat_guard_id && b->combat_until_tick > w->clock.tick)
            out->on_duty = b->combat_guard_id == v->id;
        else if (w->expansion->alarm && b->worker_count == 1)
            out->on_duty = 1;
    }
    return out->on_duty;
}

CORE_API int32_t core_expansion_schedule(const CoreWorld *w, int32_t id, CoreWorkSchedule *out) {
    if (!w || !out)
        return CORE_ERR_NULL_ARG;
    const Villager *v = entity_find_villager(w, id);
    if (!w->expansion || !v)
        return CORE_ERR_NOT_FOUND;
    expansion_schedule(w, v, out);
    return CORE_OK;
}

CORE_API int32_t core_expansion_set_shift(CoreWorld *w, int32_t id, int32_t shift) {
    if (!w)
        return CORE_ERR_NULL_ARG;
    Villager *v = entity_find_villager(w, id);
    if (!w->expansion || !v || !v->workplace || !expansion_tower(v->workplace->type) || shift < 0 ||
        shift > 1)
        return CORE_ERR_INVALID_ARG;
    if (v->shift_kind == shift)
        return CORE_OK;
    if (v->shift_changed_day == exp_day(w))
        return CORE_ERR_REJECTED;
    Building *b = v->workplace;
    if (b->combat_guard_id && b->combat_until_tick > w->clock.tick)
        return CORE_ERR_REJECTED;
    for (int i = 0; i < b->worker_count; i++)
        if (b->workers[i] != v && b->workers[i]->shift_kind == shift) {
            Villager *other = b->workers[i];
            if (other->shift_changed_day == exp_day(w))
                return CORE_ERR_REJECTED;
            other->shift_kind = v->shift_kind;
            other->shift_changed_day = exp_day(w);
            other->shift_adapt_days = 0;
            other->shift_sleep_day = 0;
        }
    v->shift_kind = shift;
    v->shift_changed_day = exp_day(w);
    v->shift_adapt_days = 0;
    v->shift_sleep_day = 0;
    return CORE_OK;
}

CORE_API int32_t core_expansion_assign_shift(CoreWorld *w, int32_t id, int32_t building_id, int32_t shift) {
    if (!w)
        return CORE_ERR_NULL_ARG;
    Villager *v = entity_find_villager(w, id);
    Building *b = entity_find_building(w, building_id);
    if (!w->expansion || !v || !b)
        return CORE_ERR_NOT_FOUND;
    if (!expansion_tower(b->type) || shift < 0 || shift > 1)
        return CORE_ERR_INVALID_ARG;
    if (v->workplace == b)
        return core_expansion_set_shift(w, id, shift);
    for (int i = 0; i < b->worker_count; i++)
        if (b->workers[i]->shift_kind == shift)
            return CORE_ERR_REJECTED;
    int rc = core_assign_worker(w, id, building_id);
    if (rc != CORE_OK)
        return rc;
    v->shift_kind = shift;
    v->shift_changed_day = exp_day(w);
    v->shift_adapt_days = 0;
    v->shift_sleep_day = 0;
    return CORE_OK;
}

void expansion_mood_event(World *w, Villager *v, int kind, int amount, int source, int cooldown, int cap) {
    if (kind < 0 || kind >= 32 || !amount)
        return;
    if (v->mood_day != exp_day(w)) {
        memset(v->mood_counts, 0, sizeof(v->mood_counts));
        v->mood_day = exp_day(w);
    }
    if (kind == 17 && v->mood_ticks[kind] && w->clock.tick + 1 - v->mood_ticks[kind] < cooldown)
        return;
    if (source) {
        for (int i = 0; i < 32; i++)
            if (v->mood_recent[i][0] == kind + 1 && v->mood_recent[i][1] == source &&
                w->clock.tick + 1 - v->mood_recent[i][2] < cooldown)
                return;
    } else if (v->mood_ticks[kind] && w->clock.tick + 1 - v->mood_ticks[kind] < cooldown)
        return;
    if (cap > 0 && v->mood_counts[kind] >= cap)
        return;
    v->mood = exp_clamp(v->mood + amount, -200, 200);
    v->mood_ticks[kind] = w->clock.tick + 1;
    v->mood_counts[kind]++;
    if (source) {
        int fixed = kind == 15 ? 0 : kind == 24 ? 1 : kind == 26 ? 2 : -1;
        int i = fixed >= 0 ? fixed : 3 + v->mood_recent_cursor % 29;
        if (fixed < 0) {
            v->mood_recent_cursor = (v->mood_recent_cursor + 1) % 29;
        }
        v->mood_recent[i][0] = kind + 1;
        v->mood_recent[i][1] = source;
        v->mood_recent[i][2] = w->clock.tick + 1;
    }
    v->mood_cause = 32 + kind;
    v->mood_event_tick = w->clock.tick;
    v->mood_event_amount = amount;
    v->mood_event_source = source;
    v->mood_source_kind = 0;
    v->mood_source_type = 0;
    if (kind == 17 || kind == 18)
        v->mood_source_kind = v->damage_kind;
    else if (kind == 13)
        v->mood_source_kind = 2;
    else if (kind == 7 || kind == 10 || kind == 14 || kind == 21)
        v->mood_source_kind = 3;
    else if (kind == 19 || kind == 27)
        v->mood_source_kind = 4;
    else if (kind == 15 || kind == 20 || kind == 24 || kind == 26)
        v->mood_source_kind = 5;
    if (v->mood_source_kind == 1 && source >= 1 && source <= w->threat.raid.enemy_count)
        v->mood_source_type = w->threat.raid.enemies[source - 1].type + 1;
    if (v->mood_source_kind == 2)
        for (int j = 0; j < w->expansion->animal_count; j++)
            if (w->expansion->animals[j].id == source)
                v->mood_source_type = w->expansion->animals[j].kind + 1;
}

void expansion_people_needs_tick(World *w, Villager *v) {
    int minute = exp_minute(w), h = v->needs.hunger, t = v->water;
    int asleep = v->state == CORE_VS_SLEEP && v->at_target;
    if (minute % 60 == 0) {
        if (!asleep) {
            if (t < 40)
                expansion_mood_event(w, v, 3, -1, 0, 12, 6);
            if (h < 20)
                expansion_mood_event(w, v, 5, -2, 0, 6, 24);
            else if (h < 40)
                expansion_mood_event(w, v, 5, -1, 0, 6, 24);
            else if (h < 70)
                expansion_mood_event(w, v, 5, -1, 0, 12, 6);
            if (v->needs.rest < 30)
                expansion_mood_event(w, v, 12, -1, 0, 12, 4);
        }
        if (expansion_person_protected(w, v))
            expansion_mood_event(w, v, 11, 1, 0, 144, 1);
        if (asleep && v->needs.rest >= 90)
            expansion_mood_event(w, v, 6, 2, 0, 144, 1);
    }
    if (!asleep && t < 40 && !v->thirst_stage)
        expansion_mood_event(w, v, 2, -2, 0, 12, 1);
    if (!asleep && h < 70 && !v->hunger_stage)
        expansion_mood_event(w, v, 4, -1, 0, 12, 1);
    v->thirst_stage = t < 40;
    v->hunger_stage = h < 70;
    if (w->threat.raid.id && w->threat.raid.first_discovered) {
        expansion_mood_event(w, v, 15, -3, w->threat.raid.id, 1000000000, 0);
        if (asleep)
            expansion_mood_event(w, v, 26, -2, w->threat.raid.id, 1000000000, 0);
    }
    if (asleep || v->inside_building)
        return;
    int x = v->current_x_milli / 1000, y = v->current_y_milli / 1000;
    Raid *raid = &w->threat.raid;
    if (raid->id) {
        for (int i = 0; i < raid->enemy_count; i++) {
            const Enemy *enemy = &raid->enemies[i];
            const CoreEnemyRuntime *r = &enemy->runtime;
            if (enemy->state != CORE_ENEMY_ALIVE || r->valid != 1)
                continue;
            int distance = exp_dist(x, y, r->x_milli / 1000, r->y_milli / 1000);
            if (distance <= 4)
                expansion_mood_event(w, v, 24, -5, raid->id, 1000000000, 0);
            if (distance <= 1 && grid_in_bounds(&w->grid, r->x_milli / 1000, r->y_milli / 1000) &&
                w->grid.cost[grid_index(&w->grid, r->x_milli / 1000, r->y_milli / 1000)])
                expansion_mood_event(w, v, 16, -4, enemy->id, 18, 4);
        }
    }
    if (!expansion_person_protected(w, v))
        for (int i = 0; i < w->expansion->animal_count; i++) {
            const CoreAnimalView *animal = &w->expansion->animals[i];
            if (!animal->alive || animal->owner || EXP_ANIMALS[animal->kind].behavior != 2)
                continue;
            int distance = exp_dist(x, y, animal->x_milli / 1000, animal->y_milli / 1000);
            if (distance <= 4)
                expansion_mood_event(w, v, 13, -2, animal->id, 18, 4);
            if (distance <= 1 && grid_in_bounds(&w->grid, animal->x_milli / 1000, animal->y_milli / 1000) &&
                w->grid.cost[grid_index(&w->grid, animal->x_milli / 1000, animal->y_milli / 1000)])
                expansion_mood_event(w, v, 16, -4, animal->id, 18, 4);
        }
}
