/*
 * difficulty.h — 난이도 프리셋 (평화 / 보통 / 혹독)
 *
 * 난이도가 바꾸는 값은 모두 이 표 한 줄에 모여 있습니다: 시작 주민·자원, 기분의 기본값,
 * 습격의 세기와 간격, 자연 사건의 확률과 피해, 승급 공물. 다른 모듈은 difficulty_preset 으로 묻기만 합니다.
 */
#ifndef CASTLE_DIFFICULTY_H
#define CASTLE_DIFFICULTY_H

#include <stdint.h>

#include "castle_core.h"

typedef struct DifficultyPreset {
    int32_t base_mood;         /* 기분 계산의 기본값 */
    int32_t raid_strength_pct; /* 습격의 적 수 % */
    int32_t raid_interval_pct; /* 습격 사이 간격 % (클수록 드묾) */
    int32_t nature_pct;        /* 자연 사건의 확률과 피해 % */
    int32_t tribute_pct;       /* 승급 공물 % */
} DifficultyPreset;

/* 난이도의 설정. 범위 밖이면 보통 */
const DifficultyPreset *difficulty_preset(int32_t difficulty);

#endif
