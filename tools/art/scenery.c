#include "image_ops.h"
#include "pipeline.h"
static void characters(const char *in, const char *out) {
    Image old = art_read_png(out, "../painted/villagers.png"), atlas = GenImageColor(1152, 1152, BLANK);
    if (old.data)
        ImageDraw(&atlas, old, (Rectangle){0, 0, (float)old.width, (float)old.height},
                  (Rectangle){0, 0, (float)old.width, (float)old.height}, WHITE);
    const char *names[2] = {"villagers_silver.png", "villagers_navy.png"};
    for (int k = 0; k < 2; k++) {
        Image src = art_read_png(in, names[k]);
        if (src.data)
            for (int sex = 0; sex < 2; sex++)
                for (int d = 0; d < 3; d++)
                    art_sequence(&atlas, src, sex * 3 + d, 6, 8 + k * 2 + sex, d * 4, 4, 0);
        UnloadImage(src);
    }
    art_save_png(out, "villagers_identity.png", atlas);
    UnloadImage(atlas);
    UnloadImage(old);
}
static void animals(const char *in, const char *out) {
    Image old = art_read_png(out, "animal_walk.png"), walk = GenImageColor(384, 2880, BLANK),
          attacks = GenImageColor(384, 2880, BLANK);
    if (old.data)
        ImageDraw(&walk, old, (Rectangle){0, 0, (float)old.width, (float)old.height},
                  (Rectangle){0, 0, (float)old.width, (float)old.height}, WHITE);
    Image fight = art_read_png(in, "animal_attacks.png"), fantasy = art_read_png(in, "fantasy.png"),
          raw = art_read_png(in, "livestock_babies.png");
    const int kinds[9] = {11, 12, 13, 14, 15, 16, 17, 19, 20};
    if (fight.data)
        for (int i = 0; i < 9; i++)
            art_sequence(&attacks, fight, i, 9, kinds[i], 0, 4, 0);
    if (fantasy.data)
        for (int i = 0; i < 6; i++) {
            art_sequence(&walk, fantasy, i, 6, 24 + i, 0, 8, 0);
            art_sequence(&attacks, fantasy, i, 6, 24 + i, 0, 8, 4);
        }
    Image babies = GenImageColor(384, 384, BLANK);
    if (raw.data)
        for (int i = 0; i < 4; i++)
            art_sequence(&babies, raw, i, 4, i, 0, 4, 0);
    art_save_png(out, "animal_walk_identity.png", walk);
    art_save_png(out, "animal_attack_identity.png", attacks);
    art_save_png(out, "animal_babies.png", babies);
    Image icons = GenImageColor(384, 768, BLANK);
    for (int k = 0; k < 30; k++)
        ImageDraw(&icons, walk, (Rectangle){96, k * 96.0f, 96, 96},
                  (Rectangle){(k % 4) * 96.0f, (k / 4) * 96.0f, 96, 96}, WHITE);
    art_save_png(out, "animals_identity.png", icons);
    UnloadImage(icons);
    UnloadImage(babies);
    UnloadImage(raw);
    UnloadImage(old);
    UnloadImage(walk);
    UnloadImage(attacks);
    UnloadImage(fight);
    UnloadImage(fantasy);
}
static void raiders(const char *in, const char *out) {
    const char *names[6] = {"raid_bandits.png", "raid_monsters.png", "raid_undead.png",
                            "raid_forest.png",  "raid_north.png",    "raid_cult.png"};
    const int counts[6] = {4, 5, 5, 5, 4, 5};
    Image walk = GenImageColor(1536, 2016, BLANK), attack = GenImageColor(1536, 2016, BLANK);
    int type = 0;
    for (int k = 0; k < 6; k++) {
        Image src = art_read_png(in, names[k]);
        if (src.data)
            for (int n = 0; n < counts[k]; n++, type++) {
                Image frames[24];
                int max = 1;
                for (int d = 0; d < 3; d++)
                    for (int f = 0; f < 8; f++) {
                        Image im = art_cut(src, f == 7 ? 0 : f, n * 3 + d, 8, counts[k] * 3);
                        frames[d * 8 + f] = im;
                        if (im.width > max)
                            max = im.width;
                        if (im.height > max)
                            max = im.height;
                    }
                for (int d = 0; d < 3; d++)
                    for (int f = 0; f < 8; f++)
                        art_paste(f < 4 ? &walk : &attack, frames[d * 8 + f], (type % 4) * 4 + f % 4,
                                  (type / 4) * 3 + d, 96, 92.0f / max);
            }
        UnloadImage(src);
    }
    art_save_png(out, "raider_walk.png", walk);
    art_save_png(out, "raider_attack.png", attack);
    UnloadImage(walk);
    UnloadImage(attack);
}
/* 각 행의 연결 성분을 분리해 원본의 불균등한 수목 간격도 안전하게 자릅니다. */
static void trees(const char *out) {
    Image src = art_read_png(art_input("nature"), "trees.png"), atlas = GenImageColor(640, 512, BLANK);
    if (!src.data) {
        UnloadImage(atlas);
        return;
    }
    art_clean(&src);
    Color *pixels = src.data;
    int cells = src.width * src.height;
    int *labels = calloc((size_t)cells, sizeof(int)), *queue = malloc((size_t)cells * sizeof(int));
    if (!labels || !queue) {
        art_errors++;
        free(labels);
        free(queue);
        UnloadImage(src);
        UnloadImage(atlas);
        return;
    }
    for (int row = 0; row < 4; row++) {
        int top = art_seam(src, row, 4, 0, 0, src.width), bottom = art_seam(src, row + 1, 4, 0, 0, src.width);
        struct Part {
            int label, area, x0, y0, x1, y1;
        } parts[2048];
        int count = 0;
        for (int y = top; y < bottom; y++)
            for (int x = 0; x < src.width; x++) {
                int p = y * src.width + x;
                if (labels[p] || !pixels[p].a)
                    continue;
                if (count == 2048) {
                    art_errors++;
                    continue;
                }
                int head = 0, tail = 1, label = row * 2048 + count + 1;
                queue[0] = p;
                labels[p] = label;
                struct Part part = {label, 0, x, y, x, y};
                while (head < tail) {
                    int q = queue[head++], xx = q % src.width, yy = q / src.width;
                    part.area++;
                    if (xx < part.x0)
                        part.x0 = xx;
                    if (xx > part.x1)
                        part.x1 = xx;
                    if (yy < part.y0)
                        part.y0 = yy;
                    if (yy > part.y1)
                        part.y1 = yy;
                    for (int dy = -1; dy <= 1; dy++)
                        for (int dx = -1; dx <= 1; dx++) {
                            int nx = xx + dx, ny = yy + dy;
                            if (nx < 0 || nx >= src.width || ny < top || ny >= bottom)
                                continue;
                            int n = ny * src.width + nx;
                            if (!labels[n] && pixels[n].a) {
                                labels[n] = label;
                                queue[tail++] = n;
                            }
                        }
                }
                parts[count++] = part;
            }
        if (count < 5) {
            art_errors++;
            continue;
        }
        int selected[5];
        for (int c = 0; c < 5; c++) {
            int best = -1;
            for (int n = 0; n < count; n++) {
                int used = 0;
                for (int k = 0; k < c; k++)
                    used |= selected[k] == n;
                if (!used && (best < 0 || parts[n].area > parts[best].area))
                    best = n;
            }
            selected[c] = best;
        }
        for (int a = 0; a < 5; a++)
            for (int b = a + 1; b < 5; b++)
                if (parts[selected[a]].x0 > parts[selected[b]].x0) {
                    int t = selected[a];
                    selected[a] = selected[b];
                    selected[b] = t;
                }
        for (int c = 0; c < 5; c++) {
            struct Part body = parts[selected[c]];
            Image frame = GenImageColor(src.width, bottom - top, BLANK);
            Color *p = frame.data;
            for (int n = 0; n < count; n++) {
                struct Part f = parts[n];
                int nearest = 0, best = INT32_MAX;
                for (int k = 0; k < 5; k++) {
                    struct Part v = parts[selected[k]];
                    int dx = (f.x0 + f.x1 - v.x0 - v.x1) / 2;
                    int d = dx * dx;
                    if (d < best) {
                        best = d;
                        nearest = k;
                    }
                }
                if (nearest != c || (n != selected[c] && (f.x1 < body.x0 - 24 || f.x0 > body.x1 + 24)))
                    continue;
                for (int y = f.y0; y <= f.y1; y++)
                    for (int x = f.x0; x <= f.x1; x++)
                        if (labels[y * src.width + x] == f.label)
                            p[(y - top) * src.width + x] = pixels[y * src.width + x];
            }
            ImageAlphaCrop(&frame, .1f);
            float scale = 124.0f / (frame.width > frame.height ? frame.width : frame.height);
            art_paste(&atlas, frame, c, row, 128, scale);
        }
    }
    art_save_png(out, "trees.png", atlas);
    free(labels);
    free(queue);
    UnloadImage(src);
    UnloadImage(atlas);
}
static void scenery(const char *in, const char *out) {
    Image raw = art_read_png(in, "gates.png"), atlas = GenImageColor(512, 512, BLANK);
    if (raw.data)
        for (int r = 0; r < 4; r++)
            for (int c = 0; c < 4; c++) {
                Image f = art_cut(raw, c, r, 4, 4);
                float scale = 124.0f / (f.width > f.height ? f.width : f.height);
                art_paste(&atlas, f, c, r, 128, scale);
            }
    art_save_png(out, "gates.png", atlas);
    UnloadImage(atlas);
    UnloadImage(raw);
    raw = art_read_png(in, "farms.png");
    atlas = GenImageColor(1024, 1280, BLANK);
    if (raw.data)
        for (int r = 0; r < 5; r++)
            for (int c = 0; c < 4; c++) {
                Image f = art_cut(raw, c, r, 4, 5);
                ImageResizeNN(&f, 256, 256);
                ImageDraw(&atlas, f, (Rectangle){0, 0, 256, 256},
                          (Rectangle){c * 256.0f, r * 256.0f, 256, 256}, WHITE);
                UnloadImage(f);
            }
    art_save_png(out, "farms_planting.png", atlas);
    UnloadImage(atlas);
    UnloadImage(raw);
    raw = art_read_png(in, "greenhouse_turns.png");
    atlas = GenImageColor(1024, 1024, BLANK);
    if (raw.data)
        for (int r = 0; r < 4; r++)
            for (int c = 0; c < 4; c++) {
                Image f = art_cut(raw, c, r, 4, 4);
                float scale = 252.0f / (f.width > f.height ? f.width : f.height);
                art_paste(&atlas, f, c, r, 256, scale);
            }
    art_save_png(out, "greenhouse_turns.png", atlas);
    UnloadImage(atlas);
    UnloadImage(raw);
}
static void roads(const char *in, const char *out) {
    Image raw = art_read_png(in, "road_surfaces.png"), atlas = GenImageColor(1024, 3072, BLANK);
    if (raw.data)
        for (int type = 0; type < 3; type++) {
            Image tile = ImageFromImage(
                raw, (Rectangle){type * (raw.width / 3.0f), 0, raw.width / 3.0f, (float)raw.height});
            ImageResizeNN(&tile, 64, 64);
            Color *p = tile.data;
            for (int mask = 0; mask < 256; mask++)
                for (int y = 0; y < 64; y++)
                    for (int x = 0; x < 64; x++) {
                        int on = (x >= 8 && x < 56 && y >= 8 && y < 56) ||
                                 ((mask & 1) && y < 32 && x >= 8 && x < 56) ||
                                 ((mask & 2) && x >= 32 && y >= 8 && y < 56) ||
                                 ((mask & 4) && y >= 32 && x >= 8 && x < 56) ||
                                 ((mask & 8) && x < 32 && y >= 8 && y < 56);
                        if (x >= 56 && y < 8)
                            on = (mask & 19) == 19;
                        if (x >= 56 && y >= 56)
                            on = (mask & 38) == 38;
                        if (x < 8 && y >= 56)
                            on = (mask & 76) == 76;
                        if (x < 8 && y < 8)
                            on = (mask & 137) == 137;
                        if (type == 2)
                            on = 1;
                        Color color = p[y * 64 + x];
                        color.a = on ? 255 : 0;
                        ((Color *)
                             atlas.data)[(type * 1024 + (mask / 16) * 64 + y) * 1024 + (mask % 16) * 64 + x] =
                            color;
                    }
            UnloadImage(tile);
        }
    art_save_png(out, "road_connections.png", atlas);
    UnloadImage(raw);
    UnloadImage(atlas);
}
static void check_assets(const char *dir) {
    const char *names[] = {"villagers_identity.png",
                           "animal_walk_identity.png",
                           "animal_attack_identity.png",
                           "animal_babies.png",
                           "animals_identity.png",
                           "raider_walk.png",
                           "raider_attack.png",
                           "trees.png",
                           "gates.png",
                           "farms_planting.png",
                           "road_connections.png",
                           "greenhouse_turns.png"};
    const int widths[] = {1152, 384, 384, 384, 384, 1536, 1536, 640, 512, 1024, 1024, 1024},
              heights[] = {1152, 2880, 2880, 384, 768, 2016, 2016, 512, 512, 1280, 3072, 1024};
    for (int i = 0; i < 12; i++) {
        Image im = art_read_png(dir, names[i]);
        if (im.data) {
            if (im.width != widths[i] || im.height != heights[i])
                art_errors++;
            if (i == 5 || i == 6)
                for (int r = 0; r < 21; r++)
                    for (int c = 0; c < 16; c++) {
                        int pixels = 0;
                        for (int y = r * 96; y < (r + 1) * 96; y++)
                            for (int x = c * 96; x < (c + 1) * 96; x++)
                                pixels += ((Color *)im.data)[y * im.width + x].a > 0;
                        if (pixels < 50)
                            art_errors++;
                    }
            if (i == 10) {
                Color *p = im.data;
                if (p[0].a || !p[(15 * 64) * 1024 + 15 * 64].a)
                    art_errors++;
                /* 완전 연결 255 마스크는 네 모서리까지 불투명합니다. */
                for (int y = 0; y < 64; y++)
                    for (int x = 0; x < 64; x++)
                        if (!p[(15 * 64 + y) * 1024 + 15 * 64 + x].a)
                            art_errors++;
            }
        }
        UnloadImage(im);
    }
}
int art_stage_scenery(int argc, char **argv) {

    SetTraceLogLevel(LOG_WARNING);
    if (argc != 3) {
        fprintf(stderr, "usage: scenery-stage <sources> <staging-output>\n");
        return 2;
    }
    if (strcmp(argv[1], "--check") == 0)
        check_assets(argv[2]);
    else {
        characters(argv[1], argv[2]);
        animals(argv[1], argv[2]);
        raiders(argv[1], argv[2]);
        trees(argv[2]);
        scenery(argv[1], argv[2]);
        roads(argv[1], argv[2]);
    }

    printf("Art art_errors=%d\n", art_errors);
    return art_errors ? 1 : 0;
}
