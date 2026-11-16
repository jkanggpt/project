/* pack_expansion_art.c — 확장 원화의 65종×4방향과 상태·자원·동물 아틀라스 */
#include "castle_expansion.h"
#include "raylib.h"
#include "storage.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const int IDS[17][4] = {
    {16, 17, 1, 18},  {19, 20, 2, 21},  {22, 23, 24, 25}, {26, 27, 4, 28},  {5, 29, 6, 30},  {31, 32, 33, 11},
    {34, 35, 36, 12}, {37, 38, 39, 10}, {40, 41, 42, 9},  {43, 8, 44, 45},  {7, 46, 47, 48}, {49, 50, 13, 51},
    {52, 53, 54, 55}, {56, 57, 58, 59}, {14, 60, 3, 61},  {62, 15, 63, 64}, {0, -1, -1, -1},
};
static int failures;
static Image read_png(const char *dir, const char *name) {
    char path[GAME_PATH_BYTES];
    int n = 0;
    unsigned char *data;
    Image im = {0};
    if (!game_join_path(path, sizeof(path), dir, name))
        return im;
    data = game_read_bytes(path, &n);
    if (data) {
        im = LoadImageFromMemory(".png", data, n);
        free(data);
    }
    if (im.data)
        ImageFormat(&im, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    else {
        fprintf(stderr, "Missing: %s\n", path);
        failures++;
    }
    return im;
}
static void write_png(const char *dir, const char *name, Image im) {
    char path[GAME_PATH_BYTES];
    int n = 0;
    unsigned char *bytes = ExportImageToMemory(im, ".png", &n);
    if (!bytes || !game_join_path(path, sizeof(path), dir, name) ||
        !game_write_bytes(path, bytes, (size_t)n)) {
        fprintf(stderr, "Export failed: %s\n", name);
        failures++;
    }
    if (bytes)
        MemFree(bytes);
}
static Image slice(Image src, int col, int row, int cols, int rows) {
    int x = col * src.width / cols, y = row * src.height / rows;
    return ImageFromImage(src, (Rectangle){(float)x, (float)y, (float)((col + 1) * src.width / cols - x),
                                           (float)((row + 1) * src.height / rows - y)});
}
static void trim(Image *im) {
    Color *pixels = (Color *)im->data;
    int i, clear = 0, solid = 0;
    for (i = 0; i < im->width * im->height; i++) {
        if (pixels[i].a < 96) {
            pixels[i] = BLANK;
            clear++;
        } else {
            pixels[i].a = 255;
            solid++;
        }
    }
    if (!clear || !solid) {
        fprintf(stderr, "Source requires real transparent alpha\n");
        failures++;
    }
    ImageAlphaCrop(im, 0.1f);
}
static void fit(Image *im, int width, int height) {
    float scale = (float)width / im->width, other = (float)height / im->height;
    int w, h;
    if (other < scale)
        scale = other;
    w = (int)(im->width * scale + 0.5f);
    h = (int)(im->height * scale + 0.5f);
    ImageResizeNN(im, w ? w : 1, h ? h : 1);
}
static void atlas(const char *in, const char *out, const char *name, int cols, int rows, int size,
                  int alpha) {
    Image src = read_png(in, name), dst = GenImageColor(cols * size, rows * size, BLANK);
    int x, y;
    if (!src.data) {
        UnloadImage(dst);
        return;
    }
    for (y = 0; y < rows; y++)
        for (x = 0; x < cols; x++) {
            Image frame = slice(src, x, y, cols, rows);
            if (alpha) {
                trim(&frame);
                fit(&frame, size - 4, size - 4);
            } else
                ImageResizeNN(&frame, size, size);
            ImageDraw(&dst, frame, (Rectangle){0, 0, (float)frame.width, (float)frame.height},
                      (Rectangle){(float)(x * size + (size - frame.width) / 2),
                                  (float)((y + 1) * size - frame.height), (float)frame.width,
                                  (float)frame.height},
                      WHITE);
            UnloadImage(frame);
        }
    write_png(out, name, dst);
    UnloadImage(dst);
    UnloadImage(src);
}
static void check_png(const char *dir, const char *name, int alpha) {
    Image im = read_png(dir, name);
    int i, clear = 0, solid = 0;
    if (!im.data)
        return;
    for (i = 0; i < im.width * im.height; i++) {
        Color c = ((Color *)im.data)[i];
        if (c.a)
            solid++;
        else
            clear++;
    }
    if (!solid || (alpha && !clear) || (!alpha && clear)) {
        fprintf(stderr, "Invalid alpha: %s\n", name);
        failures++;
    }
    UnloadImage(im);
}
static void resource_files(const char *in, const char *out) {
    Image src = read_png(in, "resource_icons.png");
    int i;
    if (!src.data)
        return;
    for (i = 0; i < 15; i++) {
        Image im = slice(src, i % 4, i / 4, 4, 4);
        char name[64];
        trim(&im);
        fit(&im, 64, 64);
        (void)snprintf(name, sizeof(name), "res_%s.png", core_expansion_resource_key(i));
        write_png(out, name, im);
        UnloadImage(im);
    }
    UnloadImage(src);
}
int main(int argc, char **argv) {
    GameArguments args;
    int check, i, row, rot;
    const char *in, *out;
    char name[128];
    if (!game_arguments(&args, argc, argv))
        return 2;
    check = args.count == 3 && strcmp(args.values[1], "--check") == 0;
    if (!check && args.count != 3) {
        fprintf(stderr, "Usage: castle_expansion_art <source> <output> | --check <output>\n");
        game_arguments_free(&args);
        return 2;
    }
    in = args.values[1];
    out = args.values[2];
    SetTraceLogLevel(LOG_WARNING);
    (void)game_make_dirs(out);
    if (!check)
        for (i = 0; i < 17; i++) {
            Image src;
            int rows = i == 16 ? 1 : 4;
            (void)snprintf(name, sizeof(name), i == 16 ? "keep_turns.png" : "buildings_%02d.png", i);
            src = read_png(in, name);
            if (!src.data)
                continue;
            for (row = 0; row < rows; row++)
                for (rot = 0; rot < 4; rot++) {
                    CoreExpansionBuildingInfo info;
                    Image frame = slice(src, rot, row, 4, rows);
                    trim(&frame);
                    fit(&frame, 256, 384);
                    (void)core_expansion_building_info(IDS[i][row], &info);
                    (void)snprintf(name, sizeof(name), "%s_%d.png", info.key, rot);
                    write_png(out, name, frame);
                    UnloadImage(frame);
                }
            UnloadImage(src);
        }
    if (!check) {
        atlas(in, out, "status_icons.png", 6, 4, 64, 1);
        atlas(in, out, "resource_icons.png", 4, 4, 64, 1);
        atlas(in, out, "animals.png", 4, 4, 96, 1);
        atlas(in, out, "biomes.png", 4, 8, 64, 0);
        resource_files(in, out);
    }
    for (i = 0; i < 65; i++)
        for (rot = 0; rot < 4; rot++) {
            CoreExpansionBuildingInfo info;
            (void)core_expansion_building_info(i, &info);
            (void)snprintf(name, sizeof(name), "%s_%d.png", info.key, rot);
            check_png(out, name, 1);
        }
    check_png(out, "status_icons.png", 1);
    check_png(out, "resource_icons.png", 1);
    check_png(out, "animals.png", 1);
    check_png(out, "biomes.png", 0);
    game_arguments_free(&args);
    fprintf(stdout, "Expansion art: 260 building views and 4 atlases, %d errors\n", failures);
    return failures ? 1 : 0;
}
