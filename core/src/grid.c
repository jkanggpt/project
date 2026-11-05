/*
 * grid.c — 지도와 지형 생성 (설명은 grid.h)
 *
 * 지형 생성 순서
 *   1. 전부 풀밭으로 채운다.
 *   2. 서로 다른 노이즈 세 장으로 숲, 바위, 물(호수)을 칠한다. 각각 "노이즈 값 상위 몇 %" 칸을 칠하므로
 *      시드가 달라도 지형 비율은 거의 같다. (어떤 시드는 숲이 하나도 없는 일이 없음)
 *   3. 마정석 광맥 덩어리를 몇 개 흩뿌린다.
 *   4. 성채 자리를 풀밭으로 비운다.
 *   5. 연결성: 성채에서 지도 네 변까지 걸어갈 수 있게 하고, 갇힌 땅은 물로 바꾼다.
 *      (나중에 적은 지도 가장자리에서 오고, 떠나는 주민은 가장자리로 걸어 나가기 때문)
 *   6. 공정한 시작: 성채 근처에 숲, 바위, 마정석 광맥이 모자라면 덩어리를 찍어 채운다.
 *
 * 노이즈는 정수(고정소수점)로만 계산합니다. 실수 계산은 컴파일러·CPU에 따라 마지막 비트가 달라질 수 있어서,
 * 같은 시드가 Windows와 Linux에서 다른 지도를 만들 위험을 없애기 위해서입니다.
 */
#include "grid.h"

#include <string.h>

#include "mem.h"

/* 지형별 기본 이동 비용 (순서는 CoreTerrain). 바위 지대는 지나갈 수 있지만 느립니다. */
static const uint8_t TERRAIN_COST[CORE_TERRAIN_COUNT] = {
    10, /* 풀밭 */
    15, /* 숲 */
    30, /* 바위 */
    0,  /* 물: 지나갈 수 없음 */
    10, /* 마정석 광맥 */
};

static const char TERRAIN_SYMBOL[CORE_TERRAIN_COUNT] = {'.', 'T', '^', '~', '*'};

/* 지형 비율 (전체 칸 중 %) */
#define GEN_FOREST_PERCENT 26
#define GEN_ROCK_PERCENT 9
#define GEN_WATER_PERCENT 8
#define GEN_MANA_CLUSTERS_MIN 5
#define GEN_MANA_CLUSTERS_MAX 7
#define GEN_MANA_CLUSTER_SIZE_MIN 3
#define GEN_MANA_CLUSTER_SIZE_MAX 6

/* 공정한 시작: 성채 근처(GRID_FAIR_RADIUS 안)에 최소한 있어야 하는 칸 수와, 모자랄 때 찍는 덩어리 반지름 */
typedef struct FairRule {
    uint8_t terrain;
    int32_t min_count;
    int32_t blob_radius;
} FairRule;

static const FairRule FAIR_RULES[] = {
    {CORE_TERRAIN_FOREST, 30, 2},
    {CORE_TERRAIN_ROCK, 10, 1},
    {CORE_TERRAIN_MANA, 3, 1},
};
#define FAIR_RULE_COUNT ((int32_t)(sizeof(FAIR_RULES) / sizeof(FAIR_RULES[0])))
#define FAIR_BLOB_MIN_DIST 7
#define FAIR_BLOB_MAX_DIST 12
#define FAIR_MAX_ATTEMPTS 40

/* 상하좌우 네 방향 */
static const int32_t DIR_X[4] = {0, 1, 0, -1};
static const int32_t DIR_Y[4] = {-1, 0, 1, 0};

/* ---- 기본 격자 함수 --------------------------------------------------- */

int grid_alloc(Grid *grid, int32_t width, int32_t height)
{
    size_t count;

    memset(grid, 0, sizeof(*grid));
    if (width < GRID_MIN_SIZE || width > GRID_MAX_SIZE || height < GRID_MIN_SIZE || height > GRID_MAX_SIZE) {
        return CORE_ERR_INVALID_ARG;
    }
    count = (size_t)width * (size_t)height;
    grid->terrain = (uint8_t *)mem_calloc(count, sizeof(uint8_t));
    grid->cost = (uint8_t *)mem_calloc(count, sizeof(uint8_t));
    if (grid->terrain == NULL || grid->cost == NULL) {
        /* 하나만 성공했을 수도 있으니 둘 다 해제합니다 (mem_free(NULL) 은 안전) */
        mem_free(grid->terrain);
        mem_free(grid->cost);
        memset(grid, 0, sizeof(*grid));
        return CORE_ERR_OUT_OF_MEMORY;
    }
    grid->width = width;
    grid->height = height;
    return CORE_OK;
}

