/* immigration — 현재 기능의 단일 구현 */
#include "expansion.h"
#include "placement.h"
#include "rank.h"
#include "villager_ai.h"
#include <stdlib.h>
#include <string.h>

int expansion_immigration_wait(World *w) {
    static const int three_day[6] = {0, 35, 40, 45, 50, 55};
    return rng_range(&w->rng, 0, 99) < three_day[exp_clamp(w->rank, 0, 5)] ? 3 : 2;
}

void expansion_immigration_limits(int rank, int vacant, int winter, int *lo, int *hi) {
    static const int minimum[6] = {2, 2, 3, 3, 4, 4}, maximum[6] = {2, 4, 5, 6, 6, 7};
    *hi = maximum[rank] < vacant ? maximum[rank] : vacant;
    *lo = minimum[rank] < *hi ? minimum[rank] : *hi;
    if (winter) {
        *lo = (*lo + 1) / 2;
        *hi = (*hi + 1) / 2;
    }
}

int expansion_immigration_count(World *w, int vacant) {
    int lo, hi;
    expansion_immigration_limits(w->rank, vacant, 0, &lo, &hi);
    if (hi <= 0)
        return 0;
    int count = rng_range(&w->rng, lo, hi);
    return exp_season(w) == 3 ? (count + 1) / 2 : count;
}
