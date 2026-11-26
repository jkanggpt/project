#include "pipeline.h"
/* nature — 현재 아트 파이프라인 단계 */
#include "raylib.h"
#include "storage.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int errors;
static Image read_image(const char *dir, const char *name) {
    char path[GAME_PATH_BYTES];
    int n = 0;
    Image im = {0};
    unsigned char *data;
    if (!game_join_path(path, sizeof(path), dir, name))
        return im;
    data = game_read_bytes(path, &n);
    if (data) {
        im = LoadImageFromMemory(".png", data, n);
        free(data);
    }
    if (im.data)
        ImageFormat(&im, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    else
        errors++;
    return im;
}
static void write_image(const char *dir, const char *name, Image im) {
    char path[GAME_PATH_BYTES];
    int n = 0;
    unsigned char *data = ExportImageToMemory(im, ".png", &n);
    if (!data || !game_join_path(path, sizeof(path), dir, name) || !game_write_bytes(path, data, (size_t)n))
        errors++;
    if (data)
        MemFree(data);
}
static Image cell(Image src, int x, int y, int cols, int rows) {
    int left = x * src.width / cols, top = y * src.height / rows;
    return ImageFromImage(src,
                          (Rectangle){(float)left, (float)top, (float)((x + 1) * src.width / cols - left),
                                      (float)((y + 1) * src.height / rows - top)});
}
/* 생성 원화의 행 간격이 조금 달라도 투명한 골에서 잘라 이웃 종이 섞이지 않게 합니다. */
static int row_cut(Image src, int row, int rows) {
    if (!row)
        return 0;
    if (row == rows)
        return src.height;
    int nominal = row * src.height / rows, radius = src.height / rows / 2, best = nominal,
        minimum = INT32_MAX;
    const Color *p = (const Color *)src.data;
    for (int y = nominal - radius; y <= nominal + radius; y++) {
        int count = 0;
        for (int x = 0; x < src.width; x++)
            count += p[y * src.width + x].a >= 96;
        if (count < minimum || (count == minimum && abs(y - nominal) < abs(best - nominal))) {
            minimum = count;
            best = y;
        }
    }
    return best;
}
static int column_cut(Image src, int column, int top, int bottom) {
    if (!column)
        return 0;
    if (column == 4)
        return src.width;
    int nominal = column * src.width / 4, radius = src.width / 16, best = nominal, minimum = INT32_MAX;
    const Color *p = (const Color *)src.data;
    for (int x = nominal - radius; x <= nominal + radius; x++) {
        int count = 0;
        for (int y = top; y < bottom; y++)
            count += p[y * src.width + x].a >= 96;
        if (count < minimum || (count == minimum && abs(x - nominal) < abs(best - nominal))) {
            minimum = count;
            best = x;
        }
    }
    return best;
}
static void draw_frame(Image *atlas, Image src, int col, int row, int size, int alpha) {
    int width, height;
    float scale;
    if (!src.data) {
        errors++;
        return;
    }
    if (alpha) {
        Color *p = (Color *)src.data;
        int clear = 0;
        for (int i = 0; i < src.width * src.height; i++) {
            if (p[i].a < 96) {
                p[i] = BLANK;
                clear++;
            } else
                p[i].a = 255;
        }
        if (!clear)
            errors++;
        ImageAlphaCrop(&src, 0.1f);
    }
    scale = (float)(size - 4) / src.width;
    if ((float)(size - 4) / src.height < scale)
        scale = (float)(size - 4) / src.height;
    width = alpha ? (int)(src.width * scale) : size;
    height = alpha ? (int)(src.height * scale) : size;
    ImageResizeNN(&src, width > 0 ? width : 1, height > 0 ? height : 1);
    ImageDraw(atlas, src, (Rectangle){0, 0, (float)src.width, (float)src.height},
              (Rectangle){col * (float)size + (size - src.width) / 2.0f, (row + 1) * (float)size - src.height,
                          (float)src.width, (float)src.height},
              WHITE);
    UnloadImage(src);
}
static void animation_row(Image *dst, Image src, int source_row, int rows, int target_row) {
    Image frames[4];
    int maxw = 1, maxh = 1;
    int top = row_cut(src, source_row, rows), bottom = row_cut(src, source_row + 1, rows);
    for (int c = 0; c < 4; c++) {
        int left = column_cut(src, c, top, bottom), right = column_cut(src, c + 1, top, bottom);
        frames[c] = ImageFromImage(
            src, (Rectangle){(float)left, (float)top, (float)(right - left), (float)(bottom - top)});
        Color *p = (Color *)frames[c].data;
        for (int i = 0; i < frames[c].width * frames[c].height; i++)
            if (p[i].a < 96)
                p[i] = BLANK;
            else
                p[i].a = 255;
        ImageAlphaCrop(&frames[c], .1f);
        if (frames[c].width > maxw)
            maxw = frames[c].width;
        if (frames[c].height > maxh)
            maxh = frames[c].height;
    }
    float scale = 92.0f / (maxw > maxh ? maxw : maxh);
    for (int c = 0; c < 4; c++) {
        int w = (int)(frames[c].width * scale + .5f), h = (int)(frames[c].height * scale + .5f);
        ImageResizeNN(&frames[c], w > 0 ? w : 1, h > 0 ? h : 1);
        ImageDraw(dst, frames[c], (Rectangle){0, 0, (float)w, (float)h},
                  (Rectangle){c * 96.0f + (96 - w) / 2.0f, (target_row + 1) * 96.0f - h, (float)w, (float)h},
                  WHITE);
        UnloadImage(frames[c]);
    }
}
static void characters(const char *in, const char *out) {
    const int newkind[9] = {5, 16, 17, 18, 19, 20, 21, 22, 23}, swim[5] = {3, 6, 13, 18, 21},
              attack[9] = {11, 12, 13, 14, 15, 16, 17, 19, 20};
    Image old = read_image(out, "animal_walk.png"), walk = GenImageColor(384, 2304, BLANK);
    Image raw = read_image(in, "new_walk.png"), water = read_image(in, "swim.png"),
          fight = read_image(in, "attack.png");
    Image swims = GenImageColor(384, 2304, BLANK), attacks = GenImageColor(384, 2304, BLANK),
          icons = GenImageColor(384, 576, BLANK);
    if (old.data)
        ImageDraw(&walk, old, (Rectangle){0, 0, 384, 1536}, (Rectangle){0, 0, 384, 1536}, WHITE);
    /* 알파카 칸을 낙타로 교체하고 새 8종을 추가합니다. 기존 15종의 동작은 보존합니다. */
    for (int r = 0; r < 9; r++) {
        for (int y = newkind[r] * 96; y < (newkind[r] + 1) * 96; y++)
            for (int x = 0; x < 384; x++)
                ((Color *)walk.data)[y * 384 + x] = BLANK;
        if (raw.data)
            animation_row(&walk, raw, r, 9, newkind[r]);
    }
    for (int r = 0; r < 5; r++)
        if (water.data)
            animation_row(&swims, water, r, 5, swim[r]);
    for (int r = 0; r < 9; r++)
        if (fight.data)
            animation_row(&attacks, fight, r, 9, attack[r]);
    for (int k = 0; k < 24; k++) {
        Image frame = ImageFromImage(walk, (Rectangle){96, k * 96.0f, 96, 96});
        ImageDraw(&icons, frame, (Rectangle){0, 0, 96, 96},
                  (Rectangle){(k % 4) * 96.0f, (k / 4) * 96.0f, 96, 96}, WHITE);
        UnloadImage(frame);
    }
    write_image(out, "animal_walk.png", walk);
    write_image(out, "animal_swim.png", swims);
    write_image(out, "animal_attack.png", attacks);
    write_image(out, "animals.png", icons);
    UnloadImage(old);
    UnloadImage(raw);
    UnloadImage(water);
    UnloadImage(fight);
    UnloadImage(walk);
    UnloadImage(swims);
    UnloadImage(attacks);
    UnloadImage(icons);
}
static void scenery(const char *in, const char *out) {
    Image trees = read_image(in, "trees.png"), treeatlas = GenImageColor(640, 512, BLANK),
          manor = read_image(in, "manors.png");
    char name[80];
    for (int r = 0; r < 4; r++)
        for (int c = 0; c < 5; c++)
            if (trees.data)
                draw_frame(&treeatlas, cell(trees, c, r, 5, 4), c, r, 128, 1);
    write_image(out, "trees.png", treeatlas);
    UnloadImage(trees);
    UnloadImage(treeatlas);
    for (int r = 0; r < 2; r++)
        for (int c = 0; c < 3; c++)
            if (manor.data) {
                Image frame = GenImageColor(256, 256, BLANK);
                draw_frame(&frame, cell(manor, c, r, 3, 2), 0, 0, 256, 1);
                snprintf(name, sizeof(name), "manor_%d.png", r * 3 + c);
                write_image(out, name, frame);
                UnloadImage(frame);
            }
    UnloadImage(manor);
}
static void ground(const char *in, const char *out) {
    Image src = read_image(in, "biomes.png"), atlas = GenImageColor(256, 768, BLANK);
    Image transitions = GenImageColor(1024, 768, BLANK), shores = GenImageColor(1024, 768, BLANK);
    Image roads = read_image(in, "roads.png"), roadatlas = GenImageColor(192, 64, BLANK);
    if (src.data)
        for (int r = 0; r < 12; r++) {
            Image tile = cell(src, r % 3, r / 3, 3, 4);
            ImageResizeNN(&tile, 64, 64);
            for (int c = 0; c < 4; c++) {
                Image variant = ImageCopy(tile);
                if (c & 1)
                    ImageFlipHorizontal(&variant);
                if (c & 2)
                    ImageFlipVertical(&variant);
                ImageDraw(&atlas, variant, (Rectangle){0, 0, 64, 64},
                          (Rectangle){c * 64.0f, r * 64.0f, 64, 64}, WHITE);
                UnloadImage(variant);
            }
            for (int mask = 0; mask < 16; mask++)
                for (int y = 0; y < 64; y++)
                    for (int x = 0; x < 64; x++) {
                        int edge = 64;
                        if ((mask & 1) && y < edge)
                            edge = y;
                        if ((mask & 2) && 63 - x < edge)
                            edge = 63 - x;
                        if ((mask & 4) && 63 - y < edge)
                            edge = 63 - y;
                        if ((mask & 8) && x < edge)
                            edge = x;
                        unsigned noise = (unsigned)(x * 173 + y * 719 + (x * y) * 13) % 5;
                        Color p = ((Color *)tile.data)[y * 64 + x];
                        p.a = (unsigned char)(edge < 12 ? (12 - edge) * 170 / 12 : 0);
                        ((Color *)transitions.data)[(r * 64 + y) * 1024 + mask * 64 + x] = p;
                        p.a = (unsigned char)(edge < 3 + (int)(noise % 3)   ? 255
                                              : edge < 6 + (int)(noise % 3) ? 130
                                                                            : 0);
                        ((Color *)shores.data)[(r * 64 + y) * 1024 + mask * 64 + x] = p;
                    }
            UnloadImage(tile);
        }
    if (roads.data)
        for (int c = 0; c < 3; c++)
            draw_frame(&roadatlas, cell(roads, c, 0, 3, 1), c, 0, 64, 0);
    write_image(out, "biomes.png", atlas);
    write_image(out, "biome_edges.png", transitions);
    write_image(out, "shores.png", shores);
    write_image(out, "road_tiles.png", roadatlas);
    UnloadImage(src);
    UnloadImage(atlas);
    UnloadImage(transitions);
    UnloadImage(shores);
    UnloadImage(roads);
    UnloadImage(roadatlas);
}
static void check_atlas(const char *dir, const char *name, int cols, int rows, int size, unsigned mask,
                        int opaque, int animation) {
    Image im = read_image(dir, name);
    if (!im.data)
        return;
    if (im.width != cols * size || im.height != rows * size) {
        errors++;
        UnloadImage(im);
        return;
    }
    for (int r = 0; r < rows; r++)
        if (mask & (1u << r)) {
            unsigned first = 0;
            int changed = 0;
            for (int c = 0; c < cols; c++) {
                int solid = 0, clear = 0;
                unsigned hash = 2166136261u;
                for (int y = 0; y < size; y++)
                    for (int x = 0; x < size; x++) {
                        Color p = ((Color *)im.data)[(r * size + y) * im.width + c * size + x];
                        solid += p.a != 0;
                        clear += p.a != 255;
                        hash = (hash ^ (unsigned)ColorToInt(p)) * 16777619u;
                    }
                if (solid < 20 || (opaque && clear) || (!opaque && !clear))
                    errors++;
                if (!c)
                    first = hash;
                else
                    changed |= first != hash;
            }
            if (animation && !changed)
                errors++;
        }
    UnloadImage(im);
}
static void check_all(const char *dir) {
    char name[80];
    check_atlas(dir, "trees.png", 5, 4, 128, 15, 0, 0);
    check_atlas(dir, "animal_walk.png", 4, 24, 96, 0xffffff, 0, 1);
    check_atlas(dir, "animal_swim.png", 4, 24, 96,
                (1u << 3) | (1u << 6) | (1u << 13) | (1u << 18) | (1u << 21), 0, 1);
    check_atlas(dir, "animal_attack.png", 4, 24, 96,
                (1u << 11) | (1u << 12) | (1u << 13) | (1u << 14) | (1u << 15) | (1u << 16) | (1u << 17) |
                    (1u << 19) | (1u << 20),
                0, 1);
    check_atlas(dir, "biomes.png", 4, 12, 64, 4095, 1, 0);
    check_atlas(dir, "road_tiles.png", 3, 1, 64, 1, 1, 0);
    for (int r = 0; r < 6; r++) {
        snprintf(name, sizeof(name), "manor_%d.png", r);
        Image im = read_image(dir, name);
        if (!im.data || im.width < 64 || im.height < 64 || im.width > 384 || im.height > 384)
            errors++;
        if (im.data) {
            Rectangle bounds = GetImageAlphaBorder(im, .1f);
            if (bounds.width < 32 || bounds.height < 32)
                errors++;
        }
        UnloadImage(im);
    }
}
int art_stage_nature(int argc, char **argv) {

    if (argc != 3) {

        return 2;
    }
    SetTraceLogLevel(LOG_WARNING);
    if (!strcmp(argv[1], "--check"))
        check_all(argv[2]);
    else {
        characters(argv[1], argv[2]);
        scenery(argv[1], argv[2]);
        ground(argv[1], argv[2]);
        check_all(argv[2]);
    }

    printf("Art: %d errors\n", errors);
    return errors ? 1 : 0;
}