void grid_free(Grid *grid)
{
    mem_free(grid->terrain);
    mem_free(grid->cost);
    memset(grid, 0, sizeof(*grid));
}

int grid_in_bounds(const Grid *grid, int32_t x, int32_t y)
{
    return x >= 0 && y >= 0 && x < grid->width && y < grid->height;
}

int32_t grid_index(const Grid *grid, int32_t x, int32_t y)
{
    return y * grid->width + x;
}

uint8_t grid_terrain_at(const Grid *grid, int32_t x, int32_t y)
{
    if (!grid_in_bounds(grid, x, y)) {
        return CORE_TERRAIN_WATER;
    }
    return grid->terrain[grid_index(grid, x, y)];
}

uint8_t grid_terrain_cost(uint8_t terrain)
{
    if (terrain >= CORE_TERRAIN_COUNT) {
        return GRID_COST_BLOCKED;
    }
    return TERRAIN_COST[terrain];
}

void grid_rebuild_costs(Grid *grid)
{
    int32_t count = grid->width * grid->height;
    int32_t i;
    for (i = 0; i < count; i++) {
        grid->cost[i] = grid_terrain_cost(grid->terrain[i]);
    }
    grid->cost_rev++;
}

int32_t grid_count_in_radius(const Grid *grid, int32_t cx, int32_t cy, int32_t radius, uint8_t terrain)
{
    int32_t x;
    int32_t y;
    int32_t found = 0;
    for (y = cy - radius; y <= cy + radius; y++) {
        for (x = cx - radius; x <= cx + radius; x++) {
            int32_t dx = x - cx;
            int32_t dy = y - cy;
            if (dx * dx + dy * dy <= radius * radius && grid_in_bounds(grid, x, y) &&
                grid->terrain[grid_index(grid, x, y)] == terrain) {
                found++;
            }
        }
    }
    return found;
}

int32_t grid_flood_reachable(const Grid *grid, int32_t sx, int32_t sy, uint8_t *out_mark)
{
    int32_t count = grid->width * grid->height;
    int32_t *queue;
    int32_t head = 0;
    int32_t tail = 0;
    int32_t start;

    if (!grid_in_bounds(grid, sx, sy)) {
        return 0;
    }
    start = grid_index(grid, sx, sy);
    if (grid_terrain_cost(grid->terrain[start]) == GRID_COST_BLOCKED) {
        return 0;
    }
    /* 너비 우선 탐색(BFS) 대기열. 칸마다 많아야 한 번 들어가므로 칸 수만큼이면 충분합니다 */
    queue = (int32_t *)mem_malloc((size_t)count * sizeof(int32_t));
    if (queue == NULL) {
        return -1;
    }
    out_mark[start] = 1;
    queue[tail++] = start;
    while (head < tail) {
        int32_t current = queue[head++];
        int32_t x = current % grid->width;
        int32_t y = current / grid->width;
        int32_t d;
        for (d = 0; d < 4; d++) {
            int32_t nx = x + DIR_X[d];
            int32_t ny = y + DIR_Y[d];
            int32_t next;
            if (!grid_in_bounds(grid, nx, ny)) {
                continue;
            }
            next = grid_index(grid, nx, ny);
            if (out_mark[next] || grid_terrain_cost(grid->terrain[next]) == GRID_COST_BLOCKED) {
                continue;
            }
            out_mark[next] = 1;
            queue[tail++] = next;
        }
    }
    mem_free(queue);
    return tail;
}

char grid_terrain_symbol(uint8_t terrain)
{
    if (terrain >= CORE_TERRAIN_COUNT) {
        return '?';
    }
    return TERRAIN_SYMBOL[terrain];
}

