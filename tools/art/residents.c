#include "image_ops.h"
#include "pipeline.h"

static const char *const hair_names[6] = {"brown", "blond", "black", "red", "silver", "navy"};
static const char *const outfit_names[4] = {"camp", "lodge", "barracks", "garrison"};
static int appearance_id(int hair, int variant, int sex) {
    return variant == 1 ? hair * 2 + sex : 12 + hair * 6 + (variant - 2) * 2 + sex;
}
static void padded_paste(Image *atlas, Image frame, int col, int row, int size, float scale) {
    int width = (int)(frame.width * scale + .5f), height = (int)(frame.height * scale + .5f);
    if (width < 1 || height < 1 || width > size - 4 || height > size - 4) {
        art_errors++;
        UnloadImage(frame);
        return;
    }
    ImageResizeNN(&frame, width, height);
    ImageDraw(atlas, frame, (Rectangle){0, 0, (float)width, (float)height},
              (Rectangle){col * (float)size + (size - width) / 2.0f, (row + 1) * (float)size - 2 - height,
                          (float)width, (float)height},
              WHITE);
    UnloadImage(frame);
}
static void pair_frames(Image *atlas, Image source, int hair, int variant, int action, int uniform) {
    for (int sex = 0; sex < 2; sex++) {
        Image frames[12];
        int maximum = 1;
        for (int d = 0; d < 3; d++)
            for (int f = 0; f < 4; f++) {
                Image im = art_cut(source, f, sex * 3 + d, 4, 6);
                frames[d * 4 + f] = im;
                if (im.width > maximum)
                    maximum = im.width;
                if (im.height > maximum)
                    maximum = im.height;
            }
        for (int d = 0; d < 3; d++)
            for (int f = 0; f < 4; f++)
                padded_paste(atlas, frames[d * 4 + f], d * (uniform ? 8 : 4) + action * 4 + f,
                             appearance_id(hair, variant, sex), 96, 92.0f / maximum);
    }
}
static void resident_atlas(const char *in, const char *out) {
    Image atlas = GenImageColor(1152, 4608, BLANK), old = art_read_png(out, "villagers_identity.png");
    /* 기존 12종의 픽셀 원화를 사용하고 셀 가장자리 여백만 확보합니다. */
    if (old.data)
        for (int r = 0; r < 12; r++)
            for (int c = 0; c < 12; c++) {
                Image im = ImageFromImage(old, (Rectangle){c * 96.0f, r * 96.0f, 96, 96});
                art_clean(&im);
                ImageAlphaCrop(&im, .1f);
                int maximum = im.width > im.height ? im.width : im.height;
                padded_paste(&atlas, im, c, r, 96, maximum > 92 ? 92.0f / maximum : 1.0f);
            }
    UnloadImage(old);
    for (int hair = 0; hair < 6; hair++)
        for (int v = 2; v <= 4; v++) {
            char name[96];
            snprintf(name, sizeof(name), "villagers_%s_%d.png", hair_names[hair], v);
            Image source = art_read_png(in, name);
            if (source.data)
                pair_frames(&atlas, source, hair, v, 0, 0);
            UnloadImage(source);
        }
    art_save_png(out, "villagers.png", atlas);
    Image preview = GenImageColor(576, 768, (Color){28, 38, 37, 255});
    for (int i = 0; i < 48; i++)
        ImageDraw(&preview, atlas, (Rectangle){0, i * 96.0f, 96, 96},
                  (Rectangle){(i % 6) * 96.0f, (i / 6) * 96.0f, 96, 96}, WHITE);
    art_save_png(art_stage_output(), "residents_preview.png", preview);
    UnloadImage(preview);
    UnloadImage(atlas);
}
static void uniform_atlases(const char *in, const char *out) {
    for (int outfit = 0; outfit < 4; outfit++) {
        Image atlas = GenImageColor(2304, 4608, BLANK);
        char name[128];
        for (int hair = 0; hair < 6; hair++)
            for (int v = 1; v <= 4; v++)
                for (int action = 0; action < 2; action++) {
                    snprintf(name, sizeof(name), "uniform_%s_%s_%d_%s.png", outfit_names[outfit],
                             hair_names[hair], v, action ? "attack" : "walk");
                    Image source = art_read_png(in, name);
                    if (source.data)
                        pair_frames(&atlas, source, hair, v, action, 1);
                    UnloadImage(source);
                }
        snprintf(name, sizeof(name), "uniform_%s.png", outfit_names[outfit]);
        art_save_png(out, name, atlas);
        Image preview = GenImageColor(1152, 768, (Color){28, 38, 37, 255});
        for (int i = 0; i < 48; i++)
            for (int action = 0; action < 2; action++)
                ImageDraw(&preview, atlas, (Rectangle){action * 576.0f, i * 96.0f, 96, 96},
                          (Rectangle){((i % 6) * 2 + action) * 96.0f, (i / 6) * 96.0f, 96, 96}, WHITE);
        snprintf(name, sizeof(name), "uniform_%s_preview.png", outfit_names[outfit]);
        art_save_png(art_stage_output(), name, preview);
        UnloadImage(preview);
        UnloadImage(atlas);
    }
}
static void scenery_frames(const char *in, const char *out, const char *raw, const char *key, int columns,
                           int rows, int size) {
    Image source = art_read_png(in, raw);
    if (!source.data)
        return;
    for (int r = 0; r < rows; r++)
        for (int c = 0; c < columns; c++) {
            Image im = art_cut(source, c, r, columns, rows);
            char name[96];
            int maximum = im.width > im.height ? im.width : im.height;
            float scale = (size - 4.0f) / maximum;
            ImageResizeNN(&im, (int)(im.width * scale + .5f), (int)(im.height * scale + .5f));
            snprintf(name, sizeof(name), "%s_%d.png", key, r * columns + c);
            art_save_png(out, name, im);
            UnloadImage(im);
        }
    UnloadImage(source);
}
static void farm_atlas(const char *in, const char *out) {
    const char *const names[5] = {"cropland.png", "farm.png", "irrigated.png", "orchard.png", "garden.png"};
    Image atlas = GenImageColor(1024, 5376, BLANK);
    for (int type = 0; type < 5; type++) {
        Image source = art_read_png(in, names[type]);
        if (!source.data)
            continue;
        int rows = type == 3 ? 5 : 4;
        for (int row = 0; row < rows; row++)
            for (int rot = 0; rot < 4; rot++) {
                Image im = art_cut(source, rot, row, 4, rows);
                int maximum = im.width > im.height ? im.width : im.height;
                int target = type == 3 ? (row ? type * 4 + row - 1 : 20) : type * 4 + row;
                padded_paste(&atlas, im, rot, target, 256, 252.0f / maximum);
            }
        UnloadImage(source);
    }
    art_save_png(out, "farms_seasons.png", atlas);
    Image preview = GenImageColor(512, 768, (Color){28, 38, 37, 255});
    for (int r = 0; r < 6; r++)
        for (int c = 0; c < 4; c++) {
            Image im =
                ImageFromImage(atlas, (Rectangle){c * 256.0f, (r == 5 ? 20 : r * 4 + 1) * 256.0f, 256, 256});
            ImageResizeNN(&im, 128, 128);
            ImageDraw(&preview, im, (Rectangle){0, 0, 128, 128},
                      (Rectangle){c * 128.0f, r * 128.0f, 128, 128}, WHITE);
            UnloadImage(im);
        }
    art_save_png(art_stage_output(), "farms_preview.png", preview);
    UnloadImage(preview);
    UnloadImage(atlas);
}
static void small_atlas(const char *in, const char *out, const char *name, int columns, int rows, int size) {
    Image source = art_read_png(in, name), atlas = GenImageColor(columns * size, rows * size, BLANK);
    if (source.data)
        for (int row = 0; row < rows; row++)
            for (int col = 0; col < columns; col++) {
                Image im = art_cut(source, col, row, columns, rows);
                int maximum = im.width > im.height ? im.width : im.height;
                padded_paste(&atlas, im, col, row, size, (size - 4.0f) / maximum);
            }
    art_save_png(out, name, atlas);
    UnloadImage(source);
    UnloadImage(atlas);
}
static void changed_animals(const char *in, const char *out) {
    Image source = art_read_png(in, "fantasy.png"), walk = art_read_png(out, "animal_walk_identity.png"),
          attack = art_read_png(out, "animal_attack_identity.png");
    const int source_rows[2] = {1, 4}, target_rows[2] = {25, 28};
    if (source.data && walk.data && attack.data)
        for (int i = 0; i < 2; i++)
            for (int action = 0; action < 2; action++) {
                Image *atlas = action ? &attack : &walk;
                ImageDrawRectangle(atlas, 0, target_rows[i] * 96, 384, 96, BLANK);
                Image frames[4];
                int maximum = 1;
                for (int f = 0; f < 4; f++) {
                    frames[f] = art_cut(source, action * 4 + f, source_rows[i], 8, 6);
                    if (frames[f].width > maximum)
                        maximum = frames[f].width;
                    if (frames[f].height > maximum)
                        maximum = frames[f].height;
                }
                for (int f = 0; f < 4; f++)
                    padded_paste(atlas, frames[f], f, target_rows[i], 96, 92.0f / maximum);
            }
    art_save_png(out, "animal_walk_fantasy.png", walk);
    art_save_png(out, "animal_attack_fantasy.png", attack);
    Image icons = art_read_png(out, "animals_identity.png");
    if (icons.data && walk.data)
        for (int i = 0; i < 2; i++) {
            int k = target_rows[i];
            ImageDrawRectangle(&icons, (k % 4) * 96, (k / 4) * 96, 96, 96, BLANK);
            ImageDraw(&icons, walk, (Rectangle){96, k * 96.0f, 96, 96},
                      (Rectangle){(k % 4) * 96.0f, (k / 4) * 96.0f, 96, 96}, WHITE);
        }
    art_save_png(out, "animals_fantasy.png", icons);
    UnloadImage(icons);
    UnloadImage(source);
    UnloadImage(walk);
    UnloadImage(attack);
}
static void changed_cult(const char *in, const char *out) {
    Image source = art_read_png(in, "raid_cult.png"), walk = art_read_png(out, "raider_walk.png"),
          attack = art_read_png(out, "raider_attack.png");
    if (source.data && walk.data && attack.data)
        for (int i = 0; i < 5; i++) {
            Image frames[24];
            int maximum = 1, type = 23 + i;
            for (int d = 0; d < 3; d++)
                for (int f = 0; f < 8; f++) {
                    Image im = art_cut(source, f, i * 3 + d, 8, 15);
                    frames[d * 8 + f] = im;
                    if (im.width > maximum)
                        maximum = im.width;
                    if (im.height > maximum)
                        maximum = im.height;
                }
            for (int d = 0; d < 3; d++)
                for (int f = 0; f < 8; f++) {
                    Image *atlas = f < 4 ? &walk : &attack;
                    int col = (type % 4) * 4 + f % 4, row = (type / 4) * 3 + d;
                    ImageDrawRectangle(atlas, col * 96, row * 96, 96, 96, BLANK);
                    padded_paste(atlas, frames[d * 8 + f], col, row, 96, 92.0f / maximum);
                }
        }
    art_save_png(out, "enemies_walk.png", walk);
    art_save_png(out, "enemies_attack.png", attack);
    UnloadImage(source);
    UnloadImage(walk);
    UnloadImage(attack);
}
static void sprite_manifest(const char *out) {
    char path[GAME_PATH_BYTES];
    size_t capacity = 1024 * 1024, used = 0;
    char *csv = malloc(capacity);
    if (!csv) {
        art_errors++;
        return;
    }
    used += (size_t)snprintf(csv, capacity,
                             "appearance,outfit,direction,action,frame,x,y,width,height,pivot_x,pivot_y\n");
    for (int person = 0; person < 48; person++)
        for (int outfit = 0; outfit < 5; outfit++)
            for (int d = 0; d < 3; d++)
                for (int action = 0; action < (outfit ? 2 : 1); action++)
                    for (int f = 0; f < 4; f++) {
                        int col = d * (outfit ? 8 : 4) + action * 4 + f;
                        used += (size_t)snprintf(csv + used, capacity - used,
                                                 "%d,%d,%d,%d,%d,%d,%d,96,96,48,94\n", person, outfit, d,
                                                 action, f, col * 96, person * 96);
                    }
    if (!game_join_path(path, sizeof(path), out, "sprites_layout.csv") || !game_write_bytes(path, csv, used))
        art_errors++;
    free(csv);
}
static void check_cells(const char *out, const char *name, int columns, int rows, int size) {
    Image im = art_read_png(out, name);
    if (!im.data)
        return;
    if (im.width != columns * size || im.height != rows * size) {
        fprintf(stderr, u8"잘못된 크기: %s\n", name);
        art_errors++;
        UnloadImage(im);
        return;
    }
    Color *pixels = im.data;
    for (int row = 0; row < rows; row++)
        for (int col = 0; col < columns; col++) {
            int count = 0, border = 0, white = 0;
            int optional = 0, margin = 2;
            /* 기존 프레임의 발밑은 보존하고 새 프레임에2픽셀 여백을 요구합니다. */
            if (!strncmp(name, "raider_", 7) || !strncmp(name, "enemies_", 8)) {
                int type = row / 3 * 4 + col / 4;
                if (type < 23)
                    margin = 0;
            }
            if (!strncmp(name, "animal_", 7) && row != 25 && row != 28)
                margin = 0;
            if (!strcmp(name, "animal_attack_fantasy.png")) {
                CoreAnimalInfo info;
                core_expansion_animal_info(row, &info);
                optional = !info.attack || row == 2 || row == 4 || row == 5 || row == 15 || row == 22;
            }
            for (int y = 0; y < size; y++)
                for (int x = 0; x < size; x++)
                    if (pixels[(row * size + y) * im.width + col * size + x].a) {
                        Color c = pixels[(row * size + y) * im.width + col * size + x];
                        white += c.a >= 128 && c.r > 225 && c.g > 225 && c.b > 225;
                        count++;
                        if (x < margin || x >= size - margin || y < margin || y >= size - margin)
                            border++;
                    }
            int character = !strncmp(name, "uniform_", 8) || !strcmp(name, "villagers.png");
            if ((!count && !optional) || border || (character && white > size * size / 3)) {
                fprintf(stderr, u8"잘못된 셀: %s %d,%d (픽셀 %d, 여백 침범 %d)\n", name, col, row, count,
                        border);
                art_errors++;
            }
        }
    UnloadImage(im);
}
int art_stage_residents(int argc, char **argv) {

    const char *in = art_input("residents"), *out = art_stage_output();
    SetTraceLogLevel(LOG_WARNING);
    if (argc > 1 && !strcmp(argv[1], "--check")) {
        if (argc > 2)
            out = argv[2];
        check_cells(out, "villagers.png", 12, 48, 96);
        for (int i = 0; i < 4; i++) {
            char name[64];
            snprintf(name, sizeof(name), "uniform_%s.png", outfit_names[i]);
            check_cells(out, name, 24, 48, 96);
        }
        check_cells(out, "enemies_walk.png", 16, 21, 96);
        check_cells(out, "enemies_attack.png", 16, 21, 96);
        check_cells(out, "animal_walk_fantasy.png", 4, 30, 96);
        check_cells(out, "animal_attack_fantasy.png", 4, 30, 96);
        check_cells(out, "farms_seasons.png", 4, 21, 256);
        check_cells(out, "life_icons.png", 5, 1, 64);
        check_cells(out, "rocks.png", 3, 5, 128);
    } else {
        int base = argc > 1 && !strcmp(argv[1], "--base"), scene = argc > 1 && !strcmp(argv[1], "--scenery"),
            uniform = argc > 1 && !strcmp(argv[1], "--uniform");
        if (base || (!scene && !uniform))
            resident_atlas(in, out);
        if (uniform || (!base && !scene))
            uniform_atlases(in, out);
        if (scene || (!base && !uniform)) {
            farm_atlas(in, out);
            scenery_frames(in, out, "manors.png", "manor", 3, 2, 384);
            scenery_frames(in, out, "gatherer.png", "gatherer", 4, 1, 256);
            scenery_frames(in, out, "garrison.png", "garrison", 4, 1, 384);
            small_atlas(in, out, "rocks.png", 3, 5, 128);
            small_atlas(in, out, "life_icons.png", 5, 1, 64);
            changed_animals(in, out);
            changed_cult(in, out);
        }
        if (!base && !scene)
            sprite_manifest(out);
    }

    if (art_errors)
        fprintf(stderr, u8"원화 처리 오류: %d\n", art_errors);
    return art_errors ? 1 : 0;
}
