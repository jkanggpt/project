/* path.c — 칸마다 힙 위치를 기억해 중복 삽입과 힙 넘침을 피합니다. */
#include "path.h"
#include <castle_core.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

void game_path_free(GamePath *path) { free(path->cells); memset(path, 0, sizeof(*path)); }
void game_pathfinder_free(GamePathfinder *f)
{
    free(f->distance); free(f->parent); free(f->heap); free(f->heap_index); free(f->closed);
    memset(f, 0, sizeof(*f));
}
int game_path_cost(uint8_t cost, int enemy)
{
    if (cost == CORE_COST_BLOCKED) return 0;
    if (cost == CORE_COST_WALL) return enemy == 3 ? 0 : enemy == 2 ? 100 : enemy ? 120 : 0;
    if (cost == CORE_COST_GATE) return enemy == 3 ? 0 : enemy == 2 ? 100 : enemy ? 120 : CORE_COST_NORMAL;
    return cost;
}
static int reserve(GamePathfinder *f, int cells)
{
    GamePathfinder next = {0};
    if (f->capacity >= cells) return 1;
    next.distance = (int *)malloc((size_t)cells * sizeof(int));
    next.parent = (int *)malloc((size_t)cells * sizeof(int));
    next.heap = (int *)malloc((size_t)cells * sizeof(int));
    next.heap_index = (int *)malloc((size_t)cells * sizeof(int));
    next.closed = (unsigned char *)malloc((size_t)cells);
    if (!next.distance || !next.parent || !next.heap || !next.heap_index || !next.closed) {
        game_pathfinder_free(&next); return 0;
    }
    next.capacity = cells;
    next.normal_cost = f->normal_cost;
    game_pathfinder_free(f); *f = next; return 1;
}
static int score(const GamePathfinder *f, int cell)
{
    if(f->goal<0)return f->distance[cell];
    /* 최소 통행 비용 이하의 휴리스틱으로 최단 경로를 보장합니다. */
    return f->distance[cell] + abs(cell % f->width - f->goal % f->width) +
                              abs(cell / f->width - f->goal / f->width);
}
static void swap_heap(GamePathfinder *f, int a, int b)
{
    int t = f->heap[a]; f->heap[a] = f->heap[b]; f->heap[b] = t;
    f->heap_index[f->heap[a]] = a; f->heap_index[f->heap[b]] = b;
}
static void up(GamePathfinder *f, int i)
{
    while (i > 0) {
        int p = (i - 1) / 2;
        if (score(f, f->heap[p]) <= score(f, f->heap[i])) break;
        swap_heap(f, p, i); i = p;
    }
}
static int pop(GamePathfinder *f)
{
    int top = f->heap[0], i = 0;
    f->heap_index[top] = -1;
    if (--f->heap_count == 0) return top;
    f->heap[0] = f->heap[f->heap_count]; f->heap_index[f->heap[0]] = 0;
    for (;;) {
        int l = i * 2 + 1, r = l + 1, n = i;
        if (l < f->heap_count && score(f, f->heap[l]) < score(f, f->heap[n])) n = l;
        if (r < f->heap_count && score(f, f->heap[r]) < score(f, f->heap[n])) n = r;
        if (n == i) break;
        swap_heap(f, n, i); i = n;
    }
    return top;
}
int game_path_find(GamePathfinder *f, const uint8_t *cost, int w, int h,
                   int sx, int sy, int tx, int ty, int enemy, GamePath *out)
{
    static const int dx[4] = {0, 1, 0, -1}, dy[4] = {-1, 0, 1, 0};
    int cells, start, goal, i, found = 0, count;
    out->count = 0;
    int edge=tx==-1 && ty==-1 && enemy;
    if (!cost || w < 1 || h < 1 || w > 512 || h > 512 || sx < 0 || sy < 0 ||
        sx >= w || sy >= h || (!edge && (tx<0 || ty<0 || tx>=w || ty>=h))) return 0;
    cells = w * h; start = sy * w + sx; goal = edge?-1:ty * w + tx;
    if (!edge && !game_path_cost(cost[goal], enemy)) return 0;
    if (!reserve(f, cells)) return -1;
    f->width = w; f->goal = goal; f->heap_count = 1; f->heap[0] = start;
    memset(f->closed, 0, (size_t)cells);
    for (i = 0; i < cells; i++) { f->distance[i] = INT_MAX / 4; f->parent[i] = -1; f->heap_index[i] = -1; }
    f->distance[start] = 0; f->heap_index[start] = 0;
    while (f->heap_count > 0) {
        int current = pop(f), d;
        if (current == goal || (edge && (current%w==0 || current%w==w-1 || current/w==0 || current/w==h-1))) { goal=current;found = 1; break; }
        f->closed[current] = 1;
        for (d = 0; d < 4; d++) {
            int x = current % w + dx[d], y = current / w + dy[d], next, step;
            if (x < 0 || y < 0 || x >= w || y >= h) continue;
            next = y * w + x; step = game_path_cost(cost[next], enemy);
            if (!enemy && cost[next] == CORE_COST_GATE && f->normal_cost == 100)
                step = 67;
            if (!step || f->closed[next] || f->distance[current] + step >= f->distance[next]) continue;
            f->distance[next] = f->distance[current] + step; f->parent[next] = current;
            if (f->heap_index[next] < 0) { f->heap_index[next] = f->heap_count; f->heap[f->heap_count++] = next; }
            up(f, f->heap_index[next]);
        }
    }
    if (!found) return 0;
    count = 1;
    for (i = goal; i != start; i = f->parent[i]) count++;
    if (count > out->capacity) {
        int *grown = (int *)realloc(out->cells, (size_t)count * sizeof(int));
        if (!grown) return -1;
        out->cells = grown; out->capacity = count;
    }
    out->count = count;
    for (i = goal; count > 0; i = f->parent[i]) out->cells[--count] = i;
    return 1;
}