int grid_terrain_from_symbol(char symbol)
{
    int i;
    for (i = 0; i < CORE_TERRAIN_COUNT; i++) {
        if (TERRAIN_SYMBOL[i] == symbol) {
            return i;
        }
    }
    return -1;
}

/* ---- 정수 노이즈 ------------------------------------------------------ */

#define NOISE_ONE 65536 /* 고정소수점 1.0 */

/* 32비트 정수를 고르게 섞는 해시 */
static uint32_t mix32(uint32_t x)
{
    x ^= x >> 16;
    x *= 0x7feb352dU;
    x ^= x >> 15;
    x *= 0x846ca68bU;
    x ^= x >> 16;
    return x;
}

/* 격자점 (x, y) 의 무작위 값 0..65535 (시드가 같으면 늘 같음) */
static int32_t lattice_value(uint32_t seed, int32_t x, int32_t y)
{
    uint32_t hx = mix32((uint32_t)x * 0x9E3779B1U);
    uint32_t hy = mix32((uint32_t)y * 0x85EBCA77U);
    return (int32_t)(mix32(seed ^ hx ^ (hy * 31U)) >> 16);
}

/* 부드러운 보간 곡선 3t² - 2t³ (t 는 0..NOISE_ONE) */
static int32_t fade(int32_t t)
{
    int64_t tt = t;
    return (int32_t)(tt * tt * (3 * (int64_t)NOISE_ONE - 2 * tt) / ((int64_t)NOISE_ONE * NOISE_ONE));
}

static int32_t lerp_fixed(int32_t a, int32_t b, int32_t t)
{
    return a + (int32_t)(((int64_t)(b - a) * t) / NOISE_ONE);
}

/* 값 노이즈: cell 칸마다 무작위 값을 두고 그 사이를 부드럽게 잇습니다. x, y 는 0 이상 */
static int32_t value_noise(uint32_t seed, int32_t x, int32_t y, int32_t cell)
{
    int32_t cx = x / cell;
    int32_t cy = y / cell;
    int32_t fx = fade((x % cell) * NOISE_ONE / cell);
    int32_t fy = fade((y % cell) * NOISE_ONE / cell);
    int32_t top = lerp_fixed(lattice_value(seed, cx, cy), lattice_value(seed, cx + 1, cy), fx);
    int32_t bottom = lerp_fixed(lattice_value(seed, cx, cy + 1), lattice_value(seed, cx + 1, cy + 1), fx);
    return lerp_fixed(top, bottom, fy);
}

/* 큰 덩어리 + 중간 + 작은 굴곡을 겹친 노이즈 0..65535.
 * 층마다 격자를 조금씩 밀어 둡니다. 모든 층의 격자선이 같은 곳에 겹치면 지형 가장자리가 반듯한 직선이 되기 때문입니다. */
static int32_t fractal_noise(uint32_t seed, int32_t x, int32_t y)
{
    static const int32_t CELLS[4] = {24, 12, 6, 3};
    static const int32_t WEIGHTS[4] = {8, 4, 2, 1};
    int32_t sum = 0;
    int32_t total = 0;
    int32_t i;
    for (i = 0; i < 4; i++) {
        uint32_t layer_seed = seed + (uint32_t)i * 0x632BE5ABU;
        int32_t offset_x = (int32_t)(mix32(layer_seed) % 97U);
        int32_t offset_y = (int32_t)(mix32(layer_seed ^ 0xA5A5A5A5U) % 97U);
        sum += value_noise(layer_seed, x + offset_x, y + offset_y, CELLS[i]) * WEIGHTS[i];
        total += WEIGHTS[i];
    }
    return sum / total;
}

/* 노이즈 한 장을 field 에 채웁니다. 성채에서 exclude_radius 안쪽 칸은 -1 (칠하지 않음) */
static void fill_field(const Grid *grid, int32_t *field, uint32_t seed, int32_t keep_x, int32_t keep_y,
                       int32_t exclude_radius)
{
    int32_t x;
    int32_t y;
    for (y = 0; y < grid->height; y++) {
        for (x = 0; x < grid->width; x++) {
            int32_t dx = x - keep_x;
            int32_t dy = y - keep_y;
            int32_t i = grid_index(grid, x, y);
            if (exclude_radius > 0 && dx * dx + dy * dy <= exclude_radius * exclude_radius) {
                field[i] = -1;
            } else {
                field[i] = fractal_noise(seed, x, y);
            }
        }
    }
}

