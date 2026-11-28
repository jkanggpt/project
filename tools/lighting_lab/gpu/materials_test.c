/* 굽힌 재료의 반전·누운 방향·지면 표식을 실제 GPU 경로로 검사합니다. */
#include "gpu.h"
#include "raymath.h"
#include "storage.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef CASTLE_GPU_ROOT
#define CASTLE_GPU_ROOT "."
#endif
static Image read_image(const char *relative) {
    char path[GAME_PATH_BYTES];
    int size = 0;
    if (!game_join_path(path, sizeof(path), CASTLE_GPU_ROOT "/game/assets", relative))
        return (Image){0};
    unsigned char *bytes = game_read_bytes(path, &size);
    Image result = bytes ? LoadImageFromMemory(".png", bytes, size) : (Image){0};
    free(bytes);
    if (result.data)
        ImageFormat(&result, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    return result;
}
static int constant_direction_test(CastleGpu *gpu) {
    Image image = GenImageColor(4, 4, WHITE), map = GenImageColor(1, 1, (Color){60, 180, 220, 255});
    Texture2D albedo = LoadTextureFromImage(image), normal = LoadTextureFromImage(map);
    int errors = 0;
    for (int mode = 0; mode < 5; mode++) {
        CastleGpuFrame frame = {.width = 320,
                                .height = 240,
                                .map_width = 20,
                                .map_height = 15,
                                .pixel_scale = 1,
                                .scale = 1,
                                .view = 2,
                                .natural = mode == 2};
        CastleGpuSprite s = {0};
        s.destination[0] = 96;
        s.destination[1] = 96;
        s.destination[2] = s.destination[3] = 64;
        s.source[2] = mode == 1 ? -1.0f : 1.0f;
        s.source[3] = 1;
        s.source[0] = mode == 1 ? 1.0f : 0.0f;
        memcpy(s.normal_source, s.source, sizeof(s.source));
        for (int j = 0; j < 4; j++)
            s.tint[j] = 1;
        if (mode == 3)
            s.tint[3] = .5f;
        if (mode == 4)
            s.tint[3] = 0;
        s.shape[0] = 160;
        s.shape[1] = 1;
        s.shape[2] = .4f;
        s.tags[1] = mode == 2 ? GPU_MATERIAL_ROAD : GPU_MATERIAL_ACTOR;
        s.tags[2] = mode >= 2 ? (mode == 2 ? 14u : 12u) : 4u;
        Vector3 n = Vector3Normalize((Vector3){60 / 127.5f - 1, 180 / 127.5f - 1, 220 / 127.5f - 1});
        Vector2 sample = {128, 128};
        if (mode == 1)
            n.x = -n.x;
        if (mode >= 2) {
            float c = cosf(.4f), sn = sinf(.4f);
            n = (Vector3){n.x, -n.z, n.y};
            n = Vector3Normalize((Vector3){c * n.x - sn * n.y, sn * n.x + c * n.y, fmaxf(n.z, .15f)});
            sample = (Vector2){128 + sn * 32, 160 - c * 32};
        }
        int ok = castle_gpu_begin(gpu, &frame) &&
                 castle_gpu_sprite(gpu, albedo, normal, (Texture2D){0}, &s) && castle_gpu_render(gpu);
        Image result = ok ? LoadImageFromTexture(castle_gpu_output(gpu)) : (Image){0};
        if (result.data) {
            ImageFlipVertical(&result);
            ImageFormat(&result, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
            Color found = ((Color *)result.data)[(int)sample.y * result.width + (int)sample.x];
            if (mode < 3)
                errors += abs(found.r - (int)((n.x * .5f + .5f) * 255)) > 3 ||
                          abs(found.g - (int)((n.y * .5f + .5f) * 255)) > 3 ||
                          abs(found.b - (int)((n.z * .5f + .5f) * 255)) > 3;
            else {
                int covered = 0;
                for (int i = 0; i < result.width * result.height; i++) {
                    Color pixel = ((Color *)result.data)[i];
                    covered += pixel.b > 100 && pixel.g < 100;
                }
                errors += mode == 3 ? covered < 1900 || covered > 2200 : covered != 0;
                printf("GPU MATERIAL FADE: mode=%d covered=%d\n", mode, covered);
            }
            UnloadImage(result);
        } else
            errors++;
    }
    UnloadTexture(albedo);
    UnloadTexture(normal);
    UnloadImage(image);
    UnloadImage(map);
    printf("GPU MATERIAL AXES: cases=5 errors=%d\n", errors);
    return errors;
}
int main(void) {
    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(320, 240, "재료 방향 검사");
    CastleGpu *gpu = castle_gpu_create();
    int errors = !gpu, cases = 0;
    if (gpu)
        errors += constant_direction_test(gpu);
    const char *paths[] = {"buildings/house_1.png", "terrain/rocks.png", "units/villagers.png",
                           "roads/bridges.png"};
    const Rectangle sources[] = {
        {0, 0, 256, 282}, {128, 256, 128, 128}, {96, 0, 96, 96}, {128, 128, 128, 128}};
    for (int type = 0; gpu && type < 4; type++) {
        char path[GAME_PATH_BYTES];
        snprintf(path, sizeof(path), "materials/albedo/%s", paths[type]);
        Image albedo = read_image(path);
        snprintf(path, sizeof(path), "materials/normals/%s", paths[type]);
        Image normal = read_image(path);
        if (!albedo.data || !normal.data) {
            errors++;
            break;
        }
        Texture2D color = LoadTextureFromImage(albedo), normals = LoadTextureFromImage(normal);
        SetTextureFilter(color, TEXTURE_FILTER_POINT);
        SetTextureFilter(normals, TEXTURE_FILTER_POINT);
        Rectangle source = sources[type];
        if (source.height > albedo.height)
            source.height = (float)albedo.height;
        for (int mode = 0; mode < 3; mode++) {
            CastleGpuFrame frame = {.width = 320,
                                    .height = 240,
                                    .map_width = 20,
                                    .map_height = 15,
                                    .pixel_scale = 1,
                                    .scale = 1,
                                    .ambient = .3f,
                                    .detail = 1,
                                    .view = 2};
            CastleGpuSprite s = {0};
            float width = fminf(128, source.width), height = fminf(128, source.height);
            s.destination[0] = 64;
            s.destination[1] = 48;
            s.destination[2] = width;
            s.destination[3] = height;
            s.source[0] = source.x / albedo.width;
            s.source[1] = source.y / albedo.height;
            s.source[2] = source.width / albedo.width;
            s.source[3] = source.height / albedo.height;
            if (mode == 1) {
                s.source[0] += s.source[2];
                s.source[2] = -s.source[2];
            }
            memcpy(s.normal_source, s.source, sizeof(s.source));
            for (int j = 0; j < 4; j++)
                s.tint[j] = 1;
            s.shape[0] = 48 + height;
            s.shape[1] = 80;
            s.tags[0] = 1;
            s.tags[1] = type == 0   ? GPU_MATERIAL_BUILDING
                        : type == 1 ? GPU_MATERIAL_ROCK
                        : type == 2 ? GPU_MATERIAL_ACTOR
                                    : GPU_MATERIAL_ROAD;
            s.tags[2] = mode == 2 ? 12u : type == 3 ? 6u : 4u;
            if (mode == 2)
                s.shape[2] = .4f;
            int ok = castle_gpu_begin(gpu, &frame) &&
                     castle_gpu_sprite(gpu, color, normals, (Texture2D){0}, &s) && castle_gpu_render(gpu);
            Image actual = ok ? LoadImageFromTexture(castle_gpu_output(gpu)) : (Image){0};
            if (!actual.data) {
                errors++;
                continue;
            }
            ImageFlipVertical(&actual);
            ImageFormat(&actual, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
            int checked = 0, wrong = 0;
            for (int y = 4; y < (int)height - 4; y += 3)
                for (int x = 4; x < (int)width - 4; x += 3) {
                    float u = (x + .5f) / width, v = (y + .5f) / height;
                    int sx = (int)(source.x + (mode == 1 ? 1 - u : u) * source.width),
                        sy = (int)(source.y + v * source.height);
                    Color a = ((Color *)albedo.data)[sy * albedo.width + sx];
                    if (a.a < 240)
                        continue;
                    int nx = sx * normal.width / albedo.width, ny = sy * normal.height / albedo.height;
                    Color encoded = ((Color *)normal.data)[ny * normal.width + nx];
                    Vector3 expected = Vector3Normalize(
                        (Vector3){encoded.r / 127.5f - 1, encoded.g / 127.5f - 1, encoded.b / 127.5f - 1});
                    if (mode == 1)
                        expected.x = -expected.x;
                    Vector2 position = {64 + x + .5f, 48 + y + .5f};
                    if (mode == 2) {
                        Vector3 n = {expected.x, -expected.z, expected.y};
                        float c = cosf(.4f), sn = sinf(.4f);
                        expected = Vector3Normalize(
                            (Vector3){c * n.x - sn * n.y, sn * n.x + c * n.y, fmaxf(n.z, .15f)});
                        float dx = x + .5f - width * .5f, dy = y + .5f - height;
                        position =
                            (Vector2){64 + width * .5f + c * dx - sn * dy, 48 + height + sn * dx + c * dy};
                    }
                    int px = (int)position.x, py = (int)position.y;
                    if (px < 0 || py < 0 || px >= actual.width || py >= actual.height)
                        continue;
                    /* 회전한 격자의 재표본 오차는 별도로 허용합니다. */
                    if (mode == 2)
                        continue;
                    Color found = ((Color *)actual.data)[py * actual.width + px];
                    int tolerance = 4;
                    wrong += abs(found.r - (int)((expected.x * .5f + .5f) * 255)) > tolerance ||
                             abs(found.g - (int)((expected.y * .5f + .5f) * 255)) > tolerance ||
                             abs(found.b - (int)((expected.z * .5f + .5f) * 255)) > tolerance;
                    checked++;
                }
            if (mode == 2) {
                int covered = 0;
                for (int i = 0; i < actual.width * actual.height; i++) {
                    Color c = ((Color *)actual.data)[i];
                    if (c.b > 130 && c.g < 100)
                        covered++;
                }
                errors += covered < 8;
            } else
                errors += checked < 8 || wrong > checked / 50;
            printf("GPU MATERIAL: type=%d mode=%d samples=%d mismatches=%d\n", type, mode, checked, wrong);
            errors += castle_gpu_validate(gpu);
            cases++;
            UnloadImage(actual);
        }
        UnloadTexture(color);
        UnloadTexture(normals);
        UnloadImage(albedo);
        UnloadImage(normal);
    }
    castle_gpu_destroy(gpu);
    errors += castle_gpu_live_objects() != 0;
    CloseWindow();
    printf("GPU MATERIALS: cases=%d errors=%d\n", cases, errors);
    return errors ? 1 : 0;
}
