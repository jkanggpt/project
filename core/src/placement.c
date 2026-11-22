/*
 * placement.c — 배치, 건설, 철거, 도로 (설명은 placement.h)
 */
#include "placement.h"
#include "expansion.h"

#include <string.h>

#include "jobs.h"
#include "rank.h"
#include "threat.h"

int placement_needs_door(const BuildingDef *def, int32_t type)
{
    /* 일꾼·거주민이 있거나 저장소(먹으러 오는 곳)면 문이 필요합니다. 성벽·성문·탑은 필요 없음 */
    return def->worker_slots > 0 || def->resident_slots > 0 || def->storage > 0 || type == CORE_BLD_KEEP;
}

/* 이 지형 위에 이 종류를 지을 수 있는가 */
static int terrain_allows(int32_t type, uint8_t terrain)
{
    switch (terrain) {
    case CORE_TERRAIN_GRASS:
    case CORE_TERRAIN_FOREST:
        return 1;
    case CORE_TERRAIN_ROCK:
        return type == CORE_BLD_ROAD; /* 바위 지대에는 도로만 (포장) */
    case CORE_TERRAIN_MANA:
        return type == CORE_BLD_MANA_MINE || type == CORE_BLD_ROAD; /* 광맥은 광산을 위해 남겨 둠 */
    default:
        return 0; /* 물 */
    }
}

/* (x, y) 가 문 앞으로 쓸 수 있는 칸인가: 지도 안, 걸을 수 있는 지형, 비어 있음, 새 건물 발자국 밖 */
static int door_tile_ok(const World *world, int32_t x, int32_t y, int32_t bx, int32_t by, int32_t bw, int32_t bh)
{
    const Grid *grid = &world->grid;
    int32_t index;
    if (!grid_in_bounds(grid, x, y)) {
        return 0;
    }
    if (x >= bx && y >= by && x < bx + bw && y < by + bh) {
        return 0;
    }
    index = grid_index(grid, x, y);
    return grid_terrain_cost(grid->terrain[index]) != GRID_COST_BLOCKED && grid->occupant[index] == 0;
}

/* 문 앞 칸 찾기: 아래쪽 가운데부터, 그다음 아래 줄, 오른쪽, 왼쪽, 위 순서 */
static int find_door(const World *world, int32_t bx, int32_t by, int32_t bw, int32_t bh, int32_t *out_x,
                     int32_t *out_y)
{
    int32_t offset;
    int32_t i;
    for (offset = 0; offset < bw; offset++) {
        /* 가운데에서 가까운 칸부터: 0, -1, +1, -2, +2 … */
        int32_t step = (offset + 1) / 2;
        int32_t x = bx + bw / 2 + ((offset % 2 == 1) ? -step : step);
        if (x >= bx && x < bx + bw && door_tile_ok(world, x, by + bh, bx, by, bw, bh)) {
            *out_x = x;
            *out_y = by + bh;
            return 1;
        }
    }
    for (i = 0; i < bh; i++) {
        if (door_tile_ok(world, bx + bw, by + i, bx, by, bw, bh)) {
            *out_x = bx + bw;
            *out_y = by + i;
            return 1;
        }
        if (door_tile_ok(world, bx - 1, by + i, bx, by, bw, bh)) {
            *out_x = bx - 1;
            *out_y = by + i;
            return 1;
        }
    }
    for (i = 0; i < bw; i++) {
        if (door_tile_ok(world, bx + i, by - 1, bx, by, bw, bh)) {
            *out_x = bx + i;
            *out_y = by - 1;
            return 1;
        }
    }
    return 0;
}

/* 배치 규칙이 세는 지형 */
static int32_t rule_terrain(int32_t rule)
{
    switch (rule) {
    case CORE_PLACE_NEAR_FOREST:
        return CORE_TERRAIN_FOREST;
    case CORE_PLACE_ADJ_ROCK:
        return CORE_TERRAIN_ROCK;
    case CORE_PLACE_ON_MANA:
        return CORE_TERRAIN_MANA;
    default:
        return -1;
    }
}

int32_t placement_counted_terrain(const BuildingDef *def)
{
    return rule_terrain(def->placement);
}

int32_t placement_count_terrain(const World *world, const BuildingDef *def, int32_t x, int32_t y, int32_t w,
                                int32_t h)
{
    const Grid *grid = &world->grid;
    int32_t terrain = rule_terrain(def->placement);
    int32_t r = def->work_radius;
    int32_t count = 0;
    int32_t cx;
    int32_t cy;
    if (terrain < 0) {
        return -1;
    }
    for (cy = y - r; cy < y + h + r; cy++) {
        for (cx = x - r; cx < x + w + r; cx++) {
            int inside = cx >= x && cy >= y && cx < x + w && cy < y + h;
            /* 발자국 안은 건물이 들어서며 숲이 사라지므로 세지 않습니다. 광맥은 발자국 안도 셉니다 */
            if (inside && terrain != CORE_TERRAIN_MANA) {
                continue;
            }
            if (grid_terrain_at(grid, cx, cy) == (uint8_t)terrain) {
                count++;
            }
        }
    }
    return count;
}