/* field 값이 가장 큰 칸부터 전체의 percent% 를 넘지 않게 terrain 으로 칠합니다.
 * 256칸 히스토그램으로 문턱값을 찾으므로 정렬이 필요 없습니다. */
static void paint_top_percent(Grid *grid, const int32_t *field, int32_t percent, uint8_t terrain)
{
    int32_t histogram[256];
    int32_t count = grid->width * grid->height;
    int32_t target = count * percent / 100;
    int32_t accumulated = 0;
    int32_t bin = 255;
    int32_t i;

    memset(histogram, 0, sizeof(histogram));
    for (i = 0; i < count; i++) {
        if (field[i] >= 0) {
            histogram[field[i] >> 8]++;
        }
    }
    while (bin > 0 && accumulated + histogram[bin] <= target) {
        accumulated += histogram[bin];
        bin--;
    }
    for (i = 0; i < count; i++) {
        if (field[i] >= 0 && (field[i] >> 8) > bin) {
            grid->terrain[i] = terrain;
        }
    }
}

/* ---- 생성 단계 -------------------------------------------------------- */

static int32_t chebyshev(int32_t ax, int32_t ay, int32_t bx, int32_t by)
{
    int32_t dx = ax > bx ? ax - bx : bx - ax;
    int32_t dy = ay > by ? ay - by : by - ay;
    return dx > dy ? dx : dy;
}

static int in_keep_clear_zone(int32_t x, int32_t y, int32_t keep_x, int32_t keep_y)
{
    return chebyshev(x, y, keep_x, keep_y) <= GRID_KEEP_CLEAR;
}

/* (cx, cy) 에서 무작위로 걸으며 마정석 광맥을 size 칸 놓습니다 */
static void place_mana_cluster(Grid *grid, Rng *rng, int32_t cx, int32_t cy, int32_t size, int32_t keep_x,
                               int32_t keep_y)
{
    int32_t x = cx;
    int32_t y = cy;
    int32_t placed = 0;
    int32_t steps;
    for (steps = 0; steps < size * 4 && placed < size; steps++) {
        if (grid_in_bounds(grid, x, y) && !in_keep_clear_zone(x, y, keep_x, keep_y)) {
            uint8_t *cell = &grid->terrain[grid_index(grid, x, y)];
            if (*cell != CORE_TERRAIN_WATER && *cell != CORE_TERRAIN_MANA) {
                *cell = CORE_TERRAIN_MANA;
                placed++;
            }
        }
        {
            int32_t d = rng_range(rng, 0, 3);
            x += DIR_X[d];
            y += DIR_Y[d];
        }
    }
}

static void scatter_mana(Grid *grid, Rng *rng, int32_t keep_x, int32_t keep_y)
{
    int32_t clusters = rng_range(rng, GEN_MANA_CLUSTERS_MIN, GEN_MANA_CLUSTERS_MAX);
    int32_t i;
    for (i = 0; i < clusters; i++) {
        int32_t x = rng_range(rng, 2, grid->width - 3);
        int32_t y = rng_range(rng, 2, grid->height - 3);
        int32_t size = rng_range(rng, GEN_MANA_CLUSTER_SIZE_MIN, GEN_MANA_CLUSTER_SIZE_MAX);
        place_mana_cluster(grid, rng, x, y, size, keep_x, keep_y);
    }
}

static void clear_keep_area(Grid *grid, int32_t keep_x, int32_t keep_y)
{
    int32_t x;
    int32_t y;
    for (y = keep_y - GRID_KEEP_CLEAR; y <= keep_y + GRID_KEEP_CLEAR; y++) {
        for (x = keep_x - GRID_KEEP_CLEAR; x <= keep_x + GRID_KEEP_CLEAR; x++) {
            if (grid_in_bounds(grid, x, y)) {
                grid->terrain[grid_index(grid, x, y)] = CORE_TERRAIN_GRASS;
            }
        }
    }
}

