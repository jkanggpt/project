/* 외부 그림 없이 보관한 픽셀 마스크와 팔레트로 여덟 변형을 만듭니다. */
#include "gpu_scenery.h"
#include "trace.h"
#include "storage.h"
#include "raylib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int art_gpu_scenery(const char *source, const char *output) {
    int size = 0, palette[18], version = 0;
    unsigned char *bytes = art_read_bytes(source, &size);
    if (!bytes)
        return 1;
    char *text = malloc((size_t)size + 1);
    if (!text) {
        free(bytes);
        return 1;
    }
    memcpy(text, bytes, (size_t)size);
    text[size] = 0;
    free(bytes);
    int consumed = 0;
    int parsed =
        sscanf(text,
               "CASTLE_SCENERY %d GRASS %d %d %d %d %d %d %d %d %d "
               "SMOKE %d %d %d %d %d %d %d %d %d MASK %n",
               &version, &palette[0], &palette[1], &palette[2], &palette[3], &palette[4], &palette[5],
               &palette[6], &palette[7], &palette[8], &palette[9], &palette[10], &palette[11], &palette[12],
               &palette[13], &palette[14], &palette[15], &palette[16], &palette[17], &consumed);
    unsigned char mask[32][16] = {{0}};
    int error = parsed != 19 || version != 1;
    for (int i = 0; i < 18; i++)
        if (!error && (palette[i] < 0 || palette[i] > 255))
            error = 1;
    const char *cursor = text + consumed;
    for (int y = 0; y < 32 && !error; y++) {
        if (cursor + 16 > text + size) {
            error = 1;
            break;
        }
        for (int x = 0; x < 16; x++) {
            char c = *cursor++;
            if (c != '.' && (c < '1' || c > '3')) {
                error = 1;
                break;
            }
            mask[y][x] = c == '.' ? 0 : (unsigned char)(c - '0');
        }
        if (*cursor == '\r')
            cursor++;
        if (*cursor != '\n' && *cursor != 0)
            error = 1;
        if (*cursor)
            cursor++;
    }
    Image image = GenImageColor(128, 32, BLANK);
    if (!image.data)
        error = 1;
    for (int kind = 0; kind < 2 && !error; kind++)
        for (int variant = 0; variant < 8; variant++)
            for (int y = 0; y < 16; y++)
                for (int x = 0; x < 16; x++) {
                    int sx = variant & 1 ? 15 - x : x, sy = y;
                    if (!kind)
                        sx += y < 10 ? (variant / 2) % 3 - 1 : 0;
                    else if (variant & 2)
                        sy = 15 - y;
                    if (kind && (variant & 4)) {
                        int swap = sx;
                        sx = sy;
                        sy = swap;
                    }
                    if (sx < 0 || sx >= 16)
                        continue;
                    int level = mask[kind * 16 + sy][sx];
                    if (!level)
                        continue;
                    int index = kind * 9 + (level - 1) * 3;
                    ((Color *)image.data)[(kind * 16 + y) * 128 + variant * 16 + x] =
                        (Color){(unsigned char)palette[index], (unsigned char)palette[index + 1],
                                (unsigned char)palette[index + 2], 255};
                }
    if (!error) {
        char path[GAME_PATH_BYTES];
        int count = 0;
        unsigned char *png = ExportImageToMemory(image, ".png", &count);
        error = !png || !game_join_path(path, sizeof(path), output, "scenery_masks.png") ||
                !art_write_bytes(path, png, (size_t)count);
        MemFree(png);
    }
    if (image.data)
        UnloadImage(image);
    free(text);
    printf("GPU SCENERY ART: variants=8 errors=%d\n", error);
    return error;
}
int art_gpu_contacts(const char *source, const char *output) {
    int size = 0, version = 0, consumed = 0;
    unsigned char *bytes = art_read_bytes(source, &size);
    if (!bytes)
        return 1;
    char *text = malloc((size_t)size + 1);
    if (!text) {
        free(bytes);
        return 1;
    }
    memcpy(text, bytes, (size_t)size);
    text[size] = 0;
    free(bytes);
    int error = sscanf(text, "CASTLE_CONTACTS %d %n", &version, &consumed) != 1 || version != 1;
    const char *cursor = text + consumed;
    Image image = GenImageColor(8, 1, BLANK);
    if (!image.data)
        error = 1;
    for (int i = 0; i < 8 && !error; i++) {
        int kind = 0, strip = 0, depth = 0, lower = 0, count = 0;
        if (sscanf(cursor, "%d %d %d %d %n", &kind, &strip, &depth, &lower, &count) != 4 || kind != i ||
            strip < 0 || strip > 153 || depth < 0 || depth > 24 || lower < 0 || lower > 8) {
            error = 1;
            break;
        }
        cursor += count;
        ((Color *)image.data)[i] =
            (Color){(unsigned char)strip, (unsigned char)depth, (unsigned char)lower, 255};
    }
    if (!error) {
        char path[GAME_PATH_BYTES];
        int count = 0;
        unsigned char *png = ExportImageToMemory(image, ".png", &count);
        error = !png || !game_join_path(path, sizeof(path), output, "contact_policy.png") ||
                !art_write_bytes(path, png, (size_t)count);
        MemFree(png);
    }
    if (image.data)
        UnloadImage(image);
    free(text);
    printf("GPU CONTACT ART: kinds=8 errors=%d\n", error);
    return error;
}
