/*
 * test_save.c — 저장/불러오기: 왕복 해시, .bak, 한글 경로, 망가진 파일 (퍼징), 메모리 부족,
 *               슬롯 목록용 요약(core_peek_save), 성채 이름 검사
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mem.h"
#include "platform.h"
#include "rng.h"
#include "save.h"
#include "test_town.h"
#include "test_util.h"
#include "world.h"

static uint64_t hash_of(const CoreWorld *world)
{
    uint64_t hash = 0;
    (void)core_state_hash(world, &hash);
    return hash;
}

/* 시간을 조금 진행하고 자원을 바꾼 월드 (기본값만 저장되는 실수를 잡기 위해) */
static CoreWorld *make_played_world(int32_t seed)
{
    CoreWorld *world = test_make_world(seed, CORE_DIFF_HARSH);
    if (world == NULL) {
        return NULL;
    }
    /* 걷기 도착을 알려 주며 진행 (알리지 않으면 아무도 밥을 먹지 못해 모두 떠나고 패배함) */
    test_keep_alive(world);
    (void)test_run(world, 1000);
    test_keep_alive(world);
    world->bank.amount[CORE_RES_GOLD] = 12345;
    world->bank.amount[CORE_RES_HERBS] = 7;
    return world;
}

static void remove_save_files(const char *path)
{
    char buffer[512];
    (void)platform_remove_file(path);
    snprintf(buffer, sizeof(buffer), "%s.bak", path);
    (void)platform_remove_file(buffer);
    snprintf(buffer, sizeof(buffer), "%s.tmp", path);
    (void)platform_remove_file(buffer);
}

void test_save_roundtrip_same_hash(void)
{
    const char *path = "roundtrip.sav";
    CoreWorld *world = make_played_world(2024);
    CoreWorld *loaded = NULL;
    CoreSummaryView a;
    CoreSummaryView b;
    int32_t line = -1;

    CHECK(world != NULL);
    remove_save_files(path);
    CHECK_EQ(core_save(world, path), CORE_OK);
    CHECK(!platform_file_exists("roundtrip.sav.tmp"));
    CHECK_EQ(core_load(path, NULL, &loaded, &line), CORE_OK);
    CHECK(loaded != NULL);
    CHECK_EQ(line, 0);
    CHECK(hash_of(world) == hash_of(loaded));
    CHECK_EQ(core_validate(loaded), 0);
    CHECK_EQ(core_get_summary(world, &a), CORE_OK);
    CHECK_EQ(core_get_summary(loaded, &b), CORE_OK);
    /* cost_grid_rev 는 "바뀌었는지" 알리는 수정 번호라 저장하지 않습니다 */
    a.cost_grid_rev = 0;
    b.cost_grid_rev = 0;
    CHECK(memcmp(&a, &b, sizeof(a)) == 0);

    /* 불러온 뒤에도 같은 미래가 나와야 합니다 (난수 상태까지 저장되었는지) */
    CHECK(test_run(world, 500));
    CHECK(test_run(loaded, 500));
    CHECK(hash_of(world) == hash_of(loaded));
    CHECK(rng_next(&world->rng) == rng_next(&loaded->rng));

    core_destroy(world);
    core_destroy(loaded);
    remove_save_files(path);
}

void test_save_keeps_backup(void)
{
    const char *path = "backup.sav";
    CoreWorld *world = make_played_world(5);
    CoreWorld *loaded = NULL;
    uint64_t first_hash;

    CHECK(world != NULL);
    remove_save_files(path);
    CHECK_EQ(core_save(world, path), CORE_OK);
    CHECK(!platform_file_exists("backup.sav.bak")); /* 처음 저장할 때는 백업할 것이 없음 */
    first_hash = hash_of(world);

    CHECK_EQ(core_tick(world, 10), CORE_OK);
    CHECK_EQ(core_save(world, path), CORE_OK);
    CHECK(platform_file_exists("backup.sav.bak"));

    /* .bak 은 이전 상태, 본 파일은 새 상태 */
    CHECK_EQ(core_load("backup.sav.bak", NULL, &loaded, NULL), CORE_OK);
    CHECK(hash_of(loaded) == first_hash);
    core_destroy(loaded);
    CHECK_EQ(core_load(path, NULL, &loaded, NULL), CORE_OK);
    CHECK(hash_of(loaded) == hash_of(world));
    core_destroy(loaded);
    core_destroy(world);
    remove_save_files(path);
}

