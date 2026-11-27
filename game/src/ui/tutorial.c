/* tutorial.c — 튜토리얼 안내 (설명은 tutorial.h)
 *
 * 지금은 자리표시입니다: "튜토리얼" 단추가 평화 난이도의 새 게임을 시작할 뿐 안내는 아직 없습니다.
 * 튜토리얼 작업이 고정 지도, 코어의 단계 표, 안내 창과 강조 표시로 채웁니다.
 */
#include "app.h"
#include <stdio.h>
#include <string.h>

#define TUTORIAL_CASTLE_NAME u8"튜토리얼 영지"
#define TUTORIAL_SEED "42"

int game_tutorial_start(GameApp *a) {
    char name[sizeof(a->castle_name)], seed[sizeof(a->seed_text)];
    int difficulty = a->difficulty, started;
    /* 새 게임 창에 적어 둔 값은 튜토리얼이 끝난 뒤에도 그대로 남도록 잠깐만 바꿉니다. */
    memcpy(name, a->castle_name, sizeof(name));
    memcpy(seed, a->seed_text, sizeof(seed));
    (void)snprintf(a->castle_name, sizeof(a->castle_name), "%s", TUTORIAL_CASTLE_NAME);
    (void)snprintf(a->seed_text, sizeof(a->seed_text), "%s", TUTORIAL_SEED);
    a->difficulty = CORE_DIFF_PEACEFUL;
    started = game_app_start(a);
    memcpy(a->castle_name, name, sizeof(name));
    memcpy(a->seed_text, seed, sizeof(seed));
    a->difficulty = difficulty;
    a->tutorial.active = started;
    return started;
}

void game_tutorial_update(GameApp *a, float delta) {
    (void)a;
    (void)delta;
}

void game_tutorial_draw(GameApp *a) { (void)a; }
