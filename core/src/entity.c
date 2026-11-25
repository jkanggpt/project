/*
 * entity.c — 주민과 건물의 생성, 연결, 삭제 (설명은 entity.h)
 */
#include "entity.h"
#include "castle_expansion.h"

#include <string.h>

#include "expansion.h"
#include "mem.h"
#include "world.h"

/* 포인터 배열(Villager** 또는 Building**)에 칸을 하나 더 확보합니다. 실패하면 배열은 그대로 */
static int reserve_slot(void ***array, int32_t *capacity, int32_t count)
{
    int32_t new_capacity;
    void **grown;
    if (count < *capacity) {
        return CORE_OK;
    }
    new_capacity = *capacity > 0 ? *capacity * 2 : 8;
    grown = (void **)mem_realloc(*array, (size_t)new_capacity * sizeof(void *));
    if (grown == NULL) {
        return CORE_ERR_OUT_OF_MEMORY;
    }
    *array = grown;
    *capacity = new_capacity;
    return CORE_OK;
}

Villager *entity_add_villager(World *world)
{
    void **array = (void **)world->villagers;
    Villager *villager;
    int rc = reserve_slot(&array, &world->villager_capacity, world->villager_count);
    world->villagers = (Villager **)array;
    if (rc != CORE_OK) {
        return NULL;
    }
    villager = (Villager *)mem_calloc(1, sizeof(Villager));
    if (villager == NULL) {
        return NULL;
    }
    villager->id = world->next_villager_id++;
    villager->traits[0] = -1;
    villager->traits[1] = -1;
    villager->blocked_day = -1;
    world->villagers[world->villager_count++] = villager;
    return villager;
}

Building *entity_add_building(World *world, int32_t type)
{
    void **array = (void **)world->buildings;
    Building *building;
    int rc = reserve_slot(&array, &world->building_capacity, world->building_count);
    world->buildings = (Building **)array;
    if (rc != CORE_OK) {
        return NULL;
    }
    building = (Building *)mem_calloc(1, sizeof(Building));
    if (building == NULL) {
        return NULL;
    }
    building->id = world->next_building_id++;
    building->type = type;
    building->auto_assign = 1;
    building->productivity = 100;
    world->buildings[world->building_count++] = building;
    return building;
}

Villager *entity_find_villager(const World *world, int32_t id)
{
    int32_t i;
    for (i = 0; i < world->villager_count; i++) {
        if (world->villagers[i]->id == id) {
            return world->villagers[i];
        }
    }
    return NULL;
}

Building *entity_find_building(const World *world, int32_t id)
{
    int32_t i;
    for (i = 0; i < world->building_count; i++) {
        if (world->buildings[i]->id == id) {
            return world->buildings[i];
        }
    }
    return NULL;
}

int entity_building_covers(const Building *building, int32_t x, int32_t y)
{
    return x >= building->x && y >= building->y && x < building->x + building->w && y < building->y + building->h;
}

/* 정렬된 Villager* 배열에 끼워 넣습니다 (id 순서를 지켜 저장/불러오기 뒤에도 순서가 같게) */
static int insert_sorted(Villager ***array, int32_t *count, int32_t *capacity, Villager *villager)
{
    void **raw = (void **)*array;
    int32_t at;
    int rc = reserve_slot(&raw, capacity, *count);
    *array = (Villager **)raw;
    if (rc != CORE_OK) {
        return rc;
    }
    at = *count;
    while (at > 0 && (*array)[at - 1]->id > villager->id) {
        (*array)[at] = (*array)[at - 1];
        at--;
    }
    (*array)[at] = villager;
    (*count)++;
    return CORE_OK;
}

/* 배열에서 villager 를 빼고 뒤를 당깁니다 (순서 유지) */
static void remove_from(Villager **array, int32_t *count, const Villager *villager)
{
    int32_t i;
    for (i = 0; i < *count; i++) {
        if (array[i] == villager) {
            memmove(&array[i], &array[i + 1], (size_t)(*count - i - 1) * sizeof(Villager *));
            (*count)--;
            return;
        }
    }
}

int entity_link_worker(Building *building, Villager *villager, int32_t slots)
{
    int rc;
    if (building->worker_count >= slots) {
        return CORE_ERR_REJECTED;
    }
    entity_unlink_worker(villager);
    rc = insert_sorted(&building->workers, &building->worker_count, &building->worker_capacity, villager);
    if (rc == CORE_OK) {
        villager->workplace = building; /* 반대 방향 연결 */
    }
    return rc;
}

