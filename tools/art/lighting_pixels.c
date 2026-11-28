/* 원본 픽셀·실루엣을 유지하고 생성한 무조명·법선 지침을 적용합니다. */
#include "lighting_pixels.h"
#include "raylib.h"
#include "raymath.h"
#include "storage.h"
#include "trace.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static Image read_image(const char *directory, const char *name, int traced) {
    char path[GAME_PATH_BYTES];
    int size = 0;
    if (!game_join_path(path, sizeof(path), directory, name))
        return (Image){0};
    unsigned char *bytes = traced ? art_read_bytes(path, &size) : game_read_bytes(path, &size);
    Image image = bytes ? LoadImageFromMemory(".png", bytes, size) : (Image){0};
    free(bytes);
    if (image.data)
        ImageFormat(&image, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    return image;
}
static int write_image(const char *directory, const char *name, Image image, int traced) {
    char path[GAME_PATH_BYTES];
    int size = 0;
    unsigned char *bytes = ExportImageToMemory(image, ".png", &size);
    int ok = bytes && game_join_path(path, sizeof(path), directory, name);
    if (ok)
        ok =
            traced ? art_write_bytes(path, bytes, (size_t)size) : game_write_bytes(path, bytes, (size_t)size);
    MemFree(bytes);
    return ok;
}
int art_pixel_reference(const char *root, const char *output) {
    const char *paths[] = {"game/assets/buildings/house_0.png", "game/assets/terrain/trees.png",
                           "game/assets/units/villagers.png",   "game/assets/terrain/terrain.png",
                           "game/assets/roads/base.png",        "game/assets/terrain/rocks.png"};
    const Rectangle rectangles[] = {{0, 0, 256, 282}, {256, 0, 128, 128}, {0, 0, 96, 96},
                                    {0, 0, 64, 64},   {0, 0, 64, 64},     {128, 0, 128, 128}};
    const int scales[] = {1, 2, 3, 4, 4, 2};
    Image sheet = GenImageColor(PIXEL_LIGHT_WIDTH, PIXEL_LIGHT_HEIGHT, BLANK);
    int errors = !sheet.data || !game_make_dirs(output);
    for (int kind = 0; kind < 6 && !errors; kind++) {
        Image original = read_image(root, paths[kind], 0);
        if (!original.data) {
            errors++;
            break;
        }
        Image frame = ImageFromImage(original, rectangles[kind]);
        UnloadImage(original);
        if (!frame.data) {
            errors++;
            break;
        }
        ImageResizeNN(&frame, frame.width * scales[kind], frame.height * scales[kind]);
        int x = kind % 3 * PIXEL_LIGHT_CELL + (PIXEL_LIGHT_CELL - frame.width) / 2;
        int y = kind / 3 * PIXEL_LIGHT_CELL + 312 - frame.height;
        for (int yy = 0; yy < frame.height; yy++)
            for (int xx = 0; xx < frame.width; xx++)
                ((Color *)sheet.data)[(y + yy) * sheet.width + x + xx] =
                    ((Color *)frame.data)[yy * frame.width + xx];
        UnloadImage(frame);
    }
    if (!errors)
        errors += !write_image(output, "originals.png", sheet, 0);
    if (sheet.data)
        UnloadImage(sheet);
    printf("PIXEL REFERENCE: types=6 errors=%d\n", errors);
    return errors ? 1 : 0;
}
static float luminance(Color c) { return .2126f * c.r + .7152f * c.g + .0722f * c.b; }
static unsigned char byte(float value) { return (unsigned char)fminf(255, fmaxf(0, floorf(value + .5f))); }
static float local_light(const Image *image, int x, int y, Color material) {
    float sum = 0, count = 0;
    float material_sum = material.r + material.g + material.b + 1.0f;
    for (int dy = -10; dy <= 10; dy += 2)
        for (int dx = -10; dx <= 10; dx += 2) {
            int xx = x + dx, yy = y + dy;
            if (xx < 0 || yy < 0 || xx >= image->width || yy >= image->height)
                continue;
            Color c = ((Color *)image->data)[yy * image->width + xx];
            if (c.a < 128)
                continue;
            float total = c.r + c.g + c.b + 1.0f;
            if (fabsf(c.r / total - material.r / material_sum) +
                    fabsf(c.g / total - material.g / material_sum) >
                .22f)
                continue;
            sum += luminance(c);
            count++;
        }
    return count ? sum / count : luminance(material);
}
int art_pixel_lighting(const char *source, const char *output) {
    Image original = read_image(source, "originals.png", 1), guide = read_image(source, "unlit_guide.png", 1),
          normal_guide = read_image(source, "normal_guide.png", 1);
    int errors = 0;
    if (!original.data || !guide.data || !normal_guide.data || original.width != PIXEL_LIGHT_WIDTH ||
        original.height != PIXEL_LIGHT_HEIGHT)
        errors++;
    Image albedo = {0}, normals = {0}, surface = {0};
    if (!errors) {
        ImageResizeNN(&guide, PIXEL_LIGHT_WIDTH, PIXEL_LIGHT_HEIGHT);
        ImageResizeNN(&normal_guide, PIXEL_LIGHT_WIDTH, PIXEL_LIGHT_HEIGHT);
        albedo = GenImageColor(PIXEL_LIGHT_WIDTH, PIXEL_LIGHT_HEIGHT, BLANK);
        normals = GenImageColor(PIXEL_LIGHT_WIDTH, PIXEL_LIGHT_HEIGHT, BLANK);
        surface = GenImageColor(PIXEL_LIGHT_WIDTH, PIXEL_LIGHT_HEIGHT, BLANK);
        if (!albedo.data || !normals.data || !surface.data)
            errors++;
    }
    const int grids[] = {1, 2, 3, 4, 4, 2};
    const int left[] = {32, 32, 16, 32, 32, 32}, top[] = {30, 56, 24, 56, 56, 56};
    if (!errors)
        for (int y = 0; y < PIXEL_LIGHT_HEIGHT; y++)
            for (int x = 0; x < PIXEL_LIGHT_WIDTH; x++) {
                int index = y * PIXEL_LIGHT_WIDTH + x, kind = y / 320 * 3 + x / 320;
                Color c = ((Color *)original.data)[index];
                if (!c.a)
                    continue;
                int cx = x % 320, cy = y % 320;
                Color target = ((Color *)guide.data)[index];
                float before = local_light(&original, x, y, c), after = local_light(&guide, x, y, target);
                float gain = target.a >= 128 ? fminf(1.55f, fmaxf(.72f, (after + 8) / (before + 8))) : 1;
                float maximum = fmaxf(c.r, fmaxf(c.g, c.b));
                if (maximum > 0)
                    gain = fminf(gain, fmaxf(250, maximum) / maximum);
                /* 색상·선화·세부 픽셀 대비는 원본에 남기고 큰 방향 명암만 완화합니다. */
                ((Color *)albedo.data)[index] =
                    (Color){byte(c.r * gain), byte(c.g * gain), byte(c.b * gain), c.a};
                int gx = x / 320 * 320 + left[kind] + (cx - left[kind]) / grids[kind] * grids[kind] +
                         grids[kind] / 2;
                int gy = y / 320 * 320 + top[kind] + (cy - top[kind]) / grids[kind] * grids[kind] +
                         grids[kind] / 2;
                gx = gx < 0 ? 0 : gx >= PIXEL_LIGHT_WIDTH ? PIXEL_LIGHT_WIDTH - 1 : gx;
                gy = gy < 0 ? 0 : gy >= PIXEL_LIGHT_HEIGHT ? PIXEL_LIGHT_HEIGHT - 1 : gy;
                Color encoded = ((Color *)normal_guide.data)[gy * PIXEL_LIGHT_WIDTH + gx];
                Vector3 n = {encoded.r / 127.5f - 1, encoded.g / 127.5f - 1, encoded.b / 127.5f - 1};
                if (encoded.a < 128 || Vector3Length(n) < .3f)
                    n = (Vector3){0, 0, 1};
                /* 생성 지침의 카메라 법선을 원화에 맞는 월드 표면 축으로 옮깁니다. */
                n = Vector3Normalize(n);
                if (kind == 3 || kind == 4) {
                    n = (Vector3){n.x, -n.y, fmaxf(.65f, fabsf(n.z))};
                } else if (kind == 0) {
                    int roof = cy < 183 && !(cx > 126 && cx < 191 && cy > 121 && cy < 166);
                    Vector3 base = roof ? Vector3Normalize((Vector3){cx < 160 ? -.18f : .18f, .48f, .86f})
                                        : (Vector3){0, 1, 0};
                    Vector3 right =
                        Vector3Normalize(Vector3Subtract((Vector3){1, 0, 0}, Vector3Scale(base, base.x)));
                    Vector3 up = Vector3Normalize(Vector3CrossProduct(right, base));
                    float dx = fminf(.8f, fmaxf(-.8f, n.x / fmaxf(.4f, n.z))) * .70f;
                    float dy = fminf(.8f, fmaxf(-.8f, n.y / fmaxf(.4f, n.z))) * .70f;
                    n = Vector3Add(base, Vector3Add(Vector3Scale(right, dx), Vector3Scale(up, dy)));
                } else {
                    n = (Vector3){n.x, (n.z - n.y) * .70710678f, (n.z + n.y) * .70710678f};
                }
                n = Vector3Normalize(n);
                ((Color *)normals.data)[index] =
                    (Color){byte((n.x * .5f + .5f) * 255), byte((n.y * .5f + .5f) * 255),
                            byte((n.z * .5f + .5f) * 255), c.a};
                float height = kind == 3 || kind == 4 ? 0 : fmaxf(0, (312 - cy) * .72f);
                unsigned h = (unsigned)floorf(height * 256 + .5f);
                int hx = cx - 32, hy = cy - 30;
                int window = kind == 0 &&
                             ((hx > 102 && hx < 149 && hy > 74 && hy < 116) ||
                              (hy > 145 && hy < 189 && ((hx > 48 && hx < 100) || (hx > 150 && hx < 202))));
                int lamp =
                    kind == 0 && hy > 190 && hy < 235 && ((hx > 34 && hx < 68) || (hx > 174 && hx < 216));
                unsigned char emission =
                    (window || lamp) && c.r > 140 && c.g > 90 && c.b < c.g * .85f ? (lamp ? 230 : 145) : 0;
                ((Color *)surface.data)[index] = (Color){(unsigned char)(h >> 8), (unsigned char)(h & 255),
                                                         emission, (unsigned char)(kind + 1)};
            }
    if (!errors) {
        errors += !write_image(output, "pixel_albedo.png", albedo, 1);
        errors += !write_image(output, "pixel_normals.png", normals, 1);
        errors += !write_image(output, "pixel_surface.png", surface, 1);
    }
    if (original.data)
        UnloadImage(original);
    if (guide.data)
        UnloadImage(guide);
    if (normal_guide.data)
        UnloadImage(normal_guide);
    if (albedo.data)
        UnloadImage(albedo);
    if (normals.data)
        UnloadImage(normals);
    if (surface.data)
        UnloadImage(surface);
    printf("PIXEL LIGHTING: original_silhouettes=6 errors=%d\n", errors);
    return errors ? 1 : 0;
}

