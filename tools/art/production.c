#include "image_ops.h"
#include "pipeline.h"
#include "sprite_layout.h"
static Image production_read_png(const char *dir, const char *name) {
    Image im = art_read_png(dir, name);
    if (!im.data)
        fprintf(stderr, "missing %s/%s\n", dir, name);
    return im;
}
#include "platform.h"
#include "sprite_layout.h"
#include <castle_expansion.h>

static Image production_cell(Image im, int col, int row, int columns, int rows) {
    Image frame = ImageFromImage(
        im, (Rectangle){col * (float)im.width / (float)columns, row * (float)im.height / (float)rows,
                        (float)im.width / (float)columns, (float)im.height / (float)rows});
    art_clean(&frame);
    return frame;
}
static void production_clear(Image *im, int x, int y, int w, int h) {
    Color *pixels = im->data;
    for (int yy = y; yy < y + h; yy++)
        memset(pixels + (size_t)yy * im->width + x, 0, (size_t)w * sizeof(Color));
}
static void production_fit(Image *out, Image frame, int col, int row, int size, int extent, int center) {
    if (!frame.data) {
        art_errors++;
        return;
    }
    Rectangle bounds = GetImageAlphaBorder(frame, .1f);
    if (bounds.width < 1 || bounds.height < 1) {
        UnloadImage(frame);
        return;
    }
    ImageAlphaCrop(&frame, .1f);
    float scale =
        extent / (float)((float)frame.width > (float)frame.height ? (float)frame.width : (float)frame.height);
    ImageResizeNN(&frame, (int)((float)frame.width * scale + .5f), (int)((float)frame.height * scale + .5f));
    ImageDraw(out, frame, (Rectangle){0, 0, (float)frame.width, (float)frame.height},
              (Rectangle){col * (float)size + (size - (float)frame.width) * .5f,
                          row * (float)size +
                              (center ? (size - (float)frame.height) * .5f : size - (float)frame.height - 2),
                          (float)frame.width, (float)frame.height},
              WHITE);
    UnloadImage(frame);
}
static Image production_row(Image input, int source_row, int first, int count, int columns, int rows,
                            int size) {
    Image out = GenImageColor(size * count, size, BLANK), frames[8];
    int maximum = 1;
    for (int f = 0; f < count; f++) {
        frames[f] = art_cut(input, first + f, source_row, columns, rows);
        if (frames[f].width > maximum)
            maximum = frames[f].width;
        if (frames[f].height > maximum)
            maximum = frames[f].height;
    }
    float scale = (size - 4) / (float)maximum;
    for (int f = 0; f < count; f++) {
        Image frame = frames[f];
        ImageResizeNN(&frame, (int)((float)frame.width * scale + .5f),
                      (int)((float)frame.height * scale + .5f));
        ImageDraw(&out, frame, (Rectangle){0, 0, (float)frame.width, (float)frame.height},
                  (Rectangle){f * (float)size + (size - (float)frame.width) * .5f,
                              size - (float)frame.height - 2.0f, (float)frame.width, (float)frame.height},
                  WHITE);
        UnloadImage(frame);
    }
    return out;
}
static void production_paste_row(Image *out, Image row, int index) {
    ImageDraw(out, row, (Rectangle){0, 0, (float)row.width, (float)row.height},
              (Rectangle){0, index * (float)(float)row.height, (float)row.width, (float)row.height}, WHITE);
}
static void production_animation(const char *in, const char *out, const char *raw, const char *key, int rows,
                                 int first_row, int frames, int size, int centered) {
    Image source = production_read_png(in, raw);
    if (!source.data)
        return;
    for (int r = 0; r < rows; r++) {
        Image strip = GenImageColor(size * frames, size, BLANK);
        for (int f = 0; f < frames; f++) {
            Image cell = production_cell(source, f, first_row + r, frames, rows + first_row);
            if (centered) {
                float scale = fminf(size / (float)(float)cell.width, size / (float)(float)cell.height);
                ImageResizeNN(&cell, (int)((float)cell.width * scale + .5f),
                              (int)((float)cell.height * scale + .5f));
                ImageDraw(&strip, cell, (Rectangle){0, 0, (float)cell.width, (float)cell.height},
                          (Rectangle){f * (float)size + (size - (float)cell.width) * .5f,
                                      (size - (float)cell.height) * .5f, (float)cell.width,
                                      (float)cell.height},
                          WHITE);
                UnloadImage(cell);
            } else
                production_fit(&strip, cell, f, 0, size, size - 4, 1);
        }
        char name[120];
        snprintf(name, sizeof(name), "%s_%d.png", key, r);
        art_save_png(out, name, strip);
        UnloadImage(strip);
    }
    UnloadImage(source);
}
static void production_buildings(const char *in, const char *out) {
    const char *keys[3] = {"clinic", "lighthouse", "windmill"};
    const char *raws[3] = {"clinic_alpha.png", "lighthouse.png", "windmill_base.png"};
    char name[128];
    for (int k = 0; k < 3; k++) {
        Image source = production_read_png(in, raws[k]);
        if (!source.data)
            continue;
        for (int r = 0; r < 4; r++) {
            Image frame = art_cut(source, r % 2, r / 2, 2, 2);
            if (k == 2) {
                Image canvas = GenImageColor(384, 512, BLANK);
                production_fit(&canvas, frame, 0, 0, 384, 374, 0);
                /* 본체를 384×512 가상 캔버스의 발밑에 정렬합니다. */
                Image expanded = GenImageColor(384, 512, BLANK);
                ImageDraw(&expanded, canvas, (Rectangle){0, 0, 384, 384}, (Rectangle){0, 128, 384, 384},
                          WHITE);
                UnloadImage(canvas);
                frame = expanded;
            } else {
                float scale = 384.0f / ((float)frame.width > (float)frame.height ? (float)frame.width
                                                                                 : (float)frame.height);
                ImageResizeNN(&frame, (int)((float)frame.width * scale + .5f),
                              (int)((float)frame.height * scale + .5f));
            }
            snprintf(name, sizeof(name), "%s_%d.png", keys[k], r);
            art_save_png(out, name, frame);
            UnloadImage(frame);
        }
        UnloadImage(source);
    }
    production_animation(in, out, "windmill_blades.png", "windmill_blades", 4, 0, 8, 256, 1);
    production_animation(in, out, "irrigated_wheel.png", "irrigated_wheel", 4, 0, 8, 128, 1);
    /* 조명 원화의 앞 네 행/뒤 네 행은 같은 축을 유지하는 서로 다른 레이어입니다. */
    Image lights = production_read_png(in, "lighthouse_lighting.png");
    if (lights.data)
        for (int r = 0; r < 4; r++)
            for (int layer = 0; layer < 2; layer++) {
                Image strip = GenImageColor(1024, 128, BLANK);
                for (int f = 0; f < 8; f++) {
                    Image cell = production_cell(lights, f, r + layer * 4, 8, 8);
                    ImageResizeNN(&cell, 128, 128);
                    ImageDraw(&strip, cell, (Rectangle){0, 0, 128, 128}, (Rectangle){f * 128.0f, 0, 128, 128},
                              WHITE);
                    UnloadImage(cell);
                }
                snprintf(name, sizeof(name), "lighthouse_%s_%d.png", layer ? "glow" : "lamp", r);
                art_save_png(out, name, strip);
                UnloadImage(strip);
            }
    UnloadImage(lights);
    for (int r = 0; r < 4; r++) {
        snprintf(name, sizeof(name), "windmill_%d.png", r);
        Image base = production_read_png(out, name);
        snprintf(name, sizeof(name), "windmill_blades_%d.png", r);
        Image blades = production_read_png(out, name);
        if (base.data && blades.data) {
            GameSpriteBox b = game_building_attachment(CORE_BLD_WINDMILL, r);
            ImageDraw(&base, blades, (Rectangle){0, 0, 256, 256},
                      (Rectangle){b.x * base.width, b.y * base.height, b.w * base.width, b.h * base.height},
                      WHITE);
            snprintf(name, sizeof(name), "windmill_preview_%d.png", r);
            art_save_png(out, name, base);
        }
        UnloadImage(base);
        UnloadImage(blades);
    }
    Image farms = production_read_png(out, "farms_seasons.png"),
          source = production_read_png(in, "irrigated_base.png"), previews = GenImageColor(1024, 1024, BLANK);
    if (farms.data && source.data)
        for (int season = 0; season < 4; season++)
            for (int r = 0; r < 4; r++) {
                Image frame = art_cut(source, r, season, 4, 4), base = ImageCopy(frame),
                      cell = GenImageColor(256, 256, BLANK);
                production_fit(&cell, base, 0, 0, 256, 252, 0);
                production_clear(&farms, r * 256, (8 + season) * 256, 256, 256);
                ImageDraw(&farms, cell, (Rectangle){0, 0, 256, 256},
                          (Rectangle){r * 256.0f, (8 + season) * 256.0f, 256, 256}, WHITE);
                UnloadImage(cell);
                snprintf(name, sizeof(name), "irrigated_wheel_%d.png", r);
                Image wheel = production_read_png(out, name);
                GameSpriteBox b = game_building_attachment(CORE_BLD_IRRIGATED_FARM, r);
                ImageDraw(&frame, wheel, (Rectangle){0, 0, 128, 128},
                          (Rectangle){b.x * (float)frame.width, b.y * (float)frame.height,
                                      b.w * (float)frame.width, b.h * (float)frame.height},
                          WHITE);
                if (!season) {
                    snprintf(name, sizeof(name), "irrigated_farm_%d.png", r);
                    art_save_png(out, name, frame);
                }
                production_fit(&previews, frame, r, season, 256, 252, 0);
                UnloadImage(wheel);
            }
    if (farms.data)
        art_save_png(out, "farms.png", farms);
    art_save_png(out, "irrigated_previews.png", previews);
    UnloadImage(previews);
    UnloadImage(farms);
    UnloadImage(source);
}
static Image production_four(Image source, int row, int first, int step, int columns, int rows) {
    Image out = GenImageColor(384, 96, BLANK);
    Image frames[4];
    int extent = 1;
    for (int f = 0; f < 4; f++) {
        frames[f] = art_cut(source, first + f * step, row, columns, rows);
        if (frames[f].width > extent)
            extent = frames[f].width;
        if (frames[f].height > extent)
            extent = frames[f].height;
    }
    float scale = 92.0f / extent;
    for (int f = 0; f < 4; f++) {
        Image *im = &frames[f];
        ImageResizeNN(im, (int)(im->width * scale + .5f), (int)(im->height * scale + .5f));
        ImageDraw(&out, *im, (Rectangle){0, 0, (float)im->width, (float)im->height},
                  (Rectangle){f * 96.0f + (96 - im->width) * .5f, 94.0f - im->height, (float)im->width,
                              (float)im->height},
                  WHITE);
        UnloadImage(*im);
    }
    return out;
}
static void production_animals(const char *in, const char *out) {
    Image old_walk = production_read_png(out, "animal_walk_fantasy.png"),
          old_attack = production_read_png(out, "animal_attack_fantasy.png"),
          old_swim = production_read_png(out, "animal_swim.png"),
          old_babies = production_read_png(out, "animal_babies.png");
    Image walks = GenImageColor(384, 3360, BLANK), attacks = GenImageColor(384, 3360, BLANK),
          swims = GenImageColor(384, 3360, BLANK), babies = GenImageColor(384, 3360, BLANK),
          idle = GenImageColor(576, 576, BLANK);
    Image added_attack = production_read_png(in, "animal_attacks_additional.png");
    const char *new_names[5] = {"zebra.png", "capybara.png", "goat.png", "alpaca.png", "turkey.png"},
               *pets[7] = {"cow", "sheep", "pig", "chicken", "goat", "alpaca", "turkey"};
    for (int kind = 0; kind < CORE_EXP_ANIMALS; kind++) {
        CoreAnimalInfo info;
        core_expansion_animal_info(kind, &info);
        Image walk = {0}, attack = GenImageColor(384, 96, BLANK), swim = GenImageColor(384, 96, BLANK),
              baby = GenImageColor(384, 96, BLANK);
        if (kind < 30) {
            walk = ImageFromImage(old_walk, (Rectangle){0, kind * 96.0f, 384, 96});
            ImageDraw(&attack, old_attack, (Rectangle){0, kind * 96.0f, 384, 96}, (Rectangle){0, 0, 384, 96},
                      WHITE);
            if (info.swim_milli)
                ImageDraw(&swim, old_swim, (Rectangle){0, kind * 96.0f, 384, 96}, (Rectangle){0, 0, 384, 96},
                          WHITE);
            if (info.livestock)
                ImageDraw(&baby, old_babies, (Rectangle){0, (kind - 7) * 96.0f, 384, 96},
                          (Rectangle){0, 0, 384, 96}, WHITE);
        } else {
            Image source = production_read_png(in, new_names[kind - 30]);
            walk = production_four(source, 0, 0, 2, 8, 3);
            if (info.livestock) {
                UnloadImage(baby);
                baby = production_four(source, 2, 0, 1, 8, 3);
            }
            if (info.swim_milli) {
                UnloadImage(swim);
                swim = production_four(source, 2, 0, 2, 8, 3);
            }
            UnloadImage(source);
        }
        if (kind == 2 || kind == 4 || kind == 15 || kind == 22) {
            int row = kind == 2 ? 0 : kind == 4 ? 1 : kind == 15 ? 2 : 3;
            UnloadImage(attack);
            attack = production_row(added_attack, row, 0, 4, 4, 4, 96);
        }
        /* 사용하지 않는 공격 칸은 비워 두고 실제 공격종만 등록합니다. */
        if (info.attack && GetImageAlphaBorder(attack, .1f).width < 1) {
            UnloadImage(attack);
            attack = ImageCopy(walk);
        }
        char name[128], file[160];
        if (info.livestock)
            snprintf(name, sizeof(name), "livestock_%s", pets[kind <= 10 ? kind - 7 : kind - 28]);
        else if (kind == 30 || kind == 31)
            snprintf(name, sizeof(name), "%s", kind == 30 ? "animal_zebra" : "animal_capybara");
        else
            snprintf(name, sizeof(name), "animal_kind%d", kind);
        const char *states[4] = {"walk", "attack", "swim", "baby"};
        Image images[4] = {walk, attack, swim, baby};
        for (int s = 0; s < 4; s++)
            if (s == 0 || (s == 1 && info.attack) || (s == 2 && info.swim_milli) ||
                (s == 3 && info.livestock)) {
                snprintf(file, sizeof(file), "%s_%s.png", name, states[s]);
                art_save_png(out, file, images[s]);
            }
        production_paste_row(&walks, walk, kind);
        production_paste_row(&attacks, attack, kind);
        production_paste_row(&swims, swim, kind);
        production_paste_row(&babies, baby, kind);
        ImageDraw(&idle, walk, (Rectangle){96, 0, 96, 96},
                  (Rectangle){kind % 6 * 96.0f, kind / 6 * 96.0f, 96, 96}, WHITE);
        for (int s = 0; s < 4; s++)
            UnloadImage(images[s]);
    }
    Image alpaca = production_read_png(art_input("settlement"), "animal_walk_1.png");
    if (alpaca.data) {
        Image original = art_cut(alpaca, 0, 1, 4, 4), cell = GenImageColor(96, 96, BLANK);
        production_fit(&cell, original, 0, 0, 96, 92, 0);
        production_clear(&idle, 288, 480, 96, 96);
        ImageDraw(&idle, cell, (Rectangle){0, 0, 96, 96}, (Rectangle){288, 480, 96, 96}, WHITE);
        UnloadImage(cell);
    }
    art_save_png(out, "animals.png", idle);
    art_save_png(out, "animal_walk.png", walks);
    art_save_png(out, "animal_attack.png", attacks);
    art_save_png(out, "animal_swim_all.png", swims);
    art_save_png(out, "animal_young.png", babies);
    UnloadImage(alpaca);
    UnloadImage(idle);
    UnloadImage(walks);
    UnloadImage(attacks);
    UnloadImage(swims);
    UnloadImage(babies);
    UnloadImage(added_attack);
    UnloadImage(old_walk);
    UnloadImage(old_attack);
    UnloadImage(old_swim);
    UnloadImage(old_babies);
}
static int production_road_pixel(int mask, int x, int y) {
    int west = x < 8, east = x >= 56, north = y < 8, south = y >= 56;
    if (!west && !east && !north && !south)
        return 1;
    if (north && !west && !east)
        return mask & 1;
    if (east && !north && !south)
        return mask & 2;
    if (south && !west && !east)
        return mask & 4;
    if (west && !north && !south)
        return mask & 8;
    if (north && east)
        return (mask & 1) && (mask & 2) && (mask & 16);
    if (south && east)
        return (mask & 2) && (mask & 4) && (mask & 32);
    if (south && west)
        return (mask & 4) && (mask & 8) && (mask & 64);
    return (mask & 8) && (mask & 1) && (mask & 128);
}
static void production_roads(const char *in, const char *out) {
    Image base_roads = production_read_png(out, "road_connections.png"),
          connected = GenImageColor(1024, 3072, BLANK);
    if (base_roads.data)
        ImageDraw(&connected, base_roads, (Rectangle){0, 0, 1024, 1024}, (Rectangle){0, 0, 1024, 1024}, WHITE);
    const char *keys[2] = {"gravel", "paved"};
    for (int t = 0; t < 2; t++) {
        char name[128];
        snprintf(name, sizeof(name), "%s_roads.png", keys[t]);
        Image source = production_read_png(in, name);
        Image material = ImageFromImage(source, (Rectangle){source.width * .825f, source.height * .825f,
                                                            source.width * .1f, source.height * .1f});
        ImageResizeNN(&material, 64, 64);
        Image tiles = GenImageColor(256, 256, BLANK), edges = GenImageColor(256, 256, BLANK);
        Color *m = material.data, *pixels = connected.data, *main = tiles.data, *edge = edges.data;
        for (int mask = 0; mask < 256; mask++)
            for (int y = 0; y < 64; y++)
                for (int x = 0; x < 64; x++)
                    if (production_road_pixel(mask, x, y)) {
                        Color color = m[y * 64 + x];
                        color.a = 255;
                        pixels[((t + 1) * 1024 + (mask / 16) * 64 + y) * 1024 + mask % 16 * 64 + x] = color;
                        if (mask < 16) {
                            int p = (mask / 4 * 64 + y) * 256 + mask % 4 * 64 + x;
                            main[p] = color;
                            if (!production_road_pixel(mask, x - 1, y) ||
                                !production_road_pixel(mask, x + 1, y) ||
                                !production_road_pixel(mask, x, y - 1) ||
                                !production_road_pixel(mask, x, y + 1)) {
                                color.a = 120;
                                edge[p] = color;
                            }
                        }
                    }
        snprintf(name, sizeof(name), "%s_road_tiles.png", keys[t]);
        art_save_png(out, name, tiles);
        snprintf(name, sizeof(name), "%s_road_edges.png", keys[t]);
        art_save_png(out, name, edges);
        UnloadImage(source);
        UnloadImage(material);
        UnloadImage(tiles);
        UnloadImage(edges);
    }
    art_save_png(out, "road_connections_all.png", connected);
    UnloadImage(connected);
    UnloadImage(base_roads);
}
static void production_boats(const char *in, const char *out) {
    const char *raw[2] = {"fishing_boats.png", "fishing_large.png"};
    for (int k = 0; k < 2; k++) {
        Image source = production_read_png(k ? in : art_input("services"), raw[k]);
        for (int r = 0; r < 4; r++) {
            int source_row = k ? r : (int[]){0, 3, 2, 1}[r];
            Image strip = GenImageColor(512, 128, BLANK);
            for (int f = 0; f < 4; f++)
                production_fit(&strip, art_cut(source, f, source_row, 4, 4), f, 0, 128, 120, 1);
            char name[128];
            snprintf(name, sizeof(name), "fishing_%s_boat_%d.png", k ? "large" : "small", r);
            art_save_png(out, name, strip);
            UnloadImage(strip);
        }
        UnloadImage(source);
        char raw_net[64];
        snprintf(raw_net, sizeof(raw_net), "%s_nets.png", k ? "large" : "small");
        production_animation(in, out, raw_net, k ? "large_net" : "small_net", 4, 0, 8, 128, 1);
    }
    /* 항적의 첫 네 행과 마지막 아이콘 행은 별도로 분리합니다. */
    Image source = production_read_png(in, "boat_wake.png");
    if (source.data) {
        for (int r = 0; r < 4; r++) {
            Image strip = GenImageColor(384, 96, BLANK);
            for (int f = 0; f < 4; f++)
                production_fit(&strip, production_cell(source, f, r, 4, 5), f, 0, 96, 92, 1);
            char name[96];
            snprintf(name, sizeof(name), "boat_wake_%d.png", r);
            art_save_png(out, name, strip);
            UnloadImage(strip);
        }
        Image icons = GenImageColor(256, 64, BLANK);
        for (int f = 0; f < 4; f++)
            production_fit(&icons, production_cell(source, f, 4, 4, 5), f, 0, 64, 60, 1);
        art_save_png(out, "icon_fishing_states.png", icons);
        Image light = ImageFromImage(icons, (Rectangle){192, 0, 64, 64});
        art_save_png(out, "icon_lighthouse.png", light);
        UnloadImage(light);
        UnloadImage(icons);
    }
    UnloadImage(source);
    for (int r = 0; r < 4; r++) {
        char name[128], text[512];
        snprintf(name, sizeof(name), "fishing_passenger_offsets_%d.json", r);
        GameSpriteBox s0 = game_boat_seat(0, r, 0), s1 = game_boat_seat(0, r, 1),
                      l0 = game_boat_seat(1, r, 0), l1 = game_boat_seat(1, r, 1);
        int n = snprintf(
            text, sizeof(text),
            "{\"direction\":%d,\"small\":[[%.3f,%.3f],[%.3f,%.3f]],\"large\":[[%.3f,%.3f],[%.3f,%.3f]]}\n", r,
            s0.x, s0.y, s1.x, s1.y, l0.x, l0.y, l1.x, l1.y);
        char path[GAME_PATH_BYTES];
        if (!game_join_path(path, sizeof(path), out, name) || !game_write_bytes(path, text, (size_t)n))
            art_errors++;
    }
}
static void production_extras(const char *in, const char *out) {
    Image paved = production_read_png(out, "paved_road_tiles.png");
    for (int r = 0; r < 4; r++) {
        Image tile = ImageFromImage(paved, (Rectangle){192, 192, 64, 64});
        char name[80];
        snprintf(name, sizeof(name), "paved_road_%d.png", r);
        art_save_png(out, name, tile);
        UnloadImage(tile);
    }
    UnloadImage(paved);
    Image raw = production_read_png(in, "harvest.png"), atlas = production_row(raw, 0, 0, 6, 6, 1, 128);
    art_save_png(out, "field_harvest_phases.png", atlas);
    UnloadImage(atlas);
    UnloadImage(raw);
    const char *base[2] = {"manor_0.png", "clinic_0.png"},
               *keys[2] = {"icon_territory.png", "icon_clinic.png"};
    for (int k = 0; k < 2; k++) {
        Image source = production_read_png(out, base[k]), icon = GenImageColor(64, 64, BLANK);
        production_fit(&icon, source, 0, 0, 64, 60, 1);
        art_save_png(out, keys[k], icon);
        UnloadImage(icon);
    }
}

