#include "image_ops.h"
#include "pipeline.h"
/* image_ops — 투명 경계·셀 분리·최근접 확대의 공용 구현 */
#include "raylib.h"
#include "storage.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int art_errors;
static const void *cached_pixels;
static int cached_rows, cached_cuts[65];
static int *row_scores, *row_parent;
static int64_t *row_cost;
static const void *column_pixels;
static int column_top, column_bottom, column_count, column_cuts[65];
static void optimal_cuts(const int *scores, int extent, int count, int *cuts) {
    size_t slots = (size_t)(count + 1) * (extent + 1);
    int64_t *cost = malloc(slots * sizeof(*cost));
    int *parent = calloc(slots, sizeof(*parent));
    if (!cost || !parent) {
        art_errors++;
        free(cost);
        free(parent);
        for (int i = 0; i <= count; i++)
            cuts[i] = i * extent / count;
        return;
    }
    for (size_t i = 0; i < slots; i++)
        cost[i] = INT64_MAX / 4;
    cost[0] = 0;
    int nominal = extent / count, low = nominal / 2, high = nominal * 17 / 10;
    for (int row = 1; row <= count; row++)
        for (int y = row * low; y <= extent && y <= row * high; y++) {
            if (row == count && y != extent)
                continue;
            for (int length = low; length <= high && length <= y; length++) {
                int previous = y - length;
                int64_t value = cost[(size_t)(row - 1) * (extent + 1) + previous] +
                                (int64_t)(length - nominal) * (length - nominal) / 8;
                if (row < count)
                    value += (int64_t)scores[y] * 1000;
                size_t index = (size_t)row * (extent + 1) + y;
                if (value < cost[index]) {
                    cost[index] = value;
                    parent[index] = previous;
                }
            }
        }
    int y = extent;
    cuts[count] = y;
    for (int row = count; row > 0; row--) {
        y = parent[(size_t)row * (extent + 1) + y];
        cuts[row - 1] = y;
    }
    free(cost);
    free(parent);
}
Image art_read_png(const char *dir, const char *name) {
    cached_pixels = NULL;
    column_pixels = NULL;
    char path[GAME_PATH_BYTES];
    int count = 0;
    Image im = {0};
    if (!game_join_path(path, sizeof(path), dir, name)) {
        art_errors++;
        return im;
    }
    unsigned char *p = game_read_bytes(path, &count);
    if (p) {
        im = LoadImageFromMemory(".png", p, count);
        free(p);
    }
    if (!im.data)
        art_errors++;
    else
        ImageFormat(&im, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    return im;
}
void art_save_png(const char *dir, const char *name, Image im) {
    char path[GAME_PATH_BYTES];
    int count = 0;
    unsigned char *p = ExportImageToMemory(im, ".png", &count);
    if (!p || !game_join_path(path, sizeof(path), dir, name) || !game_write_bytes(path, p, (size_t)count))
        art_errors++;
    MemFree(p);
}
void art_clean(Image *im) {
    Color *p = im->data;
    for (int i = 0; i < im->width * im->height; i++)
        if (p[i].a < 96)
            p[i] = BLANK;
        else
            p[i].a = 255;
}
int art_seam(Image im, int n, int count, int vertical, int start, int end) {
    int extent = vertical ? im.width : im.height;
    if (!n)
        return 0;
    if (n == count)
        return extent;
    if (vertical && count <= 64) {
        if (column_pixels != im.data || column_top != start || column_bottom != end ||
            column_count != count) {
            int *scores = calloc((size_t)im.width + 1, sizeof(int));
            if (!scores) {
                art_errors++;
                return n * extent / count;
            }
            const Color *pixels = im.data;
            for (int x = 0; x < im.width; x++)
                for (int y = start; y < end; y++)
                    scores[x] += pixels[y * im.width + x].a >= 96;
            optimal_cuts(scores, im.width, count, column_cuts);
            free(scores);
            column_pixels = im.data;
            column_top = start;
            column_bottom = end;
            column_count = count;
        }
        return column_cuts[n];
    }
    if (!vertical && count <= 64) {
        if (cached_pixels != im.data || cached_rows != count) {
            size_t slots = (size_t)(count + 1) * (im.height + 1);
            free(row_scores);
            free(row_parent);
            free(row_cost);
            row_scores = calloc((size_t)im.height + 1, sizeof(int));
            row_parent = malloc(slots * sizeof(int));
            row_cost = malloc(slots * sizeof(int64_t));
            if (!row_scores || !row_parent || !row_cost) {
                art_errors++;
                return n * extent / count;
            }
            const Color *pixels = im.data;
            for (int y = 0; y < im.height; y++)
                for (int x = 0; x < im.width; x++)
                    row_scores[y] += pixels[y * im.width + x].a >= 96;
            for (size_t i = 0; i < slots; i++)
                row_cost[i] = INT64_MAX / 4;
            row_cost[0] = 0;
            int nominal = im.height / count, low = nominal / 2, high = nominal * 17 / 10;
            for (int row = 1; row <= count; row++)
                for (int y = row * low; y <= im.height && y <= row * high; y++) {
                    if (row == count && y != im.height)
                        continue;
                    for (int length = low; length <= high && length <= y; length++) {
                        int previous = y - length;
                        int64_t value = row_cost[(size_t)(row - 1) * (im.height + 1) + previous];
                        value += (int64_t)(length - nominal) * (length - nominal) / 8;
                        if (row < count)
                            value += (int64_t)row_scores[y] * 1000;
                        size_t index = (size_t)row * (im.height + 1) + y;
                        if (value < row_cost[index]) {
                            row_cost[index] = value;
                            row_parent[index] = previous;
                        }
                    }
                }
            int y = im.height;
            cached_cuts[count] = y;
            for (int row = count; row > 0; row--) {
                y = row_parent[(size_t)row * (im.height + 1) + y];
                cached_cuts[row - 1] = y;
            }
            cached_pixels = im.data;
            cached_rows = count;
        }
        return cached_cuts[n];
    }
    int nominal = n * extent / count, best = nominal, score = INT32_MAX, radius = extent / count / 3;
    Color *p = im.data;
    for (int x = nominal - radius; x <= nominal + radius; x++) {
        int found = 0;
        for (int y = start; y < end; y++)
            found += p[vertical ? y * im.width + x : x * im.width + y].a >= 96;
        if (found < score || (found == score && abs(x - nominal) < abs(best - nominal))) {
            best = x;
            score = found;
        }
    }
    return best;
}
Image art_cut(Image im, int col, int row, int cols, int rows) {
    int top = art_seam(im, row, rows, 0, 0, im.width), bottom = art_seam(im, row + 1, rows, 0, 0, im.width);
    int left = art_seam(im, col, cols, 1, top, bottom), right = art_seam(im, col + 1, cols, 1, top, bottom);
    Image result = ImageFromImage(
        im, (Rectangle){(float)left, (float)top, (float)(right - left), (float)(bottom - top)});
    art_clean(&result);
    ImageAlphaCrop(&result, .1f);
    return result;
}
void art_paste(Image *atlas, Image frame, int col, int row, int size, float scale) {
    int w = (int)(frame.width * scale + .5f), h = (int)(frame.height * scale + .5f);
    if (w < 1 || h < 1) {
        art_errors++;
        UnloadImage(frame);
        return;
    }
    ImageResizeNN(&frame, w, h);
    ImageDraw(
        atlas, frame, (Rectangle){0, 0, (float)w, (float)h},
        (Rectangle){col * (float)size + (size - w) / 2.0f, (row + 1) * (float)size - h, (float)w, (float)h},
        WHITE);
    UnloadImage(frame);
}
void art_sequence(Image *atlas, Image src, int source_row, int rows, int target_row, int target_col, int cols,
                  int offset) {
    Image frames[4];
    int max = 1;
    for (int f = 0; f < 4; f++) {
        frames[f] = art_cut(src, f + offset, source_row, cols, rows);
        if (frames[f].width > max)
            max = frames[f].width;
        if (frames[f].height > max)
            max = frames[f].height;
    }
    for (int f = 0; f < 4; f++)
        art_paste(atlas, frames[f], target_col + f, target_row, 96, 92.0f / max);
}

void art_image_reset(void) {
    free(row_scores);
    free(row_parent);
    free(row_cost);
    row_scores = NULL;
    row_parent = NULL;
    row_cost = NULL;
    cached_pixels = NULL;
    column_pixels = NULL;
    cached_rows = 0;
    column_count = 0;
}