/* 실제 스프라이트를 가까운 이웃 필터로 겹쳐 작은 비교 지도를 만듭니다. */
static void scene_sprite(Image *buffers, const Image *atlases, int kind, int center, int foot, int width,
                         int owner) {
    const int left[] = {32, 32, 16, 32, 32, 32}, top[] = {30, 56, 24, 56, 56, 56};
    const int widths[] = {256, 256, 288, 256, 256, 256}, heights[] = {282, 256, 288, 256, 256, 256};
    float scale = (float)width / widths[kind];
    int height = (int)floorf(heights[kind] * scale + .5f), x0 = center - width / 2, y0 = foot - height;
    for (int y = 0; y < height; y++)
        for (int x = 0; x < width; x++) {
            int xx = x0 + x, yy = y0 + y;
            if (xx < 0 || yy < 0 || xx >= 512 || yy >= 320)
                continue;
            int sx = kind % 3 * 320 + left[kind] + (int)((x + .5f) / scale);
            int sy = kind / 3 * 320 + top[kind] + (int)((y + .5f) / scale);
            int source_index = sy * PIXEL_LIGHT_WIDTH + sx, dest = yy * 512 + xx;
            if (((Color *)atlases[0].data)[source_index].a < 128)
                continue;
            for (int pass = 0; pass < 4; pass++) {
                Color pixel = ((Color *)atlases[pass].data)[source_index];
                if (pass < 3)
                    pixel.a = 255;
                ((Color *)buffers[pass].data)[dest] = pixel;
            }
            Color *surface = &((Color *)buffers[3].data)[dest];
            float z = (surface->r + surface->g / 256.0f) * scale;
            unsigned fixed = (unsigned)floorf(z * 256 + .5f);
            surface->r = (unsigned char)(fixed >> 8);
            surface->g = (unsigned char)(fixed & 255);
            unsigned char identity = (unsigned char)owner;
            ((Color *)buffers[5].data)[dest] = (Color){identity, identity, identity, 255};
        }
}
int art_pixel_scene(const char *root, const char *output) {
    char directory[GAME_PATH_BYTES];
    if (!game_make_dirs(output) ||
        !game_join_path(directory, sizeof(directory), root, "game/assets/lighting_lab"))
        return 1;
    Image atlases[4] = {{0}}, buffers[6] = {{0}};
    if (!game_join_path(directory, sizeof(directory), root, "art/sources/lighting_pixels"))
        return 1;
    atlases[0] = read_image(directory, "originals.png", 0);
    if (!game_join_path(directory, sizeof(directory), root, "game/assets/lighting_lab"))
        return 1;
    atlases[1] = read_image(directory, "pixel_albedo.png", 0);
    atlases[2] = read_image(directory, "pixel_normals.png", 0);
    atlases[3] = read_image(directory, "pixel_surface.png", 0);
    int errors = 0;
    for (int i = 0; i < 4; i++)
        if (!atlases[i].data)
            errors++;
    for (int i = 0; i < 6; i++) {
        buffers[i] = GenImageColor(512, 320, BLANK);
        if (!buffers[i].data)
            errors++;
    }
    if (!errors) {
        for (int y = 0; y < 10; y++)
            for (int x = 0; x < 16; x++) {
                int kind = y == 8 || (x == 4 && y >= 6 && y <= 8) ? 4 : 3;
                scene_sprite(buffers, atlases, kind, x * 32 + 16, y * 32 + 32, 32, 0);
            }
        scene_sprite(buffers, atlases, 0, 430, 150, 86, 1);
        scene_sprite(buffers, atlases, 1, 352, 212, 128, 2);
        scene_sprite(buffers, atlases, 0, 160, 218, 144, 3);
        scene_sprite(buffers, atlases, 2, 252, 272, 42, 4);
        scene_sprite(buffers, atlases, 5, 428, 280, 80, 5);
        scene_sprite(buffers, atlases, 2, 72, 286, 38, 6);
        for (int i = 0; i < 512 * 320; i++) {
            Color *surface = &((Color *)buffers[3].data)[i];
            ((Color *)buffers[4].data)[i] = (Color){surface->a, surface->a, surface->a, 255};
            surface->a = 255;
        }
        const char *names[] = {"original.png", "albedo.png",   "normals.png",
                               "surface.png",  "material.png", "owners.png"};
        for (int i = 0; i < 6; i++)
            errors += !write_image(output, names[i], buffers[i], 0);
        const char *json = "{\"width\":512,\"height\":320,\"contacts\":[[352,212,25,7,5,0.27],"
                           "[252,272,7,3,2,0.24],[428,280,27,8,4,0.28],[72,286,6,3,2,0.24]],\"lamps\":[["
                           "116,224,44,115],[198,224,44,115],[405,153,26,80],[454,153,26,80]],"
                           "\"lampOwners\":[3,3,1,1],\"casters\":["
                           "[2,352,212,64,42,24,32,0.78],[2,352,212,24,5,5,24,0.86],"
                           "[4,252,272,16,6,4,16,0.84],"
                           "[5,428,280,17,26,14,17,0.92],[6,72,286,14,5.5,3.8,14,0.84]],"
                           "\"buildings\":[{\"owner\":1,\"opacity\":0.94,\"walls\":[398,128,0,462,150,48],"
                           "\"roof\":[392,124,468,154,48,78],\"chimney\":[448,128,68,456,136,94]},"
                           "{\"owner\":3,\"opacity\":0.94,\"walls\":[106,182,0,214,218,80],"
                           "\"roof\":[96,176,224,224,80,130],\"chimney\":[194,185,114,208,199,148]}]}\n";
        char path[GAME_PATH_BYTES];
        errors += !(game_join_path(path, sizeof(path), output, "scene.json") &&
                    game_write_bytes(path, json, strlen(json)));
    }
    for (int i = 0; i < 4; i++)
        if (atlases[i].data)
            UnloadImage(atlases[i]);
    for (int i = 0; i < 6; i++)
        if (buffers[i].data)
            UnloadImage(buffers[i]);
    printf("PIXEL SCENE: unchanged_sprite_shapes=6 errors=%d\n", errors);
    return errors ? 1 : 0;
}