/* side: 0 북, 1 동, 2 남, 3 서. 그 변의 칸 중 하나라도 닿았으면 1 */
static int edge_reached(const Grid *grid, const uint8_t *mark, int32_t side)
{
    int32_t i;
    int32_t length = (side == 0 || side == 2) ? grid->width : grid->height;
    for (i = 0; i < length; i++) {
        int32_t x = (side == 0 || side == 2) ? i : (side == 1 ? grid->width - 1 : 0);
        int32_t y = (side == 1 || side == 3) ? i : (side == 2 ? grid->height - 1 : 0);
        if (mark[grid_index(grid, x, y)]) {
            return 1;
        }
    }
    return 0;
}

/* 성채에서 변의 가운데까지 물을 메워 땅길을 냅니다. 상하좌우로만 움직여서 길이 끊기지 않게 합니다 */
static void carve_corridor(Grid *grid, int32_t keep_x, int32_t keep_y, int32_t side)
{
    int32_t tx = (side == 1) ? grid->width - 1 : (side == 3 ? 0 : keep_x);
    int32_t ty = (side == 2) ? grid->height - 1 : (side == 0 ? 0 : keep_y);
    int32_t x = keep_x;
    int32_t y = keep_y;
    for (;;) {
        uint8_t *cell = &grid->terrain[grid_index(grid, x, y)];
        if (*cell == CORE_TERRAIN_WATER) {
            *cell = CORE_TERRAIN_GRASS;
        }
        if (x == tx && y == ty) {
            break;
        }
        if (x != tx) {
            x += (tx > x) ? 1 : -1;
        } else {
            y += (ty > y) ? 1 : -1;
        }
    }
}

static int ensure_connectivity(Grid *grid, int32_t keep_x, int32_t keep_y)
{
    int32_t count = grid->width * grid->height;
    uint8_t *mark = (uint8_t *)mem_calloc((size_t)count, sizeof(uint8_t));
    int32_t side;
    int carved = 0;
    int32_t i;

    if (mark == NULL) {
        return CORE_ERR_OUT_OF_MEMORY;
    }
    if (grid_flood_reachable(grid, keep_x, keep_y, mark) < 0) {
        mem_free(mark);
        return CORE_ERR_OUT_OF_MEMORY;
    }
    for (side = 0; side < 4; side++) {
        if (!edge_reached(grid, mark, side)) {
            carve_corridor(grid, keep_x, keep_y, side);
            carved = 1;
        }
    }
    if (carved) {
        memset(mark, 0, (size_t)count);
        if (grid_flood_reachable(grid, keep_x, keep_y, mark) < 0) {
            mem_free(mark);
            return CORE_ERR_OUT_OF_MEMORY;
        }
    }
    /* 걸어서 닿지 않는 땅(호수에 갇힌 섬)은 물로 바꿉니다. 건물을 지어도 아무도 못 가기 때문입니다 */
    for (i = 0; i < count; i++) {
        if (!mark[i] && grid->terrain[i] != CORE_TERRAIN_WATER) {
            grid->terrain[i] = CORE_TERRAIN_WATER;
        }
    }
    mem_free(mark);
    return CORE_OK;
}

/* (cx, cy) 둘레 radius 안을 terrain 으로 칠합니다. 풀밭과 숲만 바꾸고(숲이 빽빽한 시드에서도 바위와
 * 광맥이 들어갈 자리가 있도록), 물·바위·광맥·성채 자리는 건드리지 않습니다 */
static void stamp_blob(Grid *grid, int32_t cx, int32_t cy, int32_t radius, uint8_t terrain, int32_t keep_x,
                       int32_t keep_y)
{
    int32_t x;
    int32_t y;
    for (y = cy - radius; y <= cy + radius; y++) {
        for (x = cx - radius; x <= cx + radius; x++) {
            int32_t dx = x - cx;
            int32_t dy = y - cy;
            uint8_t *cell;
            if (dx * dx + dy * dy > radius * radius + radius || !grid_in_bounds(grid, x, y) ||
                in_keep_clear_zone(x, y, keep_x, keep_y)) {
                continue;
            }
            cell = &grid->terrain[grid_index(grid, x, y)];
            if (*cell == CORE_TERRAIN_GRASS || *cell == CORE_TERRAIN_FOREST) {
                *cell = terrain;
            }
        }
    }
}

