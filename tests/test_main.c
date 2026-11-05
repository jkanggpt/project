/*
 * test_main.c — 테스트 실행기
 * 사용법: castle_tests            (모두 실행)
 *         castle_tests <이름>     (하나만 실행, CTest 가 이렇게 부름)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_util.h"
#include "validate.h"

typedef void (*TestFn)(void);

typedef struct TestCase {
    const char *name;
    TestFn run;
} TestCase;

static const TestCase TESTS[] = {
#define TEST_ENTRY(name) {#name, test_##name},
#include "test_list.h"
#undef TEST_ENTRY
};
#define TEST_COUNT ((int)(sizeof(TESTS) / sizeof(TESTS[0])))

static int g_failures = 0;
static const char *g_current_name = "test";

const char *test_current_name(void)
{
    return g_current_name;
}

void test_fail(const char *file, int line, const char *expression)
{
    g_failures++;
    fprintf(stderr, "  FAIL %s:%d: %s\n", file, line, expression);
}

void test_fail_values(const char *file, int line, const char *expression, long long left, long long right)
{
    g_failures++;
    fprintf(stderr, "  FAIL %s:%d: %s (%lld != %lld)\n", file, line, expression, left, right);
}

int test_failed(void)
{
    return g_failures > 0;
}

CoreWorld *test_make_world(int32_t seed, int32_t difficulty)
{
    CoreNewGameParams params;
    CoreWorld *world = NULL;
    memset(&params, 0, sizeof(params));
    strcpy(params.castle_name, "시험성");
    params.seed = seed;
    params.difficulty = difficulty;
    if (core_create(&params, NULL, &world) != CORE_OK) {
        return NULL;
    }
    return world;
}

static void print_violation(void *context, const char *what)
{
    (void)context;
    fprintf(stderr, "    violation: %s\n", what);
}

int32_t test_validate_verbose(const CoreWorld *world)
{
    return validate_world(world, print_violation, NULL);
}

char *test_read_file(const char *path, size_t *out_size)
{
    FILE *file = fopen(path, "rb");
    char *data;
    long size;
    if (file == NULL) {
        return NULL;
    }
    if (fseek(file, 0, SEEK_END) != 0 || (size = ftell(file)) < 0 || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return NULL;
    }
    data = (char *)malloc((size_t)size + 1);
    if (data == NULL || fread(data, 1, (size_t)size, file) != (size_t)size) {
        free(data);
        fclose(file);
        return NULL;
    }
    data[size] = '\0';
    fclose(file);
    *out_size = (size_t)size;
    return data;
}

int test_write_file(const char *path, const char *data, size_t size)
{
    FILE *file = fopen(path, "wb");
    int ok;
    if (file == NULL) {
        return 0;
    }
    ok = fwrite(data, 1, size, file) == size;
    return fclose(file) == 0 && ok;
}

static int run_one(const TestCase *test)
{
    int before = g_failures;
    g_current_name = test->name;
    printf("[ RUN  ] %s\n", test->name);
    fflush(stdout);
    test->run();
    printf("[ %s ] %s\n", g_failures == before ? " OK " : "FAIL", test->name);
    return g_failures == before;
}

int main(int argc, char **argv)
{
    int i;
    int passed = 0;

    if (argc > 1) {
        for (i = 0; i < TEST_COUNT; i++) {
            if (strcmp(TESTS[i].name, argv[1]) == 0) {
                return run_one(&TESTS[i]) ? 0 : 1;
            }
        }
        fprintf(stderr, "unknown test: %s\n", argv[1]);
        return 2;
    }
    for (i = 0; i < TEST_COUNT; i++) {
        passed += run_one(&TESTS[i]);
    }
    printf("%d/%d tests passed\n", passed, TEST_COUNT);
    return passed == TEST_COUNT ? 0 : 1;
}
