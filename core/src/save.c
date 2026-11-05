/*
 * save.c — 저장과 불러오기 (설명은 save.h)
 *
 * 저장 파일 예:
 *   MAGICCASTLE_SAVE 1
 *   [meta]
 *   name=바람언덕
 *   difficulty=1
 *   seed=12345
 *   keep=48,48
 *   [clock]
 *   tick=36
 *   [rng]
 *   state=0123456789abcdef
 *   inc=fedcba9876543211
 *   [resources]
 *   capacity=200
 *   gold=200
 *   ...
 *   [map]
 *   size=96,96
 *   row=..TT^^~~**....   (height 줄, 한 글자가 한 칸)
 *   [end]
 *
 * 읽을 때는 섹션 이름 → 해석 함수 표(함수 포인터)로 줄을 나눠 보냅니다.
 * 모든 값은 범위를 검사하고, 끝에 불변 조건 검사기까지 통과해야 성공입니다.
 */
#include "save.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "mem.h"
#include "platform.h"
#include "validate.h"

#define SAVE_LINE_INITIAL 128
#define SAVE_LINE_MAX 4096 /* 이보다 긴 줄은 망가진 파일로 봅니다 (메모리를 무한히 쓰지 않게) */

/* 자원 키 이름 (순서는 CoreResource) */
static const char *const RESOURCE_KEYS[CORE_RES_COUNT] = {"gold", "mana", "food", "wood", "stone", "herbs"};

/* ====================================================================== */
/* 쓰기                                                                    */
/* ====================================================================== */

static void write_hex64(FILE *file, uint64_t value)
{
    static const char DIGITS[] = "0123456789abcdef";
    char text[17];
    int i;
    for (i = 0; i < 16; i++) {
        text[15 - i] = DIGITS[(value >> (i * 4)) & 0xFu];
    }
    text[16] = '\0';
    fputs(text, file);
}

static int write_world(FILE *file, const World *world)
{
    int32_t x;
    int32_t y;
    int32_t i;

    fprintf(file, "%s %d\n", SAVE_MAGIC, SAVE_VERSION);
    fputs("# 마법성채 키우기 저장 파일입니다. 직접 고치면 불러오지 못할 수 있습니다.\n", file);

    fputs("[meta]\n", file);
    fprintf(file, "name=%s\n", world->castle_name);
    fprintf(file, "difficulty=%d\n", (int)world->difficulty);
    fprintf(file, "seed=%d\n", (int)world->seed);
    fprintf(file, "keep=%d,%d\n", (int)world->keep_x, (int)world->keep_y);

    fputs("[clock]\n", file);
    fprintf(file, "tick=%d\n", (int)world->clock.tick);

    fputs("[rng]\nstate=", file);
    write_hex64(file, world->rng.state);
    fputs("\ninc=", file);
    write_hex64(file, world->rng.inc);
    fputs("\n", file);

    fputs("[resources]\n", file);
    fprintf(file, "capacity=%d\n", (int)world->bank.capacity);
    for (i = 0; i < CORE_RES_COUNT; i++) {
        fprintf(file, "%s=%d\n", RESOURCE_KEYS[i], (int)world->bank.amount[i]);
    }

    fputs("[map]\n", file);
    fprintf(file, "size=%d,%d\n", (int)world->grid.width, (int)world->grid.height);
    for (y = 0; y < world->grid.height; y++) {
        fputs("row=", file);
        for (x = 0; x < world->grid.width; x++) {
            fputc(grid_terrain_symbol(world->grid.terrain[grid_index(&world->grid, x, y)]), file);
        }
        fputc('\n', file);
    }
    fputs("[end]\n", file);
    return ferror(file) ? CORE_ERR_FILE_IO : CORE_OK;
}

/* base 뒤에 suffix 를 붙인 새 문자열 (호출한 쪽이 mem_free) */
static char *path_with_suffix(const char *base, const char *suffix)
{
    size_t base_length = strlen(base);
    size_t suffix_length = strlen(suffix);
    char *joined = (char *)mem_malloc(base_length + suffix_length + 1);
    if (joined == NULL) {
        return NULL;
    }
    memcpy(joined, base, base_length);
    memcpy(joined + base_length, suffix, suffix_length + 1);
    return joined;
}

