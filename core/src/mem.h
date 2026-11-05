/*
 * mem.h — 메모리 할당 감싸개
 *
 * 코어의 모든 동적 할당은 이 함수들을 거칩니다. 안에서는 그냥 malloc/calloc/realloc/free 를 부릅니다.
 * 감싸 두는 이유: 테스트 빌드(CASTLE_TESTING)에서 "N번째 할당부터 실패"를 일부러 일으켜
 * NULL 처리 코드가 정말로 동작하는지(새는 메모리 없이 오류 코드를 돌려주는지) 확인하기 위해서입니다.
 * 게임용 DLL에는 이 시험 기능이 들어가지 않습니다.
 */
#ifndef CASTLE_MEM_H
#define CASTLE_MEM_H

#include <stddef.h>

void *mem_malloc(size_t size);
void *mem_calloc(size_t count, size_t size);
/* realloc 과 같음: 실패하면 NULL 을 돌려주고 원래 블록은 그대로 둡니다. */
void *mem_realloc(void *ptr, size_t size);
void mem_free(void *ptr);

#ifdef CASTLE_TESTING
/* n번의 할당은 성공시키고 그다음부터 실패시킵니다. n < 0 이면 실패 주입을 끕니다. */
void mem_test_fail_after(long n);
/* 지금 살아 있는(해제되지 않은) 블록 수 */
long mem_test_live_blocks(void);
#endif

#endif
