/* assets.c — 메모리 로더를 써서 한글 경로에서도 리소스를 읽습니다. */
#include "assets.h"
#include "assets_index.h"
#include "rlgl.h"
#include <castle_expansion.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 외부 녹음 없이 정해진 잡음으로 비와 낮은 천둥을 합성합니다. */
static Sound weather_sound(int thunder) {
    unsigned count = thunder ? 88200 : 44100;
    float *samples = malloc(count * sizeof(float));
    Sound result = {0};
    if (!samples)
        return result;
    unsigned random = 1234567;
    float low = 0;
    for (unsigned i = 0; i < count; i++) {
        random = random * 1664525u + 1013904223u;
        float noise = (float)(random >> 8) / 8388608.0f - 1, t = i / 44100.0f;
        low = low * .985f + noise * .015f;
        float envelope = thunder ? fminf(1, t * 35) * expf(-t * 2) : .2f;
        samples[i] = thunder ? (low * 6 + sinf(t * 83) * .06f) * envelope : noise * envelope;
    }
    result = LoadSoundFromWave((Wave){count, 44100, 32, 1, samples});
    free(samples);
    return result;
}

static void keep_alpha(GameAssets *a, Image image, Texture2D texture) {
    if (!texture.id)
        return;
    if (a->alpha_count >= 512) {
        a->missing++;
        return;
    }
    size_t count = (size_t)image.width * image.height;
    unsigned char *bits = calloc((count + 7) / 8, 1);
    Color *colors = LoadImageColors(image);
    if (!bits || !colors) {
        free(bits);
        if (colors)
            UnloadImageColors(colors);
        a->missing++;
        return;
    }
    for (size_t i = 0; i < count; i++)
        if (colors[i].a >= 32)
            bits[i / 8] |= (unsigned char)(1u << (i % 8));
    UnloadImageColors(colors);
    a->alpha[a->alpha_count++] = (GameAlphaMask){texture.id, image.width, image.height, bits};
}
int game_texture_hit(const GameAssets *a, Texture2D texture, Rectangle source, float u, float v) {
    if (!texture.id || u < 0 || u >= 1 || v < 0 || v >= 1)
        return 0;
    int x = (int)(source.x + (source.width < 0 ? 1 - u : u) * fabsf(source.width)),
        y = (int)(source.y + v * fabsf(source.height));
    for (int i = 0; i < a->alpha_count; i++)
        if (a->alpha[i].texture_id == texture.id) {
            const GameAlphaMask *m = &a->alpha[i];
            if (x < 0 || y < 0 || x >= m->width || y >= m->height)
                return 0;
            size_t p = (size_t)y * m->width + x;
            return (m->bits[p / 8] >> (p % 8)) & 1;
        }
    return 0;
}
static const CastleAssetRecord *asset_record(const char *name) {
    for (int i = 0; i < CASTLE_ASSET_COUNT; i++)
        if (!strcmp(CASTLE_ASSETS[i].id, name))
            return &CASTLE_ASSETS[i];
    return NULL;
}
static Texture2D atlas_texture(GameAssets *a, const char *dir, const char *id, Rectangle *regions,
                               int columns, int rows) {
    const CastleAssetRecord *record = asset_record(id);
    const char *name = record ? record->path : id;
    char path[GAME_PATH_BYTES];
    int n = 0;
    unsigned char *bytes;
    Image image = {0};
    Texture2D result = {0};
    if (!game_join_path(path, sizeof(path), dir, name)) {
        a->missing++;
        return result;
    }
    bytes = game_read_bytes(path, &n);
    if (bytes) {
        image = LoadImageFromMemory(".png", bytes, n);
        free(bytes);
    }
    if (!record || !image.data || image.width != record->width || image.height != record->height) {
        fprintf(stderr, "Invalid/missing texture: %s\n", name);
        a->missing++;
        if (image.data)
            UnloadImage(image);
        return result;
    }
    if (image.data) {
        if (regions)
            for (int r = 0; r < rows; r++)
                for (int c = 0; c < columns; c++) {
                    Rectangle cell = {c * (image.width / (float)columns), r * (image.height / (float)rows),
                                      image.width / (float)columns, image.height / (float)rows};
                    Image frame = ImageFromImage(image, cell);
                    Rectangle border = GetImageAlphaBorder(frame, .1f);
                    if (border.width < 1 || border.height < 1) {
                        a->missing++;
                        border = (Rectangle){0, 0, cell.width, cell.height};
                    }
                    regions[r * columns + c] =
                        (Rectangle){cell.x + border.x, cell.y + border.y, border.width, border.height};
                    UnloadImage(frame);
                }
        if (getenv("CASTLE_TRACE_ASSETS"))
            fprintf(stderr, "ASSET\t%s\t%d\t%d\t%d\t%d\n", name, image.width, image.height, columns, rows);
        result = LoadTextureFromImage(image);
        keep_alpha(a, image, result);
        UnloadImage(image);
        SetTextureFilter(result, TEXTURE_FILTER_POINT);
    } else
        a->missing++;
    return result;
}
static Texture2D texture(GameAssets *a, const char *dir, const char *name) {
    return atlas_texture(a, dir, name, NULL, 0, 0);
}
int game_assets_load(GameAssets *a, const char *dir, int audio) {
    static const char *const sounds[SND_COUNT] = {"audio.click_002",        "audio.drop_002",
                                                  "audio.error_004",        "audio.impactBell_heavy_000",
                                                  "audio.drawKnife1",       "audio.impactSoft_medium_000",
                                                  "audio.confirmation_002", "audio.maximize_006"};
    char path[GAME_PATH_BYTES], name[160];
    int i, n = 0, count = 0, *codepoints;
    unsigned char *bytes;
    memset(a, 0, sizeof(*a));
    const CastleAssetRecord *font_record = asset_record("font.korean");
    if (!font_record || !game_join_path(path, sizeof(path), dir, font_record->path))
        return 0;
    bytes = game_read_bytes(path, &n);
    if (!bytes)
        return 0;
    codepoints = (int *)malloc(12000 * sizeof(int));
    if (!codepoints) {
        free(bytes);
        return 0;
    }
    for (i = 32; i < 127; i++)
        codepoints[count++] = i;
    codepoints[count++] = 0xB7;
    codepoints[count++] = 0xD7;
    codepoints[count++] = 0xB0;
    codepoints[count++] = 0x2190;
    codepoints[count++] = 0x2192;
    for (i = 0x3131; i <= 0x3163; i++)
        codepoints[count++] = i;
    for (i = 0xAC00; i <= 0xD7A3; i++)
        codepoints[count++] = i;
    /* 한글 완성형 전체를 넣어 주민 이름과 사용자가 붙인 이름도 표시합니다. */
    a->font = LoadFontFromMemory(".ttf", bytes, n, 24, codepoints, count);
    free(codepoints);
    free(bytes);
    if (!a->font.texture.id)
        return 0;
    SetTextureFilter(a->font.texture, TEXTURE_FILTER_POINT);
    a->terrain = texture(a, dir, "terrain.terrain");
    a->roads = texture(a, dir, "roads.base");
    a->snow = texture(a, dir, "terrain.snow");
    a->props = texture(a, dir, "terrain.props");
    a->construction = texture(a, dir, "buildings.construction");
    a->villagers = texture(a, dir, "units.villagers");
    a->enemies = texture(a, dir, "units.enemies_walk");
    a->arrow = texture(a, dir, "effects.arrow");
    if (a->villagers.id && (a->villagers.width != 1152 || a->villagers.height != 4608)) {
        fprintf(stderr, "Incompatible villager atlas: expected 1152x4608\n");
        a->missing++;
    }
    a->status_icons = texture(a, dir, "ui.status_icons");
    a->life_icons = texture(a, dir, "ui.life_icons");
    a->rocks = texture(a, dir, "terrain.rocks");
    a->merchant = texture(a, dir, "units.merchant");
    a->observatory_motion =
        atlas_texture(a, dir, "buildings.observatory_motion", a->observatory_sources, 4, 16);
    a->soil_markers = texture(a, dir, "terrain.soil_markers");
    a->merchant_portrait = texture(a, dir, "ui.merchant_portrait");
    a->fishing_boats = texture(a, dir, "boats.fishing_idle");
    a->bridge_parts = texture(a, dir, "roads.bridges");
    a->ore_soil = texture(a, dir, "terrain.ore_soil");
    static const char *const uniform_names[4] = {"camp", "lodge", "barracks", "garrison"};
    for (i = 0; i < 4; i++) {
        snprintf(name, sizeof(name), "units.uniform_%s", uniform_names[i]);
        a->uniforms[i] = texture(a, dir, name);
    }
    a->animals = texture(a, dir, "animals.idle");
    a->animal_walk = texture(a, dir, "animals.walk");
    a->wall_links = texture(a, dir, "buildings.wall_links");
    a->biomes = texture(a, dir, "terrain.biomes");
    a->gates = texture(a, dir, "buildings.gates");
    a->farms = atlas_texture(a, dir, "buildings.farms", a->farm_sources, 4, 21);
    a->portraits = texture(a, dir, "ui.portraits");
    a->combat_fx = texture(a, dir, "effects.combat");
    a->factions = texture(a, dir, "ui.factions");
    a->greenhouse_turns = atlas_texture(a, dir, "buildings.greenhouse_turns", a->greenhouse_sources, 4, 4);
    a->raider_attack = texture(a, dir, "units.enemies_attack");
    a->animal_babies = texture(a, dir, "animals.young");
    a->irrigated_previews = texture(a, dir, "buildings.irrigated_previews");
    a->harvest_phases = texture(a, dir, "buildings.field_harvest_phases");
    a->territory_icon = texture(a, dir, "ui.icon_territory");
    a->clinic_icon = texture(a, dir, "ui.icon_clinic");
    a->fishing_icons = texture(a, dir, "ui.icon_fishing_states");
    for (int r = 0; r < 4; r++) {
        Texture2D *targets[6] = {&a->windmill_blades[r], &a->windmill_previews[r], &a->irrigated_wheel[r],
                                 &a->lighthouse_lamp[r], &a->lighthouse_glow[r],   &a->boat_wake[r]};
        const char *keys[6] = {"windmill_blades", "windmill_preview", "irrigated_wheel",
                               "lighthouse_lamp", "lighthouse_glow",  "boat_wake"};
        for (int k = 0; k < 6; k++) {
            snprintf(name, sizeof(name), "%s.%s.%d", k == 5 ? "boats" : "buildings", keys[k], r);
            *targets[k] = texture(a, dir, name);
        }
        for (int kind = 0; kind < 2; kind++) {
            snprintf(name, sizeof(name), "boats.fishing_%s_boat.%d", kind ? "large" : "small", r);
            a->boats[kind][r] = texture(a, dir, name);
            snprintf(name, sizeof(name), "boats.%s_net.%d", kind ? "large" : "small", r);
            a->nets[kind][r] = texture(a, dir, name);
        }
    }
    a->trees = texture(a, dir, "terrain.trees");
    a->biome_edges = texture(a, dir, "terrain.biome_edges");
    a->shores = texture(a, dir, "terrain.shores");
    a->road_tiles = texture(a, dir, "roads.connections");
    a->animal_swim = texture(a, dir, "animals.swim");
    a->animal_attack = texture(a, dir, "animals.attack");
    for (i = 0; i < 6; i++) {
        snprintf(name, sizeof(name), "buildings.manor.%d", i);
        a->manors[i] = texture(a, dir, name);
    }
    for (i = 0; i < CORE_BLD_COUNT; i++) {
        CoreExpansionBuildingInfo info;
        int rot;
        (void)core_expansion_building_info(i, &info);
        for (rot = 0; rot < 4; rot++) {
            (void)snprintf(name, sizeof(name), "buildings.%s.%d", info.key, rot);
            a->turns[i][rot] = texture(a, dir, name);
        }
        a->buildings[i] = a->turns[i][0];
    }
    for (i = 0; i < CORE_RES_COUNT; i++) {
        (void)snprintf(name, sizeof(name), "ui.resources.%s", core_expansion_resource_key(i));
        a->resources[i] = texture(a, dir, name);
    }
    if (audio) {
        InitAudioDevice();
        a->audio_ready = IsAudioDeviceReady();
        if (a->audio_ready)
            for (i = 0; i < SND_COUNT; i++) {
                if (i == SND_RAIN || i == SND_THUNDER) {
                    a->sounds[i] = weather_sound(i == SND_THUNDER);
                    continue;
                }
                Wave wave = {0};
                const CastleAssetRecord *sound_record = asset_record(sounds[i]);
                if (!sound_record || !game_join_path(path, sizeof(path), dir, sound_record->path))
                    continue;
                bytes = game_read_bytes(path, &n);
                if (bytes) {
                    wave = LoadWaveFromMemory(".ogg", bytes, n);
                    free(bytes);
                }
                if (wave.data) {
                    a->sounds[i] = LoadSoundFromWave(wave);
                    UnloadWave(wave);
                }
            }
    }
    if (a->missing)
        fprintf(stderr, "Missing art assets: %d\n", a->missing);
    return a->missing == 0;
}
void game_assets_free(GameAssets *a) {
    UnloadTexture(a->irrigated_previews);
    for (int m = 0; m < a->alpha_count; m++)
        free(a->alpha[m].bits);
    a->alpha_count = 0;
    UnloadTexture(a->harvest_phases);
    UnloadTexture(a->territory_icon);
    UnloadTexture(a->clinic_icon);
    UnloadTexture(a->fishing_icons);
    for (int r = 0; r < 4; r++) {
        UnloadTexture(a->windmill_blades[r]);
        UnloadTexture(a->windmill_previews[r]);
        UnloadTexture(a->irrigated_wheel[r]);
        UnloadTexture(a->lighthouse_lamp[r]);
        UnloadTexture(a->lighthouse_glow[r]);
        UnloadTexture(a->boat_wake[r]);
        for (int k = 0; k < 2; k++) {
            UnloadTexture(a->boats[k][r]);
            UnloadTexture(a->nets[k][r]);
        }
    }
    UnloadTexture(a->merchant);
    UnloadTexture(a->observatory_motion);
    UnloadTexture(a->soil_markers);
    UnloadTexture(a->merchant_portrait);
    UnloadTexture(a->fishing_boats);
    UnloadTexture(a->bridge_parts);
    UnloadTexture(a->ore_soil);
    if (a->portraits.id)
        UnloadTexture(a->portraits);
    if (a->combat_fx.id)
        UnloadTexture(a->combat_fx);
    if (a->factions.id)
        UnloadTexture(a->factions);
    if (a->life_icons.id)
        UnloadTexture(a->life_icons);
    if (a->rocks.id)
        UnloadTexture(a->rocks);
    for (int k = 0; k < 4; k++)
        if (a->uniforms[k].id)
            UnloadTexture(a->uniforms[k]);
    if (a->gates.id)
        UnloadTexture(a->gates);
    if (a->farms.id)
        UnloadTexture(a->farms);
    if (a->greenhouse_turns.id)
        UnloadTexture(a->greenhouse_turns);
    if (a->raider_attack.id)
        UnloadTexture(a->raider_attack);
    if (a->animal_babies.id)
        UnloadTexture(a->animal_babies);
    int i;
    if (a->trees.id)
        UnloadTexture(a->trees);
    if (a->biome_edges.id)
        UnloadTexture(a->biome_edges);
    if (a->shores.id)
        UnloadTexture(a->shores);
    if (a->road_tiles.id)
        UnloadTexture(a->road_tiles);
    if (a->animal_swim.id)
        UnloadTexture(a->animal_swim);
    if (a->animal_attack.id)
        UnloadTexture(a->animal_attack);
    for (i = 0; i < 6; i++)
        if (a->manors[i].id)
            UnloadTexture(a->manors[i]);
    if (a->terrain.id)
        UnloadTexture(a->terrain);
    if (a->roads.id)
        UnloadTexture(a->roads);
    if (a->snow.id)
        UnloadTexture(a->snow);
    if (a->props.id)
        UnloadTexture(a->props);
    if (a->construction.id)
        UnloadTexture(a->construction);
    if (a->villagers.id)
        UnloadTexture(a->villagers);
    if (a->enemies.id)
        UnloadTexture(a->enemies);
    if (a->arrow.id)
        UnloadTexture(a->arrow);
    for (i = 0; i < CORE_BLD_COUNT; i++) {
        int rot;
        for (rot = 0; rot < 4; rot++)
            if (a->turns[i][rot].id)
                UnloadTexture(a->turns[i][rot]);
    }
    if (a->status_icons.id)
        UnloadTexture(a->status_icons);
    if (a->animals.id)
        UnloadTexture(a->animals);
    if (a->animal_walk.id)
        UnloadTexture(a->animal_walk);
    if (a->wall_links.id)
        UnloadTexture(a->wall_links);
    if (a->biomes.id)
        UnloadTexture(a->biomes);
    for (i = 0; i < CORE_RES_COUNT; i++)
        if (a->resources[i].id)
            UnloadTexture(a->resources[i]);
    if (a->font.texture.id)
        UnloadFont(a->font);
    if (a->audio_ready) {
        for (i = 0; i < SND_COUNT; i++)
            if (a->sounds[i].frameCount) {
                StopSound(a->sounds[i]);
                UnloadSound(a->sounds[i]);
            }
        CloseAudioDevice();
    }
    memset(a, 0, sizeof(*a));
}
void game_sound(GameAssets *a, int sound, float volume) {
    double now, gap = sound == SND_RAIN ? .95 : sound == SND_THUNDER ? 6.5 : sound == SND_ALARM ? 1.5 : 0.08;
    if (!a->audio_ready || sound < 0 || sound >= SND_COUNT || !a->sounds[sound].frameCount)
        return;
    now = GetTime();
    if (now - a->last_sound[sound] < gap)
        return;
    a->last_sound[sound] = now;
    SetSoundVolume(a->sounds[sound], volume);
    PlaySound(a->sounds[sound]);
}
int game_screenshot(const char *path) {
    /* raylib 5.5의 화면 로더는 Windows에서 DPI를 중복 곱합니다.
     * 논리 창 크기 대신 실제 프레임버퍼 크기로 읽어 검은 여백을 피합니다. */
    Image image = {0};
    int n = 0, ok = 0;
    unsigned char *bytes;
    image.width = GetRenderWidth();
    image.height = GetRenderHeight();
    image.mipmaps = 1;
    image.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
    image.data = rlReadScreenPixels(image.width, image.height);
    if (!image.data)
        return 0;
    bytes = ExportImageToMemory(image, ".png", &n);
    if (bytes) {
        ok = game_write_bytes(path, bytes, (size_t)n);
        MemFree(bytes);
    }
    if (!ok)
        fprintf(stderr, u8"스크린숏 저장 실패: %s\n", path);
    UnloadImage(image);
    return ok;
}
