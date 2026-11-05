/*
 * mem.c — 메모리 할당 감싸개 (설명은 mem.h)
 */
#include "mem.h"

#include <stdlib.h>

#ifdef CASTLE_TESTING
/* 테스트 빌드에서만 존재하는 전역 상태입니다. 게임 DLL에는 컴파일되지 않습니다. */
static long g_fail_after = -1; /* 남은 성공 횟수, -1 이면 끔 */
static long g_live_blocks = 0;

void mem_test_fail_after(long n) { g_fail_after = n; }
long mem_test_live_blocks(void) { return g_live_blocks; }

/* 이번 할당을 실패시켜야 하면 1 */
static int should_fail(void)
{
    if (g_fail_after < 0) {
        return 0;
    }
    if (g_fail_after == 0) {
        return 1;
    }
    g_fail_after--;
    return 0;
}
#endif

void *mem_malloc(size_t size)
{
    void *p;
#ifdef CASTLE_TESTING
    if (should_fail()) {
        return NULL;
    }
#endif
    p = malloc(size == 0 ? 1 : size);
#ifdef CASTLE_TESTING
    if (p != NULL) {
        g_live_blocks++;
    }
#endif
    return p;
}

void *mem_calloc(size_t count, size_t size)
{
    void *p;
#ifdef CASTLE_TESTING
    if (should_fail()) {
        return NULL;
    }
#endif
    p = calloc(count == 0 ? 1 : count, size == 0 ? 1 : size);
#ifdef CASTLE_TESTING
    if (p != NULL) {
        g_live_blocks++;
    }
#endif
    return p;
}

void *mem_realloc(void *ptr, size_t size)
{
    void *p;
    if (ptr == NULL) {
        return mem_malloc(size);
    }
#ifdef CASTLE_TESTING
    if (should_fail()) {
        return NULL;
    }
#endif
    /* realloc(p, 0) 의 동작은 구현마다 달라서 피합니다. */
    p = realloc(ptr, size == 0 ? 1 : size);
    return p;
}

void mem_free(void *ptr)
{
    if (ptr == NULL) {
        return;
    }
#ifdef CASTLE_TESTING
    g_live_blocks--;
#endif
    free(ptr);
}
