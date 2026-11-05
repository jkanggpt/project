/*
 * api.c — 공개 API (castle_core.h) 의 구현
 *
 * 여기는 "접착제"만 있습니다: 인자를 검사하고, 알맞은 모듈 함수를 부르고, 결과를 뷰 구조체로 복사합니다.
 * 게임 규칙은 각 모듈(world, grid, economy, ...)에 있습니다.
 * 바깥(Godot, CLI)에서 무엇을 넘기든 죽지 않고 오류 코드를 돌려주는 것이 이 파일의 책임입니다.
 */
#include <string.h>

#include "castle_core.h"
#include "save.h"
#include "validate.h"
#include "world.h"

/* 공개 헤더의 상수와 내부 상수가 어긋나면 컴파일 단계에서 막습니다 */
_Static_assert(CORE_RES_COUNT == 6, "resource count changed: update save keys and C# views");
_Static_assert(sizeof(CoreSummaryView) % 4 == 0, "views must stay 4-byte aligned for C#");
_Static_assert(sizeof(CoreNewGameParams) == CORE_NAME_BYTES + 8, "unexpected padding in CoreNewGameParams");

CORE_API int32_t core_abi_version(void)
{
    return CORE_ABI_VERSION;
}

CORE_API int32_t core_get_struct_sizes(CoreStructSizes *out, int32_t out_size)
{
    if (out == NULL) {
        return CORE_ERR_NULL_ARG;
    }
    if (out_size != (int32_t)sizeof(CoreStructSizes)) {
        return CORE_ERR_STRUCT_SIZE;
    }
    out->new_game_params = (int32_t)sizeof(CoreNewGameParams);
    out->config_view = (int32_t)sizeof(CoreConfigView);
    out->summary_view = (int32_t)sizeof(CoreSummaryView);
    out->event_view = (int32_t)sizeof(CoreEventView);
    return CORE_OK;
}

CORE_API int32_t core_get_config(CoreConfigView *out)
{
    if (out == NULL) {
        return CORE_ERR_NULL_ARG;
    }
    memset(out, 0, sizeof(*out));
    out->abi_version = CORE_ABI_VERSION;
    out->minutes_per_tick = CLOCK_MINUTES_PER_TICK;
    out->ticks_per_day = CLOCK_TICKS_PER_DAY;
    out->days_per_season = CLOCK_DAYS_PER_SEASON;
    out->seasons_per_year = CLOCK_SEASONS_PER_YEAR;
    out->real_ms_per_tick = CLOCK_REAL_MS_PER_TICK;
    out->max_speed = CLOCK_MAX_SPEED;
    out->max_ticks_per_call = CLOCK_TICKS_PER_DAY * 10;
    out->night_start_minute = CLOCK_NIGHT_START_MIN;
    out->night_end_minute = CLOCK_NIGHT_END_MIN;
    out->resource_count = CORE_RES_COUNT;
    out->terrain_count = CORE_TERRAIN_COUNT;
    return CORE_OK;
}

CORE_API const char *core_result_name(int32_t code)
{
    switch (code) {
    case CORE_OK:
        return "CORE_OK";
    case CORE_ERR_NULL_ARG:
        return "CORE_ERR_NULL_ARG";
    case CORE_ERR_INVALID_ARG:
        return "CORE_ERR_INVALID_ARG";
    case CORE_ERR_OUT_OF_MEMORY:
        return "CORE_ERR_OUT_OF_MEMORY";
    case CORE_ERR_BUFFER_TOO_SMALL:
        return "CORE_ERR_BUFFER_TOO_SMALL";
    case CORE_ERR_FILE_OPEN:
        return "CORE_ERR_FILE_OPEN";
    case CORE_ERR_FILE_IO:
        return "CORE_ERR_FILE_IO";
    case CORE_ERR_FORMAT:
        return "CORE_ERR_FORMAT";
    case CORE_ERR_VERSION:
        return "CORE_ERR_VERSION";
    case CORE_ERR_STRUCT_SIZE:
        return "CORE_ERR_STRUCT_SIZE";
    default:
        return "CORE_ERR_UNKNOWN";
    }
}

CORE_API int32_t core_create(const CoreNewGameParams *params, CoreWorld **out_world)
{
    if (out_world == NULL) {
        return CORE_ERR_NULL_ARG;
    }
    *out_world = NULL;
    if (params == NULL) {
        return CORE_ERR_NULL_ARG;
    }
    return world_create(params, out_world);
}

CORE_API void core_destroy(CoreWorld *world)
{
    world_destroy(world);
}

CORE_API int32_t core_tick(CoreWorld *world, int32_t ticks)
{
    if (world == NULL) {
        return CORE_ERR_NULL_ARG;
    }
    if (ticks < 1 || ticks > CLOCK_TICKS_PER_DAY * 10) {
        return CORE_ERR_INVALID_ARG;
    }
    return world_tick(world, ticks);
}