int32_t placement_productivity(const BuildingDef *def, int32_t terrain_count)
{
    int32_t percent;
    if (terrain_count < 0) {
        return 100;
    }
    percent = terrain_count * 100 / def->ideal_terrain;
    if (percent > 150) {
        percent = 150;
    }
    return percent;
}

/* 발자국 바로 옆(상하좌우)의 바위 칸 수 */
static int32_t count_adjacent_rock(const Grid *grid, int32_t x, int32_t y, int32_t w, int32_t h)
{
    int32_t count = 0;
    int32_t i;
    for (i = 0; i < w; i++) {
        count += grid_terrain_at(grid, x + i, y - 1) == CORE_TERRAIN_ROCK;
        count += grid_terrain_at(grid, x + i, y + h) == CORE_TERRAIN_ROCK;
    }
    for (i = 0; i < h; i++) {
        count += grid_terrain_at(grid, x - 1, y + i) == CORE_TERRAIN_ROCK;
        count += grid_terrain_at(grid, x + w, y + i) == CORE_TERRAIN_ROCK;
    }
    return count;
}

/* 발자국 안의 광맥 칸 수 */
static int32_t count_footprint(const Grid *grid, int32_t x, int32_t y, int32_t w, int32_t h, uint8_t terrain)
{
    int32_t count = 0;
    int32_t cx;
    int32_t cy;
    for (cy = y; cy < y + h; cy++) {
        for (cx = x; cx < x + w; cx++) {
            count += grid_terrain_at(grid, cx, cy) == terrain;
        }
    }
    return count;
}

/* 도로 한 칸 검사 */
static int32_t check_road(const World *world, int32_t x, int32_t y)
{
    const Grid *grid = &world->grid;
    int32_t index;
    if (!grid_in_bounds(grid, x, y)) {
        return CORE_PLACE_OUT_OF_BOUNDS;
    }
    index = grid_index(grid, x, y);
    if (!terrain_allows(CORE_BLD_ROAD, grid->terrain[index])) {
        return CORE_PLACE_BAD_TERRAIN;
    }
    if (grid->occupant[index] != 0) {
        return CORE_PLACE_OCCUPIED;
    }
    if (grid->road[index]) {
        return CORE_PLACE_ALREADY_THERE;
    }
    return CORE_PLACE_OK;
}

/* 건물 발자국 검사 (지도, 지형, 다른 건물, 도로, 문) */
static int32_t check_footprint(const World *world, int32_t type, int32_t x, int32_t y, int32_t w, int32_t h)
{
    const Grid *grid = &world->grid;
    int32_t cx;
    int32_t cy;
    if (x < 0 || y < 0 || x + w > grid->width || y + h > grid->height) {
        return CORE_PLACE_OUT_OF_BOUNDS;
    }
    /* 가장자리 한 줄은 비워 둡니다: 이주민이 들어오고 떠나는 주민이 나가는 길 */
    if (x < 1 || y < 1 || x + w > grid->width - 1 || y + h > grid->height - 1) {
        return CORE_PLACE_TOO_CLOSE_EDGE;
    }
    for (cy = y; cy < y + h; cy++) {
        for (cx = x; cx < x + w; cx++) {
            int32_t index = grid_index(grid, cx, cy);
            if (grid->occupant[index] != 0) {
                const Building *other = entity_find_building(world, grid->occupant[index]);
                if (other != NULL && other->type == type && w == 1 && h == 1) {
                    return CORE_PLACE_ALREADY_THERE;
                }
                return CORE_PLACE_OCCUPIED;
            }
            if (!terrain_allows(type, grid->terrain[index])) {
                return CORE_PLACE_BAD_TERRAIN;
            }
            if (grid->road[index]) {
                return CORE_PLACE_ROAD_IN_WAY;
            }
            if (grid->door_count[index] > 0) {
                return CORE_PLACE_BLOCKS_DOOR;
            }
        }
    }
    return CORE_PLACE_OK;
}

/* ---- 길이 끊기지 않는지 검사 ------------------------------------------- */

/* 주민이 지나갈 수 있는 칸인가 (가상의 새 발자국 [bx,by,bw,bh] 는 type 에 따라 막힘으로 침) */
static int passable_with(const Grid *grid, int32_t index, int32_t x, int32_t y, int32_t type, int32_t bx,
                         int32_t by, int32_t bw, int32_t bh)
{
    uint8_t cost = grid->cost[index];
    if (bw > 0 && x >= bx && y >= by && x < bx + bw && y < by + bh) {
        return type == CORE_BLD_GATE; /* 성문만 주민이 지나감 */
    }
    return cost != CORE_COST_BLOCKED && cost != CORE_COST_WALL;
}

