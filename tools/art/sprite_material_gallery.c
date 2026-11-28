/* 원화·알베도·법선과 네 방향 조명을 동일한 최근접 화소로 비교합니다. */
#include "sprite_materials.h"
#include "assets_index.h"
#include "raylib.h"
#include "storage.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Selection {
    const char *id;
    int x, y, w, h;
} Selection;
static Image read_image(const char *root, const char *relative) {
    char path[GAME_PATH_BYTES];
    int size = 0;
    if (!game_join_path(path, sizeof(path), root, relative))
        return (Image){0};
    unsigned char *bytes = game_read_bytes(path, &size);
    Image image = bytes ? LoadImageFromMemory(".png", bytes, size) : (Image){0};
    free(bytes);
    if (image.data)
        ImageFormat(&image, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    return image;
}
int art_sprite_materials_gallery(const char *runtime, const char *output) {
    const Selection selections[] = {{"buildings.house.0", 0, 0, 0, 0},
                                    {"buildings.house.1", 0, 0, 0, 0},
                                    {"buildings.house.2", 0, 0, 0, 0},
                                    {"buildings.house.3", 0, 0, 0, 0},
                                    {"buildings.keep.0", 0, 0, 0, 0},
                                    {"buildings.keep.1", 0, 0, 0, 0},
                                    {"buildings.keep.2", 0, 0, 0, 0},
                                    {"buildings.keep.3", 0, 0, 0, 0},
                                    {"buildings.windmill_blades.0", 768, 0, 256, 256},
                                    {"buildings.farms", 512, 2560, 256, 256},
                                    {"buildings.observatory_motion", 256, 512, 256, 256},
                                    {"buildings.greenhouse_turns", 512, 256, 256, 256},
                                    {"terrain.trees", 0, 0, 128, 128},
                                    {"terrain.trees", 128, 0, 128, 128},
                                    {"terrain.trees", 256, 0, 128, 128},
                                    {"terrain.trees", 384, 0, 128, 128},
                                    {"terrain.trees", 512, 384, 128, 128},
                                    {"terrain.rocks", 0, 0, 128, 128},
                                    {"terrain.rocks", 128, 256, 128, 128},
                                    {"terrain.rocks", 256, 512, 128, 128},
                                    {"terrain.props", 0, 0, 256, 256},
                                    {"terrain.props", 256, 256, 256, 256},
                                    {"roads.bridges", 128, 128, 128, 128},
                                    {"boats.fishing_large_boat.2", 256, 0, 128, 128},
                                    {"units.villagers", 0, 0, 96, 96},
                                    {"units.villagers", 96, 0, 96, 96},
                                    {"units.villagers", 192, 0, 96, 96},
                                    {"units.villagers", 288, 0, 96, 96},
                                    {"units.villagers", 1056, 4512, 96, 96},
                                    {"units.uniform_camp", 1632, 1920, 96, 96},
                                    {"units.uniform_lodge", 1632, 1920, 96, 96},
                                    {"units.uniform_barracks", 1632, 1920, 96, 96},
                                    {"units.uniform_garrison", 1632, 1920, 96, 96},
                                    {"units.enemies_walk", 480, 576, 96, 96},
                                    {"units.enemies_attack", 576, 864, 96, 96},
                                    {"units.merchant", 192, 192, 96, 96},
                                    {"animals.walk", 192, 2112, 96, 96},
                                    {"animals.young", 192, 2112, 96, 96},
                                    {"animals.swim", 192, 2112, 96, 96},
                                    {"animals.attack", 192, 2112, 96, 96},
                                    {"animals.idle", 192, 192, 96, 96},
                                    {"boats.fishing_idle", 128, 128, 128, 128},
                                    {"boats.fishing_small_boat.0", 0, 0, 128, 128},
                                    {"boats.small_net.0", 384, 0, 128, 128},
                                    {"buildings.manor.3", 0, 0, 0, 0},
                                    {"buildings.manor.5", 0, 0, 0, 0},
                                    {"buildings.gates", 256, 128, 128, 128},
                                    {"buildings.wall_links", 512, 128, 128, 128},
                                    {"buildings.farm.0", 0, 0, 0, 0},
                                    {"buildings.irrigated_farm.0", 0, 0, 0, 0},
                                    {"buildings.orchard.0", 0, 0, 0, 0},
                                    {"buildings.stone_road.0", 0, 0, 0, 0}};
    const Vector3 lights[] = {{-.6f, .2f, .77f}, {.6f, .2f, .77f}, {0, -.7f, .71f}, {0, .7f, .71f}};
    const int count = (int)(sizeof(selections) / sizeof(selections[0])), cell = 112, rows = 12;
    if (!game_make_dirs(output))
        return 1;
    Image sheet = {0};
    for (int i = 0; i < count; i++) {
        Selection selection = selections[i];
        const CastleAssetRecord *a = NULL;
        for (int j = 0; j < CASTLE_ASSET_COUNT; j++)
            if (!strcmp(CASTLE_ASSETS[j].id, selection.id))
                a = &CASTLE_ASSETS[j];
        if (!a)
            return 1;
        if (i % rows == 0)
            sheet = GenImageColor(cell * 7, cell * rows, (Color){40, 43, 49, 255});
        char path[GAME_PATH_BYTES];
        Image original = read_image(runtime, a->path);
        snprintf(path, sizeof(path), "materials/albedo/%s", a->path);
        Image albedo = read_image(runtime, path);
        snprintf(path, sizeof(path), "materials/normals/%s", a->path);
        Image normal = read_image(runtime, path);
        if (!original.data || !albedo.data || !normal.data)
            return 1;
        if (!selection.w) {
            selection.w = original.width;
            selection.h = original.height;
        }
        int opaque = 0;
        for (int y = selection.y; y < selection.y + selection.h; y++)
            for (int x = selection.x; x < selection.x + selection.w; x++)
                if (x < original.width && y < original.height &&
                    ((Color *)original.data)[y * original.width + x].a >= 128)
                    opaque++;
        /* 새끼 등 일부 동작은 비어 있는 종 칸 대신 실제 첫 프레임을 검수합니다. */
        if (!opaque)
            for (int y = 0; y < original.height && !opaque; y++)
                for (int x = 0; x < original.width && !opaque; x++)
                    if (((Color *)original.data)[y * original.width + x].a >= 128) {
                        selection.x = x / selection.w * selection.w;
                        selection.y = y / selection.h * selection.h;
                        opaque = 1;
                    }
        float scale = fminf((cell - 8) / (float)selection.w, (cell - 8) / (float)selection.h);
        int width = (int)(selection.w * scale), height = (int)(selection.h * scale), row = i % rows;
        for (int y = 0; y < height; y++)
            for (int x = 0; x < width; x++) {
                int sx = selection.x + x * selection.w / width, sy = selection.y + y * selection.h / height;
                if (sx >= original.width || sy >= original.height)
                    continue;
                Color before = ((Color *)original.data)[sy * original.width + sx];
                if (before.a < 128)
                    continue;
                Color base = ((Color *)albedo.data)[sy * albedo.width + sx];
                Color encoded = ((Color *)normal.data)[sy * normal.height / original.height * normal.width +
                                                       sx * normal.width / original.width];
                Vector3 n = {encoded.r / 127.5f - 1, encoded.g / 127.5f - 1, encoded.b / 127.5f - 1};
                for (int col = 0; col < 7; col++) {
                    Color color = col == 0 ? before : col == 2 ? encoded : base;
                    if (col >= 3) {
                        Vector3 light = lights[col - 3];
                        float gain = .27f + .73f * fmaxf(0, n.x * light.x + n.y * light.y + n.z * light.z);
                        color.r = (unsigned char)(base.r * gain);
                        color.g = (unsigned char)(base.g * gain);
                        color.b = (unsigned char)(base.b * gain);
                    }
                    ((Color *)sheet.data)[(row * cell + cell - height - 4 + y) * sheet.width + col * cell +
                                          (cell - width) / 2 + x] = color;
                }
            }
        printf("GALLERY\t%d\t%d\t%s\t%d,%d,%d,%d\n", i / rows + 1, row + 1, selection.id, selection.x,
               selection.y, selection.w, selection.h);
        UnloadImage(original);
        UnloadImage(albedo);
        UnloadImage(normal);
        if (row == rows - 1 || i == count - 1) {
            snprintf(path, sizeof(path), "%s/gallery_%d.png", output, i / rows + 1);
            int size = 0;
            unsigned char *bytes = ExportImageToMemory(sheet, ".png", &size);
            int ok = bytes && game_write_bytes(path, bytes, (size_t)size);
            MemFree(bytes);
            UnloadImage(sheet);
            if (!ok)
                return 1;
        }
    }
    return 0;
}