int entity_link_resident(Building *building, Villager *villager, int32_t slots)
{
    int rc;
    if (building->resident_count >= slots) {
        return CORE_ERR_REJECTED;
    }
    entity_unlink_resident(villager);
    rc = insert_sorted(&building->residents, &building->resident_count, &building->resident_capacity, villager);
    if (rc == CORE_OK) {
        villager->home = building;
    }
    return rc;
}

void entity_unlink_worker(Villager *villager)
{
    if (villager->workplace != NULL) {
        if(villager->workplace->combat_guard_id==villager->id){villager->workplace->combat_guard_id=0;villager->workplace->combat_until_tick=0;}
        villager->mission=CORE_MISSION_NONE;
        remove_from(villager->workplace->workers, &villager->workplace->worker_count, villager);
        villager->workplace = NULL;
    }
}

void entity_unlink_resident(Villager *villager)
{
    if (villager->home != NULL) {
        remove_from(villager->home->residents, &villager->home->resident_count, villager);
        villager->home = NULL;
    }
}

/* 목록에서 index 자리를 마지막 원소로 채우고 개수를 줄입니다 (swap-remove).
 * 뒤에서부터 도는 반복문 안에서 불러도 안전합니다: 옮겨 온 원소는 이미 처리한 것이기 때문 */
static void swap_remove_villager(World *world, int32_t index)
{
    world->villagers[index] = world->villagers[world->villager_count - 1];
    world->villager_count--;
}

static void swap_remove_building(World *world, int32_t index)
{
    world->buildings[index] = world->buildings[world->building_count - 1];
    world->building_count--;
}

void entity_remove_villager(World *world, Villager *villager)
{
    int32_t i;
    entity_unlink_worker(villager);
    entity_unlink_resident(villager);
    if (world->expansion)
        for (int boat = 0; boat < world->expansion->boat_count; boat++) {
            CoreBoatView *b = &world->expansion->boats[boat];
            for (int crew = b->crew_count - 1; crew >= 0; crew--)
                if (b->crew[crew] == villager->id) {
                    for (int k = crew + 1; k < b->crew_count; k++)
                        b->crew[k - 1] = b->crew[k];
                    b->crew[--b->crew_count] = 0;
                    if (!b->crew_count)
                        b->state = 0;
                }
        }
    villager->target = NULL;
    for (i = 0; i < world->villager_count; i++) {
        if (world->villagers[i] == villager) {
            swap_remove_villager(world, i);
            break;
        }
    }
    mem_free(villager);
}

void entity_remove_building(World *world, Building *building)
{
    int32_t i;
    /* 1) 이 건물을 가리키는 주민 쪽 포인터를 모두 지웁니다 */
    for (i = 0; i < world->villager_count; i++) {
        Villager *villager = world->villagers[i];
        if (villager->workplace == building) {
            villager->workplace = NULL;
        }
        if (villager->home == building) {
            villager->home = NULL;
        }
        if (villager->target == building) {
            villager->target = NULL;
            villager->at_target = 0;
            villager->intent_rev++; /* 목표가 사라졌으니 의도가 바뀐 것 */
        }
    }
    /* 2) 건물 쪽 배열을 해제합니다 */
    mem_free(building->workers);
    mem_free(building->residents);
    /* 3) 월드 목록에서 빼고 해제합니다 */
    for (i = 0; i < world->building_count; i++) {
        if (world->buildings[i] == building) {
            swap_remove_building(world, i);
            break;
        }
    }
    mem_free(building);
}

void entity_free_all(World *world)
{
    int32_t i;
    for (i = 0; i < world->villager_count; i++) {
        mem_free(world->villagers[i]);
    }
    for (i = 0; i < world->building_count; i++) {
        mem_free(world->buildings[i]->workers);
        mem_free(world->buildings[i]->residents);
        mem_free(world->buildings[i]);
    }
    mem_free(world->villagers);
    mem_free(world->buildings);
    world->villagers = NULL;
    world->buildings = NULL;
    world->villager_count = 0;
    world->building_count = 0;
    world->villager_capacity = 0;
    world->building_capacity = 0;
}