void test_save_korean_path(void)
{
    /* Windows 사용자 폴더에 한글이 있는 경우를 흉내 냅니다 (Windows CI 에서 _wfopen/MoveFileExW 경로를 확인) */
    const char *dir = u8"세이브_폴더";
    const char *path = u8"세이브_폴더/바람언덕 성채.sav";
    CoreWorld *world = make_played_world(31);
    CoreWorld *loaded = NULL;
    CoreSummaryView summary;

    CHECK(world != NULL);
    CHECK_EQ(world_set_name(world, u8"바람언덕 🏰 성채"), CORE_OK);
    CHECK_EQ(platform_make_dir(dir), 0);
    remove_save_files(path);
    CHECK_EQ(core_save(world, path), CORE_OK);
    CHECK_EQ(core_save(world, path), CORE_OK); /* 두 번째 저장: 대상이 있는 상태에서 바꿔치기 + .bak */
    CHECK(platform_file_exists(u8"세이브_폴더/바람언덕 성채.sav.bak"));
    CHECK_EQ(core_load(path, NULL, &loaded, NULL), CORE_OK);
    CHECK(hash_of(world) == hash_of(loaded));
    CHECK_EQ(core_get_summary(loaded, &summary), CORE_OK);
    CHECK(strcmp(summary.castle_name, u8"바람언덕 🏰 성채") == 0);
    core_destroy(world);
    core_destroy(loaded);
    remove_save_files(path);
}

void test_save_refuses_invalid_world(void)
{
    const char *path = "invalid.sav";
    CoreWorld *world = test_make_world(4, CORE_DIFF_NORMAL);
    CHECK(world != NULL);
    remove_save_files(path);
    world->bank.amount[CORE_RES_WOOD] = -10;
    CHECK_EQ(core_save(world, path), CORE_ERR_INVALID_ARG);
    CHECK(!platform_file_exists(path));
    CHECK_EQ(core_save(world, "no_such_dir/x.sav"), CORE_ERR_INVALID_ARG);
    world->bank.amount[CORE_RES_WOOD] = 0;
    CHECK_EQ(core_save(world, "no_such_dir/x.sav"), CORE_ERR_FILE_OPEN);
    core_destroy(world);
}

void test_load_missing_file(void)
{
    CoreWorld *loaded = (CoreWorld *)&loaded;
    int32_t line = 99;
    CHECK_EQ(core_load("definitely_missing.sav", NULL, &loaded, &line), CORE_ERR_FILE_OPEN);
    CHECK(loaded == NULL);
    CHECK_EQ(line, 0);
}

/* 정상 세이브 파일 내용을 만들어 돌려줍니다 (free 필요) */
static char *make_valid_save_text(size_t *out_size)
{
    char path[128];
    CoreWorld *world = make_played_world(77);
    char *data;
    snprintf(path, sizeof(path), "%s_source.sav", test_current_name());
    if (world == NULL) {
        return NULL;
    }
    remove_save_files(path);
    if (core_save(world, path) != CORE_OK) {
        core_destroy(world);
        return NULL;
    }
    core_destroy(world);
    data = test_read_file(path, out_size);
    remove_save_files(path);
    return data;
}

