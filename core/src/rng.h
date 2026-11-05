/*
 * rng.h — 결정적 난수 생성기 (PCG32)
 *
 * 같은 시드와 같은 명령 순서면 언제나 같은 게임이 나와야 합니다. (재현 가능한 버그, 시뮬레이터 밸런싱)
 * 그래서 C 표준 rand() 대신 상태를 월드 안에 들고 다니고 저장 파일에도 쓰는 PCG32를 씁니다.
 */
#ifndef CASTLE_RNG_H
#define CASTLE_RNG_H

#include <stdint.h>

typedef struct Rng {
    uint64_t state; /* 내부 상태 */
    uint64_t inc;   /* 스트림 선택값. 항상 홀수 */
} Rng;

void rng_seed(Rng *rng, uint64_t seed, uint64_t stream);
uint32_t rng_next(Rng *rng);
/* lo 이상 hi 이하의 정수 (치우침 없음). lo > hi 면 lo */
int32_t rng_range(Rng *rng, int32_t lo, int32_t hi);
/* permille/1000 확률로 1 */
int rng_chance_permille(Rng *rng, int32_t permille);

#endif
