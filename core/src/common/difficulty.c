/*
 * difficulty.c — 난이도 프리셋 표 (설명은 difficulty.h)
 */
#include "difficulty.h"

static const DifficultyPreset PRESETS[CORE_DIFF_COUNT] = {
    {55, 60, 150, 60, 70},
    /* 보통 */
    {50, 100, 100, 100, 100},
    {45, 140, 75, 140, 130},
};

const DifficultyPreset *difficulty_preset(int32_t difficulty) {
    if (difficulty < 0 || difficulty >= CORE_DIFF_COUNT) {
        difficulty = CORE_DIFF_NORMAL;
    }
    return &PRESETS[difficulty];
}