int save_write(const World *world, const char *path)
{
    char *tmp_path;
    char *bak_path;
    FILE *file;
    int rc = CORE_OK;

    /* 망가진 상태를 저장하면 멀쩡한 예전 세이브까지 덮어쓰게 되므로 거부합니다 */
    if (validate_world(world, NULL, NULL) != 0) {
        return CORE_ERR_INVALID_ARG;
    }
    tmp_path = path_with_suffix(path, ".tmp");
    bak_path = path_with_suffix(path, ".bak");
    if (tmp_path == NULL || bak_path == NULL) {
        mem_free(tmp_path);
        mem_free(bak_path);
        return CORE_ERR_OUT_OF_MEMORY;
    }

    /* 1) 임시 파일에 씁니다. 줄바꿈을 OS 마다 바꾸지 않도록 바이너리 모드("wb")를 씁니다 */
    file = platform_fopen_utf8(tmp_path, "wb");
    if (file == NULL) {
        rc = CORE_ERR_FILE_OPEN;
    } else {
        rc = write_world(file, world);
        if (fclose(file) != 0 && rc == CORE_OK) {
            rc = CORE_ERR_FILE_IO;
        }
    }
    /* 2) 기존 세이브를 .bak 으로 복사해 둡니다 */
    if (rc == CORE_OK && platform_file_exists(path) && platform_copy_file(path, bak_path) != 0) {
        rc = CORE_ERR_FILE_IO;
    }
    /* 3) 임시 파일로 한 번에 바꿔치기합니다. 중간에 꺼져도 예전 세이브나 새 세이브 중 하나는 온전합니다 */
    if (rc == CORE_OK && platform_replace_file(tmp_path, path) != 0) {
        rc = CORE_ERR_FILE_IO;
    }
    if (rc != CORE_OK && rc != CORE_ERR_FILE_OPEN) {
        (void)platform_remove_file(tmp_path);
    }
    mem_free(tmp_path);
    mem_free(bak_path);
    return rc;
}

/* ====================================================================== */
/* 읽기                                                                    */
/* ====================================================================== */

/* 길이를 모르는 줄을 읽기 위해 버퍼를 realloc 으로 키워 가며 씁니다 */
typedef struct LineReader {
    FILE *file;
    char *buffer;
    size_t capacity;
    int32_t line_number;
} LineReader;

/* 한 줄을 읽어 buffer 에 넣습니다 ('\n', '\r' 제거). 1 = 읽음, 0 = 파일 끝, 음수 = 오류 */
static int read_line(LineReader *reader)
{
    size_t length = 0;
    int c = EOF;

    while ((c = fgetc(reader->file)) != EOF) {
        if (c == '\n') {
            break;
        }
        if (c == '\0') {
            reader->line_number++;
            return CORE_ERR_FORMAT; /* 텍스트 파일에 '\0' 이 있으면 망가진 것 */
        }
        if (length + 1 >= reader->capacity) {
            size_t new_capacity = reader->capacity * 2;
            char *grown;
            if (new_capacity > SAVE_LINE_MAX) {
                reader->line_number++;
                return CORE_ERR_FORMAT;
            }
            grown = (char *)mem_realloc(reader->buffer, new_capacity);
            if (grown == NULL) {
                return CORE_ERR_OUT_OF_MEMORY; /* 원래 buffer 는 그대로라 나중에 해제됩니다 */
            }
            reader->buffer = grown;
            reader->capacity = new_capacity;
        }
        reader->buffer[length++] = (char)c;
    }
    if (ferror(reader->file)) {
        return CORE_ERR_FILE_IO;
    }
    if (c == EOF && length == 0) {
        return 0;
    }
    if (length > 0 && reader->buffer[length - 1] == '\r') {
        length--; /* Windows 메모장 등이 넣는 CRLF */
    }
    reader->buffer[length] = '\0';
    reader->line_number++;
    return 1;
}

