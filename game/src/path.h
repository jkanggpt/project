/* path.h — 화면 라이브러리에 의존하지 않는 가중치 격자 A* */
#ifndef CASTLE_GAME_PATH_H
#define CASTLE_GAME_PATH_H
#include <stdint.h>
typedef struct GamePath { int *cells; int count; int capacity; } GamePath;
typedef struct GamePathfinder {
    int *distance, *parent, *heap, *heap_index;
    unsigned char *closed;
    int capacity, heap_count, goal, width, normal_cost;
} GamePathfinder;
void game_path_free(GamePath *path);
void game_pathfinder_free(GamePathfinder *finder);
/* 1 성공, 0 경로 없음, -1 메모리 부족. 성벽은 적에게만 비싼 통행 칸입니다. */
/* 적의 목표 (-1,-1)은 도달 가능한 가장 가까운 지도 외곽을 찾습니다. */
int game_path_find(GamePathfinder *finder, const uint8_t *cost, int width, int height,
                   int sx, int sy, int tx, int ty, int enemy, GamePath *out);
int game_path_cost(uint8_t cost, int enemy);
#endif
