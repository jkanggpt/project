/* save.c — 현재 스키마 한 종류의 원자 저장·CRC32 검증·엄격한 필드 판독.
 * 포인터는 ID로 저장하고 모든 필수 레코드와 불변 조건을 검증한 뒤 연결합니다.
 * 저장 형식의 상세 계약은 docs/SAVE_FORMAT.md에 있습니다. */
#include "save.h"
#include "expansion.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "mem.h"
#include "placement.h"
#include "platform.h"
#include "research.h"
#include "text.h"
#include "validate.h"

#define SAVE_LINE_INITIAL 128
#define SAVE_LINE_MAX 65536 /* 이보다 긴 줄은 망가진 파일로 봅니다 (메모리를 무한히 쓰지 않게) */
#define SAVE_MAX_BUILDINGS 20000
#define SAVE_MAX_VILLAGERS 5000

#define BUILDING_FIELDS 15
#define VILLAGER_INT_FIELDS 35

/* 자원 키 이름 (순서는 CoreResource) */
static const char *const RESOURCE_KEYS[CORE_RES_COUNT] = {"gold",  "mana", "food",     "wood",     "stone",
                                                          "herbs", "logs", "clay",     "bricks",   "iron_ore",
                                                          "coal",  "iron", "gold_ore", "gold_bar", "leather"};

/* ====================================================================== */
/* 쓰기                                                                    */
/* ====================================================================== */

static void write_hex64(FILE *file, uint64_t value) {
    static const char DIGITS[] = "0123456789abcdef";
    char text[17];
    int i;
    for (i = 0; i < 16; i++) {
        text[15 - i] = DIGITS[(value >> (i * 4)) & 0xFu];
    }
    text[16] = '\0';
    fputs(text, file);
}

static void write_int_list(FILE *file, const int32_t *values, int32_t count) {
    int32_t i;
    for (i = 0; i < count; i++) {
        fprintf(file, i == 0 ? "%d" : ",%d", (int)values[i]);
    }
    fputc('\n', file);
}

static void write_meta(FILE *file, const World *world) {
    fputs("[meta]\n", file);
    fprintf(file, "name=%s\n", world->castle_name);
    fprintf(file, "difficulty=%d\n", (int)world->difficulty);
    fprintf(file, "seed=%d\n", (int)world->seed);
    fprintf(file, "keep=%d,%d\n", (int)world->keep_x, (int)world->keep_y);
    fprintf(file, "keep_id=%d\n", (int)world->keep_id);
    fprintf(file, "rank=%d\n", (int)world->rank);
    fprintf(file, "next_ids=%d,%d\n", (int)world->next_villager_id, (int)world->next_building_id);
    fprintf(file, "mourning=%d\n", (int)world->mourning);
    fprintf(file, "ruleset=%d\n", world->ruleset);
    fputs("[clock]\n", file);
    fprintf(file, "tick=%d\n", (int)world->clock.tick);
    fprintf(file, "cost_revision=%d\n", (int)world->grid.cost_rev);
    fputs("[rng]\nstate=", file);
    write_hex64(file, world->rng.state);
    fputs("\ninc=", file);
    write_hex64(file, world->rng.inc);
    fputs("\n", file);
}

static void write_economy(FILE *file, const World *world) {
    int32_t i;
    fputs("[resources]\n", file);
    fprintf(file, "capacity=%d\n", (int)world->bank.capacity);
    for (i = 0; i < CORE_RES_COUNT; i++) {
        fprintf(file, "%s=%d\n", RESOURCE_KEYS[i], (int)world->bank.amount[i]);
    }
    fputs("[ledger]\n", file);
    for (i = 0; i < CORE_RES_COUNT; i++) {
        fprintf(file, "today_%s=", RESOURCE_KEYS[i]);
        write_int_list(file, world->ledger.today[i], CORE_LEDGER_COUNT);
        fprintf(file, "yesterday_%s=", RESOURCE_KEYS[i]);
        write_int_list(file, world->ledger.yesterday[i], CORE_LEDGER_COUNT);
    }
}

static void write_map(FILE *file, const World *world) {
    const Grid *grid = &world->grid;
    int32_t x;
    int32_t y;
    fputs("[map]\n", file);
    fprintf(file, "size=%d,%d\n", (int)grid->width, (int)grid->height);
    for (y = 0; y < grid->height; y++) {
        fputs("row=", file);
        for (x = 0; x < grid->width; x++) {
            fputc(grid_terrain_symbol(grid->terrain[grid_index(grid, x, y)]), file);
        }
        fputc('\n', file);
    }
    for (y = 0; y < grid->height; y++) {
        fputs("road=", file);
        for (x = 0; x < grid->width; x++) {
            fputc('0' + grid->road[grid_index(grid, x, y)], file);
        }
        fputc('\n', file);
    }
}

static void write_buildings(FILE *file, const World *world) {
    int32_t i;
    fputs("[buildings]\n", file);
    for (i = 0; i < world->building_count; i++) {
        const Building *b = world->buildings[i];
        int32_t values[BUILDING_FIELDS];
        values[0] = b->id;
        values[1] = b->type;
        values[2] = b->x;
        values[3] = b->y;
        values[4] = b->w;
        values[5] = b->h;
        values[6] = b->state;
        values[7] = b->hp;
        values[8] = b->build_progress;
        values[9] = b->door_x;
        values[10] = b->door_y;
        values[11] = b->auto_assign;
        values[12] = b->work_milli[0];
        values[13] = b->work_milli[1];
        values[14] = b->flags;
        fputs("building=", file);
        write_int_list(file, values, BUILDING_FIELDS);
    }
}

static const char *trait_key(const World *world, int32_t trait) {
    return (trait >= 0 && trait < world->defs.trait_count) ? world->defs.traits[trait].key : "-";
}

