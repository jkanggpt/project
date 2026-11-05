/*
 * threat.h — 위협과 습격: 일정, 구성, 결과
 *
 * 1) 다음 위협(도적, 마물, 적대 영주 군대)을 미리 정하고 THREAT_SCHEDULED 로 알립니다 (종류, 방향, 도착까지 틱, 세기).
 *    도착 반나절 전에 THREAT_NEAR 로 한 번 더 경보합니다. "위협은 공정하게": 언제나 준비할 시간이 있습니다.
 * 2) 도착하면 습격(Raid)을 만듭니다. 적 목록은 동적 배열이고(몇 명일지 위협마다 다름),
 *    구성은 위협 종류마다 함수 포인터 표(THREAT_KINDS)의 구성 함수가 정합니다.
 * 3) 싸움은 Godot(또는 CLI 의 숫자 전투)이 하고, 결과를 보고합니다: 건물 피해, 주민 피해, 적 처치, 약탈, 달아남, 끝.
 *    C 는 규칙대로 적용합니다: 체력, 무너짐, 죽음과 슬픔, 마정석 보상, 빼앗긴 금화.
 * 4) 보고가 끊겨도 하루가 지나면 C 가 스스로 습격을 끝냅니다. 습격 중에는 저장할 수 없습니다.
 */
#ifndef CASTLE_THREAT_H
#define CASTLE_THREAT_H

#include <stdint.h>

#include "castle_core.h"

typedef struct CoreWorld World;
typedef struct Villager Villager;

#define THREAT_FIRST_DAY 5           /* 첫 습격(작은 도적 무리)이 오는 날 */
#define THREAT_NEAR_TICKS 72         /* 도착 반나절 전에 경보 */
#define THREAT_RAID_TIMEOUT 144      /* 보고가 없어도 하루 뒤에는 습격이 끝남 */
#define THREAT_MAX_ENEMIES 64        /* 한 습격의 최대 적 수 */
#define THREAT_UNSHELTERED_SAFETY 2  /* 습격 중 밖에 있는 주민의 안전 욕구가 틱마다 이만큼 줄어듦 */

typedef struct Threat {
    int32_t type;        /* CoreThreatType */
    int32_t direction;   /* CoreDirection */
    int32_t eta_tick;    /* 도착하는 틱 */
    int32_t strength;    /* 적 수 (예고한 그대로 옴) */
    int32_t near_warned; /* 반나절 전 경보를 보냈으면 1 */
} Threat;

typedef struct Enemy {
    int32_t id;
    int32_t type;   /* 적 종류 (EnemyDef 번호) */
    int32_t state;  /* CoreEnemyState */
    int32_t looted; /* 이미 훔쳤으면 1 (두 번 훔치지 못함) */
} Enemy;

typedef struct Raid {
    int32_t id;           /* 0 = 습격 없음 */
    int32_t type;         /* CoreThreatType */
    int32_t direction;
    int32_t spawn_x;
    int32_t spawn_y;
    int32_t started_tick;
    Enemy *enemies;       /* 동적 배열: 습격 구성 */
    int32_t enemy_count;
    int32_t enemy_capacity;
    int32_t killed;
    int32_t escaped;
    int32_t stolen;
    int32_t fallen;       /* 쓰러진 주민 */
} Raid;

typedef struct ThreatState {
    Threat threats[CORE_MAX_THREATS]; /* 도착이 빠른 순 */
    int32_t threat_count;
    int32_t next_raid_id;   /* 다시 쓰지 않음 (저장함) */
    int32_t raids_done;     /* 끝난 습격 수 (첫 습격인지 판단, 저장함) */
    Raid raid;              /* 진행 중인 습격 (저장하지 않음: 습격 중에는 저장 불가) */
} ThreatState;

void threat_init(ThreatState *state);
/* 습격의 적 배열을 해제합니다 (world_destroy) */
void threat_free(ThreatState *state);

/* 새 게임: 첫 위협(작은 도적 무리)을 정합니다 */
void threat_start_game(World *world);
/* 한 틱: 경보, 도착하면 습격 시작, 습격 시간 초과 */
void threat_tick(World *world);
int threat_raid_active(const World *world);
/* 경비병인가 (다 지은 병영의 일꾼) */
int threat_is_guard(const World *world, const Villager *villager);

/* 보고 (api.c 가 인자를 검사한 뒤 부름). 습격 id 가 다르면 CORE_IGNORED */
int threat_report_building_damage(World *world, int32_t raid_id, int32_t building_id, int32_t damage);
int threat_report_villager_damage(World *world, int32_t raid_id, int32_t villager_id, int32_t damage);
int threat_report_enemy_killed(World *world, int32_t raid_id, int32_t enemy_id);
int threat_report_loot(World *world, int32_t raid_id, int32_t enemy_id, int32_t building_id, int32_t *out_amount);
int threat_report_enemy_escaped(World *world, int32_t raid_id, int32_t enemy_id);
int threat_report_raid_ended(World *world, int32_t raid_id);

/* 위협 종류의 적 수 계산 (예고할 때와 같은 값) */
int32_t threat_strength(const World *world, int32_t type);

#endif