/* 불러오는 중인 상태 */
typedef struct LoadState {
    World *world;
    int32_t section;         /* 지금 섹션 (SECTIONS 의 번호), -1 = 아직 없음 */
    uint32_t sections_seen;  /* 섹션별 비트 */
    uint32_t keys_seen[8];   /* 섹션마다 읽은 키 비트 */
    int32_t rows_read;
    int ended;               /* [end] 를 읽었으면 1 */
} LoadState;

typedef int (*SectionParser)(LoadState *state, const char *key, const char *value);

typedef struct SectionDef {
    const char *name;
    SectionParser parse;
    uint32_t required_keys; /* 반드시 있어야 하는 키 비트 */
} SectionDef;

/* 이 키를 처음 읽었으면 1, 같은 키가 두 번 나왔으면 0 */
static int mark_key(LoadState *state, int bit)
{
    uint32_t mask = 1u << bit;
    if (state->keys_seen[state->section] & mask) {
        return 0;
    }
    state->keys_seen[state->section] |= mask;
    return 1;
}

/* 10진 정수 하나. 앞뒤 공백, 범위 밖, 숫자 아닌 글자를 모두 거부합니다 */
static int parse_i32(const char *text, int32_t min, int32_t max, int32_t *out)
{
    char *end = NULL;
    long value;

    if (!((text[0] >= '0' && text[0] <= '9') || (text[0] == '-' && text[1] >= '0' && text[1] <= '9'))) {
        return 0;
    }
    errno = 0;
    value = strtol(text, &end, 10);
    if (errno != 0 || end == NULL || *end != '\0' || value < min || value > max) {
        return 0;
    }
    *out = (int32_t)value;
    return 1;
}

/* "a,b" 형식의 정수 두 개 */
static int parse_pair(const char *text, int32_t min, int32_t max, int32_t *out_a, int32_t *out_b)
{
    char first[16];
    const char *comma = strchr(text, ',');
    size_t length;

    if (comma == NULL) {
        return 0;
    }
    length = (size_t)(comma - text);
    if (length == 0 || length >= sizeof(first)) {
        return 0;
    }
    memcpy(first, text, length);
    first[length] = '\0';
    return parse_i32(first, min, max, out_a) && parse_i32(comma + 1, min, max, out_b);
}

/* 정확히 16자리 16진수 */
static int parse_hex64(const char *text, uint64_t *out)
{
    uint64_t value = 0;
    int i;
    for (i = 0; i < 16; i++) {
        char c = text[i];
        uint64_t digit;
        if (c >= '0' && c <= '9') {
            digit = (uint64_t)(c - '0');
        } else if (c >= 'a' && c <= 'f') {
            digit = (uint64_t)(c - 'a' + 10);
        } else {
            return 0;
        }
        value = (value << 4) | digit;
    }
    if (text[16] != '\0') {
        return 0;
    }
    *out = value;
    return 1;
}

static int parse_meta(LoadState *state, const char *key, const char *value)
{
    World *world = state->world;
    if (strcmp(key, "name") == 0 && mark_key(state, 0)) {
        return world_set_name(world, value) == CORE_OK ? CORE_OK : CORE_ERR_FORMAT;
    }
    if (strcmp(key, "difficulty") == 0 && mark_key(state, 1)) {
        return parse_i32(value, 0, CORE_DIFF_COUNT - 1, &world->difficulty) ? CORE_OK : CORE_ERR_FORMAT;
    }
    if (strcmp(key, "seed") == 0 && mark_key(state, 2)) {
        return parse_i32(value, 0, INT32_MAX, &world->seed) ? CORE_OK : CORE_ERR_FORMAT;
    }
    if (strcmp(key, "keep") == 0 && mark_key(state, 3)) {
        return parse_pair(value, 0, GRID_MAX_SIZE - 1, &world->keep_x, &world->keep_y) ? CORE_OK
                                                                                         : CORE_ERR_FORMAT;
    }
    return CORE_ERR_FORMAT; /* 모르는 키이거나 같은 키가 두 번 */
}

static int parse_clock(LoadState *state, const char *key, const char *value)
{
    if (strcmp(key, "tick") == 0 && mark_key(state, 0)) {
        return parse_i32(value, 0, CLOCK_MAX_TICK, &state->world->clock.tick) ? CORE_OK : CORE_ERR_FORMAT;
    }
    return CORE_ERR_FORMAT;
}

