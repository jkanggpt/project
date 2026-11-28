/* lighting_lab.c — 동일한 표면에서 무조명 색·법선·높이·발광을 함께 굽습니다. */
#include "lighting_lab.h"
#include "raymath.h"
#include "storage.h"
#include "trace.h"
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    MAT_GRASS,
    MAT_STONE,
    MAT_WOOD,
    MAT_ROOF,
    MAT_PLASTER,
    MAT_CLOTH,
    MAT_LEAVES,
    MAT_WATER,
    MAT_DIRT,
    MAT_SKIN,
    MAT_DARK,
    MAT_FLAME,
    MAT_GLASS
};
static void add(LabPrimitive *p, int *n, int shape, int material, int solid, float x, float y, float z,
                float w, float d, float h) {
    if (*n < LAB_MAX_MODEL)
        p[(*n)++] = (LabPrimitive){{x, y, z}, {w, d, h}, shape, material, solid};
}
int lab_model(int kind, LabPrimitive out[LAB_MAX_MODEL]) {
    int n = 0;
    if (kind <= LAB_WATER) {
        const int materials[4] = {MAT_GRASS, MAT_STONE, MAT_DIRT, MAT_WATER};
        add(out, &n, 0, materials[kind], 0, 0, 0, -.0001f, 8, 8, .0001f);
    } else if (kind == LAB_HOUSE || kind == LAB_OPEN_HOUSE) {
        add(out, &n, 0, MAT_WOOD, 0, 0, 0, -.2f, 24, 18, .2f);
        add(out, &n, 0, MAT_PLASTER, 1, 0, -17, 14, 24, 1, 14);
        add(out, &n, 0, MAT_PLASTER, 1, -23, 0, 14, 1, 16, 14);
        add(out, &n, 0, MAT_PLASTER, 1, 23, 0, 14, 1, 16, 14);
        /* 앞 벽의 창 두 개와 중앙 출입구는 차폐 모양에서도 비워 둡니다. */
        add(out, &n, 0, MAT_PLASTER, 1, -21, 17, 14, 3, 1, 14);
        add(out, &n, 0, MAT_PLASTER, 1, 21, 17, 14, 3, 1, 14);
        add(out, &n, 0, MAT_PLASTER, 1, -8, 17, 14, 3, 1, 14);
        add(out, &n, 0, MAT_PLASTER, 1, 8, 17, 14, 3, 1, 14);
        add(out, &n, 0, MAT_PLASTER, 1, 0, 17, 26, 18, 1, 2);
        add(out, &n, 0, MAT_PLASTER, 1, -14.5f, 17, 3, 3.5f, 1, 3);
        add(out, &n, 0, MAT_PLASTER, 1, 14.5f, 17, 3, 3.5f, 1, 3);
        add(out, &n, 0, MAT_GLASS, 0, -14.5f, 17.15f, 15, 3.4f, .12f, 8.8f);
        add(out, &n, 0, MAT_GLASS, 0, 14.5f, 17.15f, 15, 3.4f, .12f, 8.8f);
        if (kind == LAB_HOUSE)
            add(out, &n, 2, MAT_ROOF, 1, 0, 0, 28, 26, 20, 15);
    } else if (kind == LAB_NPC || (kind >= LAB_NPC_STEP_A && kind <= LAB_NPC_STEP_C)) {
        float phase = kind == LAB_NPC ? 0 : (kind - LAB_NPC_STEP_A + 1) * PI / 2;
        float stride = sinf(phase) * 2;
        add(out, &n, 0, MAT_CLOTH, 1, -1.8f, stride, 4.5f, 1.3f, 1.5f, 4.5f);
        add(out, &n, 0, MAT_CLOTH, 1, 1.8f, -stride, 4.5f, 1.3f, 1.5f, 4.5f);
        add(out, &n, 1, MAT_CLOTH, 1, 0, 0, 13, 4, 2.8f, 6);
        add(out, &n, 1, MAT_CLOTH, 1, -4.2f, -stride, 13, 1.1f, 1.2f, 4.5f);
        add(out, &n, 1, MAT_CLOTH, 1, 4.2f, stride, 13, 1.1f, 1.2f, 4.5f);
        add(out, &n, 1, MAT_SKIN, 1, 0, 0, 22, 3.5f, 3, 3.6f);
        add(out, &n, 1, MAT_DARK, 1, 0, -.6f, 24.5f, 3.7f, 3.1f, 1.3f);
    } else if (kind == LAB_TREE) {
        add(out, &n, 0, MAT_WOOD, 1, 0, 0, 11, 2, 2, 11);
        add(out, &n, 1, MAT_LEAVES, 1, -7, 0, 25, 9, 9, 10);
        add(out, &n, 1, MAT_LEAVES, 1, 7, 0, 27, 10, 9, 11);
        add(out, &n, 1, MAT_LEAVES, 1, 0, -4, 34, 10, 9, 10);
    } else if (kind == LAB_ROCK) {
        add(out, &n, 1, MAT_STONE, 1, -3, 0, 6, 10, 8, 8);
        add(out, &n, 1, MAT_STONE, 1, 6, 2, 4, 6, 5, 5);
    } else if (kind == LAB_CRATE) {
        add(out, &n, 0, MAT_WOOD, 1, 0, 0, 8, 8, 6, 8);
    } else if (kind == LAB_TORCH) {
        add(out, &n, 0, MAT_WOOD, 1, 0, 0, 9, 1, 1, 9);
        add(out, &n, 0, MAT_DARK, 1, 0, 0, 18, 3, 3, 2);
        add(out, &n, 1, MAT_FLAME, 0, 0, 0, 24, 2.5f, 2.5f, 5);
    } else if (kind == LAB_WALL) {
        add(out, &n, 0, MAT_STONE, 1, 0, 0, 14, 24, 2, 14);
        add(out, &n, 0, MAT_STONE, 1, -22, 0, 17, 3, 3, 17);
        add(out, &n, 0, MAT_STONE, 1, 22, 0, 17, 3, 3, 17);
    } else if (kind == LAB_BALL) {
        add(out, &n, 1, MAT_PLASTER, 1, 0, 0, 7, 7, 7, 7);
    }
    return n;
}