/* 성채 문에서 걸어서 닿는 칸을 scratch_mark 에 표시합니다 (새 발자국이 있다고 치고). 닿은 칸 수.
 * stop_index >= 0 이면 그 칸에 닿는 즉시 멈춥니다 (가까운 문 하나만 확인할 때 지도 전체를 돌지 않게). */
static int32_t flood_from_keep(const World *world, int32_t type, int32_t bx, int32_t by, int32_t bw, int32_t bh,
                               int32_t stop_index)
{
    static const int32_t DX[4] = {0, 1, 0, -1};
    static const int32_t DY[4] = {-1, 0, 1, 0};
    const Grid *grid = &world->grid;
    const Building *keep = world_keep(world);
    int32_t cells = grid->width * grid->height;
    int32_t head = 0;
    int32_t tail = 0;
    int32_t start;
    memset(grid->scratch_mark, 0, (size_t)cells);
    if (keep == NULL || !grid_in_bounds(grid, keep->door_x, keep->door_y)) {
        return 0;
    }
    start = grid_index(grid, keep->door_x, keep->door_y);
    if (!passable_with(grid, start, keep->door_x, keep->door_y, type, bx, by, bw, bh)) {
        return 0;
    }
    grid->scratch_mark[start] = 1;
    grid->scratch_queue[tail++] = start;
    while (head < tail) {
        int32_t current = grid->scratch_queue[head++];
        int32_t cx = current % grid->width;
        int32_t cy = current / grid->width;
        int32_t d;
        for (d = 0; d < 4; d++) {
            int32_t nx = cx + DX[d];
            int32_t ny = cy + DY[d];
            int32_t next;
            if (!grid_in_bounds(grid, nx, ny)) {
                continue;
            }
            next = grid_index(grid, nx, ny);
            if (grid->scratch_mark[next] || !passable_with(grid, next, nx, ny, type, bx, by, bw, bh)) {
                continue;
            }
            grid->scratch_mark[next] = 1;
            grid->scratch_queue[tail++] = next;
            if (next == stop_index) {
                return tail;
            }
        }
    }
    return tail;
}

/* 지나갈 수 있는 칸인가 (지도 밖은 막힘) */
static int walkable_at(const Grid *grid, int32_t x, int32_t y)
{
    uint8_t cost;
    if (!grid_in_bounds(grid, x, y)) {
        return 0;
    }
    cost = grid->cost[grid_index(grid, x, y)];
    return cost != CORE_COST_BLOCKED && cost != CORE_COST_WALL;
}

/*
 * 빠른 검사: 발자국을 한 칸 둘러싼 테두리에서 지나갈 수 있는 칸들이 한 덩어리(한 줄로 이어짐)인가?
 * 그렇다면 발자국을 지나던 길은 모두 테두리를 따라 돌아갈 수 있으므로, 막아도 끊기는 곳이 없습니다.
 * (테두리에서 이웃한 두 칸은 언제나 상하좌우로 붙어 있습니다. 모서리 칸이 두 변을 이어 줍니다.)
 * 덩어리가 둘 이상이면 끊길 수도 있으니 지도 전체를 검사합니다.
 */
static int ring_is_connected(const Grid *grid, int32_t x, int32_t y, int32_t w, int32_t h)
{
    int32_t length = 2 * (w + 2) + 2 * h;
    int32_t runs = 0;
    int32_t walkable = 0;
    int previous;
    int32_t i;
    int32_t rx = x - 1;
    int32_t ry = y - 1;
    /* 테두리를 시계 방향으로 한 바퀴: 위 → 오른쪽 → 아래 → 왼쪽. 마지막 칸이 첫 칸과 이웃합니다. */
    previous = walkable_at(grid, x - 1, y); /* 첫 칸 (x-1, y-1) 바로 앞 칸 */
    for (i = 0; i < length; i++) {
        int here = walkable_at(grid, rx, ry);
        walkable += here;
        if (here && !previous) {
            runs++;
        }
        previous = here;
        if (ry == y - 1 && rx < x + w) {
            rx++;
        } else if (rx == x + w && ry < y + h) {
            ry++;
        } else if (ry == y + h && rx > x - 1) {
            rx--;
        } else {
            ry--;
        }
    }
    return walkable == length || runs <= 1;
}

