/*
 * walker.c — CLI 용 걷기 흉내 (설명은 walker.h)
 */
#include "walker.h"

#include <stdlib.h>
#include <string.h>

#define WALKER_INF 0x3FFFFFFF

void walker_init(Walker *walker)
{
    memset(walker, 0, sizeof(*walker));
    walker->cost_rev = -1;
}

void walker_free(Walker *walker)
{
    free(walker->entries);
    free(walker->villagers);
    free(walker->cost);
    free(walker->distance);
    free(walker->heap);
    memset(walker, 0, sizeof(*walker));
}

/* 주민에게 이 칸이 지나갈 수 있는 칸인지, 비용은 얼마인지 (0 = 못 지나감) */
static int32_t villager_cost(uint8_t cost, int normal_cost) {
    if (cost == CORE_COST_BLOCKED || cost == CORE_COST_WALL) {
        return 0;
    }
    if (cost == CORE_COST_GATE) {
        return normal_cost == 100 ? 67 : CORE_COST_NORMAL;
    }
    return cost;
}

/* ---- 최소 힙 (거리가 가장 작은 칸을 먼저 꺼냄) ---------------------------- */

static void heap_push(Walker *walker, int32_t *size, int32_t cell)
{
    int32_t i = (*size)++;
    walker->heap[i] = cell;
    while (i > 0) {
        int32_t parent = (i - 1) / 2;
        if (walker->distance[walker->heap[parent]] <= walker->distance[walker->heap[i]]) {
            break;
        }
        int32_t swap = walker->heap[parent];
        walker->heap[parent] = walker->heap[i];
        walker->heap[i] = swap;
        i = parent;
    }
}

static int32_t heap_pop(Walker *walker, int32_t *size)
{
    int32_t top = walker->heap[0];
    int32_t i = 0;
    walker->heap[0] = walker->heap[--(*size)];
    for (;;) {
        int32_t left = i * 2 + 1;
        int32_t right = left + 1;
        int32_t smallest = i;
        if (left < *size && walker->distance[walker->heap[left]] < walker->distance[walker->heap[smallest]]) {
            smallest = left;
        }
        if (right < *size && walker->distance[walker->heap[right]] < walker->distance[walker->heap[smallest]]) {
            smallest = right;
        }
        if (smallest == i) {
            break;
        }
        int32_t swap = walker->heap[smallest];
        walker->heap[smallest] = walker->heap[i];
        walker->heap[i] = swap;
        i = smallest;
    }
    return top;
}

int32_t walker_path_cost(Walker *walker, int32_t sx, int32_t sy, int32_t tx, int32_t ty)
{
    static const int32_t DX[4] = {0, 1, 0, -1};
    static const int32_t DY[4] = {-1, 0, 1, 0};
    int32_t cells = walker->width * walker->height;
    int32_t heap_size = 0;
    int32_t start;
    int32_t goal;
    int32_t i;

    if (sx < 0 || sy < 0 || tx < 0 || ty < 0 || sx >= walker->width || tx >= walker->width ||
        sy >= walker->height || ty >= walker->height) {
        return -1;
    }
    start = sy * walker->width + sx;
    goal = ty * walker->width + tx;
    if (start == goal) {
        return 0;
    }
    if (villager_cost(walker->cost[goal], walker->normal_cost) == 0) {
        return -1;
    }
    for (i = 0; i < cells; i++) {
        walker->distance[i] = WALKER_INF;
    }
    /* 출발 칸이 막혀 있어도(건물에 막힌 채 남은 주민) 일단 출발은 허용합니다 */
    walker->distance[start] = 0;
    heap_push(walker, &heap_size, start);
    while (heap_size > 0) {
        int32_t current = heap_pop(walker, &heap_size);
        int32_t cx = current % walker->width;
        int32_t cy = current / walker->width;
        int32_t d;
        if (current == goal) {
            return walker->distance[goal];
        }
        for (d = 0; d < 4; d++) {
            int32_t nx = cx + DX[d];
            int32_t ny = cy + DY[d];
            int32_t next;
            int32_t step;
            if (nx < 0 || ny < 0 || nx >= walker->width || ny >= walker->height) {
                continue;
            }
            next = ny * walker->width + nx;
            step = villager_cost(walker->cost[next], walker->normal_cost);
            if (step == 0 || walker->distance[current] + step >= walker->distance[next]) {
                continue;
            }
            walker->distance[next] = walker->distance[current] + step;
            if (heap_size < cells * 4) {
                heap_push(walker, &heap_size, next);
            }
        }
    }
    return -1;
}

