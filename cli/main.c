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
    printf("  castle_cli --simulate <일수> --seed <시드> [--bot <이름>] [--difficulty 0-2] [--out <CSV>] [--events]\n");
    printf("  castle_cli --soak <일수> --seeds <개수> [--first-seed <시드>]\n");
    printf("  castle_cli --dump-defs <폴더>       내장 기본 정의를 정의 파일로 씀\n");
    printf("  (모든 모드) --data <폴더>           정의 파일 폴더 (기본: game/data)\n");
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

typedef enum Mode { MODE_MENU, MODE_SIMULATE, MODE_SOAK, MODE_DUMP_DEFS } Mode;

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
    const char *data_dir;
    const char *dump_dir;
    int print_events;
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
    options->data_dir = "game/data";

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
        } else if (strcmp(arg, "--events") == 0) {
            options->print_events = 1;
        } else if (strcmp(arg, "--data") == 0 && value != NULL) {
            options->data_dir = value;
            i++;
        } else if (strcmp(arg, "--dump-defs") == 0 && value != NULL) {
            options->mode = MODE_DUMP_DEFS;
            options->dump_dir = value;
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

/* 정의 파일 이름 (순서는 CoreDataFile) */
static const char *const DATA_FILES[CORE_DATA_COUNT] = {"buildings.def", "traits.def",   "names.txt",
                                                        "enemies.def",   "research.def", "ranks.def"};

static void join_path(char *out, size_t size, const char *dir, const char *file)
{
    snprintf(out, size, "%s/%s", dir, file);
}

/* data 폴더의 정의 파일을 읽습니다. 없거나 망가진 파일은 기본값을 쓰고 알려 줍니다 */
static void load_defs(CoreDefs *defs, const char *data_dir, int quiet)
{
    int32_t file;
    for (file = 0; file < CORE_DATA_COUNT; file++) {
        char path[CLI_PATH_BYTES];
        int32_t line = 0;
        int32_t rc;
        join_path(path, sizeof(path), data_dir, DATA_FILES[file]);
        rc = core_defs_load_file(defs, file, path, &line);
        if (rc == CORE_ERR_FILE_OPEN && !quiet) {
            printf("정의 파일이 없어 기본값을 씁니다: %s\n", path);
        } else if (rc == CORE_ERR_FORMAT) {
            printf("정의 파일 %s 의 %d번째 줄이 잘못되어 기본값을 씁니다.\n", path, (int)line);
        } else if (rc != CORE_OK && rc != CORE_ERR_FILE_OPEN) {
            printf("정의 파일을 읽지 못했습니다: %s (%s)\n", path, core_result_name(rc));
        }
    }
}

static int dump_defs(const CoreDefs *defs, const char *dir)
{
    int32_t file;
    for (file = 0; file < CORE_DATA_COUNT; file++) {
        char path[CLI_PATH_BYTES];
        int32_t rc;
        join_path(path, sizeof(path), dir, DATA_FILES[file]);
        rc = core_defs_save_file(defs, file, path);
        if (rc != CORE_OK) {
            fprintf(stderr, "쓸 수 없습니다: %s (%s)\n", path, core_result_name(rc));
            return 1;
        }
        printf("썼습니다: %s\n", path);
    }
    return 0;
}

int main(int argc, char **argv)
{
    CoreDefs *defs = NULL;
    int status;
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
    if (core_defs_create(&defs) != CORE_OK) {
        fprintf(stderr, "메모리가 부족합니다.\n");
        return 1;
    }
    if (options.mode == MODE_DUMP_DEFS) {
        status = dump_defs(defs, options.dump_dir); /* 내장 기본값 그대로 씀 */
        core_defs_destroy(defs);
        return status;
    }
    load_defs(defs, options.data_dir, options.mode != MODE_MENU);
    if (options.mode == MODE_SIMULATE) {
        status = sim_run_simulation(options.days, options.seed, options.difficulty, options.bot, options.csv_path,
                                    defs, options.print_events);
        core_defs_destroy(defs);
        return status;
    }
    if (options.mode == MODE_SOAK) {
        status = sim_run_soak(options.days, options.seeds, options.first_seed, defs);
        core_defs_destroy(defs);
        return status;
    }

    memset(&context, 0, sizeof(context));
    context.defs = defs;
    walker_init(&context.walker);
    battle_init(&context.battle);
    strcpy(context.save_path, "castle.sav");
    printf("마법성채 키우기 — 콘솔판 (코어 ABI %d)\n", (int)core_abi_version());
    if (options.load_path != NULL && !cli_load_game(&context, options.load_path)) {
        printf("새 게임을 시작하거나 다른 파일을 불러오세요.\n");
    }
    cli_run_menu(&context);
    core_destroy(context.world);
    walker_free(&context.walker);
    core_defs_destroy(defs);
    printf("안녕히 가세요.\n");
    return 0;
}
