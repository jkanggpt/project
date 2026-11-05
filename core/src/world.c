/*
 * world.c — 월드 생성·해제와 틱 진행 (설명은 world.h)
 */
#include "world.h"

#include <string.h>

#include "mem.h"

World *world_alloc(void)
{
    World *world = (World *)mem_calloc(1, sizeof(World));
    if (world == NULL) {
        return NULL;
    }
    if (events_init(&world->events, EVENTS_INITIAL_CAPACITY) != CORE_OK) {
        mem_free(world);
        return NULL;
    }
    return world;
}

void world_destroy(World *world)
{
    if (world == NULL) {
        return;
    }
    grid_free(&world->grid);
    events_free(&world->events);
    mem_free(world);
}

/* UTF-8 바이트열 검사: 잘린 글자, 너무 긴 표현, 서로게이트, 제어 문자를 거부합니다 */
int world_name_is_valid(const char *name, int32_t max_bytes)
{
    const unsigned char *s = (const unsigned char *)name;
    int32_t length = 0;

    while (length < max_bytes && s[length] != '\0') {
        length++;
    }
    if (length == 0 || length >= max_bytes) {
        return 0; /* 비어 있거나 '\0' 자리가 없음 */
    }
    while (*s != '\0') {
        unsigned char c = *s;
        int extra;
        uint32_t code;
        int i;
        if (c < 0x20 || c == 0x7F) {
            return 0; /* 줄바꿈 등 제어 문자 (저장 파일은 한 줄에 한 값) */
        }
        if (c < 0x80) {
            s++;
            continue;
        }
        if ((c & 0xE0) == 0xC0) {
            extra = 1;
            code = c & 0x1F;
        } else if ((c & 0xF0) == 0xE0) {
            extra = 2;
            code = c & 0x0F;
        } else if ((c & 0xF8) == 0xF0) {
            extra = 3;
            code = c & 0x07;
        } else {
            return 0;
        }
        for (i = 1; i <= extra; i++) {
            if ((s[i] & 0xC0) != 0x80) {
                return 0; /* 이어지는 바이트가 아님 ('\0' 도 여기서 걸러짐) */
            }
            code = (code << 6) | (s[i] & 0x3F);
        }
        if ((extra == 1 && code < 0x80) || (extra == 2 && code < 0x800) || (extra == 3 && code < 0x10000) ||
            code > 0x10FFFF || (code >= 0xD800 && code <= 0xDFFF)) {
            return 0;
        }
        s += extra + 1;
    }
    return 1;
}

int world_set_name(World *world, const char *name)
{
    if (!world_name_is_valid(name, CORE_NAME_BYTES)) {
        return CORE_ERR_INVALID_ARG;
    }
    memset(world->castle_name, 0, sizeof(world->castle_name));
    strcpy(world->castle_name, name); /* 길이는 위에서 확인했습니다 */
    return CORE_OK;
}

int world_create(const CoreNewGameParams *params, World **out)
{
    World *world;
    int rc;

    *out = NULL;
    if (params->seed < 0 || params->difficulty < 0 || params->difficulty >= CORE_DIFF_COUNT ||
        !world_name_is_valid(params->castle_name, CORE_NAME_BYTES)) {
        return CORE_ERR_INVALID_ARG;
    }

    world = world_alloc();
    if (world == NULL) {
        return CORE_ERR_OUT_OF_MEMORY;
    }
    (void)world_set_name(world, params->castle_name);
    world->difficulty = params->difficulty;
    world->seed = params->seed;
    world->clock.tick = CLOCK_START_TICK;
    /* 지도 생성과 게임 진행이 같은 난수 스트림을 쓰도록 시드로 초기화합니다 */
    rng_seed(&world->rng, (uint64_t)(uint32_t)params->seed, 0x4D43u);
    eco_bank_init(&world->bank, params->difficulty);

    rc = grid_alloc(&world->grid, GRID_DEFAULT_SIZE, GRID_DEFAULT_SIZE);
    if (rc != CORE_OK) {
        world_destroy(world);
        return rc;
    }
    world->keep_x = world->grid.width / 2;
    world->keep_y = world->grid.height / 2;
    rc = grid_generate(&world->grid, (uint32_t)params->seed, &world->rng, world->keep_x, world->keep_y);
    if (rc != CORE_OK) {
        world_destroy(world);
        return rc;
    }
    *out = world;
    return CORE_OK;
}