static int parse_rng(LoadState *state, const char *key, const char *value)
{
    if (strcmp(key, "state") == 0 && mark_key(state, 0)) {
        return parse_hex64(value, &state->world->rng.state) ? CORE_OK : CORE_ERR_FORMAT;
    }
    if (strcmp(key, "inc") == 0 && mark_key(state, 1)) {
        return parse_hex64(value, &state->world->rng.inc) ? CORE_OK : CORE_ERR_FORMAT;
    }
    return CORE_ERR_FORMAT;
}

static int parse_resources(LoadState *state, const char *key, const char *value)
{
    ResourceBank *bank = &state->world->bank;
    int i;
    if (strcmp(key, "capacity") == 0 && mark_key(state, 0)) {
        return parse_i32(value, 1, ECO_MAX_AMOUNT, &bank->capacity) ? CORE_OK : CORE_ERR_FORMAT;
    }
    for (i = 0; i < CORE_RES_COUNT; i++) {
        if (strcmp(key, RESOURCE_KEYS[i]) == 0 && mark_key(state, i + 1)) {
            return parse_i32(value, 0, ECO_MAX_AMOUNT, &bank->amount[i]) ? CORE_OK : CORE_ERR_FORMAT;
        }
    }
    return CORE_ERR_FORMAT;
}

static int parse_map(LoadState *state, const char *key, const char *value)
{
    Grid *grid = &state->world->grid;

    if (strcmp(key, "size") == 0 && mark_key(state, 0)) {
        int32_t width;
        int32_t height;
        if (!parse_pair(value, GRID_MIN_SIZE, GRID_MAX_SIZE, &width, &height)) {
            return CORE_ERR_FORMAT;
        }
        return grid_alloc(grid, width, height) == CORE_OK ? CORE_OK : CORE_ERR_OUT_OF_MEMORY;
    }
    if (strcmp(key, "row") == 0) {
        int32_t x;
        if (grid->terrain == NULL || state->rows_read >= grid->height || (int32_t)strlen(value) != grid->width) {
            return CORE_ERR_FORMAT; /* size 보다 먼저 나왔거나, 줄이 너무 많거나, 길이가 틀림 */
        }
        for (x = 0; x < grid->width; x++) {
            int terrain = grid_terrain_from_symbol(value[x]);
            if (terrain < 0) {
                return CORE_ERR_FORMAT;
            }
            grid->terrain[grid_index(grid, x, state->rows_read)] = (uint8_t)terrain;
        }
        state->rows_read++;
        return CORE_OK;
    }
    return CORE_ERR_FORMAT;
}

/* 섹션 이름 → 해석 함수 표 */
static const SectionDef SECTIONS[] = {
    {"meta", parse_meta, 0xFu},        /* name, difficulty, seed, keep */
    {"clock", parse_clock, 0x1u},      /* tick */
    {"rng", parse_rng, 0x3u},          /* state, inc */
    {"resources", parse_resources, 0x7Fu}, /* capacity + 자원 6개 */
    {"map", parse_map, 0x1u},          /* size (줄 수는 따로 확인) */
};
#define SECTION_COUNT ((int32_t)(sizeof(SECTIONS) / sizeof(SECTIONS[0])))

/* [end]: 모든 섹션과 필수 키가 있는지 확인합니다 */
static int finish_sections(const LoadState *state)
{
    int32_t i;
    for (i = 0; i < SECTION_COUNT; i++) {
        if (!(state->sections_seen & (1u << i))) {
            return CORE_ERR_FORMAT;
        }
        if ((state->keys_seen[i] & SECTIONS[i].required_keys) != SECTIONS[i].required_keys) {
            return CORE_ERR_FORMAT;
        }
    }
    if (state->rows_read != state->world->grid.height) {
        return CORE_ERR_FORMAT;
    }
    return CORE_OK;
}