/* 볼록 모양의 모든 반공간을 잘라 첫 교차와 바깥 법선을 구합니다. */
static int clip_plane(Vector3 n, float bound, Vector3 p, Vector3 d, float *near_t, float *far_t,
                      Vector3 *normal) {
    float denominator = Vector3DotProduct(n, d), distance = bound - Vector3DotProduct(n, p);
    if (fabsf(denominator) < 1e-7f)
        return distance >= 0;
    float t = distance / denominator;
    if (denominator < 0) {
        if (t > *near_t) {
            *near_t = t;
            *normal = Vector3Normalize(n);
        }
    } else if (t < *far_t)
        *far_t = t;
    return *near_t <= *far_t;
}
int lab_intersect(const LabPrimitive *s, Vector3 origin, Vector3 direction, float maximum, LabHit *hit) {
    Vector3 p = Vector3Subtract(origin, s->center), normal = {0};
    float near_t = -FLT_MAX, far_t = maximum;
    if (s->shape == 1) {
        Vector3 q = Vector3Divide(p, s->extent), d = Vector3Divide(direction, s->extent);
        float aa = Vector3DotProduct(d, d), bb = Vector3DotProduct(q, d), cc = Vector3DotProduct(q, q) - 1;
        float discriminant = bb * bb - aa * cc;
        if (discriminant < 0 || aa < 1e-9f)
            return 0;
        near_t = (-bb - sqrtf(discriminant)) / aa;
        far_t = (-bb + sqrtf(discriminant)) / aa;
        if (near_t < .001f)
            near_t = far_t;
        Vector3 at = Vector3Add(p, Vector3Scale(direction, near_t));
        normal =
            Vector3Normalize((Vector3){at.x / (s->extent.x * s->extent.x), at.y / (s->extent.y * s->extent.y),
                                       at.z / (s->extent.z * s->extent.z)});
    } else {
        Vector3 normals[7] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, -1}, {0, 0, 1}, {0, 0, 0}};
        float bounds[7] = {s->extent.x, s->extent.x, s->extent.y, s->extent.y, s->extent.z, s->extent.z, 0};
        int count = 6;
        if (s->shape == 2) {
            float slope = s->extent.z / s->extent.x;
            bounds[4] = 1;
            normals[5] = (Vector3){slope, 0, 1};
            bounds[5] = s->extent.z;
            normals[6] = (Vector3){-slope, 0, 1};
            bounds[6] = s->extent.z;
            count = 7;
        }
        for (int i = 0; i < count; i++)
            if (!clip_plane(normals[i], bounds[i], p, direction, &near_t, &far_t, &normal))
                return 0;
        if (near_t < .001f)
            return 0;
    }
    if (near_t < .001f || near_t >= maximum || far_t < near_t)
        return 0;
    if (hit) {
        hit->distance = near_t;
        hit->position = Vector3Add(origin, Vector3Scale(direction, near_t));
        hit->normal = normal;
        hit->material = s->material;
        if (s->shape == 2 && fabsf(normal.y) > .5f)
            hit->material = MAT_PLASTER;
    }
    return 1;
}