/* text 를 파일로 쓰고 불러 봅니다. 결과 코드를 돌려줍니다 */
static int32_t load_text(const char *text, size_t size, int32_t *out_line)
{
    char path[128];
    CoreWorld *loaded = NULL;
    int32_t rc;
    snprintf(path, sizeof(path), "%s_case.sav", test_current_name());
    if (!test_write_file(path, text, size)) {
        return 12345;
    }
    rc = core_load(path, NULL, &loaded, out_line);
    if (rc == CORE_OK) {
        /* 망가뜨렸는데도 읽혔다면, 적어도 불변 조건은 지켜야 합니다 */
        if (core_validate(loaded) != 0) {
            rc = 54321;
        }
        core_destroy(loaded);
    } else if (loaded != NULL) {
        rc = 99999; /* 실패했는데 월드를 돌려주면 안 됨 */
    }
    (void)platform_remove_file(path);
    return rc;
}

/* text 안의 needle 을 replacement 로 한 번 바꾼 새 문자열 (free 필요) */
static char *replace_once(const char *text, const char *needle, const char *replacement)
{
    const char *at = strstr(text, needle);
    size_t prefix;
    size_t total;
    char *result;
    if (at == NULL) {
        return NULL;
    }
    prefix = (size_t)(at - text);
    total = strlen(text) - strlen(needle) + strlen(replacement);
    result = (char *)malloc(total + 1);
    if (result == NULL) {
        return NULL;
    }
    memcpy(result, text, prefix);
    strcpy(result + prefix, replacement);
    strcat(result, at + strlen(needle));
    return result;
}

void test_load_wrong_version(void)
{
    size_t size = 0;
    char *text = make_valid_save_text(&size);
    char *changed;
    int32_t line = 0;

    CHECK(text != NULL);
    CHECK_EQ(load_text(text, size, &line), CORE_OK);

    changed = replace_once(text, "MAGICCASTLE_SAVE 7", "MAGICCASTLE_SAVE 999");
    CHECK(changed != NULL);
    CHECK_EQ(load_text(changed, strlen(changed), &line), CORE_ERR_VERSION);
    CHECK_EQ(line, 1);
    free(changed);

    changed = replace_once(text, "MAGICCASTLE_SAVE 7", "SOMETHING_ELSE 2");
    CHECK(changed != NULL);
    CHECK_EQ(load_text(changed, strlen(changed), &line), CORE_ERR_FORMAT);
    free(changed);

    /* Windows 메모장이 CRLF 로 바꿔 저장해도 읽혀야 합니다 */
    {
        size_t i;
        size_t j = 0;
        char *crlf = (char *)malloc(size * 2 + 1);
        CHECK(crlf != NULL);
        for (i = 0; i < size; i++) {
            if (text[i] == '\n') {
                crlf[j++] = '\r';
            }
            crlf[j++] = text[i];
        }
        CHECK_EQ(load_text(crlf, j, &line), CORE_OK);
        free(crlf);
    }
    CHECK_EQ(load_text("", 0, &line), CORE_ERR_FORMAT);
    free(text);
}