/* "[이름]" 줄 처리 */
static int begin_section(LoadState *state, char *line)
{
    size_t length = strlen(line);
    int32_t i;

    if (length < 3 || line[length - 1] != ']') {
        return CORE_ERR_FORMAT;
    }
    line[length - 1] = '\0';
    if (strcmp(line + 1, "end") == 0) {
        int rc = finish_sections(state);
        if (rc == CORE_OK) {
            state->ended = 1;
        }
        return rc;
    }
    for (i = 0; i < SECTION_COUNT; i++) {
        if (strcmp(line + 1, SECTIONS[i].name) == 0) {
            if (state->sections_seen & (1u << i)) {
                return CORE_ERR_FORMAT; /* 같은 섹션이 두 번 */
            }
            state->sections_seen |= 1u << i;
            state->section = i;
            return CORE_OK;
        }
    }
    return CORE_ERR_FORMAT;
}

/* 첫 줄 "MAGICCASTLE_SAVE <버전>" 확인 */
static int check_header(const char *line)
{
    size_t magic_length = strlen(SAVE_MAGIC);
    int32_t version;
    if (strncmp(line, SAVE_MAGIC, magic_length) != 0 || line[magic_length] != ' ') {
        return CORE_ERR_FORMAT;
    }
    if (!parse_i32(line + magic_length + 1, 0, INT32_MAX, &version)) {
        return CORE_ERR_FORMAT;
    }
    return version == SAVE_VERSION ? CORE_OK : CORE_ERR_VERSION;
}

/* 파일 내용을 state->world 에 읽어 들입니다 */
static int read_world(LineReader *reader, LoadState *state)
{
    int rc = read_line(reader);
    if (rc != 1) {
        return rc == 0 ? CORE_ERR_FORMAT : rc; /* 빈 파일 */
    }
    rc = check_header(reader->buffer);
    if (rc != CORE_OK) {
        return rc;
    }
    while ((rc = read_line(reader)) == 1) {
        char *line = reader->buffer;
        char *equals;
        if (line[0] == '\0' || line[0] == '#') {
            continue; /* 빈 줄, 주석 */
        }
        if (state->ended) {
            return CORE_ERR_FORMAT; /* [end] 뒤에 내용이 있음 */
        }
        if (line[0] == '[') {
            rc = begin_section(state, line);
        } else {
            equals = strchr(line, '=');
            if (state->section < 0 || equals == NULL) {
                return CORE_ERR_FORMAT;
            }
            *equals = '\0';
            rc = SECTIONS[state->section].parse(state, line, equals + 1);
        }
        if (rc != CORE_OK) {
            return rc;
        }
    }
    if (rc < 0) {
        return rc;
    }
    return state->ended ? CORE_OK : CORE_ERR_FORMAT; /* [end] 가 없으면 잘린 파일 */
}

int save_read(const char *path, World **out_world, int32_t *out_line)
{
    LineReader reader;
    LoadState state;
    int rc;

    *out_world = NULL;
    *out_line = 0;

    memset(&reader, 0, sizeof(reader));
    reader.file = platform_fopen_utf8(path, "rb");
    if (reader.file == NULL) {
        return CORE_ERR_FILE_OPEN;
    }
    reader.capacity = SAVE_LINE_INITIAL;
    reader.buffer = (char *)mem_malloc(reader.capacity);

    memset(&state, 0, sizeof(state));
    state.section = -1;
    state.world = world_alloc();

    if (reader.buffer == NULL || state.world == NULL) {
        rc = CORE_ERR_OUT_OF_MEMORY;
    } else {
        rc = read_world(&reader, &state);
    }
    if (rc == CORE_OK) {
        /* 비용 격자는 저장하지 않고 지형에서 다시 계산합니다 */
        grid_rebuild_costs(&state.world->grid);
        if (validate_world(state.world, NULL, NULL) != 0) {
            rc = CORE_ERR_FORMAT;
        }
    }
    fclose(reader.file);
    mem_free(reader.buffer);

    if (rc != CORE_OK) {
        /* 반쯤 읽은 월드는 모두 해제합니다. 호출한 쪽의 기존 월드는 건드리지 않았습니다 */
        world_destroy(state.world);
        *out_line = reader.line_number;
        return rc;
    }
    *out_world = state.world;
    return CORE_OK;
}