static unsigned char channel(float x) {
    return (unsigned char)fmaxf(0, fminf(255, floorf(x * 255 + .50001f)));
}
static Color material_color(const Image *materials, int material, Vector3 p, Vector3 n) {
    if (material == MAT_SKIN) {
        Color skin = {205, 153, 111, 255};
        if (n.y > .35f && p.z > 21 && p.z < 22.2f && fabsf(fabsf(p.x) - 1.4f) < .6f)
            skin = (Color){36, 30, 28, 255};
        return skin;
    }
    if (material == MAT_DARK)
        return (Color){48, 37, 31, 255};
    if (material == MAT_FLAME)
        return (Color){255, 149, 42, 255};
    if (material == MAT_GLASS)
        return (Color){234, 164, 70, 255};
    float u = p.x / 16, v = p.y / 16;
    if (fabsf(n.z) < .5f) {
        u = (fabsf(n.x) > .5f ? p.y : p.x) / 16;
        v = -p.z / 16;
    }
    u -= floorf(u);
    v -= floorf(v);
    int cw = materials->width / 3, ch = materials->height / 3;
    int x = material % 3 * cw + (int)(u * (cw - 2)) + 1, y = material / 3 * ch + (int)(v * (ch - 2)) + 1;
    Color result = ((Color *)materials->data)[y * materials->width + x];
    result.a = 255;
    if (material == MAT_CLOTH && p.z < 2)
        result = (Color){48, 37, 31, 255};
    return result;
}
/* 재질 노멀의 녹색은 텍스처 위쪽입니다. 표면의 월드 축으로 옮겨 합칩니다. */
static Vector3 material_normal(const Image *normals, int material, Vector3 p, Vector3 n) {
    if (material >= MAT_SKIN)
        return n;
    Vector3 u_axis = {1, 0, 0}, up_axis = {0, -1, 0};
    float u = p.x / 16, v = p.y / 16;
    if (fabsf(n.z) < .5f) {
        if (fabsf(n.x) > .5f) {
            u = p.y / 16;
            u_axis = (Vector3){0, 1, 0};
        }
        v = -p.z / 16;
        up_axis = (Vector3){0, 0, 1};
    }
    u -= floorf(u);
    v -= floorf(v);
    int cw = normals->width / 3, ch = normals->height / 3;
    int x = material % 3 * cw + (int)(u * (cw - 2)) + 1, y = material / 3 * ch + (int)(v * (ch - 2)) + 1;
    Color pixel = ((Color *)normals->data)[y * normals->width + x];
    Vector3 detail = {pixel.r / 127.5f - 1, pixel.g / 127.5f - 1, pixel.b / 127.5f - 1};
    Vector3 tangent =
        Vector3Normalize(Vector3Subtract(u_axis, Vector3Scale(n, Vector3DotProduct(u_axis, n))));
    Vector3 up = Vector3Subtract(up_axis, Vector3Scale(n, Vector3DotProduct(up_axis, n)));
    up = Vector3Normalize(Vector3Subtract(up, Vector3Scale(tangent, Vector3DotProduct(up, tangent))));
    const float strengths[9] = {.24f, .40f, .32f, .35f, .10f, .24f, .28f, .15f, .34f};
    float dx = fmaxf(-1, fminf(1, detail.x / fmaxf(.3f, detail.z))) * strengths[material];
    float dy = fmaxf(-1, fminf(1, detail.y / fmaxf(.3f, detail.z))) * strengths[material];
    return Vector3Normalize(Vector3Add(n, Vector3Add(Vector3Scale(tangent, dx), Vector3Scale(up, dy))));
}
static int write_png(const char *out, const char *name, Image im) {
    char path[GAME_PATH_BYTES];
    int length = 0;
    unsigned char *bytes = ExportImageToMemory(im, ".png", &length);
    int ok = bytes && game_join_path(path, sizeof(path), out, name) &&
             art_write_bytes(path, bytes, (size_t)length);
    MemFree(bytes);
    return ok;
}
int art_lighting_bake(const char *source, const char *output) {
    int length = 0, errors = 0;
    unsigned char *bytes = art_read_bytes(source, &length);
    Image materials = bytes ? LoadImageFromMemory(".png", bytes, length) : (Image){0};
    free(bytes);
    if (!materials.data || materials.width < 9 || materials.height < 9) {
        if (materials.data)
            UnloadImage(materials);
        return 1;
    }
    ImageFormat(&materials, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    char normal_path[GAME_PATH_BYTES];
    snprintf(normal_path, sizeof(normal_path), "%s", source);
    char *slash = strrchr(normal_path, '/'), *back = strrchr(normal_path, '\\');
    if (back && (!slash || back > slash))
        slash = back;
    if (slash)
        *(slash + 1) = 0;
    else
        normal_path[0] = 0;
    size_t used = strlen(normal_path);
    snprintf(normal_path + used, sizeof(normal_path) - used, "material_normals.png");
    int normal_length = 0;
    unsigned char *normal_bytes = art_read_bytes(normal_path, &normal_length);
    Image material_normals =
        normal_bytes ? LoadImageFromMemory(".png", normal_bytes, normal_length) : (Image){0};
    free(normal_bytes);
    if (!material_normals.data || material_normals.width < 9 || material_normals.height < 9) {
        UnloadImage(materials);
        if (material_normals.data)
            UnloadImage(material_normals);
        return 1;
    }
    ImageFormat(&material_normals, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    Image albedo = GenImageColor(LAB_ATLAS, LAB_ATLAS, BLANK);
    Image normals = GenImageColor(LAB_ATLAS, LAB_ATLAS, BLANK);
    Image surface = GenImageColor(LAB_ATLAS, LAB_ATLAS, BLANK);
    if (!albedo.data || !normals.data || !surface.data) {
        UnloadImage(materials);
        UnloadImage(material_normals);
        UnloadImage(albedo);
        UnloadImage(normals);
        UnloadImage(surface);
        return 1;
    }
    Vector3 direction = Vector3Normalize((Vector3){0, -1, -1});
    for (int kind = 0; kind < LAB_KIND_COUNT; kind++) {
        LabPrimitive model[LAB_MAX_MODEL];
        int count = lab_model(kind, model);
        for (int y = 0; y < LAB_CELL; y++)
            for (int x = 0; x < LAB_CELL; x++) {
                float sx = (x + .5f) / LAB_SCALE - LAB_ANCHOR_X, sy = (y + .5f) / LAB_SCALE - LAB_ANCHOR_Y;
                Vector3 origin = {sx, sy + 128, 128};
                LabHit closest = {.distance = 1000};
                int found = 0;
                for (int i = 0; i < count; i++) {
                    LabHit hit;
                    if (lab_intersect(&model[i], origin, direction, closest.distance, &hit)) {
                        closest = hit;
                        found = 1;
                    }
                }
                if (!found)
                    continue;
                Vector3 n = closest.normal, at = closest.position;
                if (kind == LAB_WATER)
                    n = Vector3Normalize((Vector3){.10f * cosf(at.x * .5f), .10f * sinf(at.y * .6f), 1});
                if (kind == LAB_ROCK)
                    n = Vector3Normalize((Vector3){roundf(n.x * 4), roundf(n.y * 4), roundf(n.z * 4)});
                Vector3 base_normal = n;
                n = material_normal(&material_normals, closest.material, at, n);
                int index = (kind / 4 * LAB_CELL + y) * LAB_ATLAS + kind % 4 * LAB_CELL + x;
                ((Color *)albedo.data)[index] = material_color(&materials, closest.material, at, base_normal);
                ((Color *)normals.data)[index] = (Color){channel(n.x * .5f + .5f), channel(n.y * .5f + .5f),
                                                         channel(n.z * .5f + .5f), 255};
                unsigned height = (unsigned)fmaxf(0, fminf(65535, floorf(at.z * 256 + .50001f)));
                float emission = closest.material == MAT_FLAME ? 1 : closest.material == MAT_GLASS ? .65f : 0;
                float roughness = closest.material == MAT_WATER   ? .10f
                                  : closest.material == MAT_GLASS ? .20f
                                                                  : .85f;
                ((Color *)surface.data)[index] =
                    (Color){(unsigned char)(height >> 8), (unsigned char)(height & 255), channel(emission),
                            channel(roughness)};
            }
    }
    errors += !write_png(output, "lighting_albedo.png", albedo);
    errors += !write_png(output, "lighting_normals.png", normals);
    errors += !write_png(output, "lighting_surface.png", surface);
    UnloadImage(materials);
    UnloadImage(material_normals);
    UnloadImage(albedo);
    UnloadImage(normals);
    UnloadImage(surface);
    printf("LIGHTING BAKE: kinds=%d errors=%d\n", LAB_KIND_COUNT, errors);
    return errors ? 1 : 0;
}
int lab_geometry_check(void) {
    LabPrimitive model[LAB_MAX_MODEL];
    LabHit hit;
    int errors = 0;
    int count = lab_model(LAB_GRASS, model);
    if (count != 1 || !lab_intersect(model, (Vector3){0, 0, 10}, (Vector3){0, 0, -1}, 20, &hit) ||
        hit.normal.z < .999f || fabsf(hit.position.z) > .001f)
        errors++;
    count = lab_model(LAB_HOUSE, model);
    /* 앞 창은 열려 있지만 옆 벽은 점광원을 막아야 합니다. */
    int window_blocked = 0, wall_blocked = 0;
    for (int i = 0; i < count; i++)
        if (model[i].occluder) {
            window_blocked |=
                lab_intersect(&model[i], (Vector3){-14.5f, 0, 16}, (Vector3){0, 1, 0}, 30, NULL);
            wall_blocked |= lab_intersect(&model[i], (Vector3){-21, 0, 16}, (Vector3){0, 1, 0}, 30, NULL);
        }
    if (window_blocked || !wall_blocked)
        errors++;
    LabPrimitive box = {{0, 0, 8}, {4, 4, 8}, 0, MAT_WOOD, 1};
    if (!lab_intersect(&box, (Vector3){0, 0, 30}, (Vector3){0, 0, -1}, 40, &hit) || hit.normal.z < .999f)
        errors++;
    if (lab_intersect(&box, (Vector3){20, 0, 2}, (Vector3){1, 0, 0}, 30, NULL))
        errors++;
    printf("LIGHTING GEOMETRY: floor_normal=+Z window_clear=%d wall_blocks=%d errors=%d\n", !window_blocked,
           wall_blocked, errors);
    return errors ? 1 : 0;
}
