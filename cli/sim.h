/*
 * sim.h — 자동 실행 모드
 *
 * --simulate: "적당히 하는 플레이어" 봇이 게임을 하며 하루마다 CSV 한 줄을 씁니다 (밸런스 조정용)
 * --soak:     무작위 명령으로 오래 돌리며 매 틱 불변 조건을 검사합니다 (안정성 확인용)
 */
#ifndef CASTLE_CLI_SIM_H
#define CASTLE_CLI_SIM_H

#include <stdint.h>

#include "castle_core.h"

/* csv_path 가 NULL 이면 표준 출력에 씁니다. 성공 0, 실패 1 */
/* print_events 가 1 이면 하루마다 알림 문장을 표준 오류로 출력합니다 (밸런스 조정 때 원인 찾기) */
int sim_run_simulation(int days, int32_t seed, int difficulty, const char *bot_name, const char *csv_path,
                       const CoreDefs *defs, int print_events);

/* 성공(위반 0) 0, 실패 1 */
int sim_run_soak(int days, int seeds, int32_t first_seed, const CoreDefs *defs);
/* 성능 측정: 현재 영지를 만들고 하루 적응 뒤 닷새 동안 공용 GameSession 진행 시간을 잽니다.
 * 평균과 가장 느린 틱을 출력. 0 = 성공 */
int sim_run_bench(int villagers, int enemies, int32_t seed, const CoreDefs *defs);

/* 봇 이름이 있으면 1 */
int sim_bot_exists(const char *bot_name);
/* 그래픽 자동 검사도 같은 C 봇의 명령을 쓰되, 이동과 전투는 자신의 실행 계층이 풉니다. */
int sim_bot_daily(CoreWorld *world, const char *bot_name);
int sim_expansion_bot_daily(CoreWorld *world);
/* 실제 거래 직원이 일하는 정오에 호출합니다. */
void sim_expansion_bot_trade(CoreWorld *world);
/* 봇 이름 목록을 출력 */
void sim_print_bots(void);

#endif
