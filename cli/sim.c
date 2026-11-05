/*
 * sim.c — 시뮬레이터와 소크 테스트 (설명은 sim.h)
 */
#include "sim.h"

#include <stdio.h>
#include <string.h>

#include "castle_core.h"
#include "format.h"

/* ---- 봇 (이름 → 하루 행동 함수 표) -------------------------------------- */

/* 하루가 시작될 때 불립니다. 봇은 공개 API 명령만 씁니다 */
typedef void (*BotDailyFn)(CoreWorld *world, const CoreSummaryView *summary);

typedef struct Bot {
    const char *name;
    const char *description;
    BotDailyFn on_new_day;
} Bot;

static void bot_idle(CoreWorld *world, const CoreSummaryView *summary)
{
    /* 아무것도 하지 않는 기준선 봇. 건설 명령이 생기는 마일스톤 2에서 "reasonable" 봇이 추가됩니다 */
    (void)world;
    (void)summary;
}

static const Bot BOTS[] = {
    {"idle", "아무것도 하지 않음 (기준선)", bot_idle},
};
#define BOT_COUNT ((int)(sizeof(BOTS) / sizeof(BOTS[0])))

static const Bot *find_bot(const char *name)
{
    int i;
    for (i = 0; i < BOT_COUNT; i++) {
        if (strcmp(BOTS[i].name, name) == 0) {
            return &BOTS[i];
        }
    }
    return NULL;
}

int sim_bot_exists(const char *bot_name)
{
    return find_bot(bot_name) != NULL;
}

void sim_print_bots(void)
{
    int i;
    for (i = 0; i < BOT_COUNT; i++) {
        printf("  %-10s %s\n", BOTS[i].name, BOTS[i].description);
    }
}

static int32_t create_world(int32_t seed, int difficulty, const char *name, CoreWorld **out)
{
    CoreNewGameParams params;
    memset(&params, 0, sizeof(params));
    strncpy(params.castle_name, name, sizeof(params.castle_name) - 1);
    params.seed = seed;
    params.difficulty = difficulty;
    return core_create(&params, out);
}

static void write_csv_row(FILE *out, const CoreSummaryView *summary)
{
    int i;
    fprintf(out, "%d,%d,%d", (int)summary->day, (int)summary->year, (int)summary->season);
    for (i = 0; i < CORE_RES_COUNT; i++) {
        fprintf(out, ",%d", (int)summary->resources[i]);
    }
    /* 인구, 평균 기분, 위협은 마일스톤 2~4 에서 채워집니다 */
    fprintf(out, ",%d,0,0,0\n", (int)summary->storage_capacity);
}

int sim_run_simulation(int days, int32_t seed, int difficulty, const char *bot_name, const char *csv_path)
{
    const Bot *bot = find_bot(bot_name);
    CoreWorld *world = NULL;
    CoreConfigView config;
    CoreSummaryView summary;
    FILE *out = stdout;
    int day;
    int32_t rc;
    int status = 0;

    if (bot == NULL) {
        fprintf(stderr, "모르는 봇입니다: %s\n", bot_name);
        return 1;
    }
    (void)core_get_config(&config);
    rc = create_world(seed, difficulty, "Simulation", &world);
    if (rc != CORE_OK) {
        fprintf(stderr, "월드를 만들 수 없습니다: %s\n", core_result_name(rc));
        return 1;
    }
    if (csv_path != NULL) {
        out = fopen(csv_path, "w");
        if (out == NULL) {
            fprintf(stderr, "CSV 파일을 열 수 없습니다: %s\n", csv_path);
            core_destroy(world);
            return 1;
        }
    }
    fprintf(out, "day,year,season,gold,mana,food,wood,stone,herbs,capacity,population,mood,threats\n");
    (void)core_get_summary(world, &summary);
    write_csv_row(out, &summary);
    for (day = 0; day < days; day++) {
        CoreEventView event;
        bot->on_new_day(world, &summary);
        rc = core_tick(world, config.ticks_per_day);
        if (rc != CORE_OK || core_validate(world) != 0) {
            fprintf(stderr, "시뮬레이션 %d일째에 문제가 생겼습니다 (%s)\n", day + 1, core_result_name(rc));
            status = 1;
            break;
        }
        while (core_poll_event(world, &event) == 1) {
            /* 시뮬레이터는 피드백 이벤트를 쓰지 않으므로 비우기만 합니다 */
        }
        (void)core_get_summary(world, &summary);
        write_csv_row(out, &summary);
    }
    if (out != stdout && fclose(out) != 0) {
        status = 1;
    }
    core_destroy(world);
    if (status == 0 && csv_path != NULL) {
        printf("시뮬레이션 완료: %d일, 시드 %d, 봇 %s → %s\n", days, (int)seed, bot_name, csv_path);
    }
    return status;
}