CORE_API int32_t core_get_summary(const CoreWorld *world, CoreSummaryView *out)
{
    int32_t i;
    if (world == NULL || out == NULL) {
        return CORE_ERR_NULL_ARG;
    }
    memset(out, 0, sizeof(*out));
    out->tick = world->clock.tick;
    out->day = clock_day(&world->clock);
    out->minute_of_day = clock_minute_of_day(&world->clock);
    out->season = clock_season(&world->clock);
    out->day_of_season = clock_day_of_season(&world->clock);
    out->year = clock_year(&world->clock);
    out->is_night = clock_is_night(&world->clock);
    out->difficulty = world->difficulty;
    out->seed = world->seed;
    out->map_width = world->grid.width;
    out->map_height = world->grid.height;
    out->keep_x = world->keep_x;
    out->keep_y = world->keep_y;
    out->keep_size = GRID_KEEP_SIZE;
    out->cost_grid_rev = world->grid.cost_rev;
    for (i = 0; i < CORE_RES_COUNT; i++) {
        out->resources[i] = world->bank.amount[i];
        out->resource_capacity[i] = eco_is_stored_good(i) ? world->bank.capacity : 0;
    }
    /* 이름 버퍼는 크기가 같고 항상 '\0' 으로 끝나므로 통째로 복사합니다 */
    memcpy(out->castle_name, world->castle_name, CORE_NAME_BYTES);
    out->castle_name[CORE_NAME_BYTES - 1] = '\0';
    return CORE_OK;
}

CORE_API int32_t core_get_map_size(const CoreWorld *world, int32_t *out_width, int32_t *out_height)
{
    if (world == NULL || out_width == NULL || out_height == NULL) {
        return CORE_ERR_NULL_ARG;
    }
    *out_width = world->grid.width;
    *out_height = world->grid.height;
    return CORE_OK;
}

CORE_API int32_t core_copy_terrain(const CoreWorld *world, uint8_t *out, int32_t capacity)
{
    int32_t count;
    if (world == NULL || out == NULL) {
        return CORE_ERR_NULL_ARG;
    }
    count = world->grid.width * world->grid.height;
    if (capacity < count) {
        return CORE_ERR_BUFFER_TOO_SMALL;
    }
    memcpy(out, world->grid.terrain, (size_t)count);
    return CORE_OK;
}

CORE_API int32_t core_copy_cost_grid(const CoreWorld *world, uint8_t *out, int32_t capacity, int32_t *out_rev)
{
    int32_t count;
    if (world == NULL || out == NULL) {
        return CORE_ERR_NULL_ARG;
    }
    count = world->grid.width * world->grid.height;
    if (capacity < count) {
        return CORE_ERR_BUFFER_TOO_SMALL;
    }
    memcpy(out, world->grid.cost, (size_t)count);
    if (out_rev != NULL) {
        *out_rev = world->grid.cost_rev;
    }
    return CORE_OK;
}

CORE_API int32_t core_poll_event(CoreWorld *world, CoreEventView *out)
{
    Event event;
    if (world == NULL || out == NULL) {
        return CORE_ERR_NULL_ARG;
    }
    if (!events_pop(&world->events, &event)) {
        memset(out, 0, sizeof(*out));
        return 0;
    }
    events_to_view(&event, out);
    return 1;
}

CORE_API int32_t core_save(const CoreWorld *world, const char *path_utf8)
{
    if (world == NULL || path_utf8 == NULL) {
        return CORE_ERR_NULL_ARG;
    }
    if (path_utf8[0] == '\0') {
        return CORE_ERR_INVALID_ARG;
    }
    return save_write(world, path_utf8);
}

CORE_API int32_t core_load(const char *path_utf8, CoreWorld **out_world, int32_t *out_error_line)
{
    int32_t line = 0;
    int rc;
    if (out_world == NULL) {
        return CORE_ERR_NULL_ARG;
    }
    *out_world = NULL;
    if (out_error_line != NULL) {
        *out_error_line = 0;
    }
    if (path_utf8 == NULL) {
        return CORE_ERR_NULL_ARG;
    }
    if (path_utf8[0] == '\0') {
        return CORE_ERR_INVALID_ARG;
    }
    rc = save_read(path_utf8, out_world, &line);
    if (out_error_line != NULL) {
        *out_error_line = line;
    }
    return rc;
}

CORE_API int32_t core_validate(const CoreWorld *world)
{
    if (world == NULL) {
        return CORE_ERR_NULL_ARG;
    }
    return validate_world(world, NULL, NULL);
}

CORE_API int32_t core_state_hash(const CoreWorld *world, uint64_t *out_hash)
{
    if (world == NULL || out_hash == NULL) {
        return CORE_ERR_NULL_ARG;
    }
    *out_hash = world_state_hash(world);
    return CORE_OK;
}