/* 지금 표시된 칸 중에서: 문 앞이 몇 개 닿는지, 가장자리에 닿는지 */
static void count_reached(const World *world, int32_t *doors, int *edge)
{
    const Grid *grid = &world->grid;
    int32_t i;
    *doors = 0;
    *edge = 0;
    for (i = 0; i < world->building_count; i++) {
        const Building *b = world->buildings[i];
        if (b->door_x >= 0 && grid_in_bounds(grid, b->door_x, b->door_y) &&
            grid->scratch_mark[grid_index(grid, b->door_x, b->door_y)]) {
            (*doors)++;
        }
    }
    for (i = 0; i < grid->width && !*edge; i++) {
        *edge = grid->scratch_mark[grid_index(grid, i, 0)] || grid->scratch_mark[grid_index(grid, i, grid->height - 1)];
    }
    for (i = 0; i < grid->height && !*edge; i++) {
        *edge = grid->scratch_mark[grid_index(grid, 0, i)] || grid->scratch_mark[grid_index(grid, grid->width - 1, i)];
    }
}

/* 이 자리에 지어도 지금 닿는 문과 지도 가장자리에 계속 닿는지, 새 문 앞에도 닿는지 */
static int32_t check_paths(const World *world, int32_t type, int32_t x, int32_t y, int32_t w, int32_t h,
                           int32_t door_x, int32_t door_y)
{
    const Grid *grid = &world->grid;
    int32_t doors_before;
    int32_t doors_after;
    int edge_before;
    int edge_after;
    if (type == CORE_BLD_GATE || ring_is_connected(grid, x, y, w, h)) {
        /* 아무 길도 끊기지 않음: 새 문에 닿는지만 보면 됩니다 (닿으면 바로 멈춤) */
        if (door_x < 0) {
            return CORE_PLACE_OK;
        }
        (void)flood_from_keep(world, type, x, y, w, h, grid_index(grid, door_x, door_y));
        return grid->scratch_mark[grid_index(grid, door_x, door_y)] ? CORE_PLACE_OK : CORE_PLACE_UNREACHABLE;
    }
    (void)flood_from_keep(world, type, 0, 0, 0, 0, -1);
    count_reached(world, &doors_before, &edge_before);
    if (door_x >= 0 && !grid->scratch_mark[grid_index(grid, door_x, door_y)]) {
        return CORE_PLACE_UNREACHABLE;
    }
    (void)flood_from_keep(world, type, x, y, w, h, -1);
    count_reached(world, &doors_after, &edge_after);
    if (doors_after < doors_before || (edge_before && !edge_after)) {
        return CORE_PLACE_BLOCKS_PATH;
    }
    if (door_x >= 0 && !grid->scratch_mark[grid_index(grid, door_x, door_y)]) {
        return CORE_PLACE_UNREACHABLE;
    }
    return CORE_PLACE_OK;
}

/* 배치 규칙 검사 (숲 근처, 바위 옆, 광맥 위) */
static int32_t check_rule(const World *world, const BuildingDef *def, int32_t x, int32_t y, int32_t count)
{
    switch (def->placement) {
    case CORE_PLACE_NEAR_FOREST:
        return count >= def->min_terrain ? CORE_PLACE_OK : CORE_PLACE_NEEDS_FOREST;
    case CORE_PLACE_ADJ_ROCK:
        return count_adjacent_rock(&world->grid, x, y, def->w, def->h) >= def->min_terrain ? CORE_PLACE_OK
                                                                                            : CORE_PLACE_NEEDS_ROCK;
    case CORE_PLACE_ON_MANA:
        return count_footprint(&world->grid, x, y, def->w, def->h, CORE_TERRAIN_MANA) >= def->min_terrain
                   ? CORE_PLACE_OK
                   : CORE_PLACE_NEEDS_MANA;
    default:
        return CORE_PLACE_OK;
    }
}

void placement_check(const World *world, int32_t type, int32_t x, int32_t y, CorePlacementView *out)
{
    if(world->expansion){expansion_place_check(world,type,x,y,0,0,out);return;}
    const BuildingDef *def = world_building_def(world, type);
    int32_t result;
    int32_t i;

    memset(out, 0, sizeof(*out));
    out->door_x = -1;
    out->door_y = -1;
    out->terrain_count = -1;
    out->productivity = 100;
    if (def == NULL || type == CORE_BLD_KEEP) {
        out->result = CORE_PLACE_BAD_TYPE;
        return;
    }
    for (i = 0; i < CORE_RES_COUNT; i++) {
        out->cost[i] = def->cost[i];
    }
    if (def->required_rank > world->rank) {
        out->result = CORE_PLACE_LOCKED;
        return;
    }
    if (type == CORE_BLD_ROAD) {
        result = check_road(world, x, y);
    } else {
        result = check_footprint(world, type, x, y, def->w, def->h);
        if (result == CORE_PLACE_OK && placement_needs_door(def, type) &&
            !find_door(world, x, y, def->w, def->h, &out->door_x, &out->door_y)) {
            result = CORE_PLACE_NO_DOOR;
        }
        if (result == CORE_PLACE_OK || result == CORE_PLACE_NO_DOOR) {
            out->terrain_count = placement_count_terrain(world, def, x, y, def->w, def->h);
            out->productivity = placement_productivity(def, out->terrain_count);
        }
        if (result == CORE_PLACE_OK) {
            result = check_rule(world, def, x, y, out->terrain_count);
        }
        if (result == CORE_PLACE_OK) {
            result = check_paths(world, type, x, y, def->w, def->h, out->door_x, out->door_y);
        }
    }
    /* 자원 부족은 마지막에: 지형 문제가 있으면 그것을 먼저 알려 줍니다 */
    if (result == CORE_PLACE_OK && !eco_bank_can_afford(&world->bank, def->cost)) {
        result = CORE_PLACE_CANNOT_AFFORD;
    }
    out->result = result;
}

