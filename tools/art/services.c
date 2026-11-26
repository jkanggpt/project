#include "image_ops.h"
#include "pipeline.h"

static void paste_cell(Image *atlas, Image frame, int c, int r, int size, int extent) {
    Rectangle border = GetImageAlphaBorder(frame, .1f);
    if (border.width < 2 || border.height < 2) {
        art_errors++;
        UnloadImage(frame);
        return;
    }
    ImageAlphaCrop(&frame, .1f);
    int max = frame.width > frame.height ? frame.width : frame.height;
    float scale = extent / (float)max;
    ImageResizeNN(&frame, (int)(frame.width * scale + .5f), (int)(frame.height * scale + .5f));
    ImageDraw(atlas, frame, (Rectangle){0, 0, (float)frame.width, (float)frame.height},
              (Rectangle){c * (float)size + (size - frame.width) / 2.0f,
                          (r + 1) * (float)size - frame.height - 2, (float)frame.width, (float)frame.height},
              WHITE);
    UnloadImage(frame);
}
static void building_sheet(const char *in, const char *out, const char *key) {
    char name[96];
    snprintf(name, sizeof(name), "%s.png", key);
    Image source = art_read_png(in, name);
    if (!source.data)
        return;
    for (int rot = 0; rot < 4; rot++) {
        int index = (int[]){0, 3, 2, 1}[rot];
        Image im = art_cut(source, index % 2, index / 2, 2, 2);
        int max = im.width > im.height ? im.width : im.height;
        float scale = 384.0f / max;
        ImageResizeNN(&im, (int)(im.width * scale + .5f), (int)(im.height * scale + .5f));
        snprintf(name, sizeof(name), "%s_%d.png", key, rot);
        art_save_png(out, name, im);
        UnloadImage(im);
    }
    UnloadImage(source);
}
static void small(const char *in, const char *out, const char *name, int cols, int rows, int size,
                  int extent) {
    Image source = art_read_png(in, name), atlas = GenImageColor(cols * size, rows * size, BLANK);
    if (source.data)
        for (int r = 0; r < rows; r++)
            for (int c = 0; c < cols; c++)
                paste_cell(&atlas, art_cut(source, c, r, cols, rows), c, r, size, extent);
    art_save_png(out, name, atlas);
    if (!strcmp(name, "merchant.png")) {
        Image portrait = ImageFromImage(atlas, (Rectangle){0, 0, 96, 58});
        art_save_png(out, "merchant_portrait.png", portrait);
        UnloadImage(portrait);
    }
    UnloadImage(source);
    UnloadImage(atlas);
}
static void observatory_motion(const char *in, const char *out) {
    Image atlas = GenImageColor(1024, 4096, BLANK);
    char name[96];
    for (int rot = 0; rot < 4; rot++) {
        snprintf(name, sizeof(name), "observatory_motion_%d.png", rot);
        Image source = art_read_png(in, name);
        if (!source.data)
            continue;
        Image frames[16];
        int maximum = 1;
        for (int f = 0; f < 16; f++) {
            frames[f] = art_cut(source, f % 4, f / 4, 4, 4);
            if (frames[f].width > maximum)
                maximum = frames[f].width;
            if (frames[f].height > maximum)
                maximum = frames[f].height;
        }
        float scale = 250.0f / maximum;
        for (int f = 0; f < 16; f++) {
            Image im = frames[f];
            ImageResizeNN(&im, (int)(im.width * scale + .5f), (int)(im.height * scale + .5f));
            ImageDraw(&atlas, im, (Rectangle){0, 0, (float)im.width, (float)im.height},
                      (Rectangle){f % 4 * 256.0f + (256 - im.width) / 2.0f,
                                  (rot * 4 + f / 4 + 1) * 256.0f - im.height - 2, (float)im.width,
                                  (float)im.height},
                      WHITE);
            UnloadImage(im);
        }
        UnloadImage(source);
    }
    art_save_png(out, "observatory_motion.png", atlas);
    UnloadImage(atlas);
}
static void check(const char *out) {
    const char *keys[4] = {"clay_pit", "water_tower", "observatory", "art_museum"};
    char name[96];
    for (int k = 0; k < 4; k++)
        for (int r = 0; r < 4; r++) {
            snprintf(name, sizeof(name), "%s_%d.png", keys[k], r);
            Image im = art_read_png(out, name);
            if (im.data) {
                Rectangle b = GetImageAlphaBorder(im, .1f);
                if (b.width < 32 || b.height < 32 || im.width > 384 || im.height > 384)
                    art_errors++;
            }
            UnloadImage(im);
        }
    Image motion = art_read_png(out, "observatory_motion.png");
    if (motion.data) {
        if (motion.width != 1024 || motion.height != 4096)
            art_errors++;
        for (int r = 0; r < 16; r++)
            for (int c = 0; c < 4; c++) {
                Image im = ImageFromImage(motion, (Rectangle){c * 256.0f, r * 256.0f, 256, 256});
                if (GetImageAlphaBorder(im, .1f).width < 50)
                    art_errors++;
                UnloadImage(im);
            }
    }
    UnloadImage(motion);
    Image portrait = art_read_png(out, "merchant_portrait.png");
    if (portrait.data && (portrait.width != 96 || portrait.height != 58))
        art_errors++;
    UnloadImage(portrait);
    Image soil = art_read_png(out, "soil_markers.png");
    if (soil.data && (soil.width != 512 || soil.height != 256))
        art_errors++;
    UnloadImage(soil);
    const char *files[4] = {"merchant.png", "fishing_boats.png", "bridge_parts.png", "ore_soil.png"};
    const int cols[4] = {6, 4, 4, 4}, rows[4] = {4, 4, 4, 3}, sizes[4] = {96, 128, 128, 128};
    for (int k = 0; k < 4; k++) {
        Image im = art_read_png(out, files[k]);
        if (im.data) {
            if (im.width != cols[k] * sizes[k] || im.height != rows[k] * sizes[k])
                art_errors++;
            for (int r = 0; r < rows[k]; r++)
                for (int c = 0; c < cols[k]; c++) {
                    int count = 0, transparent = 0;
                    for (int y = r * sizes[k]; y < (r + 1) * sizes[k]; y++)
                        for (int x = c * sizes[k]; x < (c + 1) * sizes[k]; x++) {
                            Color p = ((Color *)im.data)[y * im.width + x];
                            count += p.a > 0;
                            transparent += p.a == 0;
                        }
                    if (count < 20 || !transparent)
                        art_errors++;
                }
        }
        UnloadImage(im);
    }
}
int art_stage_services(int argc, char **argv) {

    SetTraceLogLevel(LOG_WARNING);
    if (argc != 3) {

        return 2;
    }
    if (!strcmp(argv[1], "--check"))
        check(argv[2]);
    else {
        const char *keys[4] = {"clay_pit", "water_tower", "observatory", "art_museum"};
        for (int i = 0; i < 4; i++)
            building_sheet(argv[1], argv[2], keys[i]);
        observatory_motion(argv[1], argv[2]);
        small(argv[1], argv[2], "soil_markers.png", 4, 2, 128, 122);
        small(argv[1], argv[2], "merchant.png", 6, 4, 96, 90);
        small(argv[1], argv[2], "fishing_boats.png", 4, 4, 128, 122);
        small(argv[1], argv[2], "ore_soil.png", 4, 3, 128, 122);
        Image parts = art_read_png(argv[1], "bridge_parts.png"), atlas = GenImageColor(512, 512, BLANK);
        if (parts.data)
            for (int r = 0; r < 4; r++)
                for (int c = 0; c < 4; c++) {
                    Image frame =
                        ImageFromImage(parts, (Rectangle){c * parts.width / 4.0f, r * parts.height / 4.0f,
                                                          parts.width / 4.0f, parts.height / 4.0f});
                    art_clean(&frame);
                    if (r < 3)
                        ImageAlphaCrop(&frame, .1f);
                    ImageResizeNN(&frame, 128, 128);
                    ImageDraw(&atlas, frame, (Rectangle){0, 0, 128, 128},
                              (Rectangle){c * 128.0f, r * 128.0f, 128, 128}, WHITE);
                    UnloadImage(frame);
                }
        art_save_png(argv[2], "bridge_parts.png", atlas);
        UnloadImage(parts);
        UnloadImage(atlas);
        check(argv[2]);
    }

    printf("Art art_errors=%d\n", art_errors);
    return art_errors ? 1 : 0;
}
