/* placement.c — 현재 배치 명령의 공용 입구와 손상·수리 */
#include "placement.h"
#include "expansion.h"
#include "jobs.h"
#include "rank.h"
#include "threat.h"
#include <string.h>

static int32_t rule_terrain(int32_t rule) {
    switch (rule) {
    case CORE_PLACE_NEAR_FOREST:
        return CORE_TERRAIN_FOREST;
    case CORE_PLACE_ADJ_ROCK:
        return CORE_TERRAIN_ROCK;
    case CORE_PLACE_ON_MANA:
        return CORE_TERRAIN_MANA;
    default:
        return -1;
    }
}

int32_t placement_counted_terrain(const BuildingDef *def) { return rule_terrain(def->placement); }

void placement_check(const World *world, int32_t type, int32_t x, int32_t y, CorePlacementView *out) {
    expansion_place_check(world, type, x, y, 0, 0, out);
}

void placement_rebuild_layers(World *world) { expansion_rebuild(world); }

void placement_refresh_capacity(World *world) { expansion_capacity(world); }

int placement_place(World *world, int32_t type, int32_t x, int32_t y, int32_t *id, int32_t *reason) {
    return expansion_place(world, type, x, y, 0, id, reason);
}

int placement_line(World *world, int32_t type, int32_t x0, int32_t y0, int32_t x1, int32_t y1,
                   int32_t *out_placed, int32_t *out_reason) {
    const BuildingDef *def = world_building_def(world, type);
    int32_t x = x0;
    int32_t y = y0;
    int32_t steps = 0;

    *out_placed = 0;
    *out_reason = CORE_PLACE_OK;
    if (def == NULL || !def->line) {
        *out_reason = CORE_PLACE_BAD_TYPE;
        return CORE_ERR_REJECTED;
    }
    /* 가로로 먼저 가고 꺾어서 세로로 갑니다 (L 자 모양) */
    for (;;) {
        CorePlacementView check;
        int32_t id;
        int32_t reason;
        placement_check(world, type, x, y, &check);
        if (check.result == CORE_PLACE_OK) {
            int rc = placement_place(world, type, x, y, &id, &reason);
            if (rc == CORE_ERR_OUT_OF_MEMORY) {
                return rc;
            }
            if (rc == CORE_OK) {
                (*out_placed)++;
            }
        } else if (check.result != CORE_PLACE_ALREADY_THERE) {
            if (*out_reason == CORE_PLACE_OK) {
                *out_reason = check.result;
            }
            if (check.result == CORE_PLACE_CANNOT_AFFORD) {
                break; /* 돈이 떨어지면 멈춤 */
            }
        }
        if (x == x1 && y == y1) {
            break;
        }
        if (++steps >= PLACEMENT_MAX_LINE) {
            break;
        }
        if (x != x1) {
            x += x1 > x ? 1 : -1;
        } else {
            y += y1 > y ? 1 : -1;
        }
    }
    if (*out_placed == 0 && *out_reason != CORE_PLACE_OK) {
        return CORE_ERR_REJECTED;
    }
    return CORE_OK;
}

static void damage_event(World *world, Building *b, CoreEventType type) {
    Event event = {0};
    event.type = type;
    event.as.building.id = b->id;
    event.as.building.type = b->type;
    event.as.building.x = b->x;
    event.as.building.y = b->y;
    event.cause_kind = b->damage_tick == world->clock.tick ? b->damage_kind : 0;
    event.source_id = b->damage_source;
    event.event_tick = world->clock.tick;
    event.type_serial = b->type_serial;
    if (event.cause_kind == 1 && event.source_id >= 1 && event.source_id <= world->threat.raid.enemy_count)
        event.source_type = world->threat.raid.enemies[event.source_id - 1].type + 1;
    if (event.cause_kind == 2 && world->expansion)
        for (int i = 0; i < world->expansion->animal_count; i++)
            if (world->expansion->animals[i].id == event.source_id)
                event.source_type = world->expansion->animals[i].kind + 1;
    events_push(&world->events, &event);
}

