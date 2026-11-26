/* balance.c — 시드·난이도별 CSV와 승급 중앙값. 시뮬레이터는 같은 프로세스에서 실행합니다. */
#include "castle_core.h"
#include "format.h"
#include "platform.h"
#include "sim.h"
#include "storage.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int number(const char *text, int min, int max, int *out) {
    char *end;
    long value;
    errno = 0;
    value = strtol(text, &end, 10);
    if (!*text || *end || errno || value < min || value > max)
        return 0;
    *out = (int)value;
    return 1;
}
static int compare(const void *a, const void *b) {
    int x = *(const int *)a, y = *(const int *)b;
    return (x > y) - (x < y);
}
static int summarize(const char *path, int first[CORE_RANK_COUNT], int *outcome) {
    FILE *file = platform_fopen_utf8(path, "rb");
    char line[2048], *token;
    int rank_col = -1, outcome_col = -1, day_col = -1, column = 0;
    if (!file)
        return 0;
    memset(first, 0, CORE_RANK_COUNT * sizeof(int));
    *outcome = 0;
    if (!fgets(line, sizeof(line), file)) {
        fclose(file);
        return 0;
    }
    for (token = strtok(line, ",\r\n"); token; token = strtok(NULL, ",\r\n"), column++) {
        if (strcmp(token, "rank") == 0)
            rank_col = column;
        if (strcmp(token, "outcome") == 0)
            outcome_col = column;
        if (strcmp(token, "day") == 0)
            day_col = column;
    }
    if (rank_col < 0 || outcome_col < 0 || day_col < 0) {
        fclose(file);
        return 0;
    }
    while (fgets(line, sizeof(line), file)) {
        int rank = 0, day = 0;
        column = 0;
        for (token = strtok(line, ",\r\n"); token; token = strtok(NULL, ",\r\n"), column++) {
            if (column == rank_col && !number(token, 0, CORE_RANK_COUNT - 1, &rank)) {
                fclose(file);
                return 0;
            }
            if (column == day_col && !number(token, 1, 100000, &day)) {
                fclose(file);
                return 0;
            }
            if (column == outcome_col && !number(token, 0, 2, outcome)) {
                fclose(file);
                return 0;
            }
        }
        if (!day) {
            fclose(file);
            return 0;
        }
        while (rank > 0) {
            if (!first[rank])
                first[rank] = day;
            rank--;
        }
    }
    fclose(file);
    return 1;
}
static int run(int argc, char **argv) {
    static const char *const files[CORE_DATA_COUNT] = {"buildings.def", "traits.def",   "names.txt",
                                                       "enemies.def",   "research.def", "ranks.def"};
    int seeds = 16, days = 100, difficulty = -1, i, d, rc = 0;
    const char *data = NULL, *out = "build/reports/balance/overview";
    char exe[GAME_PATH_BYTES], default_data[GAME_PATH_BYTES];
    CoreDefs *defs = NULL;
    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--help")) {
            puts("castle_balance [--seeds 16] [--days 100] [--difficulty 0|1|2] [--data dir] [--out dir]");
            return 0;
        }
        if (i + 1 >= argc)
            return 2;
        if (!strcmp(argv[i], "--seeds")) {
            if (!number(argv[++i], 1, 1000, &seeds))
                return 2;
        } else if (!strcmp(argv[i], "--days")) {
            if (!number(argv[++i], 1, 100000, &days))
                return 2;
        } else if (!strcmp(argv[i], "--difficulty")) {
            if (!number(argv[++i], 0, 2, &difficulty))
                return 2;
        } else if (!strcmp(argv[i], "--data"))
            data = argv[++i];
        else if (!strcmp(argv[i], "--out"))
            out = argv[++i];
        else
            return 2;
    }
    if (!data) {
        if (!game_executable_directory(exe, sizeof(exe), argv[0]) ||
            !game_join_path(default_data, sizeof(default_data), exe, "data"))
            return 1;
        data = default_data;
    }
    if (!game_make_dirs(out) || core_defs_create(&defs) != CORE_OK)
        return 1;
    for (i = 0; i < CORE_DATA_COUNT; i++) {
        char path[GAME_PATH_BYTES];
        int32_t line = 0;
        if (!game_join_path(path, sizeof(path), data, files[i]) ||
            core_defs_load_file(defs, i, path, &line) != CORE_OK) {
            fprintf(stderr, u8"정의 파일을 읽지 못했습니다: %s\n", files[i]);
            core_defs_destroy(defs);
            return 1;
        }
    }
    printf(u8"%d시드 × %d일, 봇 reasonable\n", seeds, days);
    for (d = 0; d < CORE_DIFF_COUNT; d++) {
        int *values, counts[CORE_RANK_COUNT] = {0}, wins = 0, losses = 0, rank;
        if (difficulty >= 0 && difficulty != d)
            continue;
        values = (int *)calloc((size_t)seeds * CORE_RANK_COUNT, sizeof(int));
        if (!values) {
            rc = 1;
            break;
        }
        for (i = 0; i < seeds; i++) {
            char path[GAME_PATH_BYTES], name[80];
            int first[CORE_RANK_COUNT], outcome;
            (void)snprintf(name, sizeof(name), "d%d_s%d.csv", d, 1 + i * 2);
            if (!game_join_path(path, sizeof(path), out, name) ||
                sim_run_simulation(days, 1 + i * 2, d, "reasonable", path, defs, 0) != 0 ||
                !summarize(path, first, &outcome)) {
                rc = 1;
                break;
            }
            wins += outcome == CORE_OUTCOME_WON;
            losses += outcome == CORE_OUTCOME_LOST;
            for (rank = 1; rank < CORE_RANK_COUNT; rank++)
                if (first[rank])
                    values[rank * seeds + counts[rank]++] = first[rank];
        }
        printf(u8"%s: 승리 %d/%d, 패배 %d", fmt_difficulty_name(d), wins, seeds, losses);
        for (rank = 1; rank < CORE_RANK_COUNT; rank++) {
            int count = counts[rank];
            int *list = &values[rank * seeds];
            qsort(list, (size_t)count, sizeof(int), compare);
            if (count)
                printf(u8" | %s %.1f일 (%d판)", fmt_rank_name(rank),
                       (list[(count - 1) / 2] + list[count / 2]) / 2.0, count);
            else
                printf(" | %s -", fmt_rank_name(rank));
        }
        puts("");
        free(values);
        if (rc)
            break;
    }
    core_defs_destroy(defs);
    return rc;
}
int main(int argc, char **argv) {
    GameArguments args;
    int result;
    if (!game_arguments(&args, argc, argv))
        return 1;
    result = run(args.count, args.values);
    game_arguments_free(&args);
    return result;
}
