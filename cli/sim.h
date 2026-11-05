/*
 * sim.h — 자동 실행 모드
 *
 * --simulate: "적당히 하는 플레이어" 봇이 게임을 하며 하루마다 CSV 한 줄을 씁니다 (밸런스 조정용)
 * --soak:     무작위 명령으로 오래 돌리며 매 틱 불변 조건을 검사합니다 (안정성 확인용)
 */
#ifndef CASTLE_CLI_SIM_H
#define CASTLE_CLI_SIM_H

#include <stdint.h>

/* csv_path 가 NULL 이면 표준 출력에 씁니다. 성공 0, 실패 1 */
int sim_run_simulation(int days, int32_t seed, int difficulty, const char *bot_name, const char *csv_path);

/* 성공(위반 0) 0, 실패 1 */
int sim_run_soak(int days, int seeds, int32_t first_seed);

/* 봇 이름이 있으면 1 */
int sim_bot_exists(const char *bot_name);
/* 봇 이름 목록을 출력 */
void sim_print_bots(void);

#endif
