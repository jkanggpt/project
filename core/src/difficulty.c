/*
 * difficulty.c — 난이도 프리셋 표 (설명은 difficulty.h)
 */
#include "difficulty.h"

static const DifficultyPreset PRESETS[CORE_DIFF_COUNT] = {
    /* 평화: 주민이 많고, 습격은 작고 드물며, 사건은 약하고, 공물도 적음 */
    {7, {300, 10, 80, 80, 40, 10}, 55, 60, 150, 60, 70},
    /* 보통 */
    {6, {200, 5, 60, 60, 30, 5}, 50, 100, 100, 100, 100},
    /* 혹독: 주민이 적고, 습격은 크고 잦으며, 사건은 세고, 공물도 많음 */
    {5, {150, 3, 40, 40, 20, 3}, 45, 140, 75, 140, 130},
};

const DifficultyPreset *difficulty_preset(int32_t difficulty)
{
    if (difficulty < 0 || difficulty >= CORE_DIFF_COUNT) {
        difficulty = CORE_DIFF_NORMAL;
    }
    return &PRESETS[difficulty];
}