/* 한 틱(게임 속 10분)을 진행합니다. 마일스톤 2부터 주민 상태 기계, 생산 등이 여기에 더해집니다 */
static void world_step(World *world)
{
    int32_t minute;

    world->clock.tick++;
    minute = clock_minute_of_day(&world->clock);

    if (minute == 0) {
        int32_t day = clock_day(&world->clock);
        events_push_day(&world->events, CORE_EVT_DAY_STARTED, day);
        if (clock_day_of_season(&world->clock) == 1) {
            events_push_season(&world->events, clock_season(&world->clock), clock_year(&world->clock));
        }
    }
    if (minute == CLOCK_NIGHT_START_MIN) {
        events_push_day(&world->events, CORE_EVT_NIGHT_FELL, clock_day(&world->clock));
    }
    if (minute == CLOCK_NIGHT_END_MIN) {
        events_push_day(&world->events, CORE_EVT_DAWN, clock_day(&world->clock));
    }
}

int world_tick(World *world, int32_t ticks)
{
    int32_t i;
    if (ticks < 1) {
        return CORE_ERR_INVALID_ARG;
    }
    if (world->clock.tick > CLOCK_MAX_TICK - ticks) {
        return CORE_ERR_INVALID_ARG; /* 시계가 int32 를 넘지 않게 막습니다 */
    }
    for (i = 0; i < ticks; i++) {
        world_step(world);
    }
    return CORE_OK;
}

/* ---- 상태 해시 (FNV-1a) ----------------------------------------------- */

#define FNV_OFFSET 1469598103934665603ULL
#define FNV_PRIME 1099511628211ULL

static uint64_t hash_bytes(uint64_t hash, const void *data, size_t size)
{
    const unsigned char *bytes = (const unsigned char *)data;
    size_t i;
    for (i = 0; i < size; i++) {
        hash ^= bytes[i];
        hash *= FNV_PRIME;
    }
    return hash;
}

/* 정수는 바이트 순서와 상관없이 같은 해시가 나오도록 한 바이트씩 넣습니다 */
static uint64_t hash_u64(uint64_t hash, uint64_t value)
{
    int i;
    for (i = 0; i < 8; i++) {
        unsigned char b = (unsigned char)(value >> (i * 8));
        hash = hash_bytes(hash, &b, 1);
    }
    return hash;
}

static uint64_t hash_i32(uint64_t hash, int32_t value)
{
    return hash_u64(hash, (uint64_t)(uint32_t)value);
}

uint64_t world_state_hash(const World *world)
{
    uint64_t hash = FNV_OFFSET;
    size_t cells = (size_t)world->grid.width * (size_t)world->grid.height;
    int32_t i;

    hash = hash_bytes(hash, world->castle_name, strlen(world->castle_name));
    hash = hash_i32(hash, world->difficulty);
    hash = hash_i32(hash, world->seed);
    hash = hash_i32(hash, world->keep_x);
    hash = hash_i32(hash, world->keep_y);
    hash = hash_i32(hash, world->clock.tick);
    hash = hash_u64(hash, world->rng.state);
    hash = hash_u64(hash, world->rng.inc);
    hash = hash_i32(hash, world->bank.capacity);
    for (i = 0; i < CORE_RES_COUNT; i++) {
        hash = hash_i32(hash, world->bank.amount[i]);
    }
    hash = hash_i32(hash, world->grid.width);
    hash = hash_i32(hash, world->grid.height);
    if (world->grid.terrain != NULL && world->grid.cost != NULL) {
        hash = hash_bytes(hash, world->grid.terrain, cells);
        hash = hash_bytes(hash, world->grid.cost, cells);
    }
    return hash;
}
