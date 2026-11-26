/*
 * rng.c — PCG32 난수 생성기
 * 알고리즘: Melissa O'Neill, "PCG: A Family of Simple Fast Space-Efficient
 * Statistically Good Algorithms for Random Number Generation" (pcg-random.org, 최소 C 구현과 같은 방식)
 */
#include "rng.h"

void rng_seed(Rng *rng, uint64_t seed, uint64_t stream) {
    rng->state = 0u;
    rng->inc = (stream << 1u) | 1u; /* inc 는 반드시 홀수여야 주기가 최대가 됩니다 */
    (void)rng_next(rng);
    rng->state += seed;
    (void)rng_next(rng);
}

uint32_t rng_next(Rng *rng) {
    uint64_t old = rng->state;
    uint32_t xorshifted;
    uint32_t rot;

    rng->state = old * 6364136223846793005ULL + rng->inc;
    xorshifted = (uint32_t)(((old >> 18u) ^ old) >> 27u);
    rot = (uint32_t)(old >> 59u);
    /* 32에서 빼는 대신 비트 마스크를 써서 rot == 0 일 때 32비트 시프트(정의되지 않은 동작)를 피합니다 */
    return (xorshifted >> rot) | (xorshifted << ((0u - rot) & 31u));
}

int32_t rng_range(Rng *rng, int32_t lo, int32_t hi) {
    uint32_t span;
    uint32_t limit;
    uint32_t r;

    if (lo >= hi) {
        return lo;
    }
    span = (uint32_t)((int64_t)hi - (int64_t)lo + 1);
    /* 나머지 연산의 치우침을 없애기 위해, 나누어떨어지지 않는 위쪽 구간은 다시 뽑습니다 */
    limit = (uint32_t)(0u - span) % span;
    do {
        r = rng_next(rng);
    } while (r < limit);
    return (int32_t)((int64_t)lo + (int64_t)(r % span));
}

int rng_chance_permille(Rng *rng, int32_t permille) {
    if (permille <= 0) {
        return 0;
    }
    if (permille >= 1000) {
        return 1;
    }
    return rng_range(rng, 0, 999) < permille;
}
