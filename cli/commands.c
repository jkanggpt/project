/*
 * commands.c — 대화형 메뉴 명령들 (설명은 commands.h)
 */
#include "commands.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "console.h"
#include "format.h"

#define CLI_MORNING_MINUTE (6 * 60)

/* 쌓인 이벤트를 모두 꺼내 한 줄씩 출력합니다 */
static void print_events(CoreWorld *world)
{
    CoreEventView event;
    char text[160];
    while (core_poll_event(world, &event) == 1) {
        fmt_event(&event, text, sizeof(text));
        printf("  · %s\n", text);
    }
}

static void print_result(const char *action, int32_t rc)
{
    if (rc == CORE_OK) {
        printf("%s: %s\n", action, fmt_result_message(rc));
    } else {
        printf("%s 실패: %s (%s)\n", action, fmt_result_message(rc), core_result_name(rc));
    }
}

/* 기존 게임을 새 게임으로 바꿉니다 (기존 것은 해제) */
static void replace_world(CliContext *context, CoreWorld *world)
{
    core_destroy(context->world);
    context->world = world;
}

/* ---- 명령 ------------------------------------------------------------ */

static int cmd_status(CliContext *context)
{
    CoreSummaryView summary;
    char date[96];
    int32_t i;
    int32_t rc = core_get_summary(context->world, &summary);
    if (rc != CORE_OK) {
        print_result("상태 읽기", rc);
        return 1;
    }
    fmt_date(&summary, date, sizeof(date));
    printf("\n=== %s ===\n", summary.castle_name);
    printf("날짜   : %s\n", date);
    printf("난이도 : %s   시드: %d\n", fmt_difficulty_name(summary.difficulty), (int)summary.seed);
    printf("지도   : %d×%d, 성채 위치 (%d, %d)\n", (int)summary.map_width, (int)summary.map_height,
           (int)summary.keep_x, (int)summary.keep_y);
    printf("자원   :");
    for (i = 0; i < CORE_RES_COUNT; i++) {
        printf(" %s %d", fmt_resource_name(i), (int)summary.resources[i]);
        if (i >= CORE_RES_FOOD) {
            printf("/%d", (int)summary.storage_capacity);
        }
        printf(i + 1 < CORE_RES_COUNT ? "," : "\n");
    }
    return 1;
}

static int cmd_map(CliContext *context)
{
    static const char SYMBOLS[CORE_TERRAIN_COUNT] = {'.', 'T', '^', '~', '*'};
    CoreSummaryView summary;
    uint8_t *terrain;
    int32_t x;
    int32_t y;
    int32_t half;
    int32_t rc = core_get_summary(context->world, &summary);
    if (rc != CORE_OK) {
        print_result("지도 읽기", rc);
        return 1;
    }
    /* 지도 크기는 실행 중에 정해지므로 동적으로 할당합니다 */
    terrain = (uint8_t *)malloc((size_t)summary.map_width * (size_t)summary.map_height);
    if (terrain == NULL) {
        printf("메모리가 부족해서 지도를 보여 줄 수 없습니다.\n");
        return 1;
    }
    rc = core_copy_terrain(context->world, terrain, summary.map_width * summary.map_height);
    if (rc != CORE_OK) {
        print_result("지도 읽기", rc);
        free(terrain);
        return 1;
    }
    half = summary.keep_size / 2;
    for (y = 0; y < summary.map_height; y++) {
        for (x = 0; x < summary.map_width; x++) {
            int in_keep = abs(x - summary.keep_x) <= half && abs(y - summary.keep_y) <= half;
            uint8_t t = terrain[y * summary.map_width + x];
            putchar(in_keep ? 'K' : (t < CORE_TERRAIN_COUNT ? SYMBOLS[t] : '?'));
        }
        putchar('\n');
    }
    printf("범례: K 성채  . %s  T %s  ^ %s  ~ %s  * %s\n", fmt_terrain_name(0), fmt_terrain_name(1),
           fmt_terrain_name(2), fmt_terrain_name(3), fmt_terrain_name(4));
    free(terrain);
    return 1;
}

static int cmd_advance_ticks(CliContext *context)
{
    CoreConfigView config;
    int ticks;
    int32_t rc;
    (void)core_get_config(&config);
    printf("한 틱은 게임 속 %d분입니다.\n", (int)config.minutes_per_tick);
    if (!read_int("몇 틱 진행할까요?", 1, config.max_ticks_per_call, &ticks)) {
        return 0;
    }
    rc = core_tick(context->world, ticks);
    if (rc != CORE_OK) {
        print_result("시간 진행", rc);
        return 1;
    }
    print_events(context->world);
    return cmd_status(context);
}