/* 비용 격자가 바뀌었으면 다시 복사합니다 */
static int refresh_cost(Walker *walker, const CoreWorld *world, int32_t rev)
{
    int32_t width;
    int32_t height;
    if (rev == walker->cost_rev && walker->cost != NULL) {
        return 0;
    }
    if (core_get_map_size(world, &width, &height) != CORE_OK) {
        return -1;
    }
    if (walker->cost == NULL || width != walker->width || height != walker->height) {
        size_t cells = (size_t)width * (size_t)height;
        free(walker->cost);
        free(walker->distance);
        free(walker->heap);
        walker->cost = (uint8_t *)malloc(cells);
        walker->distance = (int32_t *)malloc(cells * sizeof(int32_t));
        /* 한 칸이 여러 번 힙에 들어갈 수 있으므로 넉넉하게 */
        walker->heap = (int32_t *)malloc(cells * 4 * sizeof(int32_t));
        if (walker->cost == NULL || walker->distance == NULL || walker->heap == NULL) {
            return -1;
        }
        walker->width = width;
        walker->height = height;
    }
    if (core_copy_cost_grid(world, walker->cost, width * height, &walker->cost_rev) != CORE_OK) {
        return -1;
    }
    return 0;
}

static WalkEntry *find_entry(Walker *walker, int32_t villager_id)
{
    int32_t i;
    for (i = 0; i < walker->count; i++) {
        if (walker->entries[i].villager_id == villager_id) {
            return &walker->entries[i];
        }
    }
    return NULL;
}

static WalkEntry *add_entry(Walker *walker, int32_t villager_id)
{
    if (walker->count == walker->capacity) {
        int32_t new_capacity = walker->capacity > 0 ? walker->capacity * 2 : 32;
        WalkEntry *grown = (WalkEntry *)realloc(walker->entries, (size_t)new_capacity * sizeof(WalkEntry));
        if (grown == NULL) {
            return NULL;
        }
        walker->entries = grown;
        walker->capacity = new_capacity;
    }
    walker->entries[walker->count].villager_id = villager_id;
    walker->entries[walker->count].intent_rev = -1;
    walker->entries[walker->count].ticks_left = 0;
    return &walker->entries[walker->count++];
}

/* 이제 걷지 않는(도착했거나 떠난) 주민의 기록을 지웁니다 */
static void prune(Walker *walker, int32_t villager_count)
{
    int32_t i = 0;
    while (i < walker->count) {
        int32_t id = walker->entries[i].villager_id;
        int32_t k;
        int walking = 0;
        for (k = 0; k < villager_count; k++) {
            const CoreVillagerView *v = &walker->villagers[k];
            if (v->id == id && !v->at_target && !(v->intent_flags & CORE_INTENT_HOLD)) {
                walking = 1;
                break;
            }
        }
        if (walking) {
            i++;
        } else {
            walker->entries[i] = walker->entries[--walker->count];
        }
    }
}

static int copy_villagers(Walker *walker, const CoreWorld *world, int32_t *out_count)
{
    int32_t rc = core_copy_villagers(world, walker->villagers, walker->villager_capacity, out_count);
    if (rc == CORE_ERR_BUFFER_TOO_SMALL) {
        int32_t capacity = *out_count + 16;
        CoreVillagerView *grown =
            (CoreVillagerView *)realloc(walker->villagers, (size_t)capacity * sizeof(CoreVillagerView));
        if (grown == NULL) {
            return -1;
        }
        walker->villagers = grown;
        walker->villager_capacity = capacity;
        rc = core_copy_villagers(world, walker->villagers, walker->villager_capacity, out_count);
    }
    return rc == CORE_OK ? 0 : -1;
}

int walker_step(Walker *walker, CoreWorld *world)
{
    CoreSummaryView summary;
    int32_t count = 0;
    int32_t i;

    if (core_get_summary(world, &summary) != CORE_OK || refresh_cost(walker, world, summary.cost_grid_rev) != 0 ||
        copy_villagers(walker, world, &count) != 0) {
        return -1;
    }
    walker->normal_cost = summary.ruleset ? 100 : 10;
    prune(walker, count);
    for (i = 0; i < count; i++) {
        const CoreVillagerView *v = &walker->villagers[i];
        WalkEntry *entry;
        if (v->at_target || (v->intent_flags & CORE_INTENT_HOLD)) {
            continue;
        }
        entry = find_entry(walker, v->id);
        if (entry == NULL) {
            entry = add_entry(walker, v->id);
            if (entry == NULL) {
                return -1;
            }
        }
        if (entry->intent_rev != v->intent_rev) {
            /* 새 의도: 경로를 구해 걸리는 틱을 정함 */
            int32_t path = walker_path_cost(walker, v->loc_x, v->loc_y, v->target_x, v->target_y);
            entry->intent_rev = v->intent_rev;
            if (path < 0) {
                (void)core_villager_unreachable(world, v->id, v->intent_rev);
                continue;
            }
            /* 비용 10 = 한 칸. 걷는 속도는 한 틱에 걷는 칸 × 1000 */
            entry->ticks_left =
                walker->instant ? 0
                                : (path * (summary.ruleset ? 10 : 100) + v->walk_speed - 1) / v->walk_speed;
        }
        if (entry->ticks_left <= 0) {
            (void)core_villager_arrived(world, v->id, v->intent_rev);
        } else {
            entry->ticks_left--;
        }
    }
    return 0;
}

int32_t walker_tick(Walker *walker, CoreWorld *world)
{
    int32_t rc = core_tick(world, 1);
    if (rc == CORE_OK && walker_step(walker, world) != 0) {
        rc = CORE_ERR_OUT_OF_MEMORY;
    }
    return rc;
}
