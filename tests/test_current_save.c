/* test_current_save.c — 현재 스키마의 반복 보존·손상·미지원 입력 거부 */
#include "expansion.h"
#include "mem.h"
#include "test_util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static World *new_world(void) {
    CoreNewGameParams p = {0};
    World *w = NULL;
    p.seed = 42;
    p.ruleset = 1;
    strcpy(p.castle_name, u8"현재 저장 검사");
    if (core_create(&p, NULL, &w) != CORE_OK)
        return NULL;
    return w;
}
static uint32_t reference_crc(const char *text, size_t size) {
    uint32_t crc = UINT32_MAX;
    for (size_t i = 0; i < size; i++) {
        crc ^= (unsigned char)text[i];
        for (int b = 0; b < 8; b++)
            crc = (crc >> 1) ^ ((0u - (crc & 1u)) & UINT32_C(0xedb88320));
    }
    return ~crc;
}
static char *read_file(const char *path, size_t *size) {
    FILE *f = fopen(path, "rb");
    if (!f)
        return NULL;
    if (fseek(f, 0, SEEK_END)) {
        fclose(f);
        return NULL;
    }
    long n = ftell(f);
    if (n < 0 || fseek(f, 0, SEEK_SET)) {
        fclose(f);
        return NULL;
    }
    char *text = malloc((size_t)n + 1);
    if (!text) {
        fclose(f);
        return NULL;
    }
    if (fread(text, 1, (size_t)n, f) != (size_t)n) {
        free(text);
        fclose(f);
        return NULL;
    }
    fclose(f);
    text[n] = 0;
    *size = (size_t)n;
    return text;
}
static int write_file(const char *path, const char *bytes, size_t size) {
    FILE *f = fopen(path, "wb");
    if (!f)
        return 0;
    int ok = fwrite(bytes, 1, size, f) == size;
    if (fclose(f))
        ok = 0;
    return ok;
}
static void remove_slot(const char *path) {
    char backup[128];
    remove(path);
    snprintf(backup, sizeof(backup), "%s.bak", path);
    remove(backup);
}
void test_current_save_cycles(void) {
    World *world = new_world();
    CHECK(world);
    const char *path = "current_cycles.sav";
    remove_slot(path);
    for (int i = 0; i < 1000; i++) {
        if (i % 100 == 0)
            CHECK_EQ(core_tick(world, 1), CORE_OK);
        long blocks = mem_test_live_blocks();
        uint64_t before = 0, after = 0;
        World *copy = NULL;
        int32_t line = 0;
        CHECK_EQ(core_state_hash(world, &before), CORE_OK);
        CHECK_EQ(core_save(world, path), CORE_OK);
        CHECK_EQ(core_load(path, NULL, &copy, &line), CORE_OK);
        CHECK(copy);
        CHECK_EQ(core_state_hash(copy, &after), CORE_OK);
        CHECK_EQ(before, after);
        CHECK_EQ(core_validate(copy), 0);
        core_destroy(copy);
        CHECK_EQ(mem_test_live_blocks(), blocks);
        if (i == 0 || i == 99 || i == 999)
            printf("current save cycles=%d hash=%llu\n", i + 1, (unsigned long long)before);
    }
    core_destroy(world);
    remove_slot(path);
}
void test_current_save_unsupported(void) {
    World *world = new_world();
    CHECK(world);
    uint64_t before = 0, after = 0;
    CHECK_EQ(core_state_hash(world, &before), CORE_OK);
    const char *path = "unsupported_schema.sav";
    for (int schema = 0; schema <= CORE_SAVE_VERSION + 1; schema++) {
        if (schema == CORE_SAVE_VERSION)
            continue;
        char header[64];
        snprintf(header, sizeof(header), "MAGICCASTLE_SAVE %d\n", schema);
        CHECK(write_file(path, header, strlen(header)));
        World *copy = NULL;
        int32_t line = 0;
        CHECK_EQ(core_load(path, NULL, &copy, &line), CORE_ERR_VERSION);
        CHECK(copy == NULL);
        CHECK_EQ(line, 1);
        CoreSaveInfoView info;
        CHECK_EQ(core_peek_save(path, NULL, &info), CORE_ERR_VERSION);
        CHECK_EQ(info.version, schema);
        CHECK_EQ(core_state_hash(world, &after), CORE_OK);
        CHECK_EQ(before, after);
    }
    core_destroy(world);
    remove_slot(path);
}
void test_current_save_integrity(void) {
    CHECK_EQ(reference_crc("123456789", 9), UINT32_C(0xcbf43926));
    World *world = new_world();
    CHECK(world);
    const char *good = "integrity_current.sav", *bad = "invalid_current.sav";
    CHECK_EQ(core_save(world, good), CORE_OK);
    size_t size = 0;
    char *text = read_file(good, &size);
    CHECK(text);
    uint64_t before = 0, after = 0;
    CHECK_EQ(core_state_hash(world, &before), CORE_OK);
    char *seed = strstr(text, "seed=42");
    CHECK(seed);
    seed[6] = '3';
    CHECK(write_file(bad, text, size));
    World *copy = NULL;
    int32_t line = 0;
    CHECK_EQ(core_load(bad, NULL, &copy, &line), CORE_ERR_FORMAT);
    CHECK(copy == NULL);
    CHECK_EQ(core_state_hash(world, &after), CORE_OK);
    CHECK_EQ(before, after);
    CHECK(write_file(bad, text, size - 1));
    CHECK_EQ(core_load(bad, NULL, &copy, &line), CORE_ERR_FORMAT);
    CHECK(copy == NULL);
    free(text);
    text = read_file(good, &size);
    CHECK(text);
    /* 체크섬은 올바르지만 필수 타일 하나가 없는 파일도 거부합니다. */
    char *tile = strstr(text, "\ntile=0,");
    CHECK(tile);
    tile++;
    char *end = strchr(tile, '\n');
    CHECK(end);
    end++;
    memmove(tile, end, strlen(end) + 1);
    char *footer = strstr(text, "MAGICCASTLE_CHECKSUM ");
    CHECK(footer);
    size_t payload = (size_t)(footer - text);
    uint32_t crc = reference_crc(text, payload);
    snprintf(footer, strlen(footer) + 1, "MAGICCASTLE_CHECKSUM %020llu %08x\n", (unsigned long long)payload,
             (unsigned)crc);
    CHECK(write_file(bad, text, strlen(text)));
    CHECK_EQ(core_load(bad, NULL, &copy, &line), CORE_ERR_FORMAT);
    CHECK(copy == NULL);
    CHECK(line > 1);
    CHECK_EQ(core_load(good, NULL, &copy, &line), CORE_OK);
    CHECK(copy);
    CHECK_EQ(core_state_hash(copy, &after), CORE_OK);
    CHECK_EQ(before, after);
    core_destroy(copy);
    core_destroy(world);
    free(text);
    remove_slot(good);
    remove_slot(bad);
}
