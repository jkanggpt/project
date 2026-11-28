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
typedef struct Guide {
    char id[128];
    int shape, roof, ridge, height, roughness, emission, glow_top, glow_bottom;
} Guide;
typedef struct Tone {
    uint64_t sum[64];
    unsigned count[64];
} Tone;
typedef struct Catalog {
    Profile profiles[16];
    Entry entries[512];
    Guide guides[512];
    int profiles_count, count, guides_count;
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
    bytes = read(sources, "sprite_materials/guides.txt", &size);
    if (!bytes)
        return 0;
    text = calloc((size_t)size + 1, 1);
    if (!text) {
        free(bytes);
        return 0;
    }
    memcpy(text, bytes, (size_t)size);
    free(bytes);
    ok = ok && sscanf(text, "CASTLE_MATERIAL_GUIDES %d\nGUIDES %d\n%n", &version, &c->guides_count, &offset) == 2 &&
         version == 1 && c->guides_count > 0 && c->guides_count <= 512;
    line = text + offset;
    for (int i = 0; ok && i < c->guides_count; i++) {
        Guide *g = &c->guides[i];
        ok = sscanf(line, "%127s %d %d %d %d %d %d %d %d%n", g->id, &g->shape, &g->roof, &g->ridge,
                    &g->height, &g->roughness, &g->emission, &g->glow_top, &g->glow_bottom, &offset) == 9 &&
             g->shape >= 0 && g->shape <= 9 && g->roof >= 0 && g->roof <= 256 &&
             g->ridge >= 0 && g->ridge <= 256 && g->height >= 0 && g->height <= 256 &&
             g->roughness >= 0 && g->roughness <= 255 && g->emission >= 0 && g->emission <= 3 &&
             g->glow_top >= 0 && g->glow_bottom <= 256 && g->glow_top <= g->glow_bottom && record(g->id);
        line += offset;
    }
    free(text);
    return ok;
}
static const Guide *guide(const Catalog *c, const Entry *e) {
    for (int i = 0; i < c->guides_count; i++)
        if (!strcmp(e->id, c->guides[i].id))
            return &c->guides[i];
    return NULL;
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
static int chroma(Color c) {
    int total = c.r + c.g + c.b + 1;
    return clamp(c.r * 8 / total, 0, 7) * 8 + clamp(c.g * 8 / total, 0, 7);
}
static Color delight(Color c, int local, int reference, const Profile *p) {
    /* 색상별 저주파 밝기만 중화하고 선화·작은 질감의 상대 대비를 보존합니다. */
    if (luminance(c) < 32 || local < 24 || reference < 24)
        return c;
    int gain = clamp(4096 + (reference * 4096 / local - 4096) * 3 / 4, p->minimum, p->maximum);
    int maximum = c.r > c.g ? c.r : c.g;
    maximum = maximum > c.b ? maximum : c.b;
    if (maximum)
        gain = clamp(gain, 0, 255 * 4096 / maximum);
    c.r = (unsigned char)((c.r * gain + 2048) / 4096);
    c.g = (unsigned char)((c.g * gain + 2048) / 4096);
    c.b = (unsigned char)((c.b * gain + 2048) / 4096);
    return c;
}
static void tone_add(Tone *tone, Color c) {
    if (c.a < 128 || luminance(c) < 32)
        return;
    int key = chroma(c);
    tone->sum[key] += (unsigned)luminance(c);
    tone->count[key]++;
}
static int tone_mean(const Tone *tone, int key, int fallback) {
    return tone->count[key] ? (int)(tone->sum[key] / tone->count[key]) : fallback;
}
static int patch_mean(const Tone *patches, int columns, int rows, int x, int y, int key, int reference) {
    int bx = x >= 8 ? (x - 8) / 16 : 0, by = y >= 8 ? (y - 8) / 16 : 0;
    int fx = x >= 8 ? (x - 8) % 16 * 16 : 0, fy = y >= 8 ? (y - 8) % 16 * 16 : 0;
    int sum = 0, weights = 0;
    for (int yy = 0; yy < 2; yy++)
        for (int xx = 0; xx < 2; xx++) {
            const Tone *t = &patches[clamp(by + yy, 0, rows - 1) * columns + clamp(bx + xx, 0, columns - 1)];
            int weight = (xx ? fx : 256 - fx) * (yy ? fy : 256 - fy) / 256;
            if (t->count[key] >= 6) {
                sum += tone_mean(t, key, reference) * weight;
                weights += weight;
            }
        }
    return weights ? sum / weights : reference;
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
static int roof_pixel(Guide g, Color c, int y) {
    int colored = (c.r * 100 > c.g * 124 && c.r * 100 > c.b * 130) ||
                  (c.b * 100 > c.r * 125 && c.b * 100 > c.g * 97) ||
                  (c.g * 100 > c.r * 120 && c.g * 100 > c.b * 110);
    return (g.shape == 1 || g.shape == 2 || g.shape == 3 || g.shape == 9) &&
           ((y < g.roof && (colored || luminance(c) < 45)) ||
            (colored && y < clamp(g.roof + 32, 0, 210)));
}
static unsigned char emission(Color c, Guide g, int y) {
    if (!g.emission || y < g.glow_top || y > g.glow_bottom)
        return 0;
    int light = luminance(c), hot = 0;
    if (g.emission == 2)
        hot = (c.b > 140 && c.b * 100 > c.r * 120 && c.b * 100 > c.g * 112) ||
              (c.g > 145 && c.b > 145 && c.g * 100 > c.r * 130 && c.b * 100 > c.r * 130);
    else
        hot = c.r > 190 && c.g > (g.emission == 3 ? 75 : 125) && c.r * 100 > c.g * 108 &&
              c.g * 100 > c.b * 145 && light > (g.emission == 3 ? 100 : 150);
    return hot ? (unsigned char)clamp((light - (g.emission == 3 ? 85 : 120)) * 3, 0, 240) : 0;
}
static Vector macro_normal(Profile p, Guide g, Color c, int x, int y, int dx, int dy) {
    Vector n = {0, 0, 4096};
    if (roof_pixel(g, c, y)) {
        int direction = x < g.ridge ? -1 : 1;
        if (g.shape == 3)
            n = (Vector){(x - g.ridge) * 2800 / 128, 1200 + y * 900 / (g.roof + 1), 3200};
        else if (g.shape == 2)
            n = x < g.ridge / 2 || x > g.ridge + (256 - g.ridge) / 2
                    ? (Vector){direction * 2300, 1100, 3250}
                    : (Vector){0, 2650, 3050};
        else
            n = (Vector){direction * (g.shape == 9 ? 1100 : 1800), 2150, 3250};
    } else if (g.shape == 1 || g.shape == 2 || g.shape == 3 || g.shape == 6 || g.shape == 9) {
        if ((g.shape == 6 && y < g.roof) || (g.shape == 9 && plant(c) && y > g.roof))
            n = (Vector){0, 0, 4096};
        else
            n = (Vector){x < 30 ? -1500 : x > 226 ? 1500 : 0, 3900, 850};
    } else if (g.shape == 4 || g.shape == 5 || g.shape == 8 || (g.shape == 7 && plant(c))) {
        int curve = p.curve;
        n = (Vector){0, g.shape == 7 ? 400 : p.y, g.shape == 7 ? 4050 : p.z};
        if (!strcmp(p.name, "foliage") && y > 180 && abs(x - 128) < 40 && !plant(c)) {
            n = (Vector){0, 3650, 1550};
            curve = 200;
        }
        n.x -= dx * curve;
        n.y -= dy * curve / 2;
        n.z += dy * curve / 2;
        if (g.shape == 5)
            n.x += (x - 128) * 800 / 128;
        if (g.shape == 4 && strcmp(p.name, "foliage")) {
            n.x = (n.x + (x - 128) * 1100 / 128) / 512 * 512;
            n.y = n.y / 512 * 512;
            n.z = n.z / 512 * 512;
        }
    }
    n.z = clamp(n.z, 600, 8192);
    return unit(n);
}
static int bake_frame(const Color *source, int stride, Color *albedo, unsigned char *normal,
                      int normal_stride, int w, int h, int divisor, Profile p,
                      unsigned char *surface, const Guide *shape, const Tone *reference) {
    uint16_t *distance = malloc((size_t)w * h * sizeof(*distance));
    Vector *vectors = malloc((size_t)w * h * sizeof(*vectors));
    Color *surfaces = malloc((size_t)w * h * sizeof(*surfaces));
    int columns = (w + 15) / 16, rows = (h + 15) / 16;
    Tone *patches = calloc((size_t)columns * rows, sizeof(*patches));
    if (!distance || !vectors || !patches || !surfaces) {
        free(distance); free(vectors); free(patches); free(surfaces);
        return 0;
    }
    Guide fallback = {"fixture", p.kind == 1 ? 1 : p.kind == 2 ? 4 : !strcmp(p.name, "field") ? 7 : 0,
                      p.roof, 128, p.kind ? 256 : 32, 235, 0, 0, 0};
    Guide g = shape ? *shape : fallback;
    Tone frame_tone = {0};
    int left = w, right = 0, top = h, bottom = 0;
    distance_field(source, stride, w, h, distance, g.shape == 7);
    for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) {
        Color c = source[y * stride + x];
        tone_add(&frame_tone, c);
        tone_add(&patches[(y / 16) * columns + x / 16], c);
        if (c.a >= 32) {
            if (x < left) left = x;
            if (x > right) right = x;
            if (y < top) top = y;
            if (y > bottom) bottom = y;
        }
    }
    if (!reference) reference = &frame_tone;
    for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) {
        Color c = source[y * stride + x];
        int key = chroma(c), mean = tone_mean(reference, key, luminance(c));
        int local = patch_mean(patches, columns, rows, x, y, key, mean);
        albedo[y * stride + x] = c.a >= 32 ? delight(c, local, mean, &p) : c;
    }
    for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) {
        int at = y * stride + x, i = y * w + x;
        Color c = source[at];
        vectors[i] = (Vector){0, 0, 4096}; surfaces[i] = (Color){0, 0, 255, 255};
        if (c.a < 32) continue;
        int xx = (x - left) * 256 / (right - left + 1), yy = (y - top) * 256 / (bottom - top + 1);
        int dx = (int)distance[y * w + clamp(x + 2, 0, w - 1)] - distance[y * w + clamp(x - 2, 0, w - 1)];
        int dy = (int)distance[clamp(y + 2, 0, h - 1) * w + x] - distance[clamp(y - 2, 0, h - 1) * w + x];
        Vector base = macro_normal(p, g, c, xx, yy, clamp(dx, -6, 6), clamp(dy, -6, 6));
        Color l = albedo[y * stride + clamp(x - 1, 0, w - 1)],
              r = albedo[y * stride + clamp(x + 1, 0, w - 1)],
              u = albedo[clamp(y - 1, 0, h - 1) * stride + x],
              d = albedo[clamp(y + 1, 0, h - 1) * stride + x];
        int gx = similar(albedo[at], l) && similar(albedo[at], r) ? clamp(luminance(r) - luminance(l), -12, 12) : 0;
        int gy = similar(albedo[at], u) && similar(albedo[at], d) ? clamp(luminance(d) - luminance(u), -12, 12) : 0;
        base.x -= gx * p.detail; base.y -= gy * p.detail / 2; base.z += gy * p.detail / 2;
        vectors[i] = unit(base);
        int height = g.height * clamp(bottom - y, 0, h) / (bottom - top + 1);
        if (g.shape == 0 || (g.shape == 7 && !plant(c))) height = 0;
        int roughness = g.roughness;
        if (g.shape == 7) roughness = plant(c) ? 238 : c.g < c.r ? 247 : g.roughness;
        if (g.shape <= 3 && !roof_pixel(g, c, yy) && c.b * 100 > c.r * 135 && c.b * 100 > c.g * 108)
            roughness = 65;
        int neighbors = (luminance(l) + luminance(r) + luminance(u) + luminance(d)) / 4;
        int ao = luminance(c) >= 32 ? 255 - clamp((neighbors - luminance(c) - 18) * 2, 0, 40) : 255;
        surfaces[i] = (Color){(unsigned char)clamp(height, 0, 255), emission(c, g, yy),
                              (unsigned char)roughness, (unsigned char)ao};
    }
    for (int y = 0; y < h; y += divisor) for (int x = 0; x < w; x += divisor) {
        Vector sum = {0}; int count = 0, height = 0, roughness = 0, glow = 0, ao = 255;
        for (int yy = 0; yy < divisor; yy++) for (int xx = 0; xx < divisor; xx++)
            if (source[(y + yy) * stride + x + xx].a >= 32) {
                int at = (y + yy) * w + x + xx;
                Vector v = vectors[at]; sum.x += v.x; sum.y += v.y; sum.z += v.z;
                Color s = surfaces[at]; count++; height += s.r; roughness += s.b;
                if (s.g > glow) glow = s.g;
                if (s.a < ao) ao = s.a;
            }
        sum = unit(sum);
        int at = y / divisor * normal_stride + x / divisor;
        normal[at * 3] = encode(sum.x); normal[at * 3 + 1] = encode(sum.y); normal[at * 3 + 2] = encode(sum.z);
        if (surface) {
            Color result = {(unsigned char)(count ? height / count : 0), (unsigned char)glow,
                            (unsigned char)(count ? roughness / count : 255), (unsigned char)ao};
            memcpy(surface + at * 4, &result, sizeof(result));
        }
    }
    free(distance); free(vectors); free(patches); free(surfaces);
    return 1;
}
/* 회전 .0~.3은 하나의 원화 계열로 묶어 같은 밝기 기준을 사용합니다. */
static void family_key(const char *id, char *key, size_t bytes) {
    snprintf(key, bytes, "%s", id);
    size_t length = strlen(key);
    if (length > 2 && key[length - 2] == '.' && key[length - 1] >= '0' && key[length - 1] <= '3')
        key[length - 2] = 0;
}
static void tone_merge(Tone *into, const Tone *from) {
    for (int i = 0; i < 64; i++) {
        into->sum[i] += from->sum[i];
        into->count[i] += from->count[i];
    }
}
static Image entry_image(const char *input, const Entry *e, int staged) {
    const CastleAssetRecord *a = record(e->id);
    Image image = a ? image_read(input, staged ? e->stage : a->path) : (Image){0};
    if (image.data && (image.width != e->width || image.height != e->height)) {
        UnloadImage(image);
        image = (Image){0};
    }
    return image;
}
static Profile cell_profile(const Catalog *c, const Entry *e, const Profile *base, int cell) {
    Profile selected = *base;
    if (!strcmp(e->id, "terrain.props")) {
        const char *name = cell < 4 ? "foliage" : cell < 8 ? "rock" : "flat";
        for (int j = 0; j < c->profiles_count; j++)
            if (!strcmp(name, c->profiles[j].name))
                selected = c->profiles[j];
    }
    return selected;
}
int art_sprite_materials(const char *sources, const char *input, const char *output, int staged) {
    Catalog *c = malloc(sizeof(*c));
    Tone *tones = c ? calloc(512, sizeof(*tones)) : NULL;
    if (!c || !tones || !catalog(c, sources)) {
        free(c);
        free(tones);
        fprintf(stderr, "재료 목록을 읽지 못했습니다\n");
        return 1;
    }
    int files = 0, frames = 0, failed = 0;
    /* 1단계: 같은 계열의 모든 프레임에서 색상별 평균 밝기를 모읍니다. */
    for (int i = 0; i < c->count && !failed; i++) {
        const Entry *e = &c->entries[i];
        if (strcmp(e->mode, "baked"))
            continue;
        if (!guide(c, e) || !profile(c, e)) {
            fprintf(stderr, "재료 지침 누락: %s\n", e->id);
            failed = 1;
            break;
        }
        Image original = entry_image(input, e, staged);
        if (!original.data) {
            fprintf(stderr, "재료 입력 오류: %s\n", e->id);
            failed = 1;
            break;
        }
        const Color *pixels = original.data;
        for (int at = 0; at < e->width * e->height; at++)
            tone_add(&tones[i], pixels[at]);
        UnloadImage(original);
    }
    for (int i = 0; i < c->count && !failed; i++) {
        const Entry *e = &c->entries[i];
        if (strcmp(e->mode, "baked"))
            continue;
        const CastleAssetRecord *a = record(e->id);
        const Profile *p = profile(c, e);
        const Guide *g = guide(c, e);
        char key[128], other[128];
        family_key(e->id, key, sizeof(key));
        Tone reference = {0};
        for (int j = 0; j < c->count; j++) {
            family_key(c->entries[j].id, other, sizeof(other));
            if (!strcmp(key, other) && !strcmp(c->entries[j].mode, "baked"))
                tone_merge(&reference, &tones[j]);
        }
        Image original = entry_image(input, e, staged);
        if (!original.data) {
            failed = 1;
            break;
        }
        Image albedo = ImageCopy(original);
        int nw = e->width / e->divisor, nh = e->height / e->divisor;
        Image normals = {calloc((size_t)nw * nh, 3), nw, nh, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8};
        Image surface = {calloc((size_t)nw * nh, 4), nw, nh, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
        int ok = albedo.data && normals.data && surface.data, w = e->width / e->columns, h = e->height / e->rows;
        for (int row = 0; ok && row < e->rows; row++)
            for (int col = 0; ok && col < e->columns; col++) {
                int at = row * h * e->width + col * w, n = row * h / e->divisor * nw + col * w / e->divisor;
                Profile selected = cell_profile(c, e, p, row * e->columns + col);
                ok = bake_frame((Color *)original.data + at, e->width, (Color *)albedo.data + at,
                                (unsigned char *)normals.data + n * 3, nw, w, h, e->divisor, selected,
                                (unsigned char *)surface.data + n * 4, g, &reference);
            }
        ok = ok && write_image(output, "albedo", a->path, albedo) &&
             write_image(output, "normals", a->path, normals) && write_image(output, "surface", a->path, surface);
        UnloadImage(original);
        if (albedo.data)
            UnloadImage(albedo);
        free(normals.data);
        free(surface.data);
        if (!ok) {
            fprintf(stderr, "재료 굽기 실패: %s\n", e->id);
            failed = 1;
            break;
        }
        files++;
        frames += e->columns * e->rows;
    }
    free(tones);
    free(c);
    printf("SPRITE MATERIALS: sheets=%d cells=%d errors=%d\n", files, frames, failed);
    return failed;
}
int art_sprite_materials_check(const char *sources, const char *runtime) {
    Catalog *catalog_data = malloc(sizeof(*catalog_data));
    if (!catalog_data || !catalog(catalog_data, sources)) {
        free(catalog_data);
        return 1;
    }
    const Catalog *c = catalog_data;
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
        for (int j = 0; j < c->count; j++)
            if (!strcmp(c->entries[j].id, a->id))
                found++;
        if (found != 1) {
            fprintf(stderr, "세계 재료 목록 누락·중복: %s\n", a->id);
            errors++;
        }
    }
    for (int i = 0; i < c->count; i++) {
        const Entry *e = &c->entries[i];
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
                snprintf(expected, sizeof(expected), "materials/surface/%s", record(e->id)->path);
                errors += strcmp(expected, m->surface) != 0;
            }
        }
        errors += found != 1;
    }
    for (int i = 0; i < c->count; i++) {
        const Entry *e = &c->entries[i];
        if (strcmp(e->mode, "baked"))
            continue;
        const CastleAssetRecord *a = record(e->id);
        const Profile *p = profile(c, e);
        char path[GAME_PATH_BYTES];
        Image original = image_read(runtime, a->path);
        snprintf(path, sizeof(path), "materials/albedo/%s", a->path);
        Image albedo = image_read(runtime, path);
        snprintf(path, sizeof(path), "materials/normals/%s", a->path);
        Image normal = image_read(runtime, path);
        snprintf(path, sizeof(path), "materials/surface/%s", a->path);
        Image surface = image_read(runtime, path);
        const Guide *g = guide(c, e);
        int ok = p && g && original.data && original.width == e->width && original.height == e->height &&
                 albedo.data && normal.data && albedo.width == e->width && albedo.height == e->height &&
                 normal.width == e->width / e->divisor && normal.height == e->height / e->divisor &&
                 surface.data && surface.width == normal.width && surface.height == normal.height;
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
        /* 표면 RGBA: R 상대 높이, G 발광, B 거칠기, A 틈새 차폐. 빈 칸은 평평·무광·차폐 없음입니다. */
        if (ok)
            for (int y = 0; y < surface.height; y++)
                for (int x = 0; x < surface.width; x++) {
                    Color m = ((Color *)surface.data)[y * surface.width + x];
                    int opaque = 0;
                    for (int yy = 0; yy < e->divisor; yy++)
                        for (int xx = 0; xx < e->divisor; xx++)
                            opaque += ((Color *)original.data)[(y * e->divisor + yy) * e->width + x * e->divisor + xx].a >= 32;
                    if (!opaque)
                        invalid += m.r != 0 || m.g != 0 || m.b != 255 || m.a != 255;
                    else
                        invalid += m.a < 215 || (!g->emission && m.g != 0) || m.r > (g->height * 255 + 255) / 256 + 1;
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
        if (surface.data)
            UnloadImage(surface);
        files++;
        pixels += (uint64_t)e->width * e->height;
    }
    printf("MATERIAL CONTRACTS: sheets=%d pixels=%llu errors=%d\n", files, (unsigned long long)pixels,
           errors);
    free(catalog_data);
    return errors ? 1 : 0;
}
int art_sprite_materials_test(void) {
    static Color source[32 * 16], albedo[32 * 16];
    static unsigned char normals[32 * 16 * 3], repeat[32 * 16 * 3], surface[32 * 16 * 4];
    memset(source, 0, sizeof(source));
    Profile p = {"rock", 2, 0, 1900, 3550, 480, 40, 3584, 5120, -1800, 2400, 2800, 3500};
    for (int y = 2; y < 14; y++)
        for (int x = 2; x < 14; x++)
            source[y * 32 + x] = (Color){100, 110, 120, 255};
    int ok = bake_frame(source, 32, albedo, normals, 32, 16, 16, 1, p, surface, NULL, NULL);
    for (int y = 0; y < 16; y++)
        for (int x = 16; x < 32; x++)
            source[y * 32 + x] = (Color){240, 4, 12, 255};
    ok = ok && bake_frame(source, 32, albedo, repeat, 32, 16, 16, 1, p, NULL, NULL, NULL) &&
         !memcmp(normals, repeat, sizeof(normals));
    int frame_independence = ok;
    int left = (8 * 32 + 2) * 3, right = (8 * 32 + 13) * 3, top = (2 * 32 + 8) * 3,
        bottom = (13 * 32 + 8) * 3;
    ok = ok && normals[left] < 128 && normals[right] > 128 && normals[top + 2] > normals[bottom + 2] &&
         normals[top + 1] < normals[bottom + 1];
    int axes = ok;
    for (int y = 0; y < 16; y++)
        for (int x = 0; x < 16; x++)
            ok = ok && albedo[y * 32 + x].a == source[y * 32 + x].a;
    /* 표면: 위로 갈수록 높고, 빈 칸은 평평·무광, 차폐는 제한된 범위입니다. */
    ok = ok && surface[(3 * 32 + 8) * 4] > surface[(12 * 32 + 8) * 4] && surface[0] == 0 && surface[2] == 255 &&
         surface[3] == 255 && surface[(8 * 32 + 8) * 4 + 3] >= 215;
    int surfaces = ok;
    Color plot[16 * 16], corrected[16 * 16];
    unsigned char plot_normals[16 * 16 * 3], plot_surface[16 * 16 * 4];
    Profile field = {"field", 0, 0, 400, 4050, 280, 28, 3712, 4864, -1800, 2400, 2800, 3500};
    for (int i = 0; i < 16 * 16; i++)
        plot[i] = (Color){100, 70, 40, 255};
    for (int y = 2; y <= 6; y++)
        for (int x = 2; x <= 6; x++)
            plot[y * 16 + x] = (Color){80, 130, 55, 255};
    ok = ok && bake_frame(plot, 16, corrected, plot_normals, 16, 16, 16, 1, field, plot_surface, NULL, NULL);
    ok = ok && plot_normals[(4 * 16 + 2) * 3] < 128 && plot_normals[(4 * 16 + 6) * 3] > 128 &&
         plot_normals[(12 * 16 + 12) * 3] == 128 && plot_surface[(12 * 16 + 12) * 4] == 0;
    int field_plant = ok;
    /* 같은 계열 기준: 어둡게 칠한 회전과 밝게 칠한 회전의 큰 명암 차이를 줄입니다. */
    Color dark[16 * 16], light_side[16 * 16], dark_out[16 * 16], light_out[16 * 16];
    unsigned char scratch[16 * 16 * 3];
    for (int i = 0; i < 16 * 16; i++) {
        dark[i] = (Color){92, 74, 60, 255};
        light_side[i] = (Color){132, 106, 86, 255};
    }
    Tone family = {0};
    for (int i = 0; i < 16 * 16; i++) {
        tone_add(&family, dark[i]);
        tone_add(&family, light_side[i]);
    }
    ok = ok && bake_frame(dark, 16, dark_out, scratch, 16, 16, 16, 1, p, NULL, NULL, &family) &&
         bake_frame(light_side, 16, light_out, scratch, 16, 16, 16, 1, p, NULL, NULL, &family);
    int before = luminance(light_side[136]) - luminance(dark[136]),
        after = luminance(light_out[136]) - luminance(dark_out[136]);
    ok = ok && after < before;
    int family_tone = ok;
    /* 발광 지침: 따뜻한 창 화소만 발광하고 지침이 없으면 0입니다. */
    Guide window = {"fixture.window", 1, 80, 128, 256, 200, 1, 0, 256};
    Color lamp[16 * 16], lamp_out[16 * 16];
    unsigned char lamp_surface[16 * 16 * 4];
    for (int i = 0; i < 16 * 16; i++)
        lamp[i] = (Color){120, 100, 90, 255};
    lamp[8 * 16 + 8] = (Color){250, 200, 90, 255};
    ok = ok && bake_frame(lamp, 16, lamp_out, scratch, 16, 16, 16, 1, p, lamp_surface, &window, NULL) &&
         lamp_surface[(8 * 16 + 8) * 4 + 1] > 0 && lamp_surface[(2 * 16 + 2) * 4 + 1] == 0;
    ok = ok && bake_frame(lamp, 16, lamp_out, scratch, 16, 16, 16, 1, p, lamp_surface, NULL, NULL) &&
         lamp_surface[(8 * 16 + 8) * 4 + 1] == 0;
    int emission_guide = ok;
    printf("MATERIAL FIXTURES: frame_independence=%d alpha=1 world_axes=%d surface=%d field_plant=%d "
           "family_tone=%d emission=%d errors=%d\n",
           frame_independence, axes, surfaces, field_plant, family_tone, emission_guide, !ok);
    return ok ? 0 : 1;
}
