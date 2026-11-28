/* 실제 게임과 같은 GPU 경로로 원화·움직임·수명·성능을 검사합니다. */
#include "gpu.h"
#include "storage.h"
#include "platform.h"
#include "raymath.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef CASTLE_GPU_ROOT
#define CASTLE_GPU_ROOT "."
#endif

typedef struct Lab {
    CastleGpu *gpu;
    Texture2D albedo, normal, surface, water, scenery, contact_policy, animal, bridge;
    CastleGpuFrame frame;
    unsigned char terrain[64 * 64];
    int frames, benchmark, hidden, dense, lights, warmup, hud, overflow;
    int natural_scene;
    int shadow_scene, probe_kind, flip_probe;
    int hidden_case;
    Vector2 center;
    unsigned int owner_prefix;
    const char *capture, *report;
    int capture_logical;
    char assets[GAME_PATH_BYTES];
    float azimuth, elevation, start_time, seconds;
    double elapsed_seconds;
    Font font;
} Lab;
typedef struct Sample {
    double wall, phase[5];
    int timed;
    uint64_t frame_id;
} Sample;
static Texture2D texture(const Lab *lab, const char *relative) {
    char path[GAME_PATH_BYTES];
    if (!game_join_path(path, sizeof(path), lab->assets, relative))
        return (Texture2D){0};
    int count = 0;
    unsigned char *bytes = game_read_bytes(path, &count);
    Image image = bytes ? LoadImageFromMemory(".png", bytes, count) : (Image){0};
    free(bytes);
    Texture2D result = image.data ? LoadTextureFromImage(image) : (Texture2D){0};
    if (image.data)
        UnloadImage(image);
    if (result.id)
        SetTextureFilter(result, TEXTURE_FILTER_POINT);
    return result;
}
static float random_value(unsigned int seed) {
    seed ^= seed >> 16;
    seed *= 0x7feb352du;
    seed ^= seed >> 15;
    return (seed & 0xffffu) / 65535.0f;
}
static void sprite(Lab *lab, int kind, unsigned int owner, Rectangle source, Rectangle dest, float scale) {
    if (owner)
        owner |= lab->owner_prefix;
    CastleGpuSprite s = {0};
    s.destination[0] = dest.x;
    s.destination[1] = dest.y;
    s.destination[2] = dest.width;
    s.destination[3] = dest.height;
    s.source[0] = source.x / lab->albedo.width;
    s.source[1] = source.y / lab->albedo.height;
    s.source[2] = source.width / lab->albedo.width;
    s.source[3] = source.height / lab->albedo.height;
    memcpy(s.normal_source, s.source, sizeof(s.source));
    for (int i = 0; i < 4; i++)
        s.tint[i] = 1;
    s.shape[0] = dest.y + dest.height;
    s.shape[1] = scale;
    s.shape[2] = 1.25f;
    s.tags[0] = owner;
    s.tags[1] = (unsigned int)kind;
    s.tags[2] = 1;
    s.tags[3] = owner * 7919;
    (void)castle_gpu_sprite(lab->gpu, lab->albedo, lab->normal, lab->surface, &s);
}
static void original_sprite(Lab *lab, Texture2D texture, int kind, unsigned int owner, Rectangle source,
                            Rectangle destination, int ground) {
    CastleGpuSprite s = {0};
    s.destination[0] = destination.x;
    s.destination[1] = destination.y;
    s.destination[2] = destination.width;
    s.destination[3] = destination.height;
    s.source[0] = source.x / texture.width;
    s.source[1] = source.y / texture.height;
    s.source[2] = source.width / texture.width;
    s.source[3] = source.height / texture.height;
    for (int i = 0; i < 4; i++)
        s.tint[i] = 1;
    s.shape[0] = destination.y + destination.height;
    s.shape[1] = destination.height * .72f;
    s.tags[0] = owner;
    s.tags[1] = (uint32_t)kind;
    s.tags[2] = ground ? 2u : 0u;
    s.tags[3] = owner * 7919u;
    (void)castle_gpu_sprite(lab->gpu, texture, (Texture2D){0}, (Texture2D){0}, &s);
}
static void caster(Lab *lab, unsigned int owner, int dynamic, int kind, float x, float y, float z, float rx,
                   float ry, float rz, float opacity, int contact) {
    owner |= lab->owner_prefix;
    CastleGpuCaster c = {{x, y, z, 0},
                         {rx, ry, rz, opacity},
                         {x, y, rx, ry},
                         {owner, (unsigned int)dynamic, (unsigned int)kind, (unsigned int)contact},
                         {0}};
    if (kind == 1) {
        c.contact[2] = rx;
        c.contact[3] = fmaxf(2, ry * .5f);
    }
    (void)castle_gpu_caster(lab->gpu, &c);
}
static void dense_scene(Lab *lab) {
    memset(lab->terrain, 1, sizeof(lab->terrain));
    for (int y = 0; y < 32; y++)
        for (int x = 0; x < 32; x++)
            sprite(lab, GPU_MATERIAL_GRASS, 0, (Rectangle){32, 376, 256, 256},
                   (Rectangle){x * 32.0f, y * 32.0f, 32, 32}, 0);
    for (int i = 0; i < 100; i++) {
        float x = 45 + (i % 10) * 96.0f, foot = 90 + (i / 10) * 88.0f;
        unsigned int owner = (unsigned int)i + 1;
        sprite(lab, 1, owner, (Rectangle){32, 30, 256, 282}, (Rectangle){x - 25, foot - 55, 50, 55},
               50.0f / 256);
        caster(lab, owner, 0, 0, x, foot - 7, 15, 19, 7, 15, .94f, 1);
        caster(lab, owner, 0, 2, x, foot - 7, 30, 22, 9, 18, .94f, 0);
        if (i < 64) {
            CastleGpuEmitter emitter = {{x + 14, foot - 8, 51, 0}, {(float)owner, 0, 0, 0}};
            (void)castle_gpu_emitter(lab->gpu, &emitter);
        }
        int cell_x = (int)(x / 16), cell_y = (int)(foot / 16);
        for (int yy = cell_y - 1; yy <= cell_y; yy++)
            for (int xx = cell_x - 1; xx <= cell_x + 1; xx++)
                lab->terrain[yy * 64 + xx] = 0;
    }
    for (int i = 0; i < 200; i++) {
        float x = 16 + random_value((unsigned int)i * 31 + 9) * 990,
              foot = 30 + random_value((unsigned int)i * 77 + 31) * 980;
        unsigned int owner = (unsigned int)i + 1000;
        sprite(lab, 2, owner, (Rectangle){352, 56, 256, 256}, (Rectangle){x - 19, foot - 38, 38, 38},
               38.0f / 256);
        caster(lab, owner, 1, 1, x, foot, 22, 12, 7, 11, .78f, 3);
        caster(lab, owner, 0, 1, x, foot, 9, 2, 2, 9, .86f, 0);
    }
    for (int i = 0; i < 500; i++) {
        float x = 16 + random_value((unsigned int)i * 69 + 17) * 990,
              foot = 30 + random_value((unsigned int)i * 41 + 51) * 980;
        x += floorf(sinf(lab->frame.time * .7f + i) * 5);
        unsigned int owner = (unsigned int)i + 2000;
        sprite(lab, 3, owner, (Rectangle){656, 24, 288, 288}, (Rectangle){x - 9, foot - 18, 18, 18},
               18.0f / 288);
        caster(lab, owner, 1, 1, x, foot, 8, 3, 2, 8, .84f, 1);
    }
    for (int i = 0; i < lab->lights; i++) {
        CastleGpuLight light = {
            {128 + (i % 4) * 256.0f, 128 + (i / 4) * 256.0f, 38, 130}, {1.12f, .67f, .28f, .8f}, {0}};
        (void)castle_gpu_light(lab->gpu, &light);
    }
    castle_gpu_terrain(lab->gpu, lab->terrain, 64, 64);
}
static void shadow_scene(Lab *lab) {
    memset(lab->terrain, 1, sizeof(lab->terrain));
    Rectangle source = lab->probe_kind == GPU_MATERIAL_TREE    ? (Rectangle){352, 56, 256, 256}
                       : lab->probe_kind == GPU_MATERIAL_ACTOR ? (Rectangle){656, 24, 288, 288}
                                                               : (Rectangle){672, 376, 256, 256};
    float size = lab->probe_kind == GPU_MATERIAL_TREE    ? 96.0f
                 : lab->probe_kind == GPU_MATERIAL_ACTOR ? 48.0f
                                                         : 80.0f;
    float normal_scale = size / source.width;
    if (lab->flip_probe) {
        source.x += source.width;
        source.width = -source.width;
    }
    sprite(lab, lab->probe_kind, 1, source, (Rectangle){256 - size * .5f, 224 - size, size, size},
           normal_scale);
    if (lab->probe_kind == GPU_MATERIAL_TREE) {
        caster(lab, 1, 1, 1, 256, 224, 45, 31, 18, 22, .78f, 3);
        caster(lab, 1, 0, 1, 256, 224, 18, 4, 4, 18, .86f, 0);
    } else if (lab->probe_kind == GPU_MATERIAL_ACTOR)
        caster(lab, 1, 1, 1, 256, 224, 18, 7.6f, 5, 18, .84f, 1);
    else
        caster(lab, 1, 0, 1, 256, 224, 17, 26, 14, 17, .92f, 1);
    if (!lab->frame.sun_enabled) {
        CastleGpuLight light = {{256, 304, 80, 220}, {1.12f, .73f, .36f, 1}, {0}};
        (void)castle_gpu_light(lab->gpu, &light);
    }
    castle_gpu_terrain(lab->gpu, lab->terrain, lab->frame.map_width, lab->frame.map_height);
}
static void natural_scene(Lab *lab) {
    memset(lab->terrain, 1, sizeof(lab->terrain));
    for (int y = 0; y < 40; y++)
        for (int x = 0; x < 64; x++) {
            float px = x * 16.0f + 8, py = y * 16.0f + 8;
            if ((px - 584) * (px - 584) + (py - 320) * (py - 320) < 88 * 88 ||
                (px - 648) * (px - 648) + (py - 280) * (py - 280) < 52 * 52)
                lab->terrain[y * 64 + x] = 2;
            if ((y == 22 && x >= 24 && x <= 30) || (x == 30 && y >= 19 && y <= 24))
                lab->terrain[y * 64 + x] = 4;
            if (x >= 26 && x <= 29 && y >= 18 && y <= 19)
                lab->terrain[y * 64 + x] = 16;
            if (y == 20 && x >= 30 && x <= 42)
                lab->terrain[y * 64 + x] |= 64u;
        }
    for (int x = 31; x <= 41; x++)
        original_sprite(lab, lab->bridge, 5, 0, (Rectangle){(float)(x % 4) * 128, 0, 128, 128},
                        (Rectangle){x * 16.0f, 320, 16, 16}, 1);
    sprite(lab, 1, 1, (Rectangle){32, 30, 256, 282}, (Rectangle){416, 249.5f, 64, 70.5f}, .25f);
    caster(lab, 1, 0, 0, 448, 315, 18, 24, 7, 18, .94f, 1);
    caster(lab, 1, 0, 2, 448, 315, 36, 28, 10, 20, .94f, 0);
    caster(lab, 1, 0, 0, 466, 309, 58, 3, 3, 9, .94f, 0);
    CastleGpuEmitter emitter = {{466, 309, 67, 0}, {1, 0, 0, 0}};
    (void)castle_gpu_emitter(lab->gpu, &emitter);
    sprite(lab, 2, 2, (Rectangle){352, 56, 256, 256}, (Rectangle){490.5f, 213, 43, 43}, 43.0f / 256);
    caster(lab, 2, 1, 1, 512, 256, 23, 14, 8, 12, .78f, 3);
    caster(lab, 2, 0, 1, 512, 256, 10, 2, 2, 10, .86f, 0);
    sprite(lab, 6, 3, (Rectangle){672, 376, 256, 256}, (Rectangle){490, 340, 28, 28}, 28.0f / 256);
    caster(lab, 3, 0, 1, 504, 368, 6, 9, 5, 6, .92f, 1);
    sprite(lab, 6, 4, (Rectangle){672, 376, 256, 256}, (Rectangle){579, 294, 26, 26}, 26.0f / 256);
    caster(lab, 4, 0, 1, 592, 320, 6, 8, 5, 6, .92f, 1);
    float walker = 464 + floorf(sinf(lab->frame.time * .55f) * 16);
    sprite(lab, 3, 5, (Rectangle){656, 24, 288, 288}, (Rectangle){walker - 12, 320, 24, 24}, 24.0f / 288);
    caster(lab, 5, 1, 1, walker, 344, 9, 3.8f, 2.5f, 9, .84f, 1);
    original_sprite(lab, lab->animal, 3, 6, (Rectangle){96, 192, 96, 96}, (Rectangle){423, 366, 18, 18}, 0);
    caster(lab, 6, 1, 1, 432, 384, 6, 4, 2.5f, 6, .84f, 1);
    /* 작은 나무 군락도 같은 원화와 월드 씨앗을 씁니다. */
    for (int i = 0; i < 12; i++) {
        float x = 338 + random_value((unsigned int)i * 73 + 9) * 104;
        float y = 210 + random_value((unsigned int)i * 37 + 5) * 45;
        unsigned int id = (unsigned int)i + 20;
        sprite(lab, 2, id, (Rectangle){352, 56, 256, 256}, (Rectangle){x - 21, y - 43, 43, 43}, 43.0f / 256);
        caster(lab, id, 1, 1, x, y, 23, 14, 8, 12, .78f, 3);
        caster(lab, id, 0, 1, x, y, 10, 2, 2, 10, .86f, 0);
    }
    for (int i = 0; i < lab->lights; i++) {
        const float positions[4][4] = {
            {428, 322, 16, 96}, {468, 322, 16, 96}, {480, 352, 24, 112}, {560, 288, 20, 80}};
        CastleGpuLight light = {0};
        memcpy(light.position, positions[i % 4], sizeof(light.position));
        light.color[0] = i % 4 == 3 ? .41f : 1.12f;
        light.color[1] = i % 4 == 3 ? .76f : .73f;
        light.color[2] = i % 4 == 3 ? .72f : .36f;
        light.color[3] = lab->frame.sun_enabled ? .08f : .95f;
        light.tags[0] = i % 4 < 2 ? 1u : 0u;
        (void)castle_gpu_light(lab->gpu, &light);
    }
    if (lab->hidden_case)
        for (int y = 0; y < 40; y++)
            for (int x = 36; x < 64; x++)
                lab->terrain[y * 64 + x] = (unsigned char)(128 | (lab->hidden_case == 1 ? 2 : 4));
    castle_gpu_terrain(lab->gpu, lab->terrain, 64, 40);
}
static void submit(Lab *lab) {
    lab->frame.sun[0] = cosf(lab->azimuth * DEG2RAD) * cosf(lab->elevation * DEG2RAD);
    lab->frame.sun[1] = sinf(lab->azimuth * DEG2RAD) * cosf(lab->elevation * DEG2RAD);
    lab->frame.sun[2] = sinf(lab->elevation * DEG2RAD);
    lab->frame.sun[3] = lab->elevation;
    (void)castle_gpu_begin(lab->gpu, &lab->frame);
    if (lab->shadow_scene) {
        shadow_scene(lab);
        return;
    }
    if (lab->frame.smoke_probe) {
        memset(lab->terrain, 1, sizeof(lab->terrain));
        castle_gpu_terrain(lab->gpu, lab->terrain, lab->frame.map_width, lab->frame.map_height);
        CastleGpuEmitter emitter = {{480, 344, 44, 0}, {1, 0, 0, 0}};
        (void)castle_gpu_emitter(lab->gpu, &emitter);
        return;
    }
    if (lab->dense) {
        dense_scene(lab);
        return;
    }
    if (lab->natural_scene) {
        natural_scene(lab);
        return;
    }
    memset(lab->terrain, 1, sizeof(lab->terrain));
    for (int y = 0; y < 10; y++)
        for (int x = 0; x < 16; x++) {
            int road = y == 8 || (x == 4 && y >= 6 && y <= 8);
            int pond = ((x - 9) * (x - 9) * 3 + (y - 2) * (y - 2) * 4) < 17;
            Rectangle source = road ? (Rectangle){352, 376, 256, 256} : (Rectangle){32, 376, 256, 256};
            if (pond) {
                CastleGpuSprite s = {0};
                s.destination[0] = x * 32.0f;
                s.destination[1] = y * 32.0f;
                s.destination[2] = 32;
                s.destination[3] = 32;
                s.source[0] = (x % 4) * 64.0f / lab->water.width;
                s.source[1] = 640.0f / lab->water.height;
                s.source[2] = 64.0f / lab->water.width;
                s.source[3] = 64.0f / lab->water.height;
                for (int i = 0; i < 4; i++)
                    s.tint[i] = 1;
                s.tags[1] = GPU_MATERIAL_WATER;
                (void)castle_gpu_sprite(lab->gpu, lab->water, (Texture2D){0}, (Texture2D){0}, &s);
            } else
                sprite(lab, road ? 5 : 4, 0, source, (Rectangle){x * 32.0f, y * 32.0f, 32, 32}, 0);
            if (road || pond)
                for (int yy = y * 2; yy < y * 2 + 2; yy++)
                    for (int xx = x * 2; xx < x * 2 + 2; xx++)
                        lab->terrain[yy * 32 + xx] = pond ? 2 : 0;
        }
    sprite(lab, 1, 1, (Rectangle){32, 30, 256, 282}, (Rectangle){387, 55, 86, 95}, 86.0f / 256);
    sprite(lab, 2, 2, (Rectangle){352, 56, 256, 256}, (Rectangle){288, 84, 128, 128}, .5f);
    sprite(lab, 1, 3, (Rectangle){32, 30, 256, 282}, (Rectangle){88, 59, 144, 159}, 144.0f / 256);
    sprite(lab, 3, 4, (Rectangle){656, 24, 288, 288}, (Rectangle){231, 230, 42, 42}, 42.0f / 288);
    sprite(lab, 6, 5, (Rectangle){672, 376, 256, 256}, (Rectangle){388, 200, 80, 80}, 80.0f / 256);
    sprite(lab, 3, 6, (Rectangle){656, 24, 288, 288}, (Rectangle){53, 248, 38, 38}, 38.0f / 288);
    for (int y = 11; y < 14; y++)
        for (int x = 6; x < 13; x++)
            lab->terrain[y * 32 + x] = 0;
    for (int y = 7; y < 10; y++)
        for (int x = 25; x < 29; x++)
            lab->terrain[y * 32 + x] = 0;
    castle_gpu_terrain(lab->gpu, lab->terrain, 32, 20);
    caster(lab, 1, 0, 0, 430, 139, 24, 32, 11, 24, .94f, 1);
    caster(lab, 1, 0, 2, 430, 139, 48, 38, 15, 30, .94f, 0);
    caster(lab, 1, 0, 0, 452, 132, 81, 4, 4, 13, .94f, 0);
    caster(lab, 2, 1, 1, 352, 212, 64, 42, 24, 32, .78f, 3);
    caster(lab, 2, 0, 1, 352, 212, 24, 5, 5, 24, .86f, 0);
    caster(lab, 3, 0, 0, 160, 200, 40, 54, 18, 40, .94f, 1);
    caster(lab, 3, 0, 2, 160, 200, 80, 64, 24, 50, .94f, 0);
    caster(lab, 3, 0, 0, 201, 192, 131, 7, 7, 17, .94f, 0);
    caster(lab, 4, 1, 1, 252, 272, 16, 6, 4, 16, .84f, 1);
    caster(lab, 5, 0, 1, 428, 280, 17, 26, 14, 17, .92f, 1);
    caster(lab, 6, 1, 1, 72, 286, 14, 5.5f, 3.8f, 14, .84f, 1);
    const float lamps[4][4] = {
        {116, 224, 44, 115}, {198, 224, 44, 115}, {405, 153, 26, 80}, {454, 153, 26, 80}};
    for (int k = 0; k < lab->lights; k++) {
        CastleGpuLight light = {0};
        memcpy(light.position, lamps[k % 4], sizeof(light.position));
        light.position[0] += (k / 4) * 18.0f;
        light.color[0] = 1.12f;
        light.color[1] = .67f;
        light.color[2] = .28f;
        light.color[3] = .72f * fminf(1, fmaxf(0, (.55f - lab->frame.ambient) / .4f));
        light.tags[0] = (unsigned int)(k % 4 < 2 ? 3 : 1) | lab->owner_prefix;
        (void)castle_gpu_light(lab->gpu, &light);
    }
    CastleGpuEmitter emitter = {{201, 192, 148, 0}, {0}};
    emitter.settings[0] = 1;
    (void)castle_gpu_emitter(lab->gpu, &emitter);
    emitter.position[0] = 452;
    emitter.position[1] = 140;
    emitter.position[2] = 94;
    emitter.settings[0] = 2;
    (void)castle_gpu_emitter(lab->gpu, &emitter);
    if (lab->overflow)
        for (int i = 0; i < 256; i++)
            caster(lab, (unsigned int)i + 10000, 1, 1, 300, 250, 10, 2, 2, 10, 0, 0);
}
static Image test_frame(Lab *lab, float time) {
    lab->frame.time = time;
    lab->frame.delta = .05f;
    submit(lab);
    BeginDrawing();
    int okay = castle_gpu_render(lab->gpu);
    EndDrawing();
    if (!okay)
        return (Image){0};
    Image image = LoadImageFromTexture(castle_gpu_output(lab->gpu));
    ImageFlipVertical(&image);
    ImageFormat(&image, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    return image;
}
static int different_pixels(Image a, Image b, Rectangle region) {
    if (!a.data || !b.data || a.width != b.width || a.height != b.height)
        return -1;
    int count = 0;
    for (int y = (int)region.y; y < (int)(region.y + region.height); y++)
        for (int x = (int)region.x; x < (int)(region.x + region.width); x++) {
            size_t i = (size_t)y * a.width + x;
            count += memcmp(&((Color *)a.data)[i], &((Color *)b.data)[i], sizeof(Color)) != 0;
        }
    return count;
}
static Image data_image(Lab *lab, int index) {
    Image image = LoadImageFromTexture(castle_gpu_data_texture(lab->gpu, index));
    if (image.data) {
        ImageFlipVertical(&image);
        ImageFormat(&image, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    }
    return image;
}
static int check_shadow_cut(Lab *lab) {
    int errors = 0;
    lab->shadow_scene = lab->frame.natural = 1;
    lab->frame.width = 512;
    lab->frame.height = 320;
    lab->frame.pixel_scale = 1;
    lab->frame.map_width = 32;
    lab->frame.map_height = 20;
    lab->frame.scale = 1;
    lab->frame.origin[0] = lab->frame.origin[1] = 0;
    lab->frame.foliage = lab->frame.grass = lab->frame.water = lab->frame.smoke = 0;
    lab->frame.shadows = 1;
    lab->elevation = 65;
    const int kinds[3] = {GPU_MATERIAL_ROCK, GPU_MATERIAL_ACTOR, GPU_MATERIAL_TREE};
    const char *names[3] = {"rock", "actor", "tree"};
    for (int object = 0; object < 3; object++)
        for (int flip = 0; flip < 2; flip++) {
            lab->probe_kind = kinds[object];
            lab->flip_probe = flip;
            for (int light = 0; light < 5; light++) {
                lab->azimuth = light == 1 ? 270.0f : light == 3 ? 45.0f : light == 4 ? 135.0f : 90.0f;
                lab->frame.sun_enabled = light != 2;
                lab->frame.view = light == 2 ? 4 : 3;
                Image shadow = test_frame(lab, 0), owner = data_image(lab, 3);
                if (!shadow.data || !owner.data) {
                    errors++;
                    UnloadImage(shadow);
                    UnloadImage(owner);
                    continue;
                }
                int leaked = 0, cast = 0, joined = 0, columns = 0;
                float size = object == 2 ? 96.0f : object == 1 ? 48.0f : 80.0f;
                float strip = object == 2 ? .12f : object == 1 ? .20f : .40f;
                int global_front = -1;
                for (int y = 0; y < owner.height; y++)
                    for (int x = 0; x < owner.width; x++) {
                        Color c = ((Color *)owner.data)[(size_t)y * owner.width + x];
                        if ((c.r || c.g || c.b || c.a) && y > global_front)
                            global_front = y;
                    }
                for (int x = 0; x < owner.width; x++) {
                    int front = -1;
                    for (int y = 0; y < owner.height; y++) {
                        Color c = ((Color *)owner.data)[(size_t)y * owner.width + x];
                        if (c.r || c.g || c.b || c.a)
                            front = y;
                    }
                    if (front < 224 - size * strip || (object == 1 && front < global_front - 2))
                        continue;
                    columns++;
                    for (int y = front + 1; y <= front + 14 && y < shadow.height; y++) {
                        Color c = ((Color *)shadow.data)[(size_t)y * shadow.width + x];
                        int dark = c.r < 240;
                        if (light == 0 && y >= front + 2)
                            leaked += dark;
                        if (light == 1) {
                            cast += dark;
                            if (y <= front + 2)
                                joined += dark;
                        }
                    }
                }
                /* 비스듬한 광선은 이웃 지지점의 그림자를 옆 열로 옮길 수 있습니다. */
                if (light >= 2)
                    for (int x = (int)(256 - size * .5f); x < (int)(256 + size * .5f); x++)
                        for (int y = global_front + 2; y <= global_front + 14 && y < shadow.height; y++)
                            leaked += ((Color *)shadow.data)[(size_t)y * shadow.width + x].r < 240;
                errors += columns == 0 || leaked != 0 || (light == 1 && (cast < 8 || joined < 4));
                printf("GPU SHADOW CUT: object=%s flip=%d light=%s columns=%d front_leaks=%d cast=%d "
                       "joined=%d\n",
                       names[object], flip,
                       light == 0   ? "sun-front"
                       : light == 1 ? "sun-back"
                       : light == 2 ? "point-front"
                       : light == 3 ? "sun-left-front"
                                    : "sun-right-front",
                       columns, leaked, cast, joined);
                UnloadImage(shadow);
                UnloadImage(owner);
            }
        }
    errors += castle_gpu_validate(lab->gpu);
    printf("GPU SHADOW CUT CHECK: errors=%d\n", errors);
    return errors != 0;
}
static Rectangle occupied_bounds(Image image, int *pixels) {
    int x0 = image.width, y0 = image.height, x1 = -1, y1 = -1;
    *pixels = 0;
    for (int y = 0; y < image.height; y++)
        for (int x = 0; x < image.width; x++) {
            Color c = ((Color *)image.data)[(size_t)y * image.width + x];
            if (!c.a)
                continue;
            if (x < x0)
                x0 = x;
            if (y < y0)
                y0 = y;
            if (x > x1)
                x1 = x;
            if (y > y1)
                y1 = y;
            (*pixels)++;
        }
    return (Rectangle){(float)x0, (float)y0, (float)(x1 - x0 + 1), (float)(y1 - y0 + 1)};
}
typedef struct GrassPriority {
    uint32_t seed, priority, rank;
} GrassPriority;
static uint32_t seed_hash(uint32_t seed) {
    seed ^= seed >> 16;
    seed *= 0x7feb352du;
    seed ^= seed >> 15;
    seed *= 0x846ca68bu;
    return seed ^ (seed >> 16);
}
static int priority_order(const void *a, const void *b) {
    const GrassPriority *x = a, *y = b;
    if (x->priority != y->priority)
        return x->priority < y->priority ? -1 : 1;
    return x->rank < y->rank ? -1 : x->rank > y->rank;
}
static int seed_order(const void *a, const void *b) {
    uint32_t x = *(const uint32_t *)a, y = *(const uint32_t *)b;
    return x < y ? -1 : x > y;
}
static int sample_order(const void *a, const void *b) {
    const Sample *x = a, *y = b;
    return x->frame_id < y->frame_id ? -1 : x->frame_id > y->frame_id;
}
static int check_natural(Lab *lab) {
    int errors = 0;
    lab->natural_scene = lab->frame.natural = 1;
    lab->frame.width = 1024;
    lab->frame.height = 768;
    lab->frame.pixel_scale = 2;
    lab->frame.map_width = 64;
    lab->frame.map_height = 40;
    lab->frame.scale = 1;
    lab->frame.origin[0] = 224;
    lab->frame.origin[1] = 112;
    lab->frame.foliage = lab->frame.smoke = lab->frame.water = lab->frame.shadows = 0;
    lab->frame.grass = 1;
    lab->frame.view = 1;
    Image first = test_frame(lab, 11), second = test_frame(lab, 11);
    int changed = different_pixels(first, second, (Rectangle){0, 0, (float)first.width, (float)first.height});
    errors += changed != 0;
    printf("GPU GRASS LAYOUT: repeat_changed=%d\n", changed);
    errors += castle_gpu_validate_scenery(lab->gpu);
    UnloadImage(first);
    UnloadImage(second);
    uint32_t original_seeds[16384], capped_seeds[512];
    size_t original_count = castle_gpu_grass_snapshot(lab->gpu, original_seeds, 16384);
    GrassPriority *priorities = calloc(original_count ? original_count : 1, sizeof(*priorities));
    if (!priorities || original_count < 512)
        errors++;
    else {
        for (size_t i = 0; i < original_count; i++)
            priorities[i] =
                (GrassPriority){original_seeds[i], seed_hash(original_seeds[i] + 777u) >> 24, (uint32_t)i};
        qsort(priorities, original_count, sizeof(*priorities), priority_order);
        for (int i = 0; i < 512; i++)
            original_seeds[i] = priorities[i].seed;
        lab->frame.grass_limit = 512;
        first = test_frame(lab, 11);
        second = test_frame(lab, 11);
        int repeat =
            different_pixels(first, second, (Rectangle){0, 0, (float)first.width, (float)first.height});
        size_t selected = castle_gpu_grass_snapshot(lab->gpu, capped_seeds, 512);
        qsort(original_seeds, 512, sizeof(uint32_t), seed_order);
        qsort(capped_seeds, selected, sizeof(uint32_t), seed_order);
        int matched = selected == 512 && memcmp(original_seeds, capped_seeds, sizeof(capped_seeds)) == 0;
        errors += !matched || repeat != 0;
        printf("GPU GRASS BUDGET: candidates=%zu selected=%zu priority_match=%d repeat_changed=%d\n",
               original_count, selected, matched, repeat);
        UnloadImage(first);
        UnloadImage(second);
        lab->frame.grass_limit = 0;
    }
    free(priorities);
    lab->frame.grass = 0;
    lab->hidden_case = 1;
    first = test_frame(lab, 0);
    lab->hidden_case = 2;
    second = test_frame(lab, 0);
    int known_changed = different_pixels(first, second, (Rectangle){0, 0, 352, (float)first.height});
    errors += known_changed != 0;
    printf("GPU FOG: unknown_terrain_changed known_pixels_changed=%d\n", known_changed);
    UnloadImage(first);
    UnloadImage(second);
    lab->hidden_case = 0;
    first = test_frame(lab, 0);
    Image owner_first = data_image(lab, 3);
    lab->frame.origin[0] += 128;
    second = test_frame(lab, 0);
    Image owner_second = data_image(lab, 3);
    int seam_changes = 0, compared = 0;
    if (!first.data || !second.data || !owner_first.data || !owner_second.data)
        errors++;
    else
        for (int y = 0; y < first.height; y++)
            for (int x = 128; x < first.width; x++) {
                size_t a = (size_t)y * first.width + x, b = (size_t)y * second.width + x - 128;
                Color oa = ((Color *)owner_first.data)[a], ob = ((Color *)owner_second.data)[b];
                if (oa.r || oa.g || oa.b || oa.a || ob.r || ob.g || ob.b || ob.a)
                    continue;
                seam_changes +=
                    memcmp(&((Color *)first.data)[a], &((Color *)second.data)[b], sizeof(Color)) != 0;
                compared++;
            }
    errors += seam_changes != 0 || compared < 10000;
    printf("GPU TERRAIN SEAM: compared=%d changed=%d\n", compared, seam_changes);
    UnloadImage(first);
    UnloadImage(second);
    UnloadImage(owner_first);
    UnloadImage(owner_second);
    const int sizes[5][2] = {{800, 600}, {1024, 768}, {1440, 900}, {1920, 1080}, {1180, 812}};
    int grid_errors = 0, roundtrips = 0;
    for (int size = 0; size < 5; size++)
        for (int scale = 1; scale <= 6; scale++) {
            lab->frame.width = sizes[size][0];
            lab->frame.height = sizes[size][1];
            lab->frame.pixel_scale = scale;
            CastleGpuViewport v = castle_gpu_viewport(&lab->frame);
            Vector2 p = {v.destination.x + 17.25f, v.destination.y + 41.75f};
            Vector2 q = castle_gpu_world_to_screen(&lab->frame, castle_gpu_screen_to_world(&lab->frame, p));
            grid_errors += fabsf(q.x - p.x) > .001f || fabsf(q.y - p.y) > .001f;
            grid_errors += v.width * scale != v.destination.width || v.height * scale != v.destination.height;
            roundtrips++;
            if (scale != 3)
                continue;
            first = test_frame(lab, 0);
            RenderTexture2D target = LoadRenderTexture(lab->frame.width, lab->frame.height);
            BeginTextureMode(target);
            ClearBackground(BLACK);
            castle_gpu_present(lab->gpu,
                               (Rectangle){0, 0, (float)lab->frame.width, (float)lab->frame.height});
            EndTextureMode();
            Image physical = LoadImageFromTexture(target.texture);
            ImageFlipVertical(&physical);
            ImageFormat(&physical, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
            if (!first.data || !physical.data)
                grid_errors++;
            else
                for (int y = 0; y < physical.height; y++)
                    for (int x = 0; x < physical.width; x++) {
                        Color expected = BLACK;
                        int sx = x - (int)v.destination.x, sy = y - (int)v.destination.y;
                        if (sx >= 0 && sy >= 0 && sx < (int)v.destination.width &&
                            sy < (int)v.destination.height)
                            expected = ((Color *)first.data)[(size_t)(sy / scale) * first.width + sx / scale];
                        grid_errors +=
                            memcmp(&expected, &((Color *)physical.data)[(size_t)y * physical.width + x],
                                   sizeof(Color)) != 0;
                    }
            UnloadImage(first);
            UnloadImage(physical);
            UnloadRenderTexture(target);
        }
    printf("GPU PIXEL GRID: roundtrips=%d presentation_errors=%d\n", roundtrips, grid_errors);
    errors += grid_errors != 0;
    lab->frame.width = 800;
    lab->frame.height = 600;
    lab->frame.pixel_scale = 2;
    lab->frame.origin[0] = 280;
    lab->frame.origin[1] = 154;
    lab->frame.view = 0;
    lab->frame.smoke = 1;
    lab->frame.smoke_probe = 1;
    lab->frame.smoke_scale = 1;
    first = test_frame(lab, 0);
    UnloadImage(first);
    Image small = data_image(lab, 6);
    int small_pixels = 0, large_pixels = 0;
    Rectangle small_box = occupied_bounds(small, &small_pixels);
    lab->frame.smoke_scale = 4;
    first = test_frame(lab, 0);
    UnloadImage(first);
    Image large = data_image(lab, 6);
    Rectangle large_box = occupied_bounds(large, &large_pixels);
    float width_ratio = large_box.width / fmaxf(1, small_box.width),
          height_ratio = large_box.height / fmaxf(1, small_box.height);
    float area_ratio = (float)large_pixels / fmaxf(1, (float)small_pixels);
    errors += width_ratio < 3.2f || width_ratio > 4.8f || height_ratio < 3.2f || height_ratio > 4.8f ||
              area_ratio < 11 || area_ratio > 22;
    printf("GPU SMOKE SCALE: width=%.3f height=%.3f covered_area=%.3f\n", width_ratio, height_ratio,
           area_ratio);
    UnloadImage(small);
    UnloadImage(large);
    lab->frame.smoke_probe = 2;
    first = test_frame(lab, 0);
    Image layer = data_image(lab, 6);
    lab->frame.smoke = 0;
    second = test_frame(lab, 0);
    int cap_errors = 0, cap_pixels = 0;
    for (int y = 0; y < first.height; y++)
        for (int x = 0; x < first.width; x++) {
            size_t at = (size_t)y * first.width + x;
            Color puff = ((Color *)layer.data)[at];
            if (puff.a < 180)
                continue;
            float opacity = .375f;
            Color ground = ((Color *)second.data)[at], actual = ((Color *)first.data)[at];
            float r = puff.r * (opacity / (puff.a / 255.0f)) + ground.r * (1 - opacity);
            float g = puff.g * (opacity / (puff.a / 255.0f)) + ground.g * (1 - opacity);
            float b = puff.b * (opacity / (puff.a / 255.0f)) + ground.b * (1 - opacity);
            cap_errors += fabsf(actual.r - r) > 2 || fabsf(actual.g - g) > 2 || fabsf(actual.b - b) > 2;
            cap_pixels++;
        }
    errors += cap_errors != 0 || cap_pixels < 10;
    printf("GPU SMOKE OPACITY: capped_pixels=%d errors=%d\n", cap_pixels, cap_errors);
    UnloadImage(first);
    UnloadImage(second);
    UnloadImage(layer);
    printf("GPU NATURAL CHECK: errors=%d\n", errors);
    return errors != 0;
}
static int check_effects(Lab *lab) {
    int errors = 0;
    lab->frame.width = 512;
    lab->frame.height = 320;
    lab->frame.scale = 1;
    lab->frame.origin[0] = lab->frame.origin[1] = 0;
    lab->frame.foliage = lab->frame.grass = lab->frame.water = lab->frame.smoke = 0;
    Rectangle whole = {0, 0, 512, 320};
    Image first = test_frame(lab, 0), second = test_frame(lab, 18);
    int stationary = different_pixels(first, second, whole);
    errors += stationary != 0;
    UnloadImage(first);
    UnloadImage(second);
    int *features[] = {&lab->frame.foliage, &lab->frame.grass, &lab->frame.water, &lab->frame.smoke};
    const char *names[] = {"foliage", "grass", "water", "smoke"};
    for (int feature = 0; feature < 4; feature++) {
        *features[feature] = 1;
        first = test_frame(lab, 0);
        for (int step = 0; feature == 3 && step < 10; step++) {
            second = test_frame(lab, step * .05f);
            UnloadImage(second);
        }
        second = test_frame(lab, 18);
        int moved = different_pixels(first, second, whole);
        errors += moved < 8;
        printf("GPU EFFECT: %s changed=%d\n", names[feature], moved);
        if (feature == 0) {
            int root = different_pixels(first, second, (Rectangle){349, 193, 3, 13});
            printf("GPU ANCHOR: trunk_changed=%d\n", root);
            errors += root != 0;
        }
        if (feature == 2 && first.data && second.data) {
            int outside = 0;
            for (int y = 0; y < 320; y++)
                for (int x = 0; x < 512; x++) {
                    int dx = x / 32 - 9, dy = y / 32 - 2;
                    if (dx * dx * 3 + dy * dy * 4 < 17)
                        continue;
                    size_t at = (size_t)y * 512 + x;
                    outside +=
                        memcmp(&((Color *)first.data)[at], &((Color *)second.data)[at], sizeof(Color)) != 0;
                }
            errors += outside != 0;
            printf("GPU WATER: outside_changed=%d\n", outside);
        }
        UnloadImage(first);
        UnloadImage(second);
        *features[feature] = 0;
    }
    /* 같은 씨앗·시각·굴뚝을 다시 열어도 이전 연기의 궤적을 이어받지 않습니다. */
    lab->frame.smoke = 1;
    lab->frame.reset_motion = 1;
    first = test_frame(lab, 9);
    lab->frame.reset_motion = 0;
    for (int step = 1; step <= 12; step++) {
        second = test_frame(lab, 9 + step * .05f);
        UnloadImage(second);
    }
    lab->frame.reset_motion = 1;
    second = test_frame(lab, 9);
    int reset_changed = different_pixels(first, second, whole);
    errors += reset_changed != 0;
    printf("GPU WORLD RESET: same_world_changed=%d\n", reset_changed);
    UnloadImage(first);
    UnloadImage(second);
    lab->frame.reset_motion = lab->frame.smoke = 0;
    first = test_frame(lab, 0);
    uint64_t fixed_updates = castle_gpu_stats(lab->gpu).static_updates;
    second = test_frame(lab, 18);
    errors += castle_gpu_stats(lab->gpu).static_updates != fixed_updates;
    UnloadImage(second);
    lab->owner_prefix = 0xbf370000u;
    second = test_frame(lab, 0);
    int owner_changed = different_pixels(first, second, whole);
    errors += owner_changed != 0;
    printf("GPU OWNER: full_id_changed=%d\n", owner_changed);
    UnloadImage(second);
    lab->owner_prefix = 0;
    lab->overflow = 1;
    second = test_frame(lab, 0);
    int overflow_changed = different_pixels(first, second, whole);
    errors += overflow_changed != 0 || castle_gpu_validate(lab->gpu) != 0 ||
              castle_gpu_stats(lab->gpu).tile_overflows == 0;
    printf("GPU OVERFLOW: fallback_changed=%d\n", overflow_changed);
    UnloadImage(first);
    UnloadImage(second);
    lab->overflow = 0;
    lab->frame.view = 3;
    first = test_frame(lab, 0);
    lab->frame.shadows = 0;
    second = test_frame(lab, 0);
    int shadow_pixels = different_pixels(first, second, whole);
    errors += shadow_pixels < 500;
    printf("GPU SHADOW: changed=%d\n", shadow_pixels);
    UnloadImage(first);
    UnloadImage(second);
    lab->frame.shadows = 1;
    lab->frame.view = 0;
    const int sizes[6][2] = {{800, 600}, {1024, 768}, {1440, 900}, {1920, 1080}, {1180, 812}, {512, 320}};
    CastleGpuStats stable = {0};
    for (int pass = 0; pass < 3; pass++) {
        for (int i = 0; i < 6; i++) {
            lab->frame.width = sizes[i][0];
            lab->frame.height = sizes[i][1];
            lab->frame.scale = fminf(sizes[i][0] / 512.0f, sizes[i][1] / 320.0f);
            first = test_frame(lab, 0);
            errors += !first.data;
            if (first.data) {
                int holes = 0;
                for (int y = 2; y < (int)(32 * lab->frame.scale); y++)
                    for (int x = 2; x < (int)(512 * lab->frame.scale) - 1; x++)
                        holes += ((Color *)first.data)[(size_t)y * first.width + x].g < 30;
                errors += holes != 0;
                if (holes)
                    printf("GPU FLOOR: %dx%d holes=%d\n", sizes[i][0], sizes[i][1], holes);
                UnloadImage(first);
            }
        }
        CastleGpuStats current = castle_gpu_stats(lab->gpu);
        if (pass)
            errors += current.live_objects != stable.live_objects ||
                      current.texture_bytes != stable.texture_bytes ||
                      current.buffer_bytes != stable.buffer_bytes;
        stable = current;
    }
    errors += castle_gpu_validate(lab->gpu) != 0;
    printf("GPU EFFECTS: stationary_changed=%d resize_passes=3 objects=%u errors=%d\n", stationary,
           stable.live_objects, errors);
    return errors != 0;
}
static int compare_double(const void *a, const void *b) {
    double x = *(const double *)a, y = *(const double *)b;
    return (x > y) - (x < y);
}
static double percentile(double *values, int count, float fraction) {
    if (!count)
        return 0;
    qsort(values, (size_t)count, sizeof(*values), compare_double);
    return values[(int)ceilf(count * fraction) - 1];
}
static void json_string(FILE *file, const char *text) {
    fputc('"', file);
    for (; *text; text++) {
        if (*text == '"' || *text == '\\')
            fputc('\\', file);
        if ((unsigned char)*text >= 32)
            fputc(*text, file);
    }
    fputc('"', file);
}
static int benchmark_report(Lab *lab, Sample *samples, int count) {
    if (!lab->report)
        return 0;
    double *values = malloc((size_t)(count ? count : 1) * sizeof(*values));
    FILE *file = platform_fopen_utf8(lab->report, "wb");
    if (!file || !values) {
        if (file)
            fclose(file);
        free(values);
        return 1;
    }
    CastleGpuDevice device = castle_gpu_device();
    CastleGpuStats stats = castle_gpu_stats(lab->gpu);
    fprintf(file, "{\n  \"renderer\": ");
    json_string(file, device.renderer);
    fprintf(file, ",\n  \"vendor\": ");
    json_string(file, device.vendor);
    fprintf(file, ",\n  \"opengl\": ");
    json_string(file, device.version);
    fprintf(file,
            ",\n  \"software\": %s,\n  \"width\": %d, \"height\": %d, \"dense\": %s,\n"
            "  \"frames\": %d, \"warmup\": %d, \"samples\": %d,\n"
            "  \"sprites\": %d, \"casters\": %d, \"lights\": %d, \"emitters\": %d,\n"
            "  \"grass_instances\": %d, \"smoke_particles\": %d,\n"
            "  \"texture_bytes\": %zu, \"buffer_bytes\": %zu, \"tile_overflows\": %d,\n"
            "  \"foliage\": %d, \"grass\": %d, \"water\": %d, \"smoke\": %d,\n",
            device.software ? "true" : "false", lab->frame.width, lab->frame.height,
            lab->dense ? "true" : "false", (int)stats.frames, lab->warmup, count, stats.sprites,
            stats.casters, stats.lights, stats.emitters, stats.grass_instances, stats.smoke_particles,
            stats.texture_bytes, stats.buffer_bytes, stats.tile_overflows, lab->frame.foliage,
            lab->frame.grass, lab->frame.water, lab->frame.smoke);
    fprintf(file,
            "  \"scene\": \"%s\", \"pixel_scale\": %d, \"logical_width\": %d, \"logical_height\": %d,\n",
            lab->shadow_scene    ? "shadow-cut"
            : lab->natural_scene ? "natural-slice"
                                 : "reference",
            stats.pixel_scale, stats.logical_width, stats.logical_height);
    fprintf(file, "  \"seed\": %u, \"elapsed_seconds\": %.4f,\n", lab->frame.world_seed,
            lab->elapsed_seconds);
    fprintf(file, "  \"sample_strategy\": \"%s\",\n",
            lab->seconds > 0 ? "rolling-one-in-sixty" : "every-frame");
    const char *names[7] = {"wall", "fields", "geometry", "shadows", "lighting", "smoke", "gpu_total"};
    for (int phase = 0; phase < 7; phase++) {
        int n = 0;
        for (int i = 0; i < count; i++) {
            if (phase && !samples[i].timed)
                continue;
            double value = samples[i].wall;
            if (phase > 0 && phase < 6)
                value = samples[i].phase[phase - 1];
            else if (phase == 6) {
                value = 0;
                for (int k = 0; k < 5; k++)
                    value += samples[i].phase[k];
            }
            values[n++] = value;
        }
        double p50 = percentile(values, n, .50f), p95 = percentile(values, n, .95f),
               p99 = percentile(values, n, .99f);
        fprintf(file, "  \"%s_ms\": {\"samples\": %d, \"p50\": %.4f, \"p95\": %.4f, \"p99\": %.4f},\n",
                names[phase], n, p50, p95, p99);
    }
    fprintf(file, "  \"measurement_family\": \"%s\",\n  \"sample_data\": [\n",
            lab->frame.pixel_scale > 0 ? "natural-world-pixel" : "reference-fit");
    for (int i = 0; i < count; i++)
        fprintf(file,
                "    {\"frame\": %llu, \"wall_ms\": %.5f, \"gpu_valid\": %s, \"phase_ms\": "
                "[%.5f,%.5f,%.5f,%.5f,%.5f]}%s\n",
                (unsigned long long)samples[i].frame_id, samples[i].wall, samples[i].timed ? "true" : "false",
                samples[i].phase[0], samples[i].phase[1], samples[i].phase[2], samples[i].phase[3],
                samples[i].phase[4], i + 1 < count ? "," : "");
    fprintf(file, "  ]\n");
    fprintf(file, "}\n");
    int error = ferror(file);
    error |= fclose(file) != 0;
    free(values);
    return error;
}
static void report(Lab *lab) {
    CastleGpuStats s = castle_gpu_stats(lab->gpu);
    printf("GPU FRAME: frames=%llu software=%d fields_ms=%.3f geometry_ms=%.3f shadows_ms=%.3f "
           "lighting_ms=%.3f smoke_ms=%.3f textures=%zu buffers=%zu objects=%u sprites=%d casters=%d "
           "lights=%d emitters=%d static_updates=%llu\n",
           (unsigned long long)s.frames, s.software, s.fields_ms, s.geometry_ms, s.shadows_ms, s.lighting_ms,
           s.smoke_ms, s.texture_bytes, s.buffer_bytes, s.live_objects, s.sprites, s.casters, s.lights,
           s.emitters, (unsigned long long)s.static_updates);
}
static const char *hud_text =
    u8"GPU 픽셀 풍경 실험실  낮 밤 그림자 풀 잎 물 연기 켜짐 꺼짐 "
    u8"방향키 햇빛 방향·높이  오른쪽 마우스 횃불  Tab 원화·법선·그림자 보기  H 안내 소프트웨어 "
    u8"휠 확대 가운데 끌기 이동 정수 배율 논리 표시 접지 비교 바위 주민 나무 반전 "
    "0123456789x./:- LGWSMHTab ms FPS";
static Font hud_font(const Lab *lab) {
    int count = 0, unique[512], unique_count = 0;
    int *points = LoadCodepoints(hud_text, &count);
    for (int i = 0; points && i < count; i++) {
        int found = 0;
        for (int j = 0; j < unique_count; j++)
            found |= unique[j] == points[i];
        if (!found && unique_count < 512)
            unique[unique_count++] = points[i];
    }
    UnloadCodepoints(points);
    char path[GAME_PATH_BYTES];
    if (!game_join_path(path, sizeof(path), lab->assets, "fonts/Galmuri11.ttf"))
        return (Font){0};
    int size = 0;
    unsigned char *bytes = game_read_bytes(path, &size);
    Font font = bytes ? LoadFontFromMemory(".ttf", bytes, size, 22, unique, unique_count) : (Font){0};
    free(bytes);
    if (font.texture.id)
        SetTextureFilter(font.texture, TEXTURE_FILTER_POINT);
    return font;
}
static void hud(const Lab *lab) {
    if (!lab->hud || !lab->font.texture.id)
        return;
    float y = (float)GetScreenHeight() - 82;
    DrawRectangle(0, (int)y, GetScreenWidth(), 82, (Color){15, 21, 24, 232});
    const char *text =
        TextFormat(u8"1 낮  2 밤  S 그림자  G 풀:%s  L 잎:%s  W 물:%s  M 연기:%s",
                   lab->frame.grass ? u8"켜짐" : u8"꺼짐", lab->frame.foliage ? u8"켜짐" : u8"꺼짐",
                   lab->frame.water ? u8"켜짐" : u8"꺼짐", lab->frame.smoke ? u8"켜짐" : u8"꺼짐");
    if (lab->shadow_scene)
        text = u8"접지 그림자 비교  3 바위  4 주민  5 나무  F 반전  1 낮  2 횃불";
    DrawTextEx(lab->font, text, (Vector2){12, y + 6}, 20, 0, RAYWHITE);
    DrawTextEx(lab->font, u8"휠: 확대  가운데 끌기: 이동  방향키: 햇빛  오른쪽: 횃불  Tab: 보기  H: 안내",
               (Vector2){12, y + 32}, 18, 0, (Color){188, 199, 182, 255});
    CastleGpuStats s = castle_gpu_stats(lab->gpu);
    DrawTextEx(lab->font,
               TextFormat("GPU %.2f ms  /  %d FPS %s  /  %dx%d  %d%s",
                          s.fields_ms + s.geometry_ms + s.shadows_ms + s.lighting_ms + s.smoke_ms, GetFPS(),
                          s.software ? u8"소프트웨어" : "", s.logical_width, s.logical_height,
                          lab->frame.pixel_scale, u8"배율"),
               (Vector2){12, y + 56}, 18, 0, (Color){224, 190, 122, 255});
}
static int run(int argc, char **argv) {
    Lab lab = {0};
    int compute = 0, resources = 0, effects = 0, natural_checks = 0, shadow_checks = 0, result = 0;
    Sample *samples = NULL;
    int sample_count = 0;
    uint64_t last_timed = 0;
    lab.lights = 4;
    lab.warmup = 60;
    lab.hud = 1;
    lab.natural_scene = 1;
    lab.probe_kind = GPU_MATERIAL_ROCK;
    lab.center = (Vector2){480, 304};
    lab.azimuth = 225;
    lab.elevation = 40;
    lab.frame = (CastleGpuFrame){.width = 1280,
                                 .height = 800,
                                 .world_seed = 314159,
                                 .pixel_scale = 3,
                                 .natural = 1,
                                 .map_width = 32,
                                 .map_height = 20,
                                 .scale = 1,
                                 .delta = 1.0f / 60,
                                 .ambient = .5f,
                                 .detail = 1.25f,
                                 .sun_enabled = 1,
                                 .shadows = 1,
                                 .foliage = 1,
                                 .grass = 1,
                                 .water = 1,
                                 .smoke = 1};
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--help")) {
            puts(u8"GPU 픽셀 풍경 실험실\n"
                 "  --assets dir --width 1920 --height 1080 --night --dense --lights 4|8|16\n"
                 "  --benchmark --frames 600 --warmup 60 --report result.json [--seconds 1800]\n"
                 "  --hidden --capture result.png --time 8 --view 0|1|2|3\n"
                 "  --no-motion --no-foliage --no-grass --no-water --no-smoke --no-shadows\n"
                 "  --azimuth 225 --elevation 40");
            puts(u8"  --scene natural-slice|reference|shadow-cut --pixel-scale 1..6\n"
                 "  --check-natural  공통 픽셀·접촉·청크·풀·연기 검사\n"
                 "  --check-shadow-cut  원화 밑선에서 둥근 그림자 절단 검사\n"
                 "  --probe-object rock|npc|tree --flip  접지 비교 대상\n"
                 "  --capture-logical  표시 확대 전의 논리 화면 저장");
            return 0;
        } else if (!strcmp(argv[i], "--check-compute"))
            compute = 1;
        else if (!strcmp(argv[i], "--check-resources"))
            resources = 1;
        else if (!strcmp(argv[i], "--check-effects"))
            effects = 1;
        else if (!strcmp(argv[i], "--check-shadow-cut"))
            shadow_checks = 1;
        else if (!strcmp(argv[i], "--flip"))
            lab.flip_probe = 1;
        else if (!strcmp(argv[i], "--probe-object") && i + 1 < argc) {
            const char *object = argv[++i];
            if (!strcmp(object, "rock"))
                lab.probe_kind = GPU_MATERIAL_ROCK;
            else if (!strcmp(object, "npc"))
                lab.probe_kind = GPU_MATERIAL_ACTOR;
            else if (!strcmp(object, "tree"))
                lab.probe_kind = GPU_MATERIAL_TREE;
            else
                return 2;
        } else if (!strcmp(argv[i], "--check-natural") || !strcmp(argv[i], "--check-pixel-grid") ||
                   !strcmp(argv[i], "--check-ground-contact") || !strcmp(argv[i], "--check-terrain-seams") ||
                   !strcmp(argv[i], "--check-grass-layout") || !strcmp(argv[i], "--check-smoke-scale"))
            natural_checks = 1;
        else if (!strcmp(argv[i], "--hidden"))
            lab.hidden = 1;
        else if (!strcmp(argv[i], "--benchmark"))
            lab.benchmark = 1;
        else if (!strcmp(argv[i], "--dense"))
            lab.dense = 1;
        else if (!strcmp(argv[i], "--scene") && i + 1 < argc) {
            const char *scene = argv[++i];
            if (!strcmp(scene, "reference")) {
                lab.natural_scene = lab.frame.natural = lab.frame.pixel_scale = 0;
            } else if (!strcmp(scene, "natural-slice"))
                lab.natural_scene = lab.frame.natural = 1;
            else if (!strcmp(scene, "shadow-cut")) {
                lab.shadow_scene = lab.frame.natural = 1;
                lab.center = (Vector2){256, 192};
                lab.frame.foliage = lab.frame.grass = lab.frame.water = lab.frame.smoke = 0;
                lab.azimuth = 90;
                lab.elevation = 65;
            } else
                return 2;
        } else if (!strcmp(argv[i], "--pixel-scale") && i + 1 < argc)
            lab.frame.pixel_scale = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--assets") && i + 1 < argc)
            snprintf(lab.assets, sizeof(lab.assets), "%s", argv[++i]);
        else if (!strcmp(argv[i], "--report") && i + 1 < argc)
            lab.report = argv[++i];
        else if (!strcmp(argv[i], "--warmup") && i + 1 < argc)
            lab.warmup = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--seconds") && i + 1 < argc)
            lab.seconds = (float)atof(argv[++i]);
        else if (!strcmp(argv[i], "--lights") && i + 1 < argc)
            lab.lights = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--time") && i + 1 < argc)
            lab.start_time = (float)atof(argv[++i]);
        else if (!strcmp(argv[i], "--seed") && i + 1 < argc) {
            int seed = atoi(argv[++i]);
            if (seed < 0)
                return 2;
            lab.frame.world_seed = (uint32_t)seed;
        } else if (!strcmp(argv[i], "--azimuth") && i + 1 < argc)
            lab.azimuth = (float)atof(argv[++i]);
        else if (!strcmp(argv[i], "--elevation") && i + 1 < argc)
            lab.elevation = (float)atof(argv[++i]);
        else if (!strcmp(argv[i], "--capture") && i + 1 < argc)
            lab.capture = argv[++i];
        else if (!strcmp(argv[i], "--capture-logical"))
            lab.capture_logical = 1;
        else if (!strcmp(argv[i], "--frames") && i + 1 < argc)
            lab.frames = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--width") && i + 1 < argc)
            lab.frame.width = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--height") && i + 1 < argc)
            lab.frame.height = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--night")) {
            lab.frame.sun_enabled = 0;
            lab.frame.ambient = .17f;
        } else if (!strcmp(argv[i], "--no-motion"))
            lab.frame.foliage = lab.frame.grass = lab.frame.water = lab.frame.smoke = 0;
        else if (!strcmp(argv[i], "--no-foliage"))
            lab.frame.foliage = 0;
        else if (!strcmp(argv[i], "--no-grass"))
            lab.frame.grass = 0;
        else if (!strcmp(argv[i], "--no-water"))
            lab.frame.water = 0;
        else if (!strcmp(argv[i], "--no-smoke"))
            lab.frame.smoke = 0;
        else if (!strcmp(argv[i], "--no-shadows"))
            lab.frame.shadows = 0;
        else if (!strcmp(argv[i], "--view") && i + 1 < argc)
            lab.frame.view = atoi(argv[++i]);
        else {
            fprintf(stderr, u8"알 수 없는 GPU 검사 인자: %s\n", argv[i]);
            return 2;
        }
    }
    if (lab.frame.width < 64 || lab.frame.height < 64 || lab.frame.width > 3840 || lab.frame.height > 2160 ||
        lab.frame.pixel_scale < 0 || lab.frame.pixel_scale > 6 || lab.frames < 0 || lab.frames > 1000000 ||
        lab.lights < 0 || lab.lights > 16 || lab.warmup < 0 || lab.warmup > 1000000 ||
        !isfinite(lab.start_time) || !isfinite(lab.azimuth) || !isfinite(lab.elevation) ||
        lab.elevation < 5 || lab.elevation > 85)
        return 2;
    if (!isfinite(lab.seconds) || lab.seconds < 0 || lab.seconds > 86400)
        return 2;
    if (!lab.assets[0]) {
        char exe[GAME_PATH_BYTES], file[GAME_PATH_BYTES];
        if (!game_executable_directory(exe, sizeof(exe), argv[0]) ||
            !game_join_path(lab.assets, sizeof(lab.assets), exe, "assets") ||
            !game_join_path(file, sizeof(file), lab.assets, "lighting_lab/pixel_albedo.png"))
            return 1;
        FILE *existing = platform_fopen_utf8(file, "rb");
        if (existing)
            fclose(existing);
        else
            snprintf(lab.assets, sizeof(lab.assets), "%s/game/assets", CASTLE_GPU_ROOT);
    }
    if (compute || resources || effects) {
        lab.natural_scene = lab.frame.natural = lab.frame.pixel_scale = 0;
    }
    if (lab.natural_scene) {
        lab.frame.map_width = 64;
        lab.frame.map_height = 40;
    }
    if (lab.dense)
        lab.frame.map_width = lab.frame.map_height = 64;
    if (lab.dense)
        lab.frame.grass_density = .9f;
    SetTraceLogLevel(LOG_WARNING);
    if (lab.shadow_scene) {
        lab.frame.map_width = 32;
        lab.frame.map_height = 20;
    }
    SetConfigFlags(compute || resources || effects || natural_checks || shadow_checks || lab.hidden
                       ? FLAG_WINDOW_HIDDEN
                       : FLAG_WINDOW_RESIZABLE);
    InitWindow(lab.frame.width, lab.frame.height, u8"GPU 픽셀 풍경 실험실");
    if (!IsWindowReady())
        return 1;
    if (compute) {
        result = castle_gpu_check_compute();
        CloseWindow();
        return result;
    }
    if (resources) {
        for (int i = 0; i < 3; i++) {
            CastleGpu *gpu = castle_gpu_create();
            if (!gpu) {
                result = 1;
                break;
            }
            castle_gpu_destroy(gpu);
            if (castle_gpu_live_objects())
                result = 1;
        }
        printf("GPU RESOURCES: live=%u errors=%d\n", castle_gpu_live_objects(), result);
        CloseWindow();
        return result;
    }
    lab.gpu = castle_gpu_create();
    if (!lab.gpu) {
        CloseWindow();
        return 1;
    }
    lab.albedo = texture(&lab, "lighting_lab/pixel_albedo.png");
    lab.normal = texture(&lab, "lighting_lab/pixel_normals.png");
    lab.surface = texture(&lab, "lighting_lab/pixel_surface.png");
    lab.water = texture(&lab, "terrain/biomes.png");
    lab.scenery = texture(&lab, "lighting_lab/scenery_masks.png");
    lab.contact_policy = texture(&lab, "lighting_lab/contact_policy.png");
    lab.animal = texture(&lab, "animals/walk.png");
    lab.bridge = texture(&lab, "roads/bridges.png");
    if (!lab.albedo.id || !lab.normal.id || !lab.surface.id || !lab.water.id || !lab.scenery.id ||
        !lab.contact_policy.id || !lab.animal.id || !lab.bridge.id) {
        result = 1;
        goto finish;
    }
    castle_gpu_scenery(lab.gpu, lab.scenery);
    castle_gpu_contact_policy(lab.gpu, lab.contact_policy);
    if (shadow_checks) {
        result = check_shadow_cut(&lab);
        goto finish;
    }
    if (natural_checks) {
        result = check_natural(&lab);
        goto finish;
    }
    if (effects) {
        result = check_effects(&lab);
        goto finish;
    }
    if ((lab.benchmark || lab.report) && !lab.frames)
        lab.frames = 600;
    if (lab.seconds > 0)
        lab.frames = 1000000;
    if (lab.capture && !lab.frames)
        lab.frames = 8;
    if (lab.report) {
        samples = calloc(lab.seconds > 0 ? 6000 : (size_t)lab.frames, sizeof(*samples));
        if (!samples) {
            result = 1;
            goto finish;
        }
    }
    if (!lab.frames)
        lab.font = hud_font(&lab);
    SetTargetFPS(lab.benchmark ? 0 : 60);
    double run_started = GetTime();
    CastleGpuStats soak_baseline = {0};
    int soak_ready = 0;
    for (int frame = 0; !WindowShouldClose(); frame++) {
        double started = GetTime();
        lab.frame.time = lab.start_time + frame / 60.0f;
        lab.frame.delta = 1.0f / 60;
        if (lab.seconds > 0) {
            lab.frame.delta = fminf(.05f, GetFrameTime());
            lab.start_time += lab.frame.delta;
            lab.frame.time = lab.start_time;
        }
        if (!lab.frames) {
            if (lab.shadow_scene) {
                if (IsKeyPressed(KEY_THREE))
                    lab.probe_kind = GPU_MATERIAL_ROCK;
                if (IsKeyPressed(KEY_FOUR))
                    lab.probe_kind = GPU_MATERIAL_ACTOR;
                if (IsKeyPressed(KEY_FIVE))
                    lab.probe_kind = GPU_MATERIAL_TREE;
                if (IsKeyPressed(KEY_F))
                    lab.flip_probe = !lab.flip_probe;
            }
            lab.frame.delta = fminf(.05f, GetFrameTime());
            lab.start_time += lab.frame.delta;
            lab.frame.time = lab.start_time;
            lab.frame.width = GetScreenWidth();
            lab.frame.height = GetScreenHeight();
        }
        float world_width = lab.dense ? 1024.0f : 512.0f, world_height = lab.dense ? 1024.0f : 320.0f;
        lab.frame.scale = fminf(lab.frame.width / world_width, lab.frame.height / world_height);
        lab.frame.origin[0] = (world_width - lab.frame.width / lab.frame.scale) * .5f;
        lab.frame.origin[1] = (world_height - lab.frame.height / lab.frame.scale) * .5f;
        if (lab.frame.pixel_scale > 0) {
            CastleGpuViewport viewport = castle_gpu_viewport(&lab.frame);
            lab.frame.scale = 1;
            lab.frame.origin[0] = lab.center.x - viewport.width * .5f;
            lab.frame.origin[1] = lab.center.y - viewport.height * .5f;
        }
        if (!lab.frames) {
            if (IsMouseButtonDown(MOUSE_BUTTON_MIDDLE)) {
                Vector2 movement = GetMouseDelta();
                lab.center.x -= movement.x / fmaxf(1, (float)lab.frame.pixel_scale);
                lab.center.y -= movement.y / fmaxf(1, (float)lab.frame.pixel_scale);
            }
            if (GetMouseWheelMove() != 0)
                lab.frame.pixel_scale = (int)fmaxf(1, fminf(6, lab.frame.pixel_scale + GetMouseWheelMove()));
            if (IsKeyPressed(KEY_ONE)) {
                lab.frame.sun_enabled = 1;
                lab.frame.ambient = .5f;
            }
            if (IsKeyPressed(KEY_TWO)) {
                lab.frame.sun_enabled = 0;
                lab.frame.ambient = .17f;
            }
            if (IsKeyPressed(KEY_S))
                lab.frame.shadows = !lab.frame.shadows;
            if (IsKeyPressed(KEY_G))
                lab.frame.grass = !lab.frame.grass;
            if (IsKeyPressed(KEY_L))
                lab.frame.foliage = !lab.frame.foliage;
            if (IsKeyPressed(KEY_W))
                lab.frame.water = !lab.frame.water;
            if (IsKeyPressed(KEY_M))
                lab.frame.smoke = !lab.frame.smoke;
            if (IsKeyPressed(KEY_TAB))
                lab.frame.view = (lab.frame.view + 1) % 4;
            if (IsKeyPressed(KEY_H))
                lab.hud = !lab.hud;
            if (IsKeyDown(KEY_LEFT))
                lab.azimuth -= .8f;
            if (IsKeyDown(KEY_RIGHT))
                lab.azimuth += .8f;
            if (IsKeyDown(KEY_UP))
                lab.elevation = fminf(80, lab.elevation + .5f);
            if (IsKeyDown(KEY_DOWN))
                lab.elevation = fmaxf(12, lab.elevation - .5f);
        }
        if (lab.frame.pixel_scale > 0) {
            CastleGpuViewport viewport = castle_gpu_viewport(&lab.frame);
            lab.frame.origin[0] = lab.center.x - viewport.width * .5f;
            lab.frame.origin[1] = lab.center.y - viewport.height * .5f;
        }
        submit(&lab);
        if (!lab.frames && IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
            Vector2 p = castle_gpu_screen_to_world(&lab.frame, GetMousePosition());
            CastleGpuLight light = {{p.x, p.y + 25, 28, 150}, {1.12f, .67f, .28f, 1.1f}, {0}};
            (void)castle_gpu_light(lab.gpu, &light);
        }
        BeginDrawing();
        ClearBackground(BLACK);
        if (!castle_gpu_render(lab.gpu)) {
            EndDrawing();
            result = 1;
            break;
        }
        castle_gpu_present(lab.gpu, (Rectangle){0, 0, (float)GetScreenWidth(), (float)GetScreenHeight()});
        if (!lab.frames)
            hud(&lab);
        EndDrawing();
        if (samples && frame >= lab.warmup) {
            CastleGpuStats stats = castle_gpu_stats(lab.gpu);
            if (lab.seconds <= 0) {
                samples[sample_count++].wall = (GetTime() - started) * 1000;
                samples[sample_count - 1].frame_id = (uint64_t)frame;
            } else if ((frame - lab.warmup) % 60 == 0) {
                size_t index = (size_t)(frame - lab.warmup) / 60 % 6000;
                samples[index] = (Sample){.wall = (GetTime() - started) * 1000, .frame_id = (uint64_t)frame};
                if (sample_count < 6000)
                    sample_count++;
            }
            if (stats.timed_frames != last_timed && stats.timed_frame_id >= (uint64_t)lab.warmup &&
                (lab.seconds > 0 || stats.timed_frame_id < (uint64_t)lab.frames)) {
                uint64_t offset = stats.timed_frame_id - (uint64_t)lab.warmup;
                Sample *timed = lab.seconds > 0 ? (offset % 60 == 0 ? &samples[offset / 60 % 6000] : NULL)
                                                : &samples[offset];
                if (timed && timed->frame_id == stats.timed_frame_id) {
                    timed->phase[0] = stats.fields_ms;
                    timed->phase[1] = stats.geometry_ms;
                    timed->phase[2] = stats.shadows_ms;
                    timed->phase[3] = stats.lighting_ms;
                    timed->phase[4] = stats.smoke_ms;
                    timed->timed = 1;
                }
            }
            last_timed = stats.timed_frames;
        }
        if (lab.seconds > 0 && frame >= lab.warmup && frame % 300 == 0) {
            CastleGpuStats current = castle_gpu_stats(lab.gpu);
            if (soak_ready && (current.live_objects != soak_baseline.live_objects ||
                               current.texture_bytes != soak_baseline.texture_bytes ||
                               current.buffer_bytes != soak_baseline.buffer_bytes)) {
                fprintf(stderr, u8"장시간 검사 중 GPU 자원 사용량이 증가했습니다.\n");
                result = 1;
                break;
            }
            soak_baseline = current;
            soak_ready = 1;
        }
        if (lab.frames && lab.seconds <= 0 && frame + 1 >= lab.frames)
            break;
        if (lab.seconds > 0 && GetTime() - run_started >= lab.seconds)
            break;
    }
    lab.elapsed_seconds = GetTime() - run_started;
    if (lab.seconds > 0 && samples)
        qsort(samples, (size_t)sample_count, sizeof(*samples), sample_order);
    report(&lab);
    result |= castle_gpu_validate(lab.gpu);
    result |= benchmark_report(&lab, samples, sample_count);
    if (lab.capture) {
        RenderTexture2D target = {0};
        Texture2D capture_texture = castle_gpu_output(lab.gpu);
        if (lab.frame.pixel_scale > 0 && !lab.capture_logical) {
            target = LoadRenderTexture(lab.frame.width, lab.frame.height);
            BeginTextureMode(target);
            ClearBackground(BLACK);
            castle_gpu_present(lab.gpu, (Rectangle){0, 0, (float)lab.frame.width, (float)lab.frame.height});
            EndTextureMode();
            capture_texture = target.texture;
        }
        Image image = LoadImageFromTexture(capture_texture);
        ImageFlipVertical(&image);
        int size = 0;
        unsigned char *png = ExportImageToMemory(image, ".png", &size);
        result |= !png || !game_write_bytes(lab.capture, png, (size_t)size);
        MemFree(png);
        UnloadImage(image);
        if (target.id)
            UnloadRenderTexture(target);
    }
finish:
    free(samples);
    if (lab.font.texture.id)
        UnloadFont(lab.font);
    UnloadTexture(lab.albedo);
    UnloadTexture(lab.normal);
    UnloadTexture(lab.surface);
    UnloadTexture(lab.water);
    UnloadTexture(lab.scenery);
    UnloadTexture(lab.contact_policy);
    UnloadTexture(lab.animal);
    UnloadTexture(lab.bridge);
    castle_gpu_destroy(lab.gpu);
    if (castle_gpu_live_objects()) {
        fprintf(stderr, "GPU resource leak: %u\n", castle_gpu_live_objects());
        result = 1;
    }
    CloseWindow();
    return result;
}
int main(int argc, char **argv) {
    GameArguments args;
    if (!game_arguments(&args, argc, argv))
        return 2;
    int result = run(args.count, args.values);
    game_arguments_free(&args);
    return result;
}