/* 건물이 칸을 막는 방식 */
static uint8_t block_code(const Building *building)
{
    if (building->state == CORE_BSTATE_RUINED &&
        (building->type == CORE_BLD_WALL || building->type == CORE_BLD_GATE)) {
        return GRID_BLOCK_NONE; /* 무너진 성벽은 지나갈 수 있음 */
    }
    if (building->type == CORE_BLD_WALL) {
        return GRID_BLOCK_WALL;
    }
    if (building->type == CORE_BLD_GATE) {
        return GRID_BLOCK_GATE;
    }
    return GRID_BLOCK_SOLID;
}

/* 건물을 지도 층에 표시하거나(add = 1) 지웁니다(add = 0) */
static void mark_building(World *world, const Building *building, int add)
{
    Grid *grid = &world->grid;
    int32_t cx;
    int32_t cy;
    for (cy = building->y; cy < building->y + building->h; cy++) {
        for (cx = building->x; cx < building->x + building->w; cx++) {
            int32_t index = grid_index(grid, cx, cy);
            grid->occupant[index] = add ? building->id : 0;
            grid->block[index] = add ? block_code(building) : GRID_BLOCK_NONE;
        }
    }
    if (building->door_x >= 0 && grid_in_bounds(grid, building->door_x, building->door_y)) {
        int32_t index = grid_index(grid, building->door_x, building->door_y);
        if (add) {
            grid->door_count[index]++;
        } else if (grid->door_count[index] > 0) {
            grid->door_count[index]--;
        }
    }
}

void placement_rebuild_layers(World *world)
{
    if(world->expansion){expansion_rebuild(world);return;}
    Grid *grid = &world->grid;
    size_t cells = (size_t)grid->width * (size_t)grid->height;
    int32_t i;
    memset(grid->occupant, 0, cells * sizeof(int32_t));
    memset(grid->door_count, 0, cells);
    memset(grid->block, 0, cells);
    for (i = 0; i < world->building_count; i++) {
        mark_building(world, world->buildings[i], 1);
    }
    grid_rebuild_costs(grid);
}

void placement_refresh_productivity(World *world)
{
    int32_t i;
    for (i = 0; i < world->building_count; i++) {
        Building *building = world->buildings[i];
        const BuildingDef *def = world_building_def(world, building->type);
        int32_t count = placement_count_terrain(world, def, building->x, building->y, building->w, building->h);
        building->productivity = placement_productivity(def, count);
    }
}

/* 숲 칸을 풀밭으로 바꿉니다 (나무를 베어 터를 닦음) */
static void clear_forest(Grid *grid, int32_t x, int32_t y, int32_t w, int32_t h)
{
    int32_t cx;
    int32_t cy;
    for (cy = y; cy < y + h; cy++) {
        for (cx = x; cx < x + w; cx++) {
            uint8_t *cell = &grid->terrain[grid_index(grid, cx, cy)];
            if (*cell == CORE_TERRAIN_FOREST) {
                *cell = CORE_TERRAIN_GRASS;
            }
        }
    }
}

/* 새 발자국 위에 서 있던 주민을 문 앞으로 옮기고, 목표가 덮였으면 다시 계획하게 합니다 */
static void relocate_villagers(World *world, const Building *building)
{
    const Building *keep = world_keep(world);
    int32_t safe_x = building->door_x >= 0 ? building->door_x : (keep != NULL ? keep->door_x : world->keep_x);
    int32_t safe_y = building->door_x >= 0 ? building->door_y : (keep != NULL ? keep->door_y : world->keep_y);
    int32_t i;
    for (i = 0; i < world->villager_count; i++) {
        Villager *villager = world->villagers[i];
        if (entity_building_covers(building, villager->loc_x, villager->loc_y)) {
            villager->loc_x = safe_x;
            villager->loc_y = safe_y;
        }
        if (villager->target == NULL && entity_building_covers(building, villager->target_x, villager->target_y)) {
            villager->target_x = villager->loc_x;
            villager->target_y = villager->loc_y;
            villager->at_target = 0;
            villager->hold_ticks = 0;
            villager->intent_rev++;
        }
    }
}