static void ruin_building(World *world, Building *building) {
    building->state = CORE_BSTATE_RUINED;
    building->hp = 0;
    building->flags = 0;
    building->work_milli[0] = 0;
    building->work_milli[1] = 0;
    while (building->worker_count > 0) {
        entity_unlink_worker(building->workers[building->worker_count - 1]);
    }
    while (building->resident_count > 0) {
        entity_unlink_resident(building->residents[building->resident_count - 1]);
    }
    damage_event(world, building, CORE_EVT_BUILDING_RUINED);
    { placement_rebuild_layers(world); /* 무너진 성벽·성문은 지나갈 수 있음 → 비용 격자가 바뀜 */ }
    placement_refresh_capacity(world);
    jobs_on_buildings_changed(world);
}

void placement_damage(World *world, Building *building, int32_t damage) {
    if (damage <= 0 || building->state == CORE_BSTATE_RUINED) {
        return;
    }

    building->hp -= damage;
    if (building->hp <= 0) {
        ruin_building(world, building);
    } else {
        events_push_building(&world->events, CORE_EVT_BUILDING_DAMAGED, building->id, building->type,
                             building->x, building->y);
    }
}

void placement_repair_cost(const World *world, const Building *building, int32_t out_cost[CORE_RES_COUNT]) {
    const BuildingDef *def = world_building_def(world, building->type);
    int32_t missing = def->max_hp - building->hp;
    int32_t any = 0;
    int32_t i;
    for (i = 0; i < CORE_RES_COUNT; i++) {
        out_cost[i] = 0;
    }
    if (building->state == CORE_BSTATE_CONSTRUCTING || missing <= 0 || def->max_hp <= 0) {
        return;
    }
    for (i = 0; i < CORE_RES_COUNT; i++) {
        /* 반올림이 아니라 올림: 조금만 부서져도 1 은 듦 */
        out_cost[i] =
            (int32_t)(((int64_t)def->cost[i] * missing + 2LL * def->max_hp - 1) / (2LL * def->max_hp));
        any += out_cost[i];
    }
    if (any == 0) {
        /* 비용이 없는 건물(성채)은 돌과 나무로 고칩니다 */
        out_cost[CORE_RES_STONE] = (missing + 19) / 20;
        out_cost[CORE_RES_WOOD] = (missing + 39) / 40;
    }
}

int placement_repair(World *world, Building *building, int32_t *out_reason) {
    const BuildingDef *def = world_building_def(world, building->type);
    int32_t cost[CORE_RES_COUNT];
    *out_reason = CORE_DENY_NONE;
    if (threat_raid_active(world)) {
        /* 맞는 동안 곧바로 고치면 성벽이 무너지지 않으므로, 습격이 끝난 뒤에 고칩니다 */
        *out_reason = CORE_DENY_RAID;
        return CORE_ERR_REJECTED;
    }
    if (building->state == CORE_BSTATE_CONSTRUCTING) {
        *out_reason = CORE_DENY_UNDER_CONSTRUCTION;
        return CORE_ERR_REJECTED;
    }
    if (building->hp >= def->max_hp) {
        *out_reason = CORE_DENY_NOT_DAMAGED;
        return CORE_ERR_REJECTED;
    }
    placement_repair_cost(world, building, cost);
    if (!eco_pay(&world->bank, &world->ledger, cost, CORE_LEDGER_REPAIR)) {
        *out_reason = CORE_DENY_NO_RESOURCES;
        return CORE_ERR_REJECTED;
    }
    building->hp = def->max_hp;
    building->state = CORE_BSTATE_ACTIVE;
    events_push_building(&world->events, CORE_EVT_BUILDING_REPAIRED, building->id, building->type,
                         building->x, building->y);
    if (world->expansion) {
        /* 수리비로 쓴 원목의 출처 잔량과 복구된 경계·도로를 즉시 맞춥니다. */
        if (world->expansion->magic_logs_milli > expansion_bank_have(world, CORE_RES_LOGS))
            world->expansion->magic_logs_milli = expansion_bank_have(world, CORE_RES_LOGS);
        world->expansion->dirty = 1;
        expansion_rebuild(world);
        expansion_capacity(world);
        expansion_assign(world);
    }
    return CORE_OK;
}