void test_load_rejects_bad_values(void)
{
    /* 바꿀 문자열 → 기대하는 결과 */
    static const char *const CASES[][2] = {
        {"difficulty=2", "difficulty=3"},
        {"difficulty=2", "difficulty=-1"},
        {"difficulty=2", "difficulty= 2"},
        {"difficulty=2", "difficulty=2x"},
        {"difficulty=2", "difficulty=99999999999999999999"},
        {"seed=77", "seed=-77"},
        {"keep=48,48", "keep=0,0"},
        {"keep=48,48", "keep=48"},
        {"tick=", "tick=-"},
        {"capacity=300", "capacity=0"},
        {"gold=12345", "gold=-1"},
        {"herbs=7", "herbs=999"}, /* 저장 한도를 넘음 → 불변 조건 검사에서 걸림 */
        {"size=96,96", "size=96,95"},
        {"size=96,96", "size=10,10"},
        {"[clock]", "[clock2]"},
        {"[rng]", "[meta]"},
        {"inc=", "inc=zz"},
        {"[end]", "[end]\nextra=1"},
        {"[end]", ""},
        {"name=", "name=\x01"},
        {"name=", "unknown=1\nname="},
        {"difficulty=2", "difficulty=2\ndifficulty=2"},
        {"row=", "row=?"},
    };
    size_t size = 0;
    char *text = make_valid_save_text(&size);
    size_t i;

    CHECK(text != NULL);
    for (i = 0; i < sizeof(CASES) / sizeof(CASES[0]); i++) {
        int32_t line = 0;
        int32_t rc;
        char *changed = replace_once(text, CASES[i][0], CASES[i][1]);
        if (changed == NULL) {
            fprintf(stderr, "  case %u: pattern not found: %s\n", (unsigned)i, CASES[i][0]);
            CHECK(changed != NULL);
        }
        rc = load_text(changed, strlen(changed), &line);
        if (rc != CORE_ERR_FORMAT) {
            fprintf(stderr, "  case %u (%s -> %s): rc=%d\n", (unsigned)i, CASES[i][0], CASES[i][1], (int)rc);
        }
        free(changed);
        CHECK_EQ(rc, CORE_ERR_FORMAT);
    }
    free(text);
}

void test_load_truncated_files(void)
{
    size_t size = 0;
    char *text = make_valid_save_text(&size);
    size_t length;
    size_t step;
    long baseline = mem_test_live_blocks();

    CHECK(text != NULL);
    step = size / 300 + 1;
    for (length = 0; length < size; length += step) {
        int32_t line = 0;
        int32_t rc = load_text(text, length, &line);
        /* 마지막 줄바꿈만 빠진 경우("[end]" 로 끝남)만 읽혀도 됩니다 */
        if (length >= size - 1) {
            CHECK(rc == CORE_OK);
        } else {
            CHECK(rc == CORE_ERR_FORMAT);
        }
        CHECK_EQ(mem_test_live_blocks(), baseline);
    }
    CHECK_EQ(load_text(text, size - 1, NULL), CORE_OK);
    free(text);
}

void test_load_garbled_files(void)
{
    size_t size = 0;
    char *text = make_valid_save_text(&size);
    char *copy;
    Rng rng;
    int round;
    int rejected = 0;
    long baseline = mem_test_live_blocks();

    CHECK(text != NULL);
    copy = (char *)malloc(size);
    CHECK(copy != NULL);
    rng_seed(&rng, 2026u, 11u);
    for (round = 0; round < 400; round++) {
        int flips = rng_range(&rng, 1, 8);
        int f;
        int32_t rc;
        memcpy(copy, text, size);
        for (f = 0; f < flips; f++) {
            size_t at = (size_t)rng_range(&rng, 0, (int32_t)size - 1);
            /* 아무 바이트나 넣습니다: '\0', 줄바꿈, 잘못된 UTF-8 까지 */
            copy[at] = (char)rng_range(&rng, 0, 255);
        }
        rc = load_text(copy, size, NULL);
        CHECK(rc == CORE_OK || rc == CORE_ERR_FORMAT || rc == CORE_ERR_VERSION);
        if (rc != CORE_OK) {
            rejected++;
        }
        CHECK_EQ(mem_test_live_blocks(), baseline);
    }
    CHECK(rejected > 300); /* 거의 모든 경우가 거부되어야 함 */

    /* 아주 긴 줄 (줄 길이 제한) */
    {
        size_t big = 20000;
        char *line = (char *)malloc(big);
        CHECK(line != NULL);
        memset(line, 'A', big);
        memcpy(line, "MAGICCASTLE_SAVE 7\nrow=", 23);
        CHECK_EQ(load_text(line, big, NULL), CORE_ERR_FORMAT);
        free(line);
    }
    free(copy);
    free(text);
}