static int cmd_next_morning(CliContext *context)
{
    CoreSummaryView summary;
    CoreConfigView config;
    int32_t minutes;
    int32_t rc;
    (void)core_get_config(&config);
    rc = core_get_summary(context->world, &summary);
    if (rc != CORE_OK) {
        print_result("상태 읽기", rc);
        return 1;
    }
    /* 다음 날 06:00 까지 남은 분 */
    minutes = 24 * 60 - summary.minute_of_day + CLI_MORNING_MINUTE;
    if (summary.minute_of_day < CLI_MORNING_MINUTE) {
        minutes = CLI_MORNING_MINUTE - summary.minute_of_day;
    }
    rc = core_tick(context->world, minutes / config.minutes_per_tick);
    if (rc != CORE_OK) {
        print_result("시간 진행", rc);
        return 1;
    }
    print_events(context->world);
    return cmd_status(context);
}

static int cmd_save(CliContext *context)
{
    char path[CLI_PATH_BYTES];
    int32_t rc;
    if (!read_text("저장할 파일 이름", context->save_path, path, sizeof(path))) {
        return 0;
    }
    rc = core_save(context->world, path);
    print_result("저장", rc);
    if (rc == CORE_OK) {
        strcpy(context->save_path, path);
    }
    return 1;
}

int cli_load_game(CliContext *context, const char *path)
{
    CoreWorld *loaded = NULL;
    int32_t line = 0;
    int32_t rc = core_load(path, &loaded, &line);
    if (rc != CORE_OK) {
        print_result("불러오기", rc);
        if (line > 0) {
            printf("  문제가 있는 줄: %d\n", (int)line);
        }
        return 0;
    }
    replace_world(context, loaded);
    if (strlen(path) < sizeof(context->save_path)) {
        strcpy(context->save_path, path);
    }
    printf("'%s' 에서 불러왔습니다.\n", path);
    return 1;
}

static int cmd_load(CliContext *context)
{
    char path[CLI_PATH_BYTES];
    if (!read_text("불러올 파일 이름", context->save_path, path, sizeof(path))) {
        return 0;
    }
    if (cli_load_game(context, path)) {
        return cmd_status(context);
    }
    return 1;
}

static int cmd_new_game(CliContext *context)
{
    CoreNewGameParams params;
    CoreWorld *world = NULL;
    int seed;
    int difficulty;
    int32_t rc;

    memset(&params, 0, sizeof(params));
    for (;;) {
        if (!read_text("성채 이름", "바람언덕", params.castle_name, sizeof(params.castle_name))) {
            return 0;
        }
        if (!read_int("지도 시드", 0, 2147483647, &seed)) {
            return 0;
        }
        printf("난이도: 0) 평화  1) 보통  2) 혹독\n");
        if (!read_int("난이도", 0, CORE_DIFF_COUNT - 1, &difficulty)) {
            return 0;
        }
        params.seed = seed;
        params.difficulty = difficulty;
        rc = core_create(&params, &world);
        if (rc == CORE_OK) {
            break;
        }
        print_result("새 게임", rc);
        if (rc != CORE_ERR_INVALID_ARG) {
            return 1; /* 메모리 부족 등: 메뉴로 돌아감 */
        }
        printf("이름에 쓸 수 없는 글자가 있습니다. 다시 입력하세요.\n");
    }
    replace_world(context, world);
    printf("새 게임을 시작했습니다.\n");
    return cmd_status(context);
}

static int cmd_quit(CliContext *context)
{
    context->running = 0;
    return 1;
}

/* 메뉴 명령 표. 0번은 언제나 종료입니다 */
static const CliCommand COMMANDS[] = {
    {"종료", cmd_quit, 0},
    {"상태 보기", cmd_status, 1},
    {"지도 보기", cmd_map, 1},
    {"시간 진행 (틱 단위)", cmd_advance_ticks, 1},
    {"다음 날 아침까지 진행", cmd_next_morning, 1},
    {"저장하기", cmd_save, 1},
    {"불러오기", cmd_load, 0},
    {"새 게임", cmd_new_game, 0},
};
#define COMMAND_COUNT ((int)(sizeof(COMMANDS) / sizeof(COMMANDS[0])))

static void print_menu(const CliContext *context)
{
    int i;
    printf("\n--- 메뉴 ---\n");
    for (i = 1; i < COMMAND_COUNT; i++) {
        if (COMMANDS[i].needs_world && context->world == NULL) {
            continue;
        }
        printf("%d) %s\n", i, COMMANDS[i].label);
    }
    printf("0) %s\n", COMMANDS[0].label);
}

void cli_run_menu(CliContext *context)
{
    context->running = 1;
    while (context->running) {
        int choice;
        const CliCommand *command;
        print_menu(context);
        if (!read_int("번호를 고르세요", 0, COMMAND_COUNT - 1, &choice)) {
            break; /* 입력이 끝남 */
        }
        command = &COMMANDS[choice];
        if (command->needs_world && context->world == NULL) {
            printf("먼저 새 게임을 시작하거나 불러오세요.\n");
            continue;
        }
        if (!command->run(context)) {
            break;
        }
    }
}
