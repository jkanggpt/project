/*
 * test_util.h — 아주 작은 테스트 도구
 * CHECK 가 실패하면 파일과 줄을 출력하고 그 테스트 함수에서 바로 돌아갑니다.
 */
#ifndef CASTLE_TEST_UTIL_H
#define CASTLE_TEST_UTIL_H

#include <stddef.h>
#include <stdint.h>

#include "castle_core.h"

int test_legacy_world(const CoreNewGameParams *params, CoreWorld **out);
void test_fail(const char *file, int line, const char *expression);
void test_fail_values(const char *file, int line, const char *expression, long long left, long long right);
int test_failed(void);
int test_v124_parameter(const char *name);
/* 지금 실행 중인 테스트 이름 (임시 파일 이름을 겹치지 않게 만드는 데 씀) */
const char *test_current_name(void);

#define CHECK(condition)                                                                                       \
    do {                                                                                                       \
        if (!(condition)) {                                                                                    \
            test_fail(__FILE__, __LINE__, #condition);                                                         \
            return;                                                                                            \
        }                                                                                                      \
    } while (0)

#define CHECK_EQ(left, right)                                                                                  \
    do {                                                                                                       \
        long long left_value_ = (long long)(left);                                                             \
        long long right_value_ = (long long)(right);                                                           \
        if (left_value_ != right_value_) {                                                                     \
            test_fail_values(__FILE__, __LINE__, #left " == " #right, left_value_, right_value_);              \
            return;                                                                                            \
        }                                                                                                      \
    } while (0)

/* 공통 도우미 */
CoreWorld *test_make_world(int32_t seed, int32_t difficulty);
/* 불변 조건 위반을 출력하며 개수를 돌려줍니다 */
int32_t test_validate_verbose(const CoreWorld *world);
/* 파일 전체를 읽어 새로 할당한 버퍼로 (free 필요). 실패하면 NULL */
char *test_read_file(const char *path, size_t *out_size);
int test_write_file(const char *path, const char *data, size_t size);

/* 테스트 함수 선언 */
#define TEST_ENTRY(name) void test_##name(void);
#include "test_list.h"
#undef TEST_ENTRY

#endif