void test_load_out_of_memory_is_clean(void)
{
    const char *path = "oom.sav";
    CoreWorld *world = make_played_world(9);
    long baseline;
    long fail_after;
    int succeeded = 0;

    CHECK(world != NULL);
    remove_save_files(path);
    CHECK_EQ(core_save(world, path), CORE_OK);
    baseline = mem_test_live_blocks();
    for (fail_after = 0; fail_after < 50 && !succeeded; fail_after++) {
        CoreWorld *loaded = NULL;
        int32_t rc;
        mem_test_fail_after(fail_after);
        rc = core_load(path, NULL, &loaded, NULL);
        mem_test_fail_after(-1);
        if (rc == CORE_OK) {
            CHECK(hash_of(loaded) == hash_of(world));
            core_destroy(loaded);
            succeeded = 1;
        } else {
            CHECK_EQ(rc, CORE_ERR_OUT_OF_MEMORY);
            CHECK(loaded == NULL);
        }
        CHECK_EQ(mem_test_live_blocks(), baseline);
    }
    CHECK(succeeded);

    /* 저장 중 메모리 부족: 기존 파일은 그대로 */
    mem_test_fail_after(0);
    CHECK_EQ(core_save(world, path), CORE_ERR_OUT_OF_MEMORY);
    mem_test_fail_after(-1);
    {
        CoreWorld *loaded = NULL;
        CHECK_EQ(core_load(path, NULL, &loaded, NULL), CORE_OK);
        core_destroy(loaded);
    }
    core_destroy(world);
    remove_save_files(path);
}

void test_save_peek_summarizes_slot(void)
{
    const char *path = "peek_slot.sav";
    CoreWorld *world = make_played_world(41);
    CoreSummaryView summary;
    CoreSaveInfoView info;

    CHECK(world != NULL);
    CHECK_EQ(world_set_name(world, u8"바람언덕 성채"), CORE_OK);
    remove_save_files(path);
    CHECK_EQ(core_save(world, path), CORE_OK);
    CHECK_EQ(core_get_summary(world, &summary), CORE_OK);

    CHECK_EQ(core_peek_save(path, NULL, &info), CORE_OK);
    CHECK_EQ(info.result, CORE_OK);
    CHECK_EQ(info.error_line, 0);
    CHECK_EQ(info.version, CORE_SAVE_VERSION);
    CHECK_EQ(info.seed, summary.seed);
    CHECK_EQ(info.difficulty, CORE_DIFF_HARSH);
    CHECK_EQ(info.tick, summary.tick);
    CHECK_EQ(info.day, summary.day);
    CHECK_EQ(info.year, summary.year);
    CHECK_EQ(info.season, summary.season);
    CHECK_EQ(info.minute_of_day, summary.minute_of_day);
    CHECK_EQ(info.rank, summary.rank);
    CHECK_EQ(info.population, summary.population);
    CHECK_EQ(info.outcome, CORE_OUTCOME_NONE);
    CHECK(strcmp(info.castle_name, u8"바람언덕 성채") == 0);

    /* 요약을 읽어도 파일과 지금 월드는 그대로 */
    CHECK_EQ(core_peek_save(path, NULL, &info), CORE_OK);
    CHECK_EQ(core_validate(world), 0);

    /* 없는 파일, 잘못된 인자 */
    CHECK_EQ(core_peek_save("no_such_slot.sav", NULL, &info), CORE_ERR_FILE_OPEN);
    CHECK_EQ(info.result, CORE_ERR_FILE_OPEN);
    CHECK_EQ(info.version, 0);
    CHECK_EQ(core_peek_save(path, NULL, NULL), CORE_ERR_NULL_ARG);
    CHECK_EQ(core_peek_save(NULL, NULL, &info), CORE_ERR_NULL_ARG);
    CHECK_EQ(info.result, CORE_ERR_NULL_ARG);
    CHECK_EQ(core_peek_save("", NULL, &info), CORE_ERR_INVALID_ARG);

    core_destroy(world);
    remove_save_files(path);
}

