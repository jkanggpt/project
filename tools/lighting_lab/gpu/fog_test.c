/* 구름이 숨은 칸을 가리고 시드·시간·카메라를 올바르게 따르는지 검사합니다. */
#include "gpu.h"
#include "raylib.h"
#include <stdio.h>
#include <string.h>

static Image snapshot(CastleGpu *gpu, const unsigned char *visibility, int width, int height) {
    Texture2D texture = castle_gpu_clouds(gpu, visibility, width, height);
    Image image = texture.id ? LoadImageFromTexture(texture) : (Image){0};
    if (image.data) {
        ImageFlipVertical(&image);
        ImageFormat(&image, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    }
    return image;
}
static int prepare(CastleGpu *gpu, CastleGpuFrame frame) {
    unsigned char cells[12 * 8];
    memset(cells, 1, sizeof(cells));
    if (!castle_gpu_begin(gpu, &frame))
        return 0;
    castle_gpu_terrain(gpu, cells, 12, 8);
    return castle_gpu_render(gpu);
}
static int byte_difference(Image a, Image b) {
    if (!a.data || !b.data || a.width != b.width || a.height != b.height)
        return -1;
    const Color *left = a.data, *right = b.data;
    int different = 0;
    for (int i = 0; i < a.width * a.height; i++)
        different += memcmp(&left[i], &right[i], sizeof(Color)) != 0;
    return different;
}
int main(void) {
    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(256, 160, "시야 구름 검사");
    CastleGpu *gpu = castle_gpu_create();
    int errors = !gpu, checks = 0;
    CastleGpuFrame frame = {.width = 192,
                            .height = 128,
                            .map_width = 12,
                            .map_height = 8,
                            .pixel_scale = 1,
                            .scale = 1,
                            .time = 3,
                            .ambient = .5f,
                            .world_seed = 42};
    unsigned char visibility[12 * 8];
    memset(visibility, 0, sizeof(visibility));
    if (gpu && !prepare(gpu, frame))
        errors++;
    Image hidden = gpu ? snapshot(gpu, visibility, 12, 8) : (Image){0};
    if (!hidden.data)
        errors++;
    else
        for (int i = 0; i < hidden.width * hidden.height; i++)
            errors += ((Color *)hidden.data)[i].a != 255;
    checks++;
    unsigned int stable = castle_gpu_live_objects();
    Image repeat = gpu ? snapshot(gpu, visibility, 12, 8) : (Image){0};
    errors += byte_difference(hidden, repeat) != 0;
    checks++;
    memset(visibility, 1, sizeof(visibility));
    Image remembered = gpu ? snapshot(gpu, visibility, 12, 8) : (Image){0};
    errors += byte_difference(hidden, remembered) != 0;
    checks++;
    for (int y = 0; y < 8; y++)
        for (int x = 0; x < 12; x++)
            visibility[y * 12 + x] = (unsigned char)(x < 6 ? 2 : 0);
    Image edge = gpu ? snapshot(gpu, visibility, 12, 8) : (Image){0};
    int feather = 0, clear = 0;
    if (!edge.data)
        errors++;
    else
        for (int y = 24; y < 104; y++)
            for (int x = 16; x < 176; x++) {
                Color c = ((Color *)edge.data)[y * edge.width + x];
                if (x >= 96)
                    errors += c.a != 255;
                if (x >= 80 && x < 96)
                    feather += c.a > 0 && c.a < 255;
                if (x < 72)
                    clear += c.a == 0;
            }
    errors += feather < 100 || clear < 1000;
    checks++;
    frame.time = 11;
    if (gpu && !prepare(gpu, frame))
        errors++;
    memset(visibility, 0, sizeof(visibility));
    Image animated = gpu ? snapshot(gpu, visibility, 12, 8) : (Image){0};
    errors += byte_difference(hidden, animated) < 100;
    checks++;
    frame.time = 3;
    frame.world_seed = 71;
    if (gpu && !prepare(gpu, frame))
        errors++;
    Image seeded = gpu ? snapshot(gpu, visibility, 12, 8) : (Image){0};
    errors += byte_difference(hidden, seeded) < 100;
    checks++;
    frame.world_seed = 42;
    frame.origin[0] = frame.origin[1] = 16;
    if (gpu && !prepare(gpu, frame))
        errors++;
    Image shifted = gpu ? snapshot(gpu, visibility, 12, 8) : (Image){0};
    if (!shifted.data || !hidden.data)
        errors++;
    else
        for (int y = 0; y < 96; y++)
            for (int x = 0; x < 160; x++)
                errors +=
                    memcmp(&((Color *)shifted.data)[y * shifted.width + x],
                           &((Color *)hidden.data)[(y + 16) * hidden.width + x + 16], sizeof(Color)) != 0;
    checks++;
    frame.origin[0] = frame.origin[1] = 0;
    frame.ambient = .2f;
    if (gpu && !prepare(gpu, frame))
        errors++;
    Image night = gpu ? snapshot(gpu, visibility, 12, 8) : (Image){0};
    unsigned long long day_sum = 0, night_sum = 0;
    if (!night.data || !hidden.data)
        errors++;
    else
        for (int i = 0; i < night.width * night.height; i++) {
            Color a = ((Color *)hidden.data)[i], b = ((Color *)night.data)[i];
            day_sum += (unsigned)a.r + a.g + a.b;
            night_sum += (unsigned)b.r + b.g + b.b;
            errors += b.a != 255;
        }
    errors += night_sum >= day_sum;
    checks++;
    errors += castle_gpu_live_objects() != stable;
    checks++;
    Image *images[] = {&hidden, &repeat, &remembered, &edge, &animated, &seeded, &shifted, &night};
    for (int i = 0; i < 8; i++)
        if (images[i]->data)
            UnloadImage(*images[i]);
    castle_gpu_destroy(gpu);
    errors += castle_gpu_live_objects() != 0;
    CloseWindow();
    printf("GPU CLOUDS: checks=%d feather=%d clear=%d errors=%d\n", checks, feather, clear, errors);
    return errors ? 1 : 0;
}
