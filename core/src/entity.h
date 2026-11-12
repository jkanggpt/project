/*
 * entity.h — 주민(Villager)과 건물(Building)
 *
 * 관계 (SPEC §6.2)
 *   - 주민은 Stats(능력치)와 Needs(욕구) 구조체를 "포함"하고,
 *     자기 집(home)과 일터(workplace) 건물을 포인터로 "참조"합니다.
 *   - 건물은 일꾼(workers)과 거주민(residents)을 Villager* 동적 배열로 "참조"합니다.
 *   - 월드는 Villager* / Building* 포인터 배열을 가지고, realloc 으로 키웁니다.
 *     주민·건물은 하나씩 따로 할당하므로 배열이 옮겨져도 주민 자체는 옮겨지지 않습니다.
 *     그래서 건물이 들고 있는 Villager* 가 깨지지 않습니다.
 *   - 지울 때는 양쪽 연결을 모두 끊은 뒤 free 합니다.
 */
#ifndef CASTLE_ENTITY_H
#define CASTLE_ENTITY_H

#include <stdint.h>

#include "castle_core.h"

typedef struct CoreWorld World;
typedef struct Building Building;
typedef struct Villager Villager;

typedef struct Stats {
    int32_t strength;  /* 힘 1..10 */
    int32_t intellect; /* 지능 */
    int32_t dexterity; /* 손재주 */
    int32_t vitality;  /* 체력 */
} Stats;

typedef struct Needs {
    int32_t hunger;  /* 0..100, 100 = 배부름 */
    int32_t rest;    /* 0..100 */
    int32_t safety;  /* 0..100 */
    int32_t housing; /* 0..100 */
} Needs;

struct Villager {
    int32_t id;
    char name[CORE_NAME_BYTES];
    int32_t age;
    Stats stats;          /* 포함 */
    Needs needs;          /* 포함 */
    int32_t traits[2];    /* 특성 번호, -1 = 없음 */
    int32_t mood;         /* 0..100 */
    int32_t health;       /* 0..100 */
    int32_t flags;        /* CORE_VF_* */
    Building *home;       /* 참조 (없으면 NULL) */
    Building *workplace;  /* 참조 */

    /* 의도: Godot 이 이것을 보고 걷게 합니다 */
    int32_t state;        /* CoreVillagerState */
    int32_t intent_rev;
    int32_t intent_flags; /* CORE_INTENT_* */
    Building *target;     /* 목표 건물 (참조, 칸 목표면 NULL) */
    int32_t target_x;
    int32_t target_y;
    int32_t at_target;
    int32_t loc_x;        /* 마지막으로 도착한 칸 */
    int32_t loc_y;

    /* 시간 기록 */
    int32_t state_ticks;    /* 지금 의도로 지낸 틱 */
    int32_t hold_ticks;     /* 길이 막혀 기다리는 남은 틱 */
    int32_t low_mood_ticks; /* 기분이 낮게 이어진 틱 */
    int32_t hunger_acc;     /* 배고픔 감소 누적 (×1000) */
    int32_t rest_acc;       /* 피로 누적 (×1000) */
    int32_t story_flags;    /* 이미 보낸 이야기 알림 (비트) */
    int32_t blocked_day;    /* 길 막힘 알림을 마지막으로 보낸 날 */
    int32_t sick_ticks;     /* 병이 저절로 낫기까지 남은 틱 (0 = 건강) */
    int32_t heal_ticks;     /* 치료소에서 치료받은 틱 */
};

struct Building {
    int32_t id;
    int32_t type;          /* CoreBuildingType */
    int32_t x;             /* 왼쪽 위 칸 */
    int32_t y;
    int32_t w;
    int32_t h;
    int32_t state;         /* CoreBuildingState */
    int32_t hp;
    int32_t build_progress;
    int32_t door_x;        /* 문 앞 칸 */
    int32_t door_y;
    int32_t auto_assign;
    int32_t work_milli[2]; /* 생산 누적 (주 자원, 두 번째 자원) × 1000 */
    int32_t productivity;  /* 지형에 따른 생산성 % (지도에서 다시 계산하므로 저장하지 않음) */
    int32_t flags;         /* CORE_BF_* (난방 못 함 등) */

    Villager **workers;    /* 일꾼 (id 순으로 정렬) */
    int32_t worker_count;
    int32_t worker_capacity;
    Villager **residents;  /* 거주민 (id 순으로 정렬) */
    int32_t resident_count;
    int32_t resident_capacity;
};

/* 새 주민/건물을 할당해 월드 목록 끝에 붙입니다. 실패하면 NULL (월드는 그대로) */
Villager *entity_add_villager(World *world);
Building *entity_add_building(World *world, int32_t type);
/* 양쪽 연결을 모두 끊고 목록에서 빼고 해제합니다 */
void entity_remove_villager(World *world, Villager *villager);
void entity_remove_building(World *world, Building *building);
/* 월드의 모든 주민·건물을 해제합니다 (world_destroy 에서) */
void entity_free_all(World *world);

Villager *entity_find_villager(const World *world, int32_t id);
Building *entity_find_building(const World *world, int32_t id);

/* 주민을 건물의 일꾼/거주민으로 연결합니다 (양쪽 모두). CORE_OK, CORE_ERR_REJECTED(자리 없음), OOM */
int entity_link_worker(Building *building, Villager *villager, int32_t slots);
int entity_link_resident(Building *building, Villager *villager, int32_t slots);
/* 연결을 끊습니다 (양쪽 모두). 연결이 없으면 아무 일도 하지 않음 */
void entity_unlink_worker(Villager *villager);
void entity_unlink_resident(Villager *villager);

/* 건물의 (x, y) 칸이 발자국 안에 있으면 1 */
int entity_building_covers(const Building *building, int32_t x, int32_t y);

#endif
