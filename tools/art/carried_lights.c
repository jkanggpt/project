/* 보관된 마스크에서 여덟 불꽃 프레임을 정수 화소로 만듭니다. */
#include "carried_lights.h"
#include "raylib.h"
#include "storage.h"
#include "trace.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int art_carried_lights(const char *source, const char *output) {
    int count = 0, version = 0, offset = 0;
    unsigned char *bytes = art_read_bytes(source, &count);
    if (!bytes)
        return 1;
    char *text = calloc((size_t)count + 1, 1);
    if (!text) {
        free(bytes);
        return 1;
    }
    memcpy(text, bytes, (size_t)count);
    free(bytes);
    int error = sscanf(text, "CASTLE_CARRIED_LIGHTS %d PALETTE %n", &version, &offset) != 1 || version != 1;
    Color palette[7] = {{0}};
    char *cursor = text + offset;
    for (int i = 1; i <= 6 && !error; i++) {
        int r = 0, g = 0, b = 0;
        offset = 0;
        error = sscanf(cursor, "%d %d %d%n", &r, &g, &b, &offset) != 3 || r < 0 || r > 255 || g < 0 ||
                g > 255 || b < 0 || b > 255;
        palette[i] = (Color){(unsigned char)r, (unsigned char)g, (unsigned char)b, 255};
        cursor += offset;
    }
    offset = 0;
    if (!error) {
        (void)sscanf(cursor, " MASK %n", &offset);
        error = !offset;
        cursor += offset;
    }
    unsigned char mask[32][16] = {{0}};
    for (int y = 0; y < 32 && !error; y++) {
        if (cursor + 16 > text + count) {
            error = 1;
            break;
        }
        for (int x = 0; x < 16; x++) {
            char c = *cursor++;
            if (c != '.' && (c < '1' || c > '6')) {
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
    error |= !image.data;
    const int shift[8] = {0, -1, 0, 1, 0, 1, 0, -1};
    for (int kind = 0; kind < 2 && !error; kind++)
        for (int frame = 0; frame < 8; frame++)
            for (int y = 0; y < 16; y++)
                for (int x = 0; x < 16; x++) {
                    int level = mask[kind * 16 + y][x];
                    if (!level)
                        continue;
                    int dx = x;
                    if (kind && level >= 4 && y < 7)
                        dx += shift[frame] * (y < 4 ? 1 : 0);
                    if (dx < 0 || dx >= 16)
                        continue;
                    if (level >= 4) {
                        unsigned noise = (unsigned)(frame * 17 + x * 7 + y * 13 + kind * 31);
                        if (noise % 5u == 0)
                            level = 6;
                        else if (noise % 4u == 0)
                            level = 4;
                    }
                    ((Color *)image.data)[(kind * 16 + y) * image.width + frame * 16 + dx] = palette[level];
                }
    char path[GAME_PATH_BYTES];
    int size = 0;
    unsigned char *png = !error ? ExportImageToMemory(image, ".png", &size) : NULL;
    if (!error)
        error = !png || !game_make_dirs(output) ||
                !game_join_path(path, sizeof(path), output, "carried_lights.png") ||
                !art_write_bytes(path, png, (size_t)size);
    MemFree(png);
    if (image.data)
        UnloadImage(image);
    free(text);
    printf("CARRIED ART: kinds=2 frames=8 errors=%d\n", error);
    return error ? 1 : 0;
}
int art_carried_lights_check(const char *output) {
    char path[GAME_PATH_BYTES];
    int size = 0;
    if (!game_join_path(path, sizeof(path), output, "carried_lights.png"))
        return 1;
    unsigned char *bytes = game_read_bytes(path, &size);
    Image image = bytes ? LoadImageFromMemory(".png", bytes, size) : (Image){0};
    free(bytes);
    int errors = !image.data || image.width != 128 || image.height != 32;
    if (image.data)
        ImageFormat(&image, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    int animated = 0;
    for (int kind = 0; image.data && !errors && kind < 2; kind++)
        for (int frame = 0; frame < 8; frame++) {
            int occupied = 0;
            for (int y = 0; y < 16; y++)
                for (int x = 0; x < 16; x++) {
                    Color c = ((Color *)image.data)[(kind * 16 + y) * image.width + frame * 16 + x];
                    occupied += c.a >= 128;
                    if ((x == 0 || x == 15 || y == 15) && c.a)
                        errors++;
                    if (frame) {
                        Color first = ((Color *)image.data)[(kind * 16 + y) * image.width + x];
                        animated += memcmp(&c, &first, sizeof(c)) != 0;
                    }
                }
            errors += occupied < 20;
        }
    errors += !animated;
    if (image.data)
        UnloadImage(image);
    printf("CARRIED ART CHECK: cells=16 animation_changes=%d errors=%d\n", animated, errors);
    return errors ? 1 : 0;
}
