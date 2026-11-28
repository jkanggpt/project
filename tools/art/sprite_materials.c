/* 법선은 세계 X·Y·Z 축이며 화면 아래쪽은 양의 Y, 위쪽 표면은 양의 Z입니다. */
#include "sprite_materials.h"
#include "assets_index.h"
#include "raylib.h"
#include "storage.h"
#include "trace.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Profile {
    char name[32];
    int kind, roof, y, z, curve, detail, minimum, maximum, lx, ly, lz, reference;
} Profile;
typedef struct Entry {
    char id[128], stage[256], profile[32], mode[32];
    int width, height, columns, rows, divisor;
} Entry;
typedef struct Vector {
    int x, y, z;
} Vector;
typedef struct Catalog {
    Profile profiles[16];
    Entry entries[512];
    int profiles_count, count;
} Catalog;

static int clamp(int n, int lo, int hi) { return n < lo ? lo : n > hi ? hi : n; }
static unsigned root(uint64_t n) {
    uint64_t result = 0, bit = UINT64_C(1) << 62;
    while (bit > n)
        bit >>= 2;
    while (bit) {
        if (n >= result + bit) {
            n -= result + bit;
            result = (result >> 1) + bit;
        } else
            result >>= 1;
        bit >>= 2;
    }
    return (unsigned)result;
}
static Vector unit(Vector v) {
    unsigned length = root((uint64_t)((int64_t)v.x * v.x + (int64_t)v.y * v.y + (int64_t)v.z * v.z));
    if (!length)
        return (Vector){0, 0, 4096};
    return (Vector){(int)((int64_t)v.x * 4096 / length), (int)((int64_t)v.y * 4096 / length),
                    (int)((int64_t)v.z * 4096 / length)};
}
static unsigned char encode(int v) { return (unsigned char)clamp(((v + 4096) * 255 + 4096) / 8192, 0, 255); }
static int luminance(Color c) { return (77 * c.r + 150 * c.g + 29 * c.b) / 256; }
static const CastleAssetRecord *record(const char *id) {
    for (int i = 0; i < CASTLE_ASSET_COUNT; i++)
        if (!strcmp(CASTLE_ASSETS[i].id, id))
            return &CASTLE_ASSETS[i];
    return NULL;
}
static unsigned char *read(const char *base, const char *relative, int *size) {
    char path[GAME_PATH_BYTES];
    return game_join_path(path, sizeof(path), base, relative) ? art_read_bytes(path, size) : NULL;
}
static int catalog(Catalog *c, const char *sources) {
    int size = 0, version = 0, offset = 0;
    memset(c, 0, sizeof(*c));
    unsigned char *bytes = read(sources, "sprite_materials/profiles.txt", &size);
    if (!bytes)
        return 0;
    char *text = calloc((size_t)size + 1, 1);
    if (!text) {
        free(bytes);
        return 0;
    }
    memcpy(text, bytes, (size_t)size);
    free(bytes);
    int ok = sscanf(text, "CASTLE_MATERIAL_PROFILES %d\nPROFILES %d\n%n", &version, &c->profiles_count,
                    &offset) == 2 &&
             version == 1 && c->profiles_count > 0 && c->profiles_count <= 16;
    char *line = text + offset;
    for (int i = 0; ok && i < c->profiles_count; i++) {
        Profile *p = &c->profiles[i];
        ok = sscanf(line, "%31s %d %d %d %d %d %d %d %d %d %d %d %d%n", p->name, &p->kind, &p->roof, &p->y,
                    &p->z, &p->curve, &p->detail, &p->minimum, &p->maximum, &p->lx, &p->ly, &p->lz,
                    &p->reference, &offset) == 13;
        line += offset;
    }
    free(text);
    bytes = read(sources, "sprite_materials/catalog.txt", &size);
    if (!bytes)
        return 0;
    text = calloc((size_t)size + 1, 1);
    if (!text) {
        free(bytes);
        return 0;
    }
    memcpy(text, bytes, (size_t)size);
    free(bytes);
    ok = ok && sscanf(text, "CASTLE_SPRITE_MATERIALS %d\nASSETS %d\n%n", &version, &c->count, &offset) == 2 &&
         version == 1 && c->count > 0 && c->count <= 512;
    line = text + offset;
    for (int i = 0; ok && i < c->count; i++) {
        Entry *e = &c->entries[i];
        ok = sscanf(line, "%127s %255s %d %d %d %d %d %31s %31s%n", e->id, e->stage, &e->width, &e->height,
                    &e->columns, &e->rows, &e->divisor, e->profile, e->mode, &offset) == 9;
        ok = ok && e->columns > 0 && e->rows > 0 && e->divisor > 0 && e->width > 0 && e->height > 0 &&
             e->width % (e->columns * e->divisor) == 0 && e->height % (e->rows * e->divisor) == 0 &&
             record(e->id);
        line += offset;
    }
    free(text);
    return ok;
}
static const Profile *profile(const Catalog *c, const Entry *e) {
    for (int i = 0; i < c->profiles_count; i++)
        if (!strcmp(e->profile, c->profiles[i].name))
            return &c->profiles[i];
    return NULL;
}
static Image image_read(const char *base, const char *relative) {
    int size = 0;
    unsigned char *bytes = read(base, relative, &size);
    Image image = bytes ? LoadImageFromMemory(".png", bytes, size) : (Image){0};
    free(bytes);
    if (image.data)
        ImageFormat(&image, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    return image;
}
static int write_image(const char *output, const char *kind, const char *path, Image image) {
    char relative[GAME_PATH_BYTES], destination[GAME_PATH_BYTES], parent[GAME_PATH_BYTES];
    snprintf(relative, sizeof(relative), "materials/%s/%s", kind, path);
    if (!game_join_path(destination, sizeof(destination), output, relative))
        return 0;
    snprintf(parent, sizeof(parent), "%s", destination);
    char *slash = strrchr(parent, '/');
    if (!slash)
        return 0;
    *slash = 0;
    if (!game_make_dirs(parent))
        return 0;
    int size = 0;
    unsigned char *bytes = ExportImageToMemory(image, ".png", &size);
    int ok = bytes && art_write_bytes(destination, bytes, (size_t)size);
    MemFree(bytes);
    return ok;
}
static int similar(Color a, Color b) {
    int sa = a.r + a.g + a.b + 1, sb = b.r + b.g + b.b + 1;
    return b.a >= 32 && abs(a.r * 256 / sa - b.r * 256 / sb) + abs(a.g * 256 / sa - b.g * 256 / sb) < 32;
}
static Color delight(Color c, Vector n, const Profile *p) {
    /* 어두운 선과 유리색을 보호하고 RGB에 같은 비율만 적용합니다. */
    if (luminance(c) < 32 || (c.b > c.r * 3 / 2 && c.b > c.g * 5 / 4))
        return c;
    int dot = (n.x * p->lx + n.y * p->ly + n.z * p->lz) / 4096;
    int light = 2048 + clamp(dot, 0, 4096) / 2;
    int gain = clamp(p->reference * 4096 / light, p->minimum, p->maximum);
    int maximum = c.r > c.g ? c.r : c.g;
    maximum = maximum > c.b ? maximum : c.b;
    if (maximum)
        gain = clamp(gain, 0, 255 * 4096 / maximum);
    c.r = (unsigned char)((c.r * gain + 2048) / 4096);
    c.g = (unsigned char)((c.g * gain + 2048) / 4096);
    c.b = (unsigned char)((c.b * gain + 2048) / 4096);
    return c;
}
static int plant(Color c) { return c.g * 100 > c.r * 110 && c.g * 100 > c.b * 110 && luminance(c) > 35; }
static void distance_field(const Color *pixels, int stride, int w, int h, uint16_t *d, int vegetation) {
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++)
            d[y * w + x] = (pixels[y * stride + x].a < 32 || (vegetation && !plant(pixels[y * stride + x])))
                               ? 0
                               : (uint16_t)(x == 0 || y == 0 || x == w - 1 || y == h - 1 ? 3 : 30000);
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            int i = y * w + x, best = d[i];
            if (x)
                best = best < d[i - 1] + 3 ? best : d[i - 1] + 3;
            if (y)
                best = best < d[i - w] + 3 ? best : d[i - w] + 3;
            if (x && y)
                best = best < d[i - w - 1] + 4 ? best : d[i - w - 1] + 4;
            if (x + 1 < w && y)
                best = best < d[i - w + 1] + 4 ? best : d[i - w + 1] + 4;
            d[i] = (uint16_t)best;
        }
    for (int y = h - 1; y >= 0; y--)
        for (int x = w - 1; x >= 0; x--) {
            int i = y * w + x, best = d[i];
            if (x + 1 < w)
                best = best < d[i + 1] + 3 ? best : d[i + 1] + 3;
            if (y + 1 < h)
                best = best < d[i + w] + 3 ? best : d[i + w] + 3;
            if (x + 1 < w && y + 1 < h)
                best = best < d[i + w + 1] + 4 ? best : d[i + w + 1] + 4;
            if (x && y + 1 < h)
                best = best < d[i + w - 1] + 4 ? best : d[i + w - 1] + 4;
            d[i] = (uint16_t)best;
        }
}
static int bake_frame(const Color *source, int stride, Color *albedo, unsigned char *normal,
                      int normal_stride, int w, int h, int divisor, Profile p) {
    uint16_t *distance = malloc((size_t)w * h * sizeof(*distance));
    Vector *vectors = malloc((size_t)w * h * sizeof(*vectors));
    if (!distance || !vectors) {
        free(distance);
        free(vectors);
        return 0;
    }
    int left = w, right = 0, top = h, bottom = 0;
    int field = !strcmp(p.name, "field");
    distance_field(source, stride, w, h, distance, field);
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++)
            if (source[y * stride + x].a >= 32) {
                if (x < left)
                    left = x;
                if (x > right)
                    right = x;
                if (y < top)
                    top = y;
                if (y > bottom)
                    bottom = y;
            }
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            int at = y * stride + x, i = y * w + x;
            Color c = source[at];
            vectors[i] = (Vector){0, 0, 4096};
            albedo[at] = c;
            if (c.a < 32)
                continue;
            int dx = (int)distance[y * w + clamp(x + 2, 0, w - 1)] - distance[y * w + clamp(x - 2, 0, w - 1)];
            int dy = (int)distance[clamp(y + 2, 0, h - 1) * w + x] - distance[clamp(y - 2, 0, h - 1) * w + x];
            dx = clamp(dx, -6, 6);
            dy = clamp(dy, -6, 6);
            Vector base = {0, p.y, p.z};
            if (p.kind == 1) {
                int roof = top + (bottom - top) * p.roof / 256;
                int colored_roof = !strcmp(p.name, "building") && y < top + (bottom - top) * 3 / 4 &&
                                   ((c.r * 100 > c.g * 135 && c.r * 100 > c.b * 140) ||
                                    (c.b * 100 > c.r * 130 && c.b * 100 > c.g * 108));
                base = (y < roof || colored_roof)
                           ? (Vector){(x * 2 - left - right) * 1200 / (right - left + 1), 2100, 3450}
                           : (Vector){(x * 2 - left - right) * 900 / (right - left + 1), p.y, p.z};
            } else if (p.kind == 2 || (field && plant(c))) {
                int curve = p.curve;
                if (!strcmp(p.name, "foliage") && y > top + (bottom - top) * 5 / 8 && c.g * 100 < c.r * 106) {
                    base = (Vector){0, 3600, 1600};
                    curve = 220;
                }
                base.x = -dx * curve;
                base.y -= dy * curve / 2;
                base.z += dy * curve / 2;
            }
            base.z = clamp(base.z, 600, 8192);
            base = unit(base);
            albedo[at] = delight(c, base, &p);
            Color l = source[y * stride + clamp(x - 1, 0, w - 1)],
                  r = source[y * stride + clamp(x + 1, 0, w - 1)],
                  u = source[clamp(y - 1, 0, h - 1) * stride + x],
                  d = source[clamp(y + 1, 0, h - 1) * stride + x];
            int gx = similar(c, l) && similar(c, r) ? clamp(luminance(r) - luminance(l), -12, 12) : 0;
            int gy = similar(c, u) && similar(c, d) ? clamp(luminance(d) - luminance(u), -12, 12) : 0;
            base.x -= gx * p.detail;
            base.y -= gy * p.detail / 2;
            base.z += gy * p.detail / 2;
            vectors[i] = unit(base);
        }
    for (int y = 0; y < h; y += divisor)
        for (int x = 0; x < w; x += divisor) {
            Vector sum = {0};
            for (int yy = 0; yy < divisor; yy++)
                for (int xx = 0; xx < divisor; xx++)
                    if (source[(y + yy) * stride + x + xx].a >= 32) {
                        Vector v = vectors[(y + yy) * w + x + xx];
                        sum.x += v.x;
                        sum.y += v.y;
                        sum.z += v.z;
                    }
            sum = unit(sum);
            int at = (y / divisor * normal_stride + x / divisor) * 3;
            normal[at] = encode(sum.x);
            normal[at + 1] = encode(sum.y);
            normal[at + 2] = encode(sum.z);
        }
    free(distance);
    free(vectors);
    return 1;
}
int art_sprite_materials(const char *sources, const char *input, const char *output, int staged) {
    Catalog c;
    if (!catalog(&c, sources))
        return 1;
    int files = 0, frames = 0;
    for (int i = 0; i < c.count; i++) {
        const Entry *e = &c.entries[i];
        if (strcmp(e->mode, "baked"))
            continue;
        const CastleAssetRecord *a = record(e->id);
        const Profile *p = profile(&c, e);
        Image original = image_read(input, staged ? e->stage : a->path);
        if (!p || !original.data || original.width != e->width || original.height != e->height) {
            if (original.data)
                UnloadImage(original);
            fprintf(stderr, "재료 입력 오류: %s\n", e->id);
            return 1;
        }
        Image albedo = ImageCopy(original);
        Image normals = {calloc((size_t)(e->width / e->divisor) * (e->height / e->divisor), 3),
                         e->width / e->divisor, e->height / e->divisor, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8};
        int ok = albedo.data && normals.data, w = e->width / e->columns, h = e->height / e->rows;
        for (int row = 0; ok && row < e->rows; row++)
            for (int col = 0; ok && col < e->columns; col++) {
                int at = row * h * e->width + col * w,
                    n = (row * h / e->divisor * normals.width + col * w / e->divisor) * 3;
                Profile selected = *p;
                if (!strcmp(e->id, "terrain.props")) {
                    const char *name = row * e->columns + col < 4   ? "foliage"
                                       : row * e->columns + col < 8 ? "rock"
                                                                    : "flat";
                    for (int j = 0; j < c.profiles_count; j++)
                        if (!strcmp(name, c.profiles[j].name))
                            selected = c.profiles[j];
                }
                ok = bake_frame((Color *)original.data + at, e->width, (Color *)albedo.data + at,
                                (unsigned char *)normals.data + n, normals.width, w, h, e->divisor, selected);
            }
        ok = ok && write_image(output, "albedo", a->path, albedo) &&
             write_image(output, "normals", a->path, normals);
        UnloadImage(original);
        if (albedo.data)
            UnloadImage(albedo);
        if (normals.data)
            UnloadImage(normals);
        if (!ok)
            return 1;
        files++;
        frames += e->columns * e->rows;
    }
    printf("SPRITE MATERIALS: sheets=%d cells=%d errors=0\n", files, frames);
    return 0;
}
int art_sprite_materials_check(const char *sources, const char *runtime) {
    Catalog c;
    if (!catalog(&c, sources))
        return 1;
    int errors = 0, files = 0;
    uint64_t pixels = 0;
    for (int i = 0; i < CASTLE_ASSET_COUNT; i++) {
        const CastleAssetRecord *a = &CASTLE_ASSETS[i];
        int world = !strncmp(a->path, "buildings/", 10) || !strncmp(a->path, "terrain/", 8) ||
                    !strncmp(a->path, "units/", 6) || !strncmp(a->path, "roads/", 6) ||
                    !strncmp(a->path, "boats/", 6) || !strncmp(a->path, "animals/", 8);
        if (!world || strcmp(a->kind, "texture"))
            continue;
        int found = 0;
        for (int j = 0; j < c.count; j++)
            if (!strcmp(c.entries[j].id, a->id))
                found++;
        if (found != 1) {
            fprintf(stderr, "세계 재료 목록 누락·중복: %s\n", a->id);
            errors++;
        }
    }
    for (int i = 0; i < c.count; i++) {
        const Entry *e = &c.entries[i];
        int found = 0;
        for (int j = 0; j < CASTLE_MATERIAL_COUNT; j++) {
            const CastleMaterialRecord *m = &CASTLE_MATERIALS[j];
            if (strcmp(m->id, e->id))
                continue;
            found++;
            errors += strcmp(m->mode, e->mode) != 0 || m->divisor != e->divisor;
            if (!strcmp(e->mode, "baked")) {
                char expected[GAME_PATH_BYTES];
                snprintf(expected, sizeof(expected), "materials/albedo/%s", record(e->id)->path);
                errors += strcmp(expected, m->albedo) != 0;
                snprintf(expected, sizeof(expected), "materials/normals/%s", record(e->id)->path);
                errors += strcmp(expected, m->normal) != 0;
            }
        }
        errors += found != 1;
    }
    for (int i = 0; i < c.count; i++) {
        const Entry *e = &c.entries[i];
        if (strcmp(e->mode, "baked"))
            continue;
        const CastleAssetRecord *a = record(e->id);
        const Profile *p = profile(&c, e);
        char path[GAME_PATH_BYTES];
        Image original = image_read(runtime, a->path);
        snprintf(path, sizeof(path), "materials/albedo/%s", a->path);
        Image albedo = image_read(runtime, path);
        snprintf(path, sizeof(path), "materials/normals/%s", a->path);
        Image normal = image_read(runtime, path);
        int ok = p && original.data && original.width == e->width && original.height == e->height &&
                 albedo.data && normal.data && albedo.width == e->width && albedo.height == e->height &&
                 normal.width == e->width / e->divisor && normal.height == e->height / e->divisor;
        int invalid = 0;
        if (ok)
            for (int at = 0; at < e->width * e->height; at++) {
                Color s = ((Color *)original.data)[at], d = ((Color *)albedo.data)[at];
                invalid += s.a != d.a;
                if (s.a) {
                    int before[3] = {s.r, s.g, s.b}, after[3] = {d.r, d.g, d.b};
                    for (int j = 0; j < 3; j++) {
                        invalid += after[j] * 4096 < before[j] * p->minimum - 4096;
                        invalid += after[j] * 4096 > before[j] * p->maximum + 4096;
                    }
                    if (luminance(s) < 32)
                        invalid += memcmp(&s, &d, sizeof(s)) != 0;
                }
            }
        if (ok)
            for (int at = 0; at < normal.width * normal.height; at++) {
                Color n = ((Color *)normal.data)[at];
                int x = n.r * 2 - 255, y = n.g * 2 - 255, z = n.b * 2 - 255, length = x * x + y * y + z * z;
                invalid += length < 63000 || length > 67500 || z <= 0;
            }
        if (!ok || invalid) {
            errors++;
            fprintf(stderr, "재료 계약 오류: %s (%d)\n", e->id, invalid);
        }
        if (original.data)
            UnloadImage(original);
        if (albedo.data)
            UnloadImage(albedo);
        if (normal.data)
            UnloadImage(normal);
        files++;
        pixels += (uint64_t)e->width * e->height;
    }
    printf("MATERIAL CONTRACTS: sheets=%d pixels=%llu errors=%d\n", files, (unsigned long long)pixels,
           errors);
    return errors ? 1 : 0;
}
int art_sprite_materials_test(void) {
    Color source[32 * 16] = {0}, albedo[32 * 16] = {0};
    unsigned char normals[32 * 16 * 3] = {0}, repeat[32 * 16 * 3] = {0};
    Profile p = {"rock", 2, 0, 1900, 3550, 480, 40, 3584, 5120, -1800, 2400, 2800, 3500};
    for (int y = 2; y < 14; y++)
        for (int x = 2; x < 14; x++)
            source[y * 32 + x] = (Color){100, 110, 120, 255};
    int ok = bake_frame(source, 32, albedo, normals, 32, 16, 16, 1, p);
    for (int y = 0; y < 16; y++)
        for (int x = 16; x < 32; x++)
            source[y * 32 + x] = (Color){240, 4, 12, 255};
    ok = ok && bake_frame(source, 32, albedo, repeat, 32, 16, 16, 1, p) &&
         !memcmp(normals, repeat, sizeof(normals));
    int left = (8 * 32 + 2) * 3, right = (8 * 32 + 13) * 3, top = (2 * 32 + 8) * 3,
        bottom = (13 * 32 + 8) * 3;
    ok = ok && normals[left] < 128 && normals[right] > 128 && normals[top + 2] > normals[bottom + 2] &&
         normals[top + 1] < normals[bottom + 1];
    for (int y = 0; y < 16; y++)
        for (int x = 0; x < 16; x++)
            ok = ok && albedo[y * 32 + x].a == source[y * 32 + x].a;
    Color plot[16 * 16], corrected[16 * 16];
    unsigned char plot_normals[16 * 16 * 3];
    Profile field = {"field", 0, 0, 400, 4050, 280, 28, 3712, 4864, -1800, 2400, 2800, 3500};
    for (int i = 0; i < 16 * 16; i++)
        plot[i] = (Color){100, 70, 40, 255};
    for (int y = 2; y <= 6; y++)
        for (int x = 2; x <= 6; x++)
            plot[y * 16 + x] = (Color){80, 130, 55, 255};
    ok = ok && bake_frame(plot, 16, corrected, plot_normals, 16, 16, 16, 1, field);
    ok = ok && plot_normals[(4 * 16 + 2) * 3] < 128 && plot_normals[(4 * 16 + 6) * 3] > 128 &&
         plot_normals[(12 * 16 + 12) * 3] == 128;
    printf("MATERIAL FIXTURES: frame_independence=1 alpha=1 world_axes=1 field_plant=1 errors=%d\n", !ok);
    return ok ? 0 : 1;
}
