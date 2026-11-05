/*
 * main.c — castle_cli 진입점: 명령줄 인자를 해석해서 대화형 메뉴, 시뮬레이터, 소크 테스트 중 하나를 실행합니다.
 */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "castle_core.h"
#include "commands.h"
#include "console.h"
#include "sim.h"

static void print_usage(void)
{
    printf("마법성채 키우기 — 콘솔판\n\n");
    printf("사용법:\n");
    printf("  castle_cli                         대화형 메뉴\n");
    printf("  castle_cli --load <파일>           세이브 파일을 불러와 시작\n");
    printf("  castle_cli --simulate <일수> --seed <시드> [--bot <이름>] [--difficulty 0-2] [--out <CSV>]\n");
    printf("  castle_cli --soak <일수> --seeds <개수> [--first-seed <시드>]\n");
    printf("  castle_cli --help\n\n");
    printf("봇:\n");
    sim_print_bots();
}

/* 명령줄 숫자 인자 (정확히 정수여야 함) */
static int parse_arg_int(const char *text, int min, int max, int *out)
{
    char *end = NULL;
    long value;
    errno = 0;
    value = strtol(text, &end, 10);
    if (text[0] == '\0' || end == NULL || *end != '\0' || errno != 0 || value < min || value > max) {
        return 0;
    }
    *out = (int)value;
    return 1;
}

typedef enum Mode { MODE_MENU, MODE_SIMULATE, MODE_SOAK } Mode;

typedef struct Options {
    Mode mode;
    int days;
    int seed;
    int seeds;
    int first_seed;
    int difficulty;
    const char *bot;
    const char *csv_path;
    const char *load_path;
} Options;

/* 성공 1, 잘못된 인자 0 */
static int parse_options(int argc, char **argv, Options *options)
{
    int i;
    memset(options, 0, sizeof(*options));
    options->mode = MODE_MENU;
    options->seeds = 1;
    options->difficulty = CORE_DIFF_NORMAL;
    options->bot = "idle";

    for (i = 1; i < argc; i++) {
        const char *arg = argv[i];
        const char *value = (i + 1 < argc) ? argv[i + 1] : NULL;
        int ok = 1;
        if (strcmp(arg, "--simulate") == 0 && value != NULL) {
            options->mode = MODE_SIMULATE;
            ok = parse_arg_int(value, 1, 100000, &options->days);
            i++;
        } else if (strcmp(arg, "--soak") == 0 && value != NULL) {
            options->mode = MODE_SOAK;
            ok = parse_arg_int(value, 1, 100000, &options->days);
            i++;
        } else if (strcmp(arg, "--seed") == 0 && value != NULL) {
            ok = parse_arg_int(value, 0, 2147483647, &options->seed);
            i++;
        } else if (strcmp(arg, "--seeds") == 0 && value != NULL) {
            ok = parse_arg_int(value, 1, 100000, &options->seeds);
            i++;
        } else if (strcmp(arg, "--first-seed") == 0 && value != NULL) {
            ok = parse_arg_int(value, 0, 2000000000, &options->first_seed);
            i++;
        } else if (strcmp(arg, "--difficulty") == 0 && value != NULL) {
            ok = parse_arg_int(value, 0, CORE_DIFF_COUNT - 1, &options->difficulty);
            i++;
        } else if (strcmp(arg, "--bot") == 0 && value != NULL) {
            options->bot = value;
            ok = sim_bot_exists(value);
            i++;
        } else if (strcmp(arg, "--out") == 0 && value != NULL) {
            options->csv_path = value;
            i++;
        } else if (strcmp(arg, "--load") == 0 && value != NULL) {
            options->load_path = value;
            i++;
        } else {
            ok = 0;
        }
        if (!ok) {
            fprintf(stderr, "잘못된 인자입니다: %s%s%s\n", arg, value ? " " : "", value ? value : "");
            return 0;
        }
    }
    return 1;
}

int main(int argc, char **argv)
{
    Options options;
    CliContext context;

    console_init();
    if (argc > 1 && (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0)) {
        print_usage();
        return 0;
    }
    if (!parse_options(argc, argv, &options)) {
        print_usage();
        return 2;
    }
    if (options.mode == MODE_SIMULATE) {
        return sim_run_simulation(options.days, options.seed, options.difficulty, options.bot, options.csv_path);
    }
    if (options.mode == MODE_SOAK) {
        return sim_run_soak(options.days, options.seeds, options.first_seed);
    }

    memset(&context, 0, sizeof(context));
    strcpy(context.save_path, "castle.sav");
    printf("마법성채 키우기 — 콘솔판 (코어 ABI %d)\n", (int)core_abi_version());
    if (options.load_path != NULL && !cli_load_game(&context, options.load_path)) {
        printf("새 게임을 시작하거나 다른 파일을 불러오세요.\n");
    }
    cli_run_menu(&context);
    core_destroy(context.world);
    printf("안녕히 가세요.\n");
    return 0;
}
