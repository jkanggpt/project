/* 인계된 휴대 등불·고정 불 원본 시트(8열×4행)를 게임 화소 크기의 아틀라스로 결정적으로 굽습니다.
   - 셀 경계는 round(index × 폭 / 열 수)로 정하고 고정 128화소 셀을 가정하지 않습니다.
   - 생성 원본의 반투명 테두리·붉은 후광·흩어진 작은 불씨 조각을 지웁니다.
   - 프레임마다 받침(바닥 띠)의 가운데를 기준점으로 맞추어 불꽃만 움직이고 몸체는 흔들리지 않습니다.
   - 정수 표본 평균·정수 색 단계만 사용하여 Windows·Linux에서 같은 바이트가 나옵니다. */
#include "carried_lights.h"
#include "../../game/src/render/fire_atlas.h"
#include "raylib.h"
#include "storage.h"
#include "trace.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SHEET_COLUMNS 8
#define SHEET_ROWS 4
#define SAMPLES 8
typedef struct Frame {
    unsigned char *keep;
    int x0, y0, width, height, left, right, top, bottom, center;
} Frame;

static int floor_div(int a, int b) { return a >= 0 ? a / b : -((-a + b - 1) / b); }
static int luminance(Color c) { return (77 * c.r + 150 * c.g + 29 * c.b) / 256; }
static int halo(Color c) { return c.r > 140 && c.g < 80 && c.b < 80 && c.r > c.g * 2; }
static int flame(Color c) { return luminance(c) >= 165 && c.r > c.b + 70; }
/* 8방향 연결 조각 중 작은 불씨·잡티를 지웁니다. */
static void keep_components(unsigned char *keep, int w, int h) {
    int *label = calloc((size_t)w * h, sizeof(int)), *stack = malloc((size_t)w * h * sizeof(int));
    int *area = calloc((size_t)w * h + 1, sizeof(int));
    if (!label || !stack || !area) {
        free(label);
        free(stack);
        free(area);
        return;
    }
    int labels = 0, largest = 0;
    for (int start = 0; start < w * h; start++) {
        if (!keep[start] || label[start])
            continue;
        int top = 0, count = 0;
        label[start] = ++labels;
        stack[top++] = start;
        while (top) {
            int at = stack[--top], x = at % w, y = at / w;
            count++;
            for (int dy = -1; dy <= 1; dy++)
                for (int dx = -1; dx <= 1; dx++) {
                    int xx = x + dx, yy = y + dy;
                    if (xx < 0 || yy < 0 || xx >= w || yy >= h)
                        continue;
                    int n = yy * w + xx;
                    if (keep[n] && !label[n]) {
                        label[n] = labels;
                        stack[top++] = n;
                    }
                }
        }
        area[labels] = count;
        if (count > largest)
            largest = count;
    }
    int minimum = largest / 40 > 90 ? largest / 40 : 90;
    for (int i = 0; i < w * h; i++)
        if (keep[i] && area[label[i]] < minimum)
            keep[i] = 0;
    free(label);
    free(stack);
    free(area);
}
static int frame_read(const Image *sheet, int column, int row, Frame *f) {
    f->x0 = (column * sheet->width + SHEET_COLUMNS / 2) / SHEET_COLUMNS;
    f->y0 = (row * sheet->height + SHEET_ROWS / 2) / SHEET_ROWS;
    f->width = ((column + 1) * sheet->width + SHEET_COLUMNS / 2) / SHEET_COLUMNS - f->x0;
    f->height = ((row + 1) * sheet->height + SHEET_ROWS / 2) / SHEET_ROWS - f->y0;
    f->keep = calloc((size_t)f->width * f->height, 1);
    if (!f->keep)
        return 0;
    const Color *pixels = sheet->data;
    for (int y = 0; y < f->height; y++)
        for (int x = 0; x < f->width; x++) {
            Color c = pixels[(f->y0 + y) * sheet->width + f->x0 + x];
            f->keep[y * f->width + x] = c.a >= 128;
        }
    /* 투명과 맞닿은 붉은 후광을 세 겹까지 벗깁니다. 불꽃 안쪽의 붉은 화소는 남습니다. */
    for (int pass = 0; pass < 3; pass++)
        for (int y = 0; y < f->height; y++)
            for (int x = 0; x < f->width; x++) {
                int i = y * f->width + x;
                Color c = pixels[(f->y0 + y) * sheet->width + f->x0 + x];
                if (!f->keep[i] || !halo(c))
                    continue;
                int open = x == 0 || y == 0 || x == f->width - 1 || y == f->height - 1 || !f->keep[i - 1] ||
                           !f->keep[i + 1] || !f->keep[i - f->width] || !f->keep[i + f->width];
                if (open)
                    f->keep[i] = 2;
            }
    for (int i = 0; i < f->width * f->height; i++)
        f->keep[i] = f->keep[i] == 1;
    keep_components(f->keep, f->width, f->height);
    f->left = f->width;
    f->top = f->height;
    f->right = f->bottom = -1;
    for (int y = 0; y < f->height; y++)
        for (int x = 0; x < f->width; x++)
            if (f->keep[y * f->width + x]) {
                f->left = x < f->left ? x : f->left;
                f->right = x > f->right ? x : f->right;
                f->top = y < f->top ? y : f->top;
                f->bottom = y > f->bottom ? y : f->bottom;
            }
    if (f->right < 0)
        return 0;
    /* 받침 띠(아래 22%)의 가운데가 프레임마다 같은 칸 위치에 오도록 합니다. */
    int band = f->bottom - (f->bottom - f->top) * 22 / 100, sum = 0, count = 0;
    for (int y = band; y <= f->bottom; y++)
        for (int x = 0; x < f->width; x++)
            if (f->keep[y * f->width + x]) {
                sum += x;
                count++;
            }
    f->center = count ? (sum + count / 2) / count : (f->left + f->right) / 2;
    return 1;
}
static int pack_row(const Image *sheet, int row, const GameFireRow *spec, int cell, Image *atlas) {
    Frame frames[SHEET_COLUMNS] = {{0}};
    int ok = 1, extent = 0;
    for (int c = 0; c < SHEET_COLUMNS && ok; c++) {
        ok = frame_read(sheet, c, row, &frames[c]);
        if (ok && frames[c].bottom - frames[c].top + 1 > extent)
            extent = frames[c].bottom - frames[c].top + 1;
    }
    const Color *pixels = sheet->data;
    Color *out = atlas->data;
    for (int c = 0; c < SHEET_COLUMNS && ok; c++) {
        const Frame *f = &frames[c];
        for (int ty = 1; ty < cell; ty++)
            for (int tx = 1; tx < cell - 1; tx++) {
                int dx = tx - spec->base_x, dy = ty - spec->base_y, covered = 0, r = 0, g = 0, b = 0;
                for (int sy = 0; sy < SAMPLES; sy++)
                    for (int sx = 0; sx < SAMPLES; sx++) {
                        int nx = 2 * SAMPLES * dx - SAMPLES + 2 * sx + 1, ny = 2 * SAMPLES * (dy - 1) + 2 * sy + 1;
                        int x = f->center + floor_div(nx * extent, 2 * SAMPLES * spec->height);
                        int y = f->bottom + 1 + floor_div(ny * extent, 2 * SAMPLES * spec->height);
                        if (x < 0 || y < 0 || x >= f->width || y >= f->height || !f->keep[y * f->width + x])
                            continue;
                        Color s = pixels[(f->y0 + y) * sheet->width + f->x0 + x];
                        covered++;
                        r += s.r;
                        g += s.g;
                        b += s.b;
                    }
                if (covered * 100 < SAMPLES * SAMPLES * 45)
                    continue;
                /* 채널당 32단계로 줄여 생성 원본의 미세한 색 잡음을 정리합니다. */
                Color q = {(unsigned char)((r / covered + 4) / 8 * 8 > 255 ? 255 : (r / covered + 4) / 8 * 8),
                           (unsigned char)((g / covered + 4) / 8 * 8 > 255 ? 255 : (g / covered + 4) / 8 * 8),
                           (unsigned char)((b / covered + 4) / 8 * 8 > 255 ? 255 : (b / covered + 4) / 8 * 8),
                           255};
                out[(row * cell + ty) * atlas->width + c * cell + tx] = q;
            }
        /* 불꽃이 아닌 몸체의 바깥 화소를 어둡게 하여 기존 원화처럼 윤곽을 세웁니다. */
        Color copy[32 * 32];
        for (int y = 0; y < cell; y++)
            for (int x = 0; x < cell; x++)
                copy[y * cell + x] = out[(row * cell + y) * atlas->width + c * cell + x];
        for (int y = 0; y < cell; y++)
            for (int x = 0; x < cell; x++) {
                Color p = copy[y * cell + x];
                if (!p.a || flame(p))
                    continue;
                int edge = x == 0 || y == 0 || x == cell - 1 || y == cell - 1 || !copy[y * cell + x - 1].a ||
                           !copy[y * cell + x + 1].a || !copy[(y - 1) * cell + x].a || !copy[(y + 1) * cell + x].a;
                if (edge)
                    out[(row * cell + y) * atlas->width + c * cell + x] =
                        (Color){(unsigned char)(p.r * 3 / 4), (unsigned char)(p.g * 3 / 4), (unsigned char)(p.b * 3 / 4),
                                255};
            }
    }
    for (int c = 0; c < SHEET_COLUMNS; c++)
        free(frames[c].keep);
    return ok;
}
static int bake(const char *source, const GameFireRow rows[4], int cell, const char *output, const char *name) {
    int size = 0;
    unsigned char *bytes = art_read_bytes(source, &size);
    Image sheet = bytes ? LoadImageFromMemory(".png", bytes, size) : (Image){0};
    free(bytes);
    if (!sheet.data || cell > 32) {
        if (sheet.data)
            UnloadImage(sheet);
        return 0;
    }
    ImageFormat(&sheet, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    Image atlas = GenImageColor(SHEET_COLUMNS * cell, SHEET_ROWS * cell, BLANK);
    int ok = atlas.data != NULL;
    for (int row = 0; ok && row < SHEET_ROWS; row++)
        ok = pack_row(&sheet, row, &rows[row], cell, &atlas);
    char path[GAME_PATH_BYTES];
    int png_size = 0;
    unsigned char *png = ok ? ExportImageToMemory(atlas, ".png", &png_size) : NULL;
    ok = ok && png && game_make_dirs(output) && game_join_path(path, sizeof(path), output, name) &&
         art_write_bytes(path, png, (size_t)png_size);
    MemFree(png);
    UnloadImage(sheet);
    if (atlas.data)
        UnloadImage(atlas);
    return ok;
}
int art_carried_lights(const char *carried, const char *stationary, const char *output) {
    int ok = bake(carried, GAME_CARRIED_ROWS, GAME_CARRIED_CELL, output, "carried_lights.png") &&
             bake(stationary, GAME_STATIONARY_ROWS, GAME_STATIONARY_CELL, output, "stationary_fires.png");
    printf("FIRE ART: carried=%dx%d stationary=%dx%d frames=8 errors=%d\n", GAME_CARRIED_CELL * 8,
           GAME_CARRIED_CELL * 4, GAME_STATIONARY_CELL * 8, GAME_STATIONARY_CELL * 4, !ok);
    return ok ? 0 : 1;
}
/* 크기·이진 알파·칸 경계·받침 고정·불꽃 중심·움직임을 검사합니다. */
static int check_atlas(const char *output, const char *name, const GameFireRow rows[4], int cell, int *animated) {
    char path[GAME_PATH_BYTES];
    int size = 0, errors = 0;
    if (!game_join_path(path, sizeof(path), output, name))
        return 1;
    unsigned char *bytes = game_read_bytes(path, &size);
    Image image = bytes ? LoadImageFromMemory(".png", bytes, size) : (Image){0};
    free(bytes);
    if (!image.data || image.width != cell * 8 || image.height != cell * 4) {
        if (image.data)
            UnloadImage(image);
        fprintf(stderr, "불 아틀라스 크기 오류: %s\n", name);
        return 1;
    }
    ImageFormat(&image, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    const Color *p = image.data;
    for (int row = 0; row < 4; row++) {
        float fx = 0, fy = 0;
        int flames = 0;
        for (int c = 0; c < 8; c++) {
            int occupied = 0, base = 0, top = cell, bottom = -1;
            for (int y = 0; y < cell; y++)
                for (int x = 0; x < cell; x++) {
                    Color q = p[(row * cell + y) * image.width + c * cell + x];
                    errors += q.a != 0 && q.a != 255;
                    if (!q.a)
                        continue;
                    occupied++;
                    errors += x == 0 || x == cell - 1 || y == 0;
                    top = y < top ? y : top;
                    bottom = y > bottom ? y : bottom;
                    if (flame(q)) {
                        fx += x + .5f;
                        fy += y + .5f;
                        flames++;
                    }
                    if (c) {
                        Color first = p[(row * cell + y) * image.width + x];
                        *animated += memcmp(&q, &first, sizeof(q)) != 0;
                    }
                }
            for (int x = 0; x < cell; x++)
                base += p[(row * cell + bottom) * image.width + c * cell + x].a != 0 ? 1 : 0;
            errors += occupied < rows[row].height * 2 || bottom != rows[row].base_y || !base ||
                      bottom - top + 1 > rows[row].height + 1;
        }
        if (flames) {
            fx /= flames;
            fy /= flames;
        }
        int off = !flames || fx < rows[row].flame_x - 1.5f || fx > rows[row].flame_x + 1.5f ||
                  fy < rows[row].flame_y - 1.5f || fy > rows[row].flame_y + 1.5f;
        if (off)
            fprintf(stderr, "불꽃 중심 차이: %s 행 %d 측정 %.1f,%.1f 기준 %.1f,%.1f\n", name, row, fx, fy,
                    rows[row].flame_x, rows[row].flame_y);
        errors += off;
    }
    UnloadImage(image);
    return errors;
}
int art_carried_lights_check(const char *output) {
    int animated = 0;
    int errors = check_atlas(output, "carried_lights.png", GAME_CARRIED_ROWS, GAME_CARRIED_CELL, &animated) +
                 check_atlas(output, "stationary_fires.png", GAME_STATIONARY_ROWS, GAME_STATIONARY_CELL, &animated);
    errors += animated < 64;
    printf("FIRE ART CHECK: rows=8 frames=64 animation_changes=%d errors=%d\n", animated, errors);
    return errors ? 1 : 0;
}