/* 다 지어진 건물의 저장 한도를 합쳐 은행 한도를 다시 정합니다 */
void placement_refresh_capacity(World *world)
{
    if(world->expansion){expansion_capacity(world);return;}
    int32_t total = rank_storage_bonus(world); /* 승급할 때마다 늘어난 저장 한도 */
    int32_t i;
    for (i = 0; i < world->building_count; i++) {
        const Building *building = world->buildings[i];
        if (building->state == CORE_BSTATE_ACTIVE) {
            total += world_building_def(world, building->type)->storage;
        }
    }
    eco_set_capacity(&world->bank, &world->ledger, total);
}

/* 다 지었을 때 */
static void complete_building(World *world, Building *building)
{
    building->state = CORE_BSTATE_ACTIVE;
    building->hp = world_building_def(world, building->type)->max_hp;
    if (building->type != CORE_BLD_WALL) {
        world->stats.buildings_built++;
    }
    events_push_building(&world->events, CORE_EVT_BUILDING_COMPLETED, building->id, building->type, building->x,
                         building->y);
    placement_refresh_capacity(world);
    jobs_on_buildings_changed(world);
}

void placement_finish_now(World *world, Building *building)
{
    if (building != NULL && building->state == CORE_BSTATE_CONSTRUCTING) {
        building->build_progress = world_building_def(world, building->type)->build_ticks;
        complete_building(world, building);
    }
}

static int place_road(World *world, int32_t x, int32_t y)
{
    Grid *grid = &world->grid;
    const BuildingDef *def = world_building_def(world, CORE_BLD_ROAD);
    int32_t index = grid_index(grid, x, y);
    if (!eco_pay(&world->bank, &world->ledger, def->cost, CORE_LEDGER_CONSTRUCTION)) {
        return CORE_ERR_REJECTED;
    }
    grid->road[index] = 1;
    clear_forest(grid, x, y, 1, 1);
    grid_rebuild_costs(grid);
    events_push_building(&world->events, CORE_EVT_BUILDING_PLACED, 0, CORE_BLD_ROAD, x, y);
    return CORE_OK;
}

int placement_place(World *world, int32_t type, int32_t x, int32_t y, int32_t *out_id, int32_t *out_reason)
{
    if(world->expansion)return expansion_place(world,type,x,y,0,out_id,out_reason);
    CorePlacementView check;
    const BuildingDef *def;
    Building *building;

    *out_id = 0;
    placement_check(world, type, x, y, &check);
    *out_reason = check.result;
    if (check.result != CORE_PLACE_OK) {
        return CORE_ERR_REJECTED;
    }
    if (type == CORE_BLD_ROAD) {
        return place_road(world, x, y);
    }
    def = world_building_def(world, type);
    /* 먼저 할당하고, 성공했을 때만 비용을 냅니다 (실패해도 월드가 그대로이게) */
    building = entity_add_building(world, type);
    if (building == NULL) {
        *out_reason = CORE_PLACE_OK;
        return CORE_ERR_OUT_OF_MEMORY;
    }
    (void)eco_pay(&world->bank, &world->ledger, def->cost, CORE_LEDGER_CONSTRUCTION);
    building->x = x;
    building->y = y;
    building->w = def->w;
    building->h = def->h;
    building->door_x = check.door_x;
    building->door_y = check.door_y;
    building->hp = def->max_hp;
    building->state = CORE_BSTATE_CONSTRUCTING;
    building->build_progress = 0;
    clear_forest(&world->grid, x, y, def->w, def->h);
    mark_building(world, building, 1);
    relocate_villagers(world, building);
    grid_rebuild_costs(&world->grid);
    placement_refresh_productivity(world);
    events_push_building(&world->events, CORE_EVT_BUILDING_PLACED, building->id, type, x, y);
    *out_id = building->id;
    if (def->build_ticks <= 0) {
        complete_building(world, building);
    }
    return CORE_OK;
}