int art_pixel_check(const char *root) {
    char path[GAME_PATH_BYTES];
    Image images[4] = {{0}};
    if (!game_join_path(path, sizeof(path), root, "art/sources/lighting_pixels"))
        return 1;
    images[0] = read_image(path, "originals.png", 0);
    if (!game_join_path(path, sizeof(path), root, "game/assets/lighting_lab"))
        return 1;
    images[1] = read_image(path, "pixel_albedo.png", 0);
    images[2] = read_image(path, "pixel_normals.png", 0);
    images[3] = read_image(path, "pixel_surface.png", 0);
    int errors = 0, counts[6] = {0};
    double sums[6] = {0}, squares[6] = {0}, vertical[6] = {0};
    for (int i = 0; i < 4; i++)
        if (!images[i].data || images[i].width != 960 || images[i].height != 640)
            errors++;
    if (!errors)
        for (int y = 0; y < 640; y++)
            for (int x = 0; x < 960; x++) {
                int index = y * 960 + x, kind = y / 320 * 3 + x / 320;
                Color original = ((Color *)images[0].data)[index], albedo = ((Color *)images[1].data)[index],
                      normal = ((Color *)images[2].data)[index], surface = ((Color *)images[3].data)[index];
                if (original.a != albedo.a || original.a != normal.a)
                    errors++;
                if (!original.a)
                    continue;
                if (surface.a != kind + 1)
                    errors++;
                Vector3 n = {normal.r / 127.5f - 1, normal.g / 127.5f - 1, normal.b / 127.5f - 1};
                if (fabsf(Vector3Length(n) - 1) > .015f)
                    errors++;
                /* 어두운 픽셀도 채널당 반올림 오차 안에서 같은 색 비율을 유지해야 합니다. */
                if (fabsf(albedo.r * (float)original.g - albedo.g * (float)original.r) >
                        .501f * (original.r + original.g) + .01f ||
                    fabsf(albedo.g * (float)original.b - albedo.b * (float)original.g) >
                        .501f * (original.g + original.b) + .01f ||
                    fabsf(albedo.r * (float)original.b - albedo.b * (float)original.r) >
                        .501f * (original.r + original.b) + .01f)
                    errors++;
                counts[kind]++;
                sums[kind] += n.x;
                squares[kind] += n.x * n.x;
                vertical[kind] += n.z;
            }
    for (int i = 0; i < 6; i++) {
        double variance = counts[i] ? squares[i] / counts[i] - sums[i] / counts[i] * sums[i] / counts[i] : 0;
        if (counts[i] < 100 || variance < .003)
            errors++;
        if ((i == 3 || i == 4) && vertical[i] / counts[i] < .75)
            errors++;
        printf("PIXEL MATERIAL: kind=%d pixels=%d normal_variance=%.4f\n", i, counts[i], variance);
    }
    for (int i = 0; i < 4; i++)
        if (images[i].data)
            UnloadImage(images[i]);
    printf("PIXEL CHECK: original_alpha=preserved original_hues=preserved errors=%d\n", errors);
    return errors ? 1 : 0;
}