static void production_check(const char *out) {
    const char *names[5] = {"animals.png", "animal_walk.png", "animal_attack.png", "animal_swim_all.png",
                            "animal_young.png"};
    for (int k = 0; k < 5; k++) {
        Image im = production_read_png(out, names[k]);
        if (!im.data)
            continue;
        if (im.width != (k ? 384 : 576) || im.height != (k ? 3360 : 576))
            art_errors++;
        if (k == 1)
            for (int row = 0; row < 35; row++)
                for (int f = 0; f < 4; f++) {
                    Image part = ImageFromImage(im, (Rectangle){f * 96.0f, row * 96.0f, 96, 96});
                    if (GetImageAlphaBorder(part, .1f).width < 1)
                        art_errors++;
                    UnloadImage(part);
                }
        UnloadImage(im);
    }
    const char *keys[10] = {"clinic",          "lighthouse",      "windmill_blades",    "irrigated_wheel",
                            "lighthouse_lamp", "lighthouse_glow", "fishing_small_boat", "fishing_large_boat",
                            "small_net",       "large_net"};
    for (int k = 0; k < 10; k++)
        for (int r = 0; r < 4; r++) {
            char name[128];
            snprintf(name, sizeof(name), "%s_%d.png", keys[k], r);
            Image im = production_read_png(out, name);
            if (!im.data)
                continue;
            if (GetImageAlphaBorder(im, .1f).width < 1)
                art_errors++;
            UnloadImage(im);
        }
}
int art_stage_production(int argc, char **argv) {

    SetTraceLogLevel(LOG_WARNING);
    const char *out = argc > 2 ? argv[2] : art_stage_output();
    if (argc > 1 && !strcmp(argv[1], "--check")) {
        production_check(out);
    } else {
        const char *in = argc > 1 ? argv[1] : art_input("production");
        production_buildings(in, out);
        fprintf(stderr, "stage art_errors=%d\n", art_errors);
        production_animals(in, out);
        fprintf(stderr, "stage art_errors=%d\n", art_errors);
        production_roads(in, out);
        fprintf(stderr, "stage art_errors=%d\n", art_errors);
        production_boats(in, out);
        fprintf(stderr, "stage art_errors=%d\n", art_errors);
        production_extras(in, out);
        fprintf(stderr, "stage art_errors=%d\n", art_errors);
    }

    printf("Art art_errors=%d\n", art_errors);
    return art_errors ? 1 : 0;
}