static void write_villagers(FILE *file, const World *world) {
    int32_t i;
    int32_t k;
    fputs("[villagers]\n", file);
    for (i = 0; i < world->villager_count; i++) {
        const Villager *v = world->villagers[i];
        int32_t values[VILLAGER_INT_FIELDS];
        values[0] = v->id;
        values[1] = v->age;
        values[2] = v->stats.strength;
        values[3] = v->stats.intellect;
        values[4] = v->stats.dexterity;
        values[5] = v->stats.vitality;
        values[6] = v->needs.hunger;
        values[7] = v->needs.rest;
        values[8] = v->needs.safety;
        values[9] = v->needs.housing;
        values[10] = v->mood;
        values[11] = v->health;
        values[12] = v->flags;
        values[13] = v->home != NULL ? v->home->id : 0;
        values[14] = v->workplace != NULL ? v->workplace->id : 0;
        values[15] = v->state;
        values[16] = v->intent_rev;
        values[17] = v->intent_flags;
        values[18] = v->target != NULL ? v->target->id : 0;
        values[19] = v->target_x;
        values[20] = v->target_y;
        values[21] = v->at_target;
        values[22] = v->loc_x;
        values[23] = v->loc_y;
        values[24] = v->state_ticks;
        values[25] = v->hold_ticks;
        values[26] = v->low_mood_ticks;
        values[27] = v->hunger_acc;
        values[28] = v->rest_acc;
        values[29] = v->story_flags;
        values[30] = v->blocked_day;
        values[31] = v->sick_ticks;
        values[32] = v->heal_ticks;
        values[33] = v->sex;
        values[34] = v->appearance;
        fputs("villager=", file);
        for (k = 0; k < VILLAGER_INT_FIELDS; k++) {
            fprintf(file, "%d,", (int)values[k]);
        }
        fprintf(file, "%s,%s,%s\n", trait_key(world, v->traits[0]), trait_key(world, v->traits[1]), v->name);
    }
}

static void write_pressure(FILE *file, const World *world) {
    const Nature *nature = &world->nature;
    int32_t i;
    fputs("[nature]\n", file);
    fprintf(file, "weather=%d\nevent=%d\nevent_days=%d\n", (int)nature->weather, (int)nature->event,
            (int)nature->event_days);
    fprintf(file, "forecast=%d\nforecast_days=%d\ncalm_days=%d\ncold_houses=%d\n", (int)nature->forecast,
            (int)nature->forecast_days, (int)nature->calm_days, (int)nature->cold_houses);
    fputs("[market]\nprices=", file);
    write_int_list(file, world->market.price, CORE_RES_COUNT);
    fputs("[merchant]\n", file);
    fprintf(file, "days_left=%d\nnext_visit=%d\n", (int)world->merchant.days_left,
            (int)world->merchant.next_visit_day);
    for (i = 0; i < world->merchant.deal_count; i++) {
        const MerchantDeal *deal = &world->merchant.deals[i];
        int32_t values[5];
        values[0] = deal->give_resource;
        values[1] = deal->give_amount;
        values[2] = deal->get_resource;
        values[3] = deal->get_amount;
        values[4] = deal->taken;
        fputs("deal=", file);
        write_int_list(file, values, 5);
    }
    fputs("[threat]\n", file);
    fprintf(file, "next_raid=%d\nraids_done=%d\n", (int)world->threat.next_raid_id,
            (int)world->threat.raids_done);
    for (i = 0; i < world->threat.threat_count; i++) {
        const Threat *threat = &world->threat.threats[i];
        int32_t values[5];
        values[0] = threat->type;
        values[1] = threat->direction;
        values[2] = threat->eta_tick;
        values[3] = threat->strength;
        values[4] = threat->near_warned;
        fputs("threat=", file);
        write_int_list(file, values, 5);
    }
}

static void write_research(FILE *file, const World *world) {
    const ResearchState *research = &world->research;
    int32_t written = 0;
    int32_t i;
    fputs("[research]\ndone=", file);
    for (i = 0; i < world->defs.research_count && i < 32; i++) {
        if (research->done & (1u << i)) {
            fprintf(file, written == 0 ? "%s" : ",%s", world->defs.research[i].key);
            written++;
        }
    }
    fputs(written == 0 ? "none\n" : "\n", file);
    fprintf(file, "active=%s\n", research->active >= 0 ? world->defs.research[research->active].key : "none");
    fprintf(file, "progress=%d\n", (int)research->progress_milli);
}

static void write_progress(FILE *file, const World *world) {
    int32_t stats[6];
    int32_t tutorial[2];
    fputs("[progress]\n", file);
    fprintf(file, "rank_ready=%d\noutcome=%d\nlose_reason=%d\n", (int)world->rank_ready_announced,
            (int)world->outcome, (int)world->lose_reason);
    tutorial[0] = world->tutorial_done;
    tutorial[1] = world->tutorial_flags;
    fputs("tutorial=", file);
    write_int_list(file, tutorial, 2);
    {
        int32_t guide[3] = {world->guide.active, world->guide.step, world->guide.flags};
        fputs("guide=", file); /* 튜토리얼 안내: 켜짐, 단계, 예약 (스키마 14) */
        write_int_list(file, guide, 3);
    }
    stats[0] = world->stats.peak_population;
    stats[1] = world->stats.enemies_killed;
    stats[2] = world->stats.villagers_died;
    stats[3] = world->stats.villagers_left;
    stats[4] = world->stats.buildings_built;
    stats[5] = world->stats.victory_day;
    fputs("stats=", file);
    write_int_list(file, stats, 6);
}

