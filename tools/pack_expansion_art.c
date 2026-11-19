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
/* 생성 시트의 열 중심이 조금 달라도 투명한 골 사이에서 잘라 이웃 그림이 섞이지 않게 합니다. */
static int building_cut(Image src, int column, int row, int rows) {
    const Color *pixels = (const Color *)src.data;
    int nominal = column * src.width / 4, radius = src.width / 16, x, y, best = nominal,
        count_best = INT32_MAX;
    int y0 = row * src.height / rows, y1 = (row + 1) * src.height / rows;
    if (!column)
        return 0;
    if (column == 4)
        return src.width;
    for (x = nominal - radius; x <= nominal + radius; x++) {
        int count = 0;
        for (y = y0; y < y1; y++)
            count += pixels[y * src.width + x].a >= 96;
        if (count < count_best || (count == count_best && abs(x - nominal) < abs(best - nominal))) {
            best = x;
            count_best = count;
        }
    }
    return best;
}
static Image building_slice(Image src, int column, int row, int rows) {
    int left = building_cut(src, column, row, rows), right = building_cut(src, column + 1, row, rows);
    int top = row * src.height / rows, bottom = (row + 1) * src.height / rows;
    return ImageFromImage(src,
                          (Rectangle){(float)left, (float)top, (float)(right - left), (float)(bottom - top)});
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
static void check_animation_assets(const char *out) {
    Image animal = read_png(out, "animal_walk.png"), wall = read_png(out, "wall_links.png");
    int row, col, x, y;
    if (animal.data) {
        if (animal.width != 384 || (animal.height != 1536 && animal.height != 2304))
            failures++;
        else
            for (row = 0; row < animal.height / 96; row++) {
                unsigned first = 0;
                int changed = 0;
                for (col = 0; col < 4; col++) {
                    unsigned hash = 2166136261u;
                    int solid = 0, feet = 0;
                    for (y = 0; y < 96; y++)
                        for (x = 0; x < 96; x++) {
                            Color p = ((Color *)animal.data)[(row * 96 + y) * animal.width + col * 96 + x];
                            solid += p.a != 0;
                            feet += p.a != 0 && y >= 89;
                            hash = (hash ^ (unsigned)ColorToInt(p)) * 16777619u;
                        }
                    if (solid < 150 || !feet) {
                        fprintf(stderr, "Empty/misaligned animal %d frame %d\n", row, col);
                        failures++;
                    }
                    if (!col)
                        first = hash;
                    else
                        changed |= hash != first;
                }
                if (!changed) {
                    fprintf(stderr, "Static animal row %d\n", row);
                    failures++;
                }
            }
        UnloadImage(animal);
    }
    if (wall.data) {
        if (wall.width != 2048 || wall.height != 512)
            failures++;
        else
            for (row = 0; row < 4; row++)
                for (col = 0; col < 16; col++) {
                    int solid = 0, edges[4] = {0};
                    for (y = 0; y < 128; y++)
                        for (x = 0; x < 128; x++) {
                            Color p = ((Color *)wall.data)[(row * 128 + y) * wall.width + col * 128 + x];
                            if (!p.a)
                                continue;
                            solid++;
                            edges[0] += y == 0;
                            edges[1] += x == 127;
                            edges[2] += y == 127;
                            edges[3] += x == 0;
                        }
                    if (solid < 500)
                        failures++;
                    for (x = 0; x < 4; x++)
                        if ((col & (1 << x)) && !edges[x]) {
                            fprintf(stderr, "Disconnected wall %d mask %d edge %d\n", row, col, x);
                            failures++;
                        }
                }
        UnloadImage(wall);
    }
}
static void animation_assets(const char *in, const char *out) {
    Image dst = GenImageColor(384, 1536, BLANK);
    int group, row, col;
    for (group = 0; group < 4; group++) {
        char name[64];
        Image src;
        (void)snprintf(name, sizeof(name), "animal_walk_%d.png", group);
        src = read_png(in, name);
        if (!src.data)
            continue;
        for (row = 0; row < 4; row++) {
            Image frames[4];
            int maxw = 0, maxh = 0;
            for (col = 0; col < 4; col++) {
                frames[col] = slice(src, col, row, 4, 4);
                trim(&frames[col]);
                if (frames[col].width > maxw)
                    maxw = frames[col].width;
                if (frames[col].height > maxh)
                    maxh = frames[col].height;
            }
            for (col = 0; col < 4; col++) {
                float scale = 92.0f / (maxw > maxh ? maxw : maxh);
                int w = (int)(frames[col].width * scale), h = (int)(frames[col].height * scale);
                ImageResizeNN(&frames[col], w ? w : 1, h ? h : 1);
                ImageDraw(&dst, frames[col],
                          (Rectangle){0, 0, (float)frames[col].width, (float)frames[col].height},
                          (Rectangle){col * 96.0f + (96 - w) / 2.0f, (group * 4 + row + 1) * 96.0f - h,
                                      (float)w, (float)h},
                          WHITE);
                UnloadImage(frames[col]);
            }
        }
        UnloadImage(src);
    }
    write_png(out, "animal_walk.png", dst);
    UnloadImage(dst);
    {
        Image src = read_png(in, "wall_modules.png"), links = GenImageColor(2048, 512, BLANK);
        if (src.data) {
            for (row = 0; row < 4; row++) {
                Image post = slice(src, 0, row, 4, 4), horizontal = slice(src, 1, row, 4, 4),
                      vertical = slice(src, 2, row, 4, 4);
                trim(&post);
                trim(&horizontal);
                trim(&vertical);
                ImageResizeNN(&post, 64, 64);
                ImageResizeNN(&horizontal, 128, 64);
                ImageResizeNN(&vertical, 64, 128);
                for (col = 0; col < 16; col++) {
                    Image tile = GenImageColor(128, 128, BLANK);
                    if (col & 1)
                        ImageDraw(&tile, vertical, (Rectangle){0, 0, 64, 64}, (Rectangle){32, 0, 64, 64},
                                  WHITE);
                    if (col & 2)
                        ImageDraw(&tile, horizontal, (Rectangle){64, 0, 64, 64}, (Rectangle){64, 32, 64, 64},
                                  WHITE);
                    if (col & 4)
                        ImageDraw(&tile, vertical, (Rectangle){0, 64, 64, 64}, (Rectangle){32, 64, 64, 64},
                                  WHITE);
                    if (col & 8)
                        ImageDraw(&tile, horizontal, (Rectangle){0, 0, 64, 64}, (Rectangle){0, 32, 64, 64},
                                  WHITE);
                    ImageDraw(&tile, post, (Rectangle){0, 0, 64, 64}, (Rectangle){32, 32, 64, 64}, WHITE);
                    ImageDraw(&links, tile, (Rectangle){0, 0, 128, 128},
                              (Rectangle){col * 128.0f, row * 128.0f, 128, 128}, WHITE);
                    UnloadImage(tile);
                }
                UnloadImage(post);
                UnloadImage(horizontal);
                UnloadImage(vertical);
            }
            UnloadImage(src);
        }
        write_png(out, "wall_links.png", links);
        UnloadImage(links);
    }
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
                    Image frame = building_slice(src, rot, row, rows);
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
        animation_assets(in, out);
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
    check_png(out, "animal_walk.png", 1);
    check_png(out, "wall_links.png", 1);
    check_animation_assets(out);
    game_arguments_free(&args);
    fprintf(stdout,
            "Expansion art: 260 building views, 6 atlases, 64 animal poses, 64 wall masks, %d errors\n",
            failures);
    return failures ? 1 : 0;
}