int placement_line(World *world, int32_t type, int32_t x0, int32_t y0, int32_t x1, int32_t y1,
                   int32_t *out_placed, int32_t *out_reason)
{
    const BuildingDef *def = world_building_def(world, type);
    int32_t x = x0;
    int32_t y = y0;
    int32_t steps = 0;

    *out_placed = 0;
    *out_reason = CORE_PLACE_OK;
    if (def == NULL || !def->line) {
        *out_reason = CORE_PLACE_BAD_TYPE;
        return CORE_ERR_REJECTED;
    }
    /* 가로로 먼저 가고 꺾어서 세로로 갑니다 (L 자 모양) */
    for (;;) {
        CorePlacementView check;
        int32_t id;
        int32_t reason;
        placement_check(world, type, x, y, &check);
        if (check.result == CORE_PLACE_OK) {
            int rc = placement_place(world, type, x, y, &id, &reason);
            if (rc == CORE_ERR_OUT_OF_MEMORY) {
                return rc;
            }
            if (rc == CORE_OK) {
                (*out_placed)++;
            }
        } else if (check.result != CORE_PLACE_ALREADY_THERE) {
            if (*out_reason == CORE_PLACE_OK) {
                *out_reason = check.result;
            }
            if (check.result == CORE_PLACE_CANNOT_AFFORD) {
                break; /* 돈이 떨어지면 멈춤 */
            }
        }
        if (x == x1 && y == y1) {
            break;
        }
        if (++steps >= PLACEMENT_MAX_LINE) {
            break;
        }
        if (x != x1) {
            x += x1 > x ? 1 : -1;
        } else {
            y += y1 > y ? 1 : -1;
        }
    }
    if (*out_placed == 0 && *out_reason != CORE_PLACE_OK) {
        return CORE_ERR_REJECTED;
    }
    return CORE_OK;
}

/* 비용의 절반을 돌려줍니다 */
static void refund_half(World *world, const BuildingDef *def)
{
    int32_t i;
    for (i = 0; i < CORE_RES_COUNT; i++) {
        if (def->cost[i] / 2 > 0) {
            (void)eco_change(&world->bank, &world->ledger, i, def->cost[i] / 2, CORE_LEDGER_REFUND);
        }
    }
}

int placement_demolish(World *world, Building *building)
{
    const BuildingDef *def = world_building_def(world, building->type);
    int32_t id = building->id;
    int32_t type = building->type;
    int32_t x = building->x;
    int32_t y = building->y;

    if (type == CORE_BLD_KEEP) {
        return CORE_ERR_REJECTED;
    }
    refund_half(world, def);
    mark_building(world, building, 0);
    entity_remove_building(world, building); /* 일꾼·거주민 연결을 양쪽 모두 끊고 해제 */
    grid_rebuild_costs(&world->grid);
    placement_refresh_productivity(world);
    placement_refresh_capacity(world);
    events_push_building(&world->events, CORE_EVT_BUILDING_REMOVED, id, type, x, y);
    jobs_on_buildings_changed(world);
    return CORE_OK;
}

int placement_remove_road(World *world, int32_t x, int32_t y)
{
    Grid *grid = &world->grid;
    int32_t index;
    if (!grid_in_bounds(grid, x, y)) {
        return CORE_ERR_INVALID_ARG;
    }
    index = grid_index(grid, x, y);
    if (!grid->road[index]) {
        return CORE_ERR_REJECTED;
    }
    grid->road[index] = 0;
    refund_half(world, world_building_def(world, CORE_BLD_ROAD));
    grid_rebuild_costs(grid);
    events_push_building(&world->events, CORE_EVT_BUILDING_REMOVED, 0, CORE_BLD_ROAD, x, y);
    return CORE_OK;
}

/* ---- 피해와 수리 ----------------------------------------------------- */

/* 무너진 건물에서는 일도 잠도 못 하므로 일꾼·거주민을 양쪽 모두 떼어 냅니다 (자동 배치가 다른 곳을 찾아 줌) */
static void damage_event(World *world,Building *b,CoreEventType type) {
    Event event={0};event.type=type;event.as.building.id=b->id;event.as.building.type=b->type;
    event.as.building.x=b->x;event.as.building.y=b->y;
    event.cause_kind=b->damage_tick==world->clock.tick?b->damage_kind:0;
    event.source_id=b->damage_source;event.event_tick=world->clock.tick;event.type_serial=b->type_serial;
    if(event.cause_kind==1 && event.source_id>=1 && event.source_id<=world->threat.raid.enemy_count)
        event.source_type=world->threat.raid.enemies[event.source_id-1].type+1;
    if(event.cause_kind==2 && world->expansion)for(int i=0;i<world->expansion->animal_count;i++)
        if(world->expansion->animals[i].id==event.source_id)event.source_type=world->expansion->animals[i].kind+1;
    events_push(&world->events,&event);
}
static void ruin_building(World *world, Building *building)
{
    building->state = CORE_BSTATE_RUINED;
    building->hp = 0;
    building->flags = 0;
    building->work_milli[0] = 0;
    building->work_milli[1] = 0;
    while (building->worker_count > 0) {
        entity_unlink_worker(building->workers[building->worker_count - 1]);
    }
    while (building->resident_count > 0) {
        entity_unlink_resident(building->residents[building->resident_count - 1]);
    }
    damage_event(world,building,CORE_EVT_BUILDING_RUINED);
    if (world->expansion || building->type == CORE_BLD_WALL || building->type == CORE_BLD_GATE) {
        placement_rebuild_layers(world); /* 무너진 성벽·성문은 지나갈 수 있음 → 비용 격자가 바뀜 */
    }
    placement_refresh_capacity(world);
    jobs_on_buildings_changed(world);
}

