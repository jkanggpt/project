/* threat.h — 현재 습격과 공용 실행기의 피해·처치·퇴각 보고 */
#ifndef CASTLE_THREAT_H
#define CASTLE_THREAT_H

#include <stdint.h>

#include "castle_core.h"

typedef struct CoreWorld World;
typedef struct Villager Villager;

#define THREAT_MAX_ENEMIES 64   /* 한 습격의 최대 적 수 */

typedef struct Threat {
    int32_t type;        /* CoreThreatType */
    int32_t direction;   /* CoreDirection */
    int32_t eta_tick;    /* 도착하는 틱 */
    int32_t strength;    /* 적 수 (예고한 그대로 옴) */
    int32_t near_warned; /* 반나절 전 경보를 보냈으면 1 */
} Threat;

typedef struct Enemy {
    int32_t id;
    int32_t type;  /* 적 종류 (EnemyDef 번호) */
    int32_t state; /* CoreEnemyState */
    int32_t summoner_id;
    int32_t looted; /* 이미 훔쳤으면 1 (두 번 훔치지 못함) */
    CoreEnemyRuntime runtime;
} Enemy;

typedef struct Raid {
    int32_t id;   /* 0 = 습격 없음 */
    int32_t type; /* CoreThreatType */
    int32_t direction;
    int32_t spawn_x;
    int32_t spawn_y;
    int32_t started_tick;
    Enemy *enemies; /* 동적 배열: 습격 구성 */
    int32_t enemy_count;
    int32_t enemy_capacity;
    int32_t killed;
    int32_t escaped;
    int32_t stolen;
    int32_t fallen; /* 쓰러진 주민 */
    int32_t phase, assault_end_tick, initial_count, first_discovered, initial_types[28];
} Raid;

typedef struct ThreatState {
    Threat threats[CORE_MAX_THREATS]; /* 도착이 빠른 순 */
    int32_t threat_count;
    int32_t next_raid_id; /* 다시 쓰지 않음 (저장함) */
    int32_t raids_done;   /* 끝난 습격 수 (첫 습격인지 판단, 저장함) */
    Raid raid;            /* 진행 중인 습격 (저장하지 않음: 습격 중에는 저장 불가) */
} ThreatState;

void threat_init(ThreatState *state);
/* 습격의 적 배열을 해제합니다 (world_destroy) */
void threat_free(ThreatState *state);

/* 새 게임: 첫 위협(작은 도적 무리)을 정합니다 */
/* 한 틱: 경보, 도착하면 습격 시작, 습격 시간 초과 */
int threat_raid_active(const World *world);
/* 진행 중인 습격을 바로 끝냅니다 (패배했을 때). 습격이 없으면 아무것도 안 함 */
void threat_abort_raid(World *world);
/* 디버그 전용 (core_debug_stress): 지금 바로 type 습격을 count 명으로 시작. 이미 습격 중이면
 * CORE_ERR_REJECTED */
int threat_debug_raid(World *world, int32_t type, int32_t direction, int32_t count);
/* 경비병인가 (다 지은 병영의 일꾼) */
int threat_is_guard(const World *world, const Villager *villager);

/* 보고 (api.c 가 인자를 검사한 뒤 부름). 습격 id 가 다르면 CORE_IGNORED */
int threat_report_building_damage(World *world, int32_t raid_id, int32_t building_id, int32_t damage);
int threat_report_villager_damage(World *world, int32_t raid_id, int32_t villager_id, int32_t damage);
int threat_report_enemy_killed(World *world, int32_t raid_id, int32_t enemy_id);
int threat_report_loot(World *world, int32_t raid_id, int32_t enemy_id, int32_t building_id,
                       int32_t *out_amount);
int threat_report_enemy_escaped(World *world, int32_t raid_id, int32_t enemy_id);
int threat_report_raid_ended(World *world, int32_t raid_id);

/* 위협 종류의 적 수 계산 (예고할 때와 같은 값) */

#endif
