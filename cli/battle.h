/*
 * battle.h — CLI 의 숫자 전투
 *
 * 게임에서는 C 그래픽 프런트엔드 이 적을 걷게 하고 싸움을 계산합니다. CLI(콘솔판, 시뮬레이터, 소크)에는 그런 화면이 없으므로,
 * 습격이 오면 적이 걸어올 시간만큼 기다렸다가 간단한 공식으로 싸움을 풀고,
 * C 그래픽 프런트엔드 과 **똑같은 보고 API**(core_report_*)로 결과를 넘깁니다. 그래서 C 코어의 습격 규칙은 CLI 로도 확인됩니다.
 *
 * 공식 (한 판 = 한 틱 = 게임 속 10분, 최대 12판):
 *   방어: 감시탑·성채 망루·경비병의 피해 × (10분 ÷ 공격 사이) 를 모아 적을 차례로 쓰러뜨림
 *   공격: 남은 적의 피해 합을 먼저 성벽 하나에(성벽이 있으면), 그다음 경비병과 건물에 나눠 줌
 *   다섯째 판부터, 성벽이 막고 있지 않으면 살아 있는 도적·마물이 성채에서 훔치고 달아남. 마지막에 남은 적은 물러남
 */
#ifndef CASTLE_CLI_BATTLE_H
#define CASTLE_CLI_BATTLE_H

#include <stdint.h>

#include "castle_core.h"

typedef struct Battle {
    int32_t raid_id;      /* 지금 풀고 있는 습격 (0 = 없음) */
    int32_t start_tick;   /* 적이 성채 근처에 닿는 틱 */
    int32_t round;        /* 지금까지 치른 판 */
    int32_t wall_id;      /* 먼저 공격받는 성벽 (0 = 없음) */
    uint32_t random;      /* 이 습격 전용 난수 (월드 난수를 건드리지 않음) */
    int32_t pool;         /* 방어 쪽 피해 누적 */
    int32_t enemy_hp;     /* 맨 앞 적의 남은 체력 */
    int32_t looted;       /* 도적·마물이 이미 훔쳐 갔으면 1 */
} Battle;

void battle_init(Battle *battle);
/* core_tick 뒤에 한 번씩 부릅니다. 습격이 있으면 한 판씩 진행합니다. CORE_OK 또는 오류 코드 */
int32_t battle_tick(Battle *battle, CoreWorld *world);

#endif