void placement_damage(World *world, Building *building, int32_t damage)
{
    if (damage <= 0 || building->state == CORE_BSTATE_RUINED) {
        return;
    }
    if (!world->expansion && building->state == CORE_BSTATE_CONSTRUCTING) {
        /* 짓는 중이면 공사가 뒤로 밀립니다 */
        building->build_progress = building->build_progress > 6 ? building->build_progress - 6 : 0;
        events_push_building(&world->events, CORE_EVT_BUILDING_DAMAGED, building->id, building->type, building->x,
                             building->y);
        return;
    }
    building->hp -= damage;
    if (building->hp <= 0) {
        ruin_building(world, building);
    } else {
        events_push_building(&world->events, CORE_EVT_BUILDING_DAMAGED, building->id, building->type, building->x,
                             building->y);
    }
}

void placement_repair_cost(const World *world, const Building *building, int32_t out_cost[CORE_RES_COUNT])
{
    const BuildingDef *def = world_building_def(world, building->type);
    int32_t missing = def->max_hp - building->hp;
    int32_t any = 0;
    int32_t i;
    for (i = 0; i < CORE_RES_COUNT; i++) {
        out_cost[i] = 0;
    }
    if (building->state == CORE_BSTATE_CONSTRUCTING || missing <= 0 || def->max_hp <= 0) {
        return;
    }
    for (i = 0; i < CORE_RES_COUNT; i++) {
        /* 반올림이 아니라 올림: 조금만 부서져도 1 은 듦 */
        out_cost[i] = (int32_t)(((int64_t)def->cost[i] * missing + 2LL * def->max_hp - 1) / (2LL * def->max_hp));
        any += out_cost[i];
    }
    if (any == 0) {
        /* 비용이 없는 건물(성채)은 돌과 나무로 고칩니다 */
        out_cost[CORE_RES_STONE] = (missing + 19) / 20;
        out_cost[CORE_RES_WOOD] = (missing + 39) / 40;
    }
}

int placement_repair(World *world, Building *building, int32_t *out_reason)
{
    const BuildingDef *def = world_building_def(world, building->type);
    int32_t cost[CORE_RES_COUNT];
    int was_ruined = building->state == CORE_BSTATE_RUINED;
    *out_reason = CORE_DENY_NONE;
    if (threat_raid_active(world)) {
        /* 맞는 동안 곧바로 고치면 성벽이 무너지지 않으므로, 습격이 끝난 뒤에 고칩니다 */
        *out_reason = CORE_DENY_RAID;
        return CORE_ERR_REJECTED;
    }
    if (building->state == CORE_BSTATE_CONSTRUCTING) {
        *out_reason = CORE_DENY_UNDER_CONSTRUCTION;
        return CORE_ERR_REJECTED;
    }
    if (building->hp >= def->max_hp) {
        *out_reason = CORE_DENY_NOT_DAMAGED;
        return CORE_ERR_REJECTED;
    }
    placement_repair_cost(world, building, cost);
    if (!eco_pay(&world->bank, &world->ledger, cost, CORE_LEDGER_REPAIR)) {
        *out_reason = CORE_DENY_NO_RESOURCES;
        return CORE_ERR_REJECTED;
    }
    building->hp = def->max_hp;
    building->state = CORE_BSTATE_ACTIVE;
    events_push_building(&world->events, CORE_EVT_BUILDING_REPAIRED, building->id, building->type, building->x,
                         building->y);
    if (world->expansion) {
        /* 수리비로 쓴 원목의 출처 잔량과 복구된 경계·도로를 즉시 맞춥니다. */
        if (world->expansion->magic_logs_milli > expansion_bank_have(world, CORE_RES_LOGS))
            world->expansion->magic_logs_milli = expansion_bank_have(world, CORE_RES_LOGS);
        world->expansion->dirty = 1;
        expansion_rebuild(world);
        expansion_capacity(world);
        expansion_assign(world);
    } else if (was_ruined) {
        if (building->type == CORE_BLD_WALL || building->type == CORE_BLD_GATE) {
            placement_rebuild_layers(world); /* 다시 막힘 */
        }
        placement_refresh_capacity(world);
        jobs_on_buildings_changed(world);
    }
    return CORE_OK;
}

void placement_tick(World *world)
{
    int32_t i;
    /* 다 지은 건물에서 jobs_on_buildings_changed 가 불리지만, 건물 목록 자체는 바뀌지 않으므로 앞에서부터 돌아도 됩니다 */
    for (i = 0; i < world->building_count; i++) {
        Building *building = world->buildings[i];
        if (building->state != CORE_BSTATE_CONSTRUCTING) {
            continue;
        }
        building->build_progress++;
        if (building->build_progress >= world_building_def(world, building->type)->build_ticks) {
            complete_building(world, building);
        }
    }
}