#define SAVE_CHECKSUM_MAGIC "MAGICCASTLE_CHECKSUM"
#define SAVE_CHECKSUM_BYTES ((long)(sizeof(SAVE_CHECKSUM_MAGIC) - 1 + 1 + 20 + 1 + 8 + 1))
static int payload_crc(FILE *file, long length, uint32_t *out) {
    unsigned char buffer[4096];
    uint32_t crc = UINT32_MAX;
    while (length > 0) {
        size_t want = length > (long)sizeof(buffer) ? sizeof(buffer) : (size_t)length;
        if (fread(buffer, 1, want, file) != want)
            return 0;
        for (size_t i = 0; i < want; i++) {
            crc ^= buffer[i];
            for (int bit = 0; bit < 8; bit++)
                crc = (crc >> 1) ^ ((0u - (crc & 1u)) & UINT32_C(0xedb88320));
        }
        length -= (long)want;
    }
    *out = ~crc;
    return 1;
}
static int append_checksum(FILE *file) {
    uint32_t crc;
    long size;
    if (fflush(file) != 0 || (size = ftell(file)) < 0 || fseek(file, 0, SEEK_SET) != 0 ||
        !payload_crc(file, size, &crc) || fseek(file, 0, SEEK_END) != 0)
        return CORE_ERR_FILE_IO;
    if (fprintf(file, "%s %020llu %08x\n", SAVE_CHECKSUM_MAGIC, (unsigned long long)size, (unsigned)crc) <
            0 ||
        fflush(file) != 0)
        return CORE_ERR_FILE_IO;
    return CORE_OK;
}
static int verify_checksum(FILE *file) {
    long cursor = ftell(file), end;
    char footer[80], *last;
    uint32_t actual;
    unsigned long long declared;
    unsigned long expected;
    if (cursor < 0 || fseek(file, 0, SEEK_END) != 0 || (end = ftell(file)) < SAVE_CHECKSUM_BYTES ||
        fseek(file, end - SAVE_CHECKSUM_BYTES, SEEK_SET) != 0)
        return 0;
    if (fread(footer, 1, (size_t)SAVE_CHECKSUM_BYTES, file) != (size_t)SAVE_CHECKSUM_BYTES)
        return 0;
    footer[SAVE_CHECKSUM_BYTES] = 0;
    size_t prefix = sizeof(SAVE_CHECKSUM_MAGIC) - 1;
    if (memcmp(footer, SAVE_CHECKSUM_MAGIC, prefix) != 0 || footer[prefix] != ' ' ||
        footer[prefix + 21] != ' ' || footer[SAVE_CHECKSUM_BYTES - 1] != '\n')
        return 0;
    for (size_t i = prefix + 1; i < prefix + 21; i++)
        if (footer[i] < '0' || footer[i] > '9')
            return 0;
    errno = 0;
    declared = strtoull(footer + prefix + 1, &last, 10);
    if (errno || last != footer + prefix + 21 || declared != (unsigned long long)(end - SAVE_CHECKSUM_BYTES))
        return 0;
    errno = 0;
    expected = strtoul(last + 1, &last, 16);
    if (errno || expected > UINT32_MAX || last != footer + SAVE_CHECKSUM_BYTES - 1)
        return 0;
    if (fseek(file, 0, SEEK_SET) != 0 || !payload_crc(file, end - SAVE_CHECKSUM_BYTES, &actual) ||
        actual != (uint32_t)expected || fseek(file, cursor, SEEK_SET) != 0)
        return 0;
    return 1;
}

static int write_world(FILE *file, const World *world) {
    fprintf(file, "%s %d\n", SAVE_MAGIC, SAVE_VERSION);
    fputs(u8"# 마법성채 키우기 저장 파일입니다. 직접 고치면 불러오지 못할 수 있습니다.\n", file);
    write_meta(file, world);
    write_economy(file, world);
    write_map(file, world);
    write_research(file, world);
    write_buildings(file, world);
    write_villagers(file, world);
    write_pressure(file, world);
    write_progress(file, world);
    (void)expansion_save(file, world);
    fputs("[end]\n", file);
    return ferror(file) ? CORE_ERR_FILE_IO : CORE_OK;
}

