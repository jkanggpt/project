/* 모든 발광체의 기여·칸별 목록·물 반사와 자원 수명을 실제 GPU에서 대조합니다. */
#include "gpu.h"
#include <stdio.h>
#include <string.h>
static Image render(CastleGpu *gpu, Texture2D color, Texture2D normal, Texture2D surface, int lamps,
                    int distant, int view, int sun, int occluded) {
    CastleGpuFrame frame = {.width = 192,
                            .height = 128,
                            .map_width = 12,
                            .map_height = 8,
                            .pixel_scale = 1,
                            .scale = 1,
                            .ambient = .04f,
                            .sun_enabled = sun,
                            .shadows = occluded,
                            .view = view,
                            .sun = {0, -.6f, .8f, 35}};
    if (!castle_gpu_begin(gpu, &frame))
        return (Image){0};
    CastleGpuSprite s = {0};
    s.destination[2] = 192;
    s.destination[3] = 128;
    s.source[2] = s.source[3] = 1;
    s.normal_source[2] = s.normal_source[3] = 1;
    for (int i = 0; i < 4; i++)
        s.tint[i] = 1;
    s.tags[1] = view == 5 ? GPU_MATERIAL_WATER : GPU_MATERIAL_GRASS;
    s.tags[2] = 4;
    if (!castle_gpu_sprite(gpu, color, normal, surface, &s))
        return (Image){0};
    for (int i = 0; i < lamps + distant; i++) {
        CastleGpuLight light = {{i < lamps ? 96.0f : 3000.0f, i < lamps ? 64.0f : 3000.0f, 12, 50},
                                {1, 1, 1, .005f},
                                {0, 0, 1, 0}};
        light.tags[3] = (unsigned int)i + 1;
        if (!castle_gpu_light(gpu, &light))
            return (Image){0};
    }
    if (occluded) {
        CastleGpuCaster caster = {{96, 64, 64, 0}, {128, 128, 128, 1}, {96, 64, 128, 128}, {1, 0, 0, 1}, {0}};
        if (!castle_gpu_caster(gpu, &caster))
            return (Image){0};
    }
    if (!castle_gpu_render(gpu))
        return (Image){0};
    Image image = LoadImageFromTexture(castle_gpu_output(gpu));
    if (image.data)
        ImageFormat(&image, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    return image;
}
static int center(Image image) { return image.data ? ((Color *)image.data)[64 * image.width + 96].r : -1; }
static int total(Image image) {
    int sum = 0;
    if (!image.data)
        return -1;
    for (int i = 0; i < image.width * image.height; i++)
        sum += ((Color *)image.data)[i].r;
    return sum;
}
int main(void) {
    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(192, 128, "여러 광원·물 반사 검사");
    CastleGpu *gpu = castle_gpu_create();
    if (!gpu) {
        CloseWindow();
        return 1;
    }
    Image pixels = GenImageColor(1, 1, (Color){100, 100, 100, 255}),
          map = GenImageColor(1, 1, (Color){128, 128, 255, 255}),
          material = GenImageColor(1, 1, (Color){0, 0, 90, 255});
    Texture2D color = LoadTextureFromImage(pixels), normal = LoadTextureFromImage(map),
              surface = LoadTextureFromImage(material);
    Image few = render(gpu, color, normal, surface, 16, 0, 0, 0, 0),
          many = render(gpu, color, normal, surface, 64, 0, 0, 0, 0),
          crowd = render(gpu, color, normal, surface, 512, 0, 0, 0, 0),
          offscreen = render(gpu, color, normal, surface, 512, 512, 0, 0, 0);
    int errors = !few.data || !many.data || !crowd.data || !offscreen.data;
    errors += center(many) <= center(few) + 8 || center(crowd) <= center(many) + 20;
    if (crowd.data && offscreen.data)
        errors += memcmp(crowd.data, offscreen.data, (size_t)crowd.width * crowd.height * 4) != 0;
    errors += castle_gpu_stats(gpu).lights != 1024;
    /* 칸 목록 한도(254)를 넘긴 광원도 전체 순회로 모두 더해집니다. */
    Image limit = render(gpu, color, normal, surface, 254, 0, 0, 0, 0);
    errors += castle_gpu_validate(gpu) || castle_gpu_stats(gpu).light_overflows != 0;
    Image beyond = render(gpu, color, normal, surface, 300, 0, 0, 0, 0);
    errors += castle_gpu_validate(gpu);
    int overflow_tiles = castle_gpu_stats(gpu).light_overflows;
    errors += overflow_tiles == 0 || center(beyond) <= center(limit);
    CastleGpuStats before = castle_gpu_stats(gpu);
    Image repeat = render(gpu, color, normal, surface, 512, 512, 0, 0, 0);
    CastleGpuStats after = castle_gpu_stats(gpu);
    errors += before.live_objects != after.live_objects || before.buffer_bytes != after.buffer_bytes ||
              before.texture_bytes != after.texture_bytes;
    Image day = render(gpu, color, normal, surface, 0, 0, 5, 1, 0),
          night = render(gpu, color, normal, surface, 0, 0, 5, 0, 0),
          blocked = render(gpu, color, normal, surface, 0, 0, 5, 1, 1);
    errors += !day.data || !night.data || !blocked.data || total(day) < 10000 || total(night) != 0 ||
              total(blocked) > total(day) / 100;
    errors += castle_gpu_validate(gpu);
    printf("GPU LIGHTS: 16=%d 64=%d 254=%d 300=%d 512=%d overflow_tiles=%d submitted=%d reflection=%d night=%d "
           "blocked=%d errors=%d\n",
           center(few), center(many), center(limit), center(beyond), center(crowd), overflow_tiles, 1024,
           total(day), total(night), total(blocked), errors);
    Image images[] = {few, many, crowd, offscreen, repeat, day, night, blocked, pixels, map, material, limit, beyond};
    for (size_t i = 0; i < sizeof(images) / sizeof(images[0]); i++)
        if (images[i].data)
            UnloadImage(images[i]);
    UnloadTexture(color);
    UnloadTexture(normal);
    UnloadTexture(surface);
    castle_gpu_destroy(gpu);
    errors += castle_gpu_live_objects() != 0;
    CloseWindow();
    return errors ? 1 : 0;
}