/* 모든 규칙을 만족하면 1 */
static int fair_start_ok(const Grid *grid, int32_t keep_x, int32_t keep_y)
{
    int32_t r;
    for (r = 0; r < FAIR_RULE_COUNT; r++) {
        if (grid_count_in_radius(grid, keep_x, keep_y, GRID_FAIR_RADIUS, FAIR_RULES[r].terrain) <
            FAIR_RULES[r].min_count) {
            return 0;
        }
    }
    return 1;
}

/* 규칙 하나가 만족될 때까지 성채 둘레 고리(7~12칸)에 덩어리를 찍습니다 */
static void apply_fair_rule(Grid *grid, Rng *rng, const FairRule *rule, int32_t keep_x, int32_t keep_y)
{
    int32_t attempts = 0;
    while (grid_count_in_radius(grid, keep_x, keep_y, GRID_FAIR_RADIUS, rule->terrain) < rule->min_count &&
           attempts < FAIR_MAX_ATTEMPTS) {
        int32_t dx = rng_range(rng, -FAIR_BLOB_MAX_DIST, FAIR_BLOB_MAX_DIST);
        int32_t dy = rng_range(rng, -FAIR_BLOB_MAX_DIST, FAIR_BLOB_MAX_DIST);
        int32_t dist2 = dx * dx + dy * dy;
        attempts++;
        if (dist2 < FAIR_BLOB_MIN_DIST * FAIR_BLOB_MIN_DIST || dist2 > FAIR_BLOB_MAX_DIST * FAIR_BLOB_MAX_DIST) {
            continue; /* 고리 밖이면 다시 뽑기 */
        }
        stamp_blob(grid, keep_x + dx, keep_y + dy, rule->blob_radius, rule->terrain, keep_x, keep_y);
    }
}

static void ensure_fair_start(Grid *grid, Rng *rng, int32_t keep_x, int32_t keep_y)
{
    int32_t pass;
    /* 바위 덩어리가 숲을 덮어 숲이 모자라게 될 수도 있으므로, 모두 만족할 때까지 몇 번 되풀이합니다 */
    for (pass = 0; pass < 3 && !fair_start_ok(grid, keep_x, keep_y); pass++) {
        int32_t r;
        for (r = 0; r < FAIR_RULE_COUNT; r++) {
            apply_fair_rule(grid, rng, &FAIR_RULES[r], keep_x, keep_y);
        }
    }
}

int grid_generate(Grid *grid, uint32_t seed, Rng *rng, int32_t keep_x, int32_t keep_y)
{
    int32_t count = grid->width * grid->height;
    int32_t *field;
    int rc;

    if (!grid_in_bounds(grid, keep_x, keep_y)) {
        return CORE_ERR_INVALID_ARG;
    }
    field = (int32_t *)mem_malloc((size_t)count * sizeof(int32_t));
    if (field == NULL) {
        return CORE_ERR_OUT_OF_MEMORY;
    }
    memset(grid->terrain, CORE_TERRAIN_GRASS, (size_t)count);

    fill_field(grid, field, seed ^ 0x5F0E57U, keep_x, keep_y, 0);
    paint_top_percent(grid, field, GEN_FOREST_PERCENT, CORE_TERRAIN_FOREST);
    fill_field(grid, field, seed ^ 0x0B0C4AU, keep_x, keep_y, 0);
    paint_top_percent(grid, field, GEN_ROCK_PERCENT, CORE_TERRAIN_ROCK);
    fill_field(grid, field, seed ^ 0x3A7E21U, keep_x, keep_y, GRID_NO_WATER_RADIUS);
    paint_top_percent(grid, field, GEN_WATER_PERCENT, CORE_TERRAIN_WATER);
    mem_free(field);

    scatter_mana(grid, rng, keep_x, keep_y);
    clear_keep_area(grid, keep_x, keep_y);

    rc = ensure_connectivity(grid, keep_x, keep_y);
    if (rc != CORE_OK) {
        return rc;
    }
    /* 공정한 시작은 연결성 다음에 합니다: 덩어리는 풀밭 위에만 찍으므로 길을 막지 않습니다 */
    ensure_fair_start(grid, rng, keep_x, keep_y);
    grid_rebuild_costs(grid);
    return CORE_OK;
}