void test_save_peek_broken_offers_backup(void)
{
    const char *path = "peek_broken.sav";
    CoreWorld *world = make_played_world(43);
    CoreSaveInfoView info;
    CoreWorld *loaded = NULL;
    size_t size = 0;
    char *text;
    char *changed;
    int32_t line = 0;

    CHECK(world != NULL);
    remove_save_files(path);
    CHECK_EQ(core_save(world, path), CORE_OK);
    CHECK_EQ(core_tick(world, 6), CORE_OK);
    CHECK_EQ(core_save(world, path), CORE_OK); /* 두 번째 저장: 첫 저장이 .bak 으로 남음 */

    /* 저장 도중 꺼진 것처럼 파일 뒤쪽 절반을 잃음 → 본 파일은 망가지고 .bak 은 멀쩡함 */
    text = test_read_file(path, &size);
    CHECK(text != NULL);
    CHECK(test_write_file(path, text, size / 2));
    CHECK_EQ(core_peek_save(path, NULL, &info), CORE_ERR_FORMAT);
    CHECK_EQ(info.result, CORE_ERR_FORMAT);
    CHECK_EQ(info.version, CORE_SAVE_VERSION); /* 첫 줄은 남아 있음 */
    CHECK(info.error_line > 1);
    CHECK_EQ(core_load(path, NULL, &loaded, &line), CORE_ERR_FORMAT);
    CHECK(loaded == NULL);
    CHECK_EQ(line, info.error_line);

    CHECK_EQ(core_peek_save("peek_broken.sav.bak", NULL, &info), CORE_OK);
    CHECK(info.tick > 0);
    CHECK_EQ(core_load("peek_broken.sav.bak", NULL, &loaded, &line), CORE_OK);
    core_destroy(loaded);

    /* 다른 버전의 세이브: 버전 번호를 알려 줌 */
    changed = replace_once(text, "MAGICCASTLE_SAVE 7", "MAGICCASTLE_SAVE 4");
    CHECK(changed != NULL);
    CHECK(test_write_file(path, changed, strlen(changed)));
    CHECK_EQ(core_peek_save(path, NULL, &info), CORE_ERR_VERSION);
    CHECK_EQ(info.version, 4);
    CHECK_EQ(info.error_line, 1);
    free(changed);

    /* 세이브가 아닌 파일 */
    CHECK(test_write_file(path, "hello\n", 6));
    CHECK_EQ(core_peek_save(path, NULL, &info), CORE_ERR_FORMAT);
    CHECK_EQ(info.version, 0);

    free(text);
    core_destroy(world);
    remove_save_files(path);
}

void test_castle_name_check(void)
{
    char longest[CORE_NAME_BYTES];
    char too_long[CORE_NAME_BYTES + 1];
    memset(longest, 'a', sizeof(longest) - 1);
    longest[sizeof(longest) - 1] = '\0';
    memset(too_long, 'a', sizeof(too_long) - 1);
    too_long[sizeof(too_long) - 1] = '\0';

    CHECK_EQ(core_check_castle_name(u8"바람언덕 성채"), CORE_OK);
    CHECK_EQ(core_check_castle_name("Castle 7"), CORE_OK);
    CHECK_EQ(core_check_castle_name(longest), CORE_OK);
    CHECK_EQ(core_check_castle_name(too_long), CORE_ERR_INVALID_ARG);
    CHECK_EQ(core_check_castle_name(""), CORE_ERR_INVALID_ARG);
    CHECK_EQ(core_check_castle_name(u8"두 줄\n이름"), CORE_ERR_INVALID_ARG); /* 세이브는 한 줄에 값 하나 */
    CHECK_EQ(core_check_castle_name("\xff\xfe"), CORE_ERR_INVALID_ARG);   /* 잘못된 UTF-8 */
    CHECK_EQ(core_check_castle_name("\xea\xb0"), CORE_ERR_INVALID_ARG);   /* 잘린 한글 */
    CHECK_EQ(core_check_castle_name(NULL), CORE_ERR_NULL_ARG);
}
