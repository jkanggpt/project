/* validate_assets.c — JSON 정본에서 생성된 목록 전체를 GPU 없이 검사합니다. */
#include "assets_index.h"
#include "raylib.h"
#include "storage.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static uint32_t checksum(const unsigned char *data, int size) {
    uint32_t crc = UINT32_MAX;
    for (int i = 0; i < size; i++) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; bit++)
            crc = (crc >> 1) ^ ((0u - (crc & 1u)) & UINT32_C(0xedb88320));
    }
    return ~crc;
}
int main(int argc, char **argv) {
    GameArguments args;
    if (!game_arguments(&args, argc, argv))
        return 2;
    const char *directory = args.count > 1 ? args.values[1] : "game/assets";
    int errors = 0;
    SetTraceLogLevel(LOG_WARNING);
    for (int i = 0; i < CASTLE_ASSET_COUNT; i++) {
        const CastleAssetRecord *record = &CASTLE_ASSETS[i];
        char path[GAME_PATH_BYTES];
        int size = 0;
        if (!game_join_path(path, sizeof(path), directory, record->path)) {
            errors++;
            continue;
        }
        unsigned char *bytes = game_read_bytes(path, &size);
        if (!bytes || size <= 0) {
            fprintf(stderr, "Missing asset: %s (%s)\n", record->id, record->path);
            free(bytes);
            errors++;
            continue;
        }
        if (checksum(bytes, size) != record->crc) {
            fprintf(stderr, "Changed asset: %s\n", record->path);
            errors++;
        }
        if (!strcmp(record->kind, "texture")) {
            Image image = LoadImageFromMemory(".png", bytes, size);
            if (!image.data || image.width != record->width || image.height != record->height ||
                record->columns < 1 || record->rows < 1 || image.width % record->columns ||
                image.height % record->rows) {
                fprintf(stderr, "Invalid image geometry: %s\n", record->path);
                errors++;
            }
            if (image.data)
                UnloadImage(image);
        } else if (!strcmp(record->kind, "audio")) {
            Wave wave = LoadWaveFromMemory(".ogg", bytes, size);
            if (!wave.data) {
                fprintf(stderr, "Invalid sound: %s\n", record->path);
                errors++;
            }
            if (wave.data)
                UnloadWave(wave);
        }
        free(bytes);
    }
    printf("ASSETS CHECK: checked=%d errors=%d\n", CASTLE_ASSET_COUNT, errors);
    game_arguments_free(&args);
    return errors ? 1 : 0;
}