/* ---- 소크 테스트 ------------------------------------------------------ */

/* 소크 테스트용 작은 난수 (코어의 난수와 섞이지 않도록 따로 둡니다) */
static uint32_t soak_random(uint32_t *state)
{
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

/* 잘못된 호출을 섞어 보냅니다. 모두 오류 코드를 돌려줘야 하고 상태를 바꾸면 안 됩니다 */
static int poke_invalid_calls(CoreWorld *world)
{
    uint8_t tiny[4];
    uint64_t before = 0;
    uint64_t after = 1;
    int ok = 1;
    (void)core_state_hash(world, &before);
    ok &= core_tick(world, 0) == CORE_ERR_INVALID_ARG;
    ok &= core_tick(world, -3) == CORE_ERR_INVALID_ARG;
    ok &= core_copy_terrain(world, tiny, (int32_t)sizeof(tiny)) == CORE_ERR_BUFFER_TOO_SMALL;
    ok &= core_get_summary(world, NULL) == CORE_ERR_NULL_ARG;
    ok &= core_save(world, "") == CORE_ERR_INVALID_ARG;
    (void)core_state_hash(world, &after);
    return ok && before == after;
}

/* 저장했다가 다시 불러와서, 불러온 월드로 계속 진행합니다. 해시가 같아야 합니다 */
static int save_load_roundtrip(CoreWorld **world, const char *path)
{
    CoreWorld *loaded = NULL;
    uint64_t before = 0;
    uint64_t after = 1;
    if (core_save(*world, path) != CORE_OK || core_load(path, &loaded, NULL) != CORE_OK) {
        return 0;
    }
    (void)core_state_hash(*world, &before);
    (void)core_state_hash(loaded, &after);
    core_destroy(*world);
    *world = loaded;
    return before == after;
}

static int soak_one_seed(int days, int32_t seed, long *ticks_run)
{
    CoreWorld *world = NULL;
    CoreSummaryView summary;
    char path[64];
    char backup[72];
    uint32_t random_state = (uint32_t)seed * 2654435761u + 1u;
    int32_t end_day;
    int32_t last_day;
    int failures = 0;

    if (create_world(seed, seed % CORE_DIFF_COUNT, "Soak", &world) != CORE_OK) {
        fprintf(stderr, "  시드 %d: 월드를 만들 수 없습니다\n", (int)seed);
        return 1;
    }
    snprintf(path, sizeof(path), "soak_%d.sav", (int)seed);
    snprintf(backup, sizeof(backup), "%s.bak", path);
    (void)core_get_summary(world, &summary);
    end_day = summary.day + days;
    last_day = summary.day;

    while (summary.day < end_day && failures == 0) {
        CoreEventView event;
        uint32_t roll = soak_random(&random_state);
        int32_t violations;

        /* 매 틱 진행 후 불변 조건 검사 */
        if (core_tick(world, 1) != CORE_OK) {
            fprintf(stderr, "  시드 %d: core_tick 실패\n", (int)seed);
            failures++;
            break;
        }
        (*ticks_run)++;
        violations = core_validate(world);
        if (violations != 0) {
            fprintf(stderr, "  시드 %d: 불변 조건 위반 %d개\n", (int)seed, (int)violations);
            failures++;
            break;
        }
        /* 이벤트는 가끔만 꺼냅니다 (꺼내지 않아도 문제없어야 함) */
        if (roll % 7 == 0) {
            while (core_poll_event(world, &event) == 1) {
            }
        }
        (void)core_get_summary(world, &summary);
        if (summary.day != last_day) {
            last_day = summary.day;
            if (roll % 10 == 0 && !save_load_roundtrip(&world, path)) {
                fprintf(stderr, "  시드 %d: 저장/불러오기 왕복이 어긋났습니다 (%d일)\n", (int)seed, (int)summary.day);
                failures++;
            }
            if (roll % 13 == 0 && !poke_invalid_calls(world)) {
                fprintf(stderr, "  시드 %d: 잘못된 호출이 상태를 바꾸었습니다\n", (int)seed);
                failures++;
            }
        }
    }
    core_destroy(world);
    remove(path);
    remove(backup);
    return failures;
}

int sim_run_soak(int days, int seeds, int32_t first_seed)
{
    int i;
    int failed_seeds = 0;
    long ticks_run = 0;
    for (i = 0; i < seeds; i++) {
        int32_t seed = first_seed + i;
        if (soak_one_seed(days, seed, &ticks_run) != 0) {
            failed_seeds++;
        }
    }
    if (failed_seeds == 0) {
        printf("소크 테스트 통과: 시드 %d개 × %d일, 틱 %ld번, 불변 조건 위반 0\n", seeds, days, ticks_run);
        return 0;
    }
    printf("소크 테스트 실패: 시드 %d개 중 %d개에서 문제\n", seeds, failed_seeds);
    return 1;
}