/* base 뒤에 suffix 를 붙인 새 문자열 (호출한 쪽이 mem_free) */
static char *path_with_suffix(const char *base, const char *suffix) {
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

int save_write(const World *world, const char *path) {
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
    file = platform_fopen_utf8(tmp_path, "w+b");
    if (file == NULL) {
        rc = CORE_ERR_FILE_OPEN;
    } else {
        rc = write_world(file, world);
        if (rc == CORE_OK)
            rc = append_checksum(file);
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
static int read_line(LineReader *reader) {
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

/* 주민의 연결은 모든 건물을 읽은 뒤에 이어야 하므로 id 를 잠시 적어 둡니다 */
typedef struct PendingLinks {
    int32_t home;
    int32_t workplace;
    int32_t target;
} PendingLinks;

/* 불러오는 중인 상태 */
typedef struct LoadState {
    World *world;
    int32_t section;        /* 지금 섹션 (SECTIONS 의 번호), -1 = 아직 없음 */
    uint32_t sections_seen; /* 섹션별 비트 */
    uint32_t keys_seen[16]; /* 섹션마다 읽은 키 비트 */
    int32_t rows_read;
    int32_t road_rows_read;
    int32_t cost_revision;
    int ended;           /* [end] 를 읽었으면 1 */
    PendingLinks *links; /* 주민마다 하나 (주민 목록과 같은 순서), 동적 배열 */
    int32_t link_capacity;
} LoadState;

typedef int (*SectionParser)(LoadState *state, const char *key, char *value);

typedef struct SectionDef {
    const char *name;
    SectionParser parse;
    uint32_t required_keys; /* 반드시 있어야 하는 키 비트 */
    uint32_t requires;      /* 먼저 읽혀 있어야 하는 섹션 비트 */
} SectionDef;

/* 이 키를 처음 읽었으면 1, 같은 키가 두 번 나왔으면 0 */
static int mark_key(LoadState *state, int bit) {
    uint32_t mask = 1u << bit;
    if (state->keys_seen[state->section] & mask) {
        return 0;
    }
    state->keys_seen[state->section] |= mask;
    return 1;
}

/* 10진 정수 하나. 앞뒤 공백, 범위 밖, 숫자 아닌 글자를 모두 거부합니다 */
static int parse_i32(const char *text, int32_t min, int32_t max, int32_t *out) {
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
static int parse_pair(const char *text, int32_t min, int32_t max, int32_t *out_a, int32_t *out_b) {
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

/* 쉼표로 나뉜 정수 count 개를 읽습니다. 성공하면 마지막 쉼표 뒤(남은 글자)의 위치, 실패하면 NULL.
 * exact 가 1 이면 남은 글자가 없어야 합니다. text 를 직접 고칩니다(쉼표 → '\0') */
static char *parse_int_list(char *text, int32_t *out, int32_t count, int exact) {
    char *cursor = text;
    int32_t i;
    for (i = 0; i < count; i++) {
        char *comma = strchr(cursor, ',');
        int last = (i == count - 1);
        if (comma != NULL && !(last && exact)) {
            *comma = '\0';
        } else if (!(last && exact) || comma != NULL) {
            return NULL;
        }
        if (!parse_i32(cursor, INT32_MIN + 1, INT32_MAX, &out[i])) {
            return NULL;
        }
        cursor = (last && exact) ? cursor + strlen(cursor) : comma + 1;
    }
    return cursor;
}

/* 정확히 16자리 16진수 */
static int parse_hex64(const char *text, uint64_t *out) {
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

static int parse_meta(LoadState *state, const char *key, char *value) {
    World *world = state->world;
    if (strcmp(key, "ruleset") == 0 && mark_key(state, 8)) {
        if (!parse_i32(value, 1, 1, &world->ruleset))
            return CORE_ERR_FORMAT;
        return expansion_allocate(world);
    }
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
    if (strcmp(key, "keep_id") == 0 && mark_key(state, 4)) {
        return parse_i32(value, 1, INT32_MAX - 1, &world->keep_id) ? CORE_OK : CORE_ERR_FORMAT;
    }
    if (strcmp(key, "rank") == 0 && mark_key(state, 5)) {
        return parse_i32(value, 0, 5, &world->rank) ? CORE_OK : CORE_ERR_FORMAT;
    }
    if (strcmp(key, "next_ids") == 0 && mark_key(state, 6)) {
        return parse_pair(value, 1, INT32_MAX - 1, &world->next_villager_id, &world->next_building_id)
                   ? CORE_OK
                   : CORE_ERR_FORMAT;
    }
    if (strcmp(key, "mourning") == 0 && mark_key(state, 7)) {
        return parse_i32(value, 0, 30, &world->mourning) ? CORE_OK : CORE_ERR_FORMAT;
    }
    return CORE_ERR_FORMAT; /* 모르는 키이거나 같은 키가 두 번 */
}

static int parse_clock(LoadState *state, const char *key, char *value) {
    if (strcmp(key, "tick") == 0 && mark_key(state, 0)) {
        return parse_i32(value, 0, CLOCK_MAX_TICK, &state->world->clock.tick) ? CORE_OK : CORE_ERR_FORMAT;
    }
    if (strcmp(key, "cost_revision") == 0 && mark_key(state, 1))
        return parse_i32(value, 0, INT32_MAX - 64, &state->cost_revision) ? CORE_OK : CORE_ERR_FORMAT;
    return CORE_ERR_FORMAT;
}

static int parse_rng(LoadState *state, const char *key, char *value) {
    if (strcmp(key, "state") == 0 && mark_key(state, 0)) {
        return parse_hex64(value, &state->world->rng.state) ? CORE_OK : CORE_ERR_FORMAT;
    }
    if (strcmp(key, "inc") == 0 && mark_key(state, 1)) {
        return parse_hex64(value, &state->world->rng.inc) ? CORE_OK : CORE_ERR_FORMAT;
    }
    return CORE_ERR_FORMAT;
}

static int parse_resources(LoadState *state, const char *key, char *value) {
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

static int parse_ledger(LoadState *state, const char *key, char *value) {
    Ledger *ledger = &state->world->ledger;
    int i;
    for (i = 0; i < CORE_RES_COUNT; i++) {
        char expected[32];
        int32_t *target = NULL;
        int bit = -1;
        snprintf(expected, sizeof(expected), "today_%s", RESOURCE_KEYS[i]);
        if (strcmp(key, expected) == 0) {
            target = ledger->today[i];
            bit = i * 2;
        }
        snprintf(expected, sizeof(expected), "yesterday_%s", RESOURCE_KEYS[i]);
        if (strcmp(key, expected) == 0) {
            target = ledger->yesterday[i];
            bit = i * 2 + 1;
        }
        if (target != NULL) {
            if (!mark_key(state, bit) || parse_int_list(value, target, CORE_LEDGER_COUNT, 1) == NULL) {
                return CORE_ERR_FORMAT;
            }
            return CORE_OK;
        }
    }
    return CORE_ERR_FORMAT;
}

static int parse_map(LoadState *state, const char *key, char *value) {
    Grid *grid = &state->world->grid;
    int32_t x;

    if (strcmp(key, "size") == 0 && mark_key(state, 0)) {
        int32_t width;
        int32_t height;
        if (!parse_pair(value, GRID_MIN_SIZE, GRID_MAX_SIZE, &width, &height)) {
            return CORE_ERR_FORMAT;
        }
        if (grid_alloc(grid, width, height) != CORE_OK)
            return CORE_ERR_OUT_OF_MEMORY;
        return state->world->expansion ? expansion_resize(state->world) : CORE_OK;
    }
    if (grid->terrain == NULL || (int32_t)strlen(value) != grid->width) {
        return CORE_ERR_FORMAT; /* size 보다 먼저 나왔거나 길이가 틀림 */
    }
    if (strcmp(key, "row") == 0) {
        if (state->rows_read >= grid->height) {
            return CORE_ERR_FORMAT;
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
    if (strcmp(key, "road") == 0) {
        if (state->road_rows_read >= grid->height) {
            return CORE_ERR_FORMAT;
        }
        for (x = 0; x < grid->width; x++) {
            if (value[x] < '0' || value[x] > (state->world->expansion ? '4' : '1')) {
                return CORE_ERR_FORMAT;
            }
            grid->road[grid_index(grid, x, state->road_rows_read)] = (uint8_t)(value[x] - '0');
        }
        state->road_rows_read++;
        return CORE_OK;
    }
    return CORE_ERR_FORMAT;
}

static int in_bounds_or_none(const Grid *grid, int32_t x, int32_t y) {
    return (x == -1 && y == -1) || grid_in_bounds(grid, x, y);
}

static int parse_building(LoadState *state, const char *key, char *value) {
    World *world = state->world;
    int32_t v[BUILDING_FIELDS];
    Building *building;
    const BuildingDef *def;

    if (strcmp(key, "building") != 0 || parse_int_list(value, v, BUILDING_FIELDS, 1) == NULL) {
        return CORE_ERR_FORMAT;
    }
    def = world_building_def(world, v[1]);
    if (def == NULL || v[1] == CORE_BLD_ROAD || v[0] < 1 || v[0] >= world->next_building_id ||
        entity_find_building(world, v[0]) != NULL || world->building_count >= SAVE_MAX_BUILDINGS) {
        return CORE_ERR_FORMAT; /* 종류 오류, id 범위 밖, id 중복, 너무 많음 */
    }
    if (v[4] < 1 || v[4] > 8 || v[5] < 1 || v[5] > 8 || !grid_in_bounds(&world->grid, v[2], v[3]) ||
        !grid_in_bounds(&world->grid, v[2] + v[4] - 1, v[3] + v[5] - 1) || v[6] < CORE_BSTATE_CONSTRUCTING ||
        v[6] > CORE_BSTATE_RUINED || v[7] < 0 || v[7] > def->max_hp || v[8] < 0 ||
        !in_bounds_or_none(&world->grid, v[9], v[10]) || (v[11] != 0 && v[11] != 1) || v[12] < 0 ||
        v[13] < 0 || v[12] > 1000000 || v[13] > 1000000 || v[14] < 0 || v[14] > CORE_BF_COLD) {
        return CORE_ERR_FORMAT;
    }
    building = entity_add_building(world, v[1]);
    if (building == NULL) {
        return CORE_ERR_OUT_OF_MEMORY;
    }
    world->next_building_id--; /* entity_add_building 이 올린 카운터를 되돌리고 저장된 id 를 씀 */
    building->id = v[0];
    building->x = v[2];
    building->y = v[3];
    building->w = v[4];
    building->h = v[5];
    building->state = v[6];
    building->hp = v[7];
    building->build_progress = v[8];
    building->door_x = v[9];
    building->door_y = v[10];
    building->auto_assign = v[11];
    building->work_milli[0] = v[12];
    building->work_milli[1] = v[13];
    building->flags = v[14];
    return CORE_OK;
}

static int reserve_links(LoadState *state, int32_t needed) {
    PendingLinks *grown;
    int32_t new_capacity;
    if (needed <= state->link_capacity) {
        return CORE_OK;
    }
    new_capacity = state->link_capacity > 0 ? state->link_capacity * 2 : 16;
    grown = (PendingLinks *)mem_realloc(state->links, (size_t)new_capacity * sizeof(PendingLinks));
    if (grown == NULL) {
        return CORE_ERR_OUT_OF_MEMORY;
    }
    state->links = grown;
    state->link_capacity = new_capacity;
    return CORE_OK;
}

/* 특성 키 → 번호. "-" 는 없음. 정의 파일이 바뀌어 모르는 특성이 되면 그냥 없음으로 둡니다 */
static int32_t trait_from_key(const World *world, const char *key) {
    if (strcmp(key, "-") == 0) {
        return -1;
    }
    return defs_find_trait(&world->defs, key);
}

static int parse_villager(LoadState *state, const char *key, char *value) {
    World *world = state->world;
    const Grid *grid = &world->grid;
    int32_t v[VILLAGER_INT_FIELDS];
    char *rest;
    char *comma;
    char *trait_b;
    char *name;
    Villager *villager;
    int rc;

    if (strcmp(key, "villager") != 0) {
        return CORE_ERR_FORMAT;
    }
    rest = parse_int_list(value, v, VILLAGER_INT_FIELDS, 0);
    if (rest == NULL) {
        return CORE_ERR_FORMAT;
    }
    /* 특성1,특성2,이름 */
    comma = strchr(rest, ',');
    if (comma == NULL) {
        return CORE_ERR_FORMAT;
    }
    *comma = '\0';
    trait_b = comma + 1;
    comma = strchr(trait_b, ',');
    if (comma == NULL) {
        return CORE_ERR_FORMAT;
    }
    *comma = '\0';
    name = comma + 1;

    if (v[0] < 1 || v[0] >= world->next_villager_id || entity_find_villager(world, v[0]) != NULL ||
        world->villager_count >= SAVE_MAX_VILLAGERS || !text_is_valid_name(name, CORE_NAME_BYTES) ||
        v[1] < 0 || v[1] > 200 || v[2] < 1 || v[2] > 10 || v[3] < 1 || v[3] > 10 || v[4] < 1 || v[4] > 10 ||
        v[5] < 1 || v[5] > 10 || v[6] < 0 || v[6] > 100 || v[7] < 0 || v[7] > 100 || v[8] < 0 || v[8] > 100 ||
        v[9] < 0 || v[9] > 100 || v[10] < (world->expansion ? -200 : 0) ||
        v[10] > (world->expansion ? 200 : 100) || v[11] < 1 || v[11] > 100 || v[12] < 0 || v[12] > 255 ||
        v[13] < 0 || v[14] < 0 || v[15] < 0 || v[15] >= CORE_VS_COUNT || v[16] < 0 || v[17] < 0 ||
        v[17] > 3 || v[18] < 0 || !grid_in_bounds(grid, v[19], v[20]) || (v[21] != 0 && v[21] != 1) ||
        !grid_in_bounds(grid, v[22], v[23]) || v[24] < 0 || v[25] < 0 || v[26] < 0 || v[27] < 0 ||
        v[27] >= 1000 || v[28] < 0 || v[28] >= 1000 || v[29] < 0 || v[30] < -1 || v[31] < 0 ||
        v[31] > NATURE_SICK_TICKS * 2 || v[32] < 0 || v[32] > NATURE_HEAL_TICKS) {
        return CORE_ERR_FORMAT;
    }
    rc = reserve_links(state, world->villager_count + 1);
    if (rc != CORE_OK) {
        return rc;
    }
    villager = entity_add_villager(world);
    if (villager == NULL) {
        return CORE_ERR_OUT_OF_MEMORY;
    }
    world->next_villager_id--; /* 저장된 id 를 씀 */
    villager->id = v[0];
    strcpy(villager->name, name); /* 길이는 text_is_valid_name 으로 확인함 */
    villager->age = v[1];
    villager->stats.strength = v[2];
    villager->stats.intellect = v[3];
    villager->stats.dexterity = v[4];
    villager->stats.vitality = v[5];
    villager->needs.hunger = v[6];
    villager->needs.rest = v[7];
    villager->needs.safety = v[8];
    villager->needs.housing = v[9];
    villager->mood = v[10];
    villager->health = v[11];
    villager->flags = v[12];
    villager->state = v[15];
    villager->intent_rev = v[16];
    villager->intent_flags = v[17];
    villager->target_x = v[19];
    villager->target_y = v[20];
    villager->at_target = v[21];
    villager->loc_x = v[22];
    villager->loc_y = v[23];
    villager->state_ticks = v[24];
    villager->hold_ticks = v[25];
    villager->low_mood_ticks = v[26];
    villager->hunger_acc = v[27];
    villager->rest_acc = v[28];
    villager->story_flags = v[29];
    villager->blocked_day = v[30];
    villager->sick_ticks = v[31];
    villager->heal_ticks = v[32];
    if (v[33] < 0 || v[33] > 1 || v[34] < 0 || v[34] > 47 || (v[34] % 2) != v[33])
        return CORE_ERR_FORMAT;
    villager->sex = v[33];
    villager->appearance = v[34];
    villager->traits[0] = trait_from_key(world, rest);
    villager->traits[1] = trait_from_key(world, trait_b);
    /* 연결은 모든 건물을 읽은 뒤 [end] 에서 */
    state->links[world->villager_count - 1].home = v[13];
    state->links[world->villager_count - 1].workplace = v[14];
    state->links[world->villager_count - 1].target = v[18];
    return CORE_OK;
}

/* [nature]: 키 7개 */
static int parse_nature(LoadState *state, const char *key, char *value) {
    static const char *const KEYS[7] = {"weather",       "event",     "event_days", "forecast",
                                        "forecast_days", "calm_days", "cold_houses"};
    static const int32_t MAX[7] = {
        CORE_WEATHER_COUNT - 1, CORE_NATURE_COUNT - 1, 30, CORE_NATURE_COUNT - 1, 30,
        CLOCK_MAX_DAYS,         SAVE_MAX_BUILDINGS};
    Nature *nature = &state->world->nature;
    int32_t *fields[7];
    int i;
    fields[0] = &nature->weather;
    fields[1] = &nature->event;
    fields[2] = &nature->event_days;
    fields[3] = &nature->forecast;
    fields[4] = &nature->forecast_days;
    fields[5] = &nature->calm_days;
    fields[6] = &nature->cold_houses;
    for (i = 0; i < 7; i++) {
        if (strcmp(key, KEYS[i]) == 0 && mark_key(state, i)) {
            return parse_i32(value, 0, MAX[i], fields[i]) ? CORE_OK : CORE_ERR_FORMAT;
        }
    }
    return CORE_ERR_FORMAT;
}

static int parse_market(LoadState *state, const char *key, char *value) {
    if (strcmp(key, "prices") == 0 && mark_key(state, 0)) {
        return parse_int_list(value, state->world->market.price, CORE_RES_COUNT, 1) != NULL ? CORE_OK
                                                                                            : CORE_ERR_FORMAT;
    }
    return CORE_ERR_FORMAT; /* 가격의 범위는 불변 조건 검사기가 확인 */
}

static int parse_merchant(LoadState *state, const char *key, char *value) {
    Merchant *merchant = &state->world->merchant;
    if (strcmp(key, "days_left") == 0 && mark_key(state, 0)) {
        return parse_i32(value, 0, MERCHANT_STAY_DAYS, &merchant->days_left) ? CORE_OK : CORE_ERR_FORMAT;
    }
    if (strcmp(key, "next_visit") == 0 && mark_key(state, 1)) {
        return parse_i32(value, 0, CLOCK_MAX_DAYS, &merchant->next_visit_day) ? CORE_OK : CORE_ERR_FORMAT;
    }
    if (strcmp(key, "deal") == 0 && merchant->deal_count < CORE_MERCHANT_MAX_DEALS) {
        int32_t v[5];
        MerchantDeal *deal;
        if (parse_int_list(value, v, 5, 1) == NULL || v[0] < 0 || v[0] >= CORE_RES_COUNT || v[1] < 1 ||
            v[1] > 100000 || v[2] < 0 || v[2] >= CORE_RES_COUNT || v[3] < 1 || v[3] > 100000 || v[4] < 0 ||
            v[4] > 1) {
            return CORE_ERR_FORMAT;
        }
        deal = &merchant->deals[merchant->deal_count++];
        deal->give_resource = v[0];
        deal->give_amount = v[1];
        deal->get_resource = v[2];
        deal->get_amount = v[3];
        deal->taken = v[4];
        return CORE_OK;
    }
    return CORE_ERR_FORMAT;
}

static int parse_threat(LoadState *state, const char *key, char *value) {
    ThreatState *threat = &state->world->threat;
    if (strcmp(key, "next_raid") == 0 && mark_key(state, 0)) {
        return parse_i32(value, 1, INT32_MAX - 1, &threat->next_raid_id) ? CORE_OK : CORE_ERR_FORMAT;
    }
    if (strcmp(key, "raids_done") == 0 && mark_key(state, 1)) {
        return parse_i32(value, 0, INT32_MAX - 1, &threat->raids_done) ? CORE_OK : CORE_ERR_FORMAT;
    }
    if (strcmp(key, "threat") == 0 && threat->threat_count < CORE_MAX_THREATS) {
        int32_t v[5];
        Threat *entry;
        if (parse_int_list(value, v, 5, 1) == NULL || v[0] <= CORE_THREAT_NONE || v[0] >= CORE_THREAT_COUNT ||
            v[1] < 0 || v[1] >= CORE_DIR_COUNT || v[2] < 0 || v[2] > CLOCK_MAX_TICK || v[3] < 1 ||
            v[3] > THREAT_MAX_ENEMIES || v[4] < 0 || v[4] > 1) {
            return CORE_ERR_FORMAT;
        }
        entry = &threat->threats[threat->threat_count++];
        entry->type = v[0];
        entry->direction = v[1];
        entry->eta_tick = v[2];
        entry->strength = v[3];
        entry->near_warned = v[4];
        return CORE_OK;
    }
    return CORE_ERR_FORMAT;
}

/* done=키,키,… : 아는 키만 끝낸 것으로 표시하고(정의 파일에서 빠진 연구는 건너뜀) 효과를 바로 다시 계산합니다
 */
static int parse_research_done(World *world, char *value) {
    char *cursor = value;
    if (strcmp(value, "none") != 0) {
        while (cursor != NULL && *cursor != '\0') {
            char *comma = strchr(cursor, ',');
            int32_t index;
            if (comma != NULL) {
                *comma = '\0';
            }
            index = defs_find_research(&world->defs, cursor);
            if (index >= 0 && index < 32) {
                world->research.done |= 1u << index;
            }
            cursor = comma != NULL ? comma + 1 : NULL;
        }
    }
    research_reapply(world);
    return CORE_OK;
}

static int parse_research(LoadState *state, const char *key, char *value) {
    World *world = state->world;
    if (strcmp(key, "done") == 0 && mark_key(state, 0)) {
        return parse_research_done(world, value);
    }
    if (strcmp(key, "active") == 0 && mark_key(state, 1)) {
        world->research.active = strcmp(value, "none") == 0 ? -1 : defs_find_research(&world->defs, value);
        return CORE_OK; /* 정의 파일에서 빠진 연구였다면 연구 중인 것이 없는 것으로 */
    }
    if (strcmp(key, "progress") == 0 && mark_key(state, 2)) {
        return parse_i32(value, 0, INT32_MAX - 1, &world->research.progress_milli) ? CORE_OK
                                                                                   : CORE_ERR_FORMAT;
    }
    return CORE_ERR_FORMAT;
}

static int parse_progress(LoadState *state, const char *key, char *value) {
    World *world = state->world;
    if (strcmp(key, "rank_ready") == 0 && mark_key(state, 0)) {
        return parse_i32(value, 0, CORE_RANK_COUNT, &world->rank_ready_announced) ? CORE_OK : CORE_ERR_FORMAT;
    }
    if (strcmp(key, "outcome") == 0 && mark_key(state, 1)) {
        return parse_i32(value, CORE_OUTCOME_NONE, CORE_OUTCOME_LOST, &world->outcome) ? CORE_OK
                                                                                       : CORE_ERR_FORMAT;
    }
    if (strcmp(key, "lose_reason") == 0 && mark_key(state, 2)) {
        return parse_i32(value, CORE_LOSE_NONE, CORE_LOSE_POPULATION, &world->lose_reason) ? CORE_OK
                                                                                           : CORE_ERR_FORMAT;
    }
    if (strcmp(key, "tutorial") == 0 && mark_key(state, 3)) {
        int32_t v[2];
        if (parse_int_list(value, v, 2, 1) == NULL || v[0] < 0 || v[0] >= (1 << CORE_TUTORIAL_COUNT) ||
            v[1] < 0 || v[1] > 1) {
            return CORE_ERR_FORMAT;
        }
        world->tutorial_done = v[0];
        world->tutorial_flags = v[1];
        return CORE_OK;
    }
    if (strcmp(key, "guide") == 0 && mark_key(state, 5)) {
        int32_t v[3];
        if (parse_int_list(value, v, 3, 1) == NULL || v[0] < 0 || v[0] > 1 || v[1] < 0 ||
            v[1] > GUIDE_STEP_COUNT || v[2] != 0 || (!v[0] && v[1])) {
            return CORE_ERR_FORMAT;
        }
        memset(&world->guide, 0, sizeof(world->guide));
        world->guide.active = v[0];
        world->guide.step = v[1];
        world->guide.flags = v[2];
        return CORE_OK;
    }
    if (strcmp(key, "stats") == 0 && mark_key(state, 4)) {
        int32_t v[6];
        int32_t i;
        if (parse_int_list(value, v, 6, 1) == NULL) {
            return CORE_ERR_FORMAT;
        }
        for (i = 0; i < 6; i++) {
            if (v[i] < 0) {
                return CORE_ERR_FORMAT;
            }
        }
        world->stats.peak_population = v[0];
        world->stats.enemies_killed = v[1];
        world->stats.villagers_died = v[2];
        world->stats.villagers_left = v[3];
        world->stats.buildings_built = v[4];
        world->stats.victory_day = v[5];
        return CORE_OK;
    }
    return CORE_ERR_FORMAT;
}

#define SEC_META 0
#define SEC_MAP 5
#define SEC_RESEARCH 12

static int parse_expansion(LoadState *state, const char *key, char *value) {
    return expansion_load_line(state->world, key, value);
}

/* 섹션 이름 → 해석 함수 표 */
static const SectionDef SECTIONS[] = {
    {"meta", parse_meta, 0x1FFu, 0},            /* 필수 키 9개 */
    {"clock", parse_clock, 0x3u, 0},            /* tick */
    {"rng", parse_rng, 0x3u, 0},                /* state, inc */
    {"resources", parse_resources, 0xFFFFu, 0}, /* capacity + 자원 15개 */
    {"ledger", parse_ledger, 0x3FFFFFFFu, 0},   /* 오늘·어제 × 자원 15개 */
    {"map", parse_map, 0x1u, 0},                /* size (줄 수는 따로 확인) */
    {"buildings", parse_building, 0u, (1u << SEC_META) | (1u << SEC_MAP) | (1u << SEC_RESEARCH)},
    {"villagers", parse_villager, 0u, (1u << SEC_META) | (1u << SEC_MAP) | (1u << 6)},
    {"nature", parse_nature, 0x7Fu, 0},     /* 키 7개 */
    {"market", parse_market, 0x1u, 0},      /* prices */
    {"merchant", parse_merchant, 0x3u, 0},  /* days_left, next_visit (+ deal 줄) */
    {"threat", parse_threat, 0x3u, 0},      /* next_raid, raids_done (+ threat 줄) */
    {"research", parse_research, 0x7u, 0},  /* done, active, progress (건물보다 먼저) */
    {"progress", parse_progress, 0x3Fu, 0}, /* rank_ready, outcome, lose_reason, tutorial, stats, guide */
    {"expansion", parse_expansion, 0u, (1u << 6) | (1u << 7)},
};
#define SECTION_COUNT ((int32_t)(sizeof(SECTIONS) / sizeof(SECTIONS[0])))

/* 주민의 집·일터·목표 id 를 포인터로 다시 잇습니다 (양쪽 모두) */
static int relink(LoadState *state) {
    World *world = state->world;
    int32_t i;
    for (i = 0; i < world->villager_count; i++) {
        Villager *villager = world->villagers[i];
        const PendingLinks *links = &state->links[i];
        if (links->home != 0) {
            Building *home = entity_find_building(world, links->home);
            if (home == NULL ||
                entity_link_resident(home, villager, world_building_def(world, home->type)->resident_slots) !=
                    CORE_OK) {
                return CORE_ERR_FORMAT;
            }
        }
        if (links->workplace != 0) {
            Building *work = entity_find_building(world, links->workplace);
            if (work == NULL ||
                entity_link_worker(work, villager, world_building_def(world, work->type)->worker_slots) !=
                    CORE_OK) {
                return CORE_ERR_FORMAT;
            }
        }
        if (links->target != 0) {
            villager->target = entity_find_building(world, links->target);
            if (villager->target == NULL) {
                return CORE_ERR_FORMAT;
            }
        }
    }
    return CORE_OK;
}

/* [end]: 모든 섹션과 필수 키가 있는지 확인하고 연결을 잇습니다 */
static int finish_sections(LoadState *state) {
    int32_t i;
    for (i = 0; i < SECTION_COUNT; i++) {
        if (i == 14 && !state->world->expansion)
            continue;
        if (!(state->sections_seen & (1u << i))) {
            return CORE_ERR_FORMAT;
        }
        if ((state->keys_seen[i] & SECTIONS[i].required_keys) != SECTIONS[i].required_keys) {
            return CORE_ERR_FORMAT;
        }
    }
    if (state->rows_read != state->world->grid.height || state->road_rows_read != state->world->grid.height) {
        return CORE_ERR_FORMAT;
    }
    return relink(state);
}

/* "[이름]" 줄 처리 */
static int begin_section(LoadState *state, char *line) {
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
            if ((state->sections_seen & SECTIONS[i].requires) != SECTIONS[i].requires) {
                return CORE_ERR_FORMAT; /* 순서가 틀림 (건물은 지도 뒤에) */
            }
            state->sections_seen |= 1u << i;
            state->section = i;
            return CORE_OK;
        }
    }
    return CORE_ERR_FORMAT;
}

/* 첫 줄 "MAGICCASTLE_SAVE <버전>" 확인 */
static int check_header(const char *line) {
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

int32_t save_peek_version(const char *path) {
    char line[64];
    size_t magic_length = strlen(SAVE_MAGIC);
    int32_t version = 0;
    char *end;
    FILE *file = platform_fopen_utf8(path, "rb");
    if (file == NULL) {
        return 0;
    }
    if (fgets(line, (int)sizeof(line), file) != NULL && strncmp(line, SAVE_MAGIC, magic_length) == 0 &&
        line[magic_length] == ' ') {
        end = line + magic_length + 1;
        end[strcspn(end, "\r\n")] = '\0';
        if (!parse_i32(end, 0, INT32_MAX, &version)) {
            version = 0;
        }
    }
    fclose(file);
    return version;
}

/* 파일 내용을 state->world 에 읽어 들입니다 */
static int read_world(LineReader *reader, LoadState *state) {
    int rc = read_line(reader);
    if (rc != 1) {
        return rc == 0 ? CORE_ERR_FORMAT : rc; /* 빈 파일 */
    }
    rc = check_header(reader->buffer);
    if (rc != CORE_OK) {
        return rc;
    }
    if (!verify_checksum(reader->file))
        return CORE_ERR_FORMAT;
    int checksum_seen = 0;
    while ((rc = read_line(reader)) == 1) {
        char *line = reader->buffer;
        char *equals;
        if (line[0] == '\0' || line[0] == '#') {
            continue; /* 빈 줄, 주석 */
        }
        if (state->ended) {
            if (!checksum_seen && strncmp(line, SAVE_CHECKSUM_MAGIC, sizeof(SAVE_CHECKSUM_MAGIC) - 1) == 0) {
                checksum_seen = 1;
                continue;
            }
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
    return state->ended && checksum_seen ? CORE_OK : CORE_ERR_FORMAT; /* [end] 가 없으면 잘린 파일 */
}

int save_read(const char *path, const Defs *defs, World **out_world, int32_t *out_line) {
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
    state.world = world_alloc(defs);

    if (reader.buffer == NULL || state.world == NULL) {
        rc = CORE_ERR_OUT_OF_MEMORY;
    } else {
        rc = read_world(&reader, &state);
    }
    if (rc == CORE_OK) {
        /* 점유·문·막힘 층과 비용 격자는 저장하지 않고 건물 목록에서 다시 만듭니다 */
        int revision = state.cost_revision;
        rc = expansion_finish_load(state.world);
        state.world->grid.cost_rev = revision;
        if (validate_world(state.world, NULL, NULL) != 0) {
            rc = CORE_ERR_FORMAT;
        }
    }
    fclose(reader.file);
    mem_free(reader.buffer);
    mem_free(state.links);

    if (rc != CORE_OK) {
        /* 반쯤 읽은 월드는 모두 해제합니다. 호출한 쪽의 기존 월드는 건드리지 않았습니다 */
        world_destroy(state.world);
        *out_line = reader.line_number;
        return rc;
    }
    *out_world = state.world;
    return CORE_OK;
}
