/* assets.c — 메모리 로더를 써서 한글 경로에서도 리소스를 읽습니다. */
#include "assets.h"
#include <castle_expansion.h>
#include "rlgl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* 외부 녹음 없이 정해진 잡음으로 비와 낮은 천둥을 합성합니다. */
static Sound weather_sound(int thunder) {
    unsigned count=thunder?88200:44100;float *samples=malloc(count*sizeof(float));
    Sound result={0};if(!samples)return result;
    unsigned random=1234567;float low=0;
    for(unsigned i=0;i<count;i++) {
        random=random*1664525u+1013904223u;
        float noise=(float)(random>>8)/8388608.0f-1, t=i/44100.0f;
        low=low*.985f+noise*.015f;
        float envelope=thunder?fminf(1,t*35)*expf(-t*2):.2f;
        samples[i]=thunder?(low*6+sinf(t*83)*.06f)*envelope:noise*envelope;
    }
    result=LoadSoundFromWave((Wave){count,44100,32,1,samples});free(samples);return result;
}

static Texture2D atlas_texture(GameAssets *a, const char *dir, const char *name,Rectangle *regions,int columns,int rows) {
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
    if (image.data) {
        if(regions)for(int r=0;r<rows;r++)for(int c=0;c<columns;c++) {
            Rectangle cell={c*(image.width/(float)columns),r*(image.height/(float)rows),image.width/(float)columns,image.height/(float)rows};
            Image frame=ImageFromImage(image,cell);
            Rectangle border=GetImageAlphaBorder(frame,.1f);
            if(border.width<1 || border.height<1){a->missing++;border=(Rectangle){0,0,cell.width,cell.height};}
            regions[r*columns+c]=(Rectangle){cell.x+border.x,cell.y+border.y,border.width,border.height};
            UnloadImage(frame);
        }
        result = LoadTextureFromImage(image);
        UnloadImage(image);
        SetTextureFilter(result, TEXTURE_FILTER_POINT);
    } else
        a->missing++;
    return result;
}
static Texture2D texture(GameAssets *a,const char *dir,const char *name) {
    return atlas_texture(a,dir,name,NULL,0,0);
}
int game_assets_load(GameAssets *a, const char *dir, int audio) {
    static const char *const sounds[SND_COUNT] = {"audio-interface/Audio/click_002.ogg",
                                                  "audio-interface/Audio/drop_002.ogg",
                                                  "audio-interface/Audio/error_004.ogg",
                                                  "audio-impact/Audio/impactBell_heavy_000.ogg",
                                                  "audio-rpg/Audio/drawKnife1.ogg",
                                                  "audio-impact/Audio/impactSoft_medium_000.ogg",
                                                  "audio-interface/Audio/confirmation_002.ogg",
                                                  "audio-interface/Audio/maximize_006.ogg"};
    char path[GAME_PATH_BYTES], name[160];
    int i, n = 0, count = 0, *codepoints;
    unsigned char *bytes;
    memset(a, 0, sizeof(*a));
    if (!game_join_path(path, sizeof(path), dir, "fonts/Galmuri11.ttf"))
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
    a->terrain = texture(a, dir, "painted/terrain.png");
    a->roads = texture(a, dir, "painted/roads.png");
    a->snow = texture(a, dir, "painted/snow.png");
    a->props = texture(a, dir, "painted/props.png");
    a->construction = texture(a, dir, "painted/construction.png");
    a->villagers = texture(a, dir, "expansion/villagers_v122.png");
    a->enemies = texture(a, dir, "expansion/raider_walk_v122.png");
    a->arrow = texture(a, dir, "painted/arrow.png");
    if (a->villagers.id && (a->villagers.width != 1152 || a->villagers.height != 4608)) {
        fprintf(stderr, "Incompatible villager atlas: expected 1152x4608\n");
        a->missing++;
    }
    a->status_icons = texture(a, dir, "expansion/status_icons.png");
    a->life_icons=texture(a,dir,"expansion/life_icons.png");
    a->rocks=texture(a,dir,"expansion/rocks.png");
    static const char *const uniform_names[4]={"camp","lodge","barracks","garrison"};
    for(i=0;i<4;i++){snprintf(name,sizeof(name),"expansion/uniform_%s.png",uniform_names[i]);a->uniforms[i]=texture(a,dir,name);}
    a->animals = texture(a, dir, "expansion/animals_v122.png");
    a->animal_walk=texture(a,dir,"expansion/animal_walk_v122.png");
    a->wall_links=texture(a,dir,"expansion/wall_links.png");
    a->biomes = texture(a, dir, "expansion/biomes.png");
    a->gates=texture(a,dir,"expansion/gates_v121.png");
    a->farms=texture(a,dir,"expansion/farms_v122.png");
    a->greenhouse_turns=atlas_texture(a,dir,"expansion/greenhouse_turns.png",a->greenhouse_sources,4,4);
    a->raider_attack=texture(a,dir,"expansion/raider_attack_v122.png");
    a->animal_babies=texture(a,dir,"expansion/animal_babies.png");
    a->trees = texture(a, dir, "expansion/trees_v121.png");
    a->biome_edges = texture(a, dir, "expansion/biome_edges.png");
    a->shores = texture(a, dir, "expansion/shores.png");
    a->road_tiles = texture(a, dir, "expansion/road_connections.png");
    a->animal_swim = texture(a, dir, "expansion/animal_swim.png");
    a->animal_attack = texture(a, dir, "expansion/animal_attack_v122.png");
    for (i = 0; i < 6; i++) {
        snprintf(name, sizeof(name), "expansion/manor_%d.png", i);
        a->manors[i] = texture(a, dir, name);
    }
    for (i = 0; i < CORE_BLD_COUNT; i++) {
        CoreExpansionBuildingInfo info;
        int rot;
        (void)core_expansion_building_info(i, &info);
        for (rot = 0; rot < 4; rot++) {
            (void)snprintf(name, sizeof(name), "expansion/%s_%d.png", info.key, rot);
            a->turns[i][rot] = texture(a, dir, name);
        }
        a->buildings[i] = a->turns[i][0];
    }
    for (i = 0; i < CORE_RES_COUNT; i++) {
        (void)snprintf(name, sizeof(name), "expansion/res_%s.png", core_expansion_resource_key(i));
        a->resources[i] = texture(a, dir, name);
    }
    if (audio) {
        InitAudioDevice();
        a->audio_ready = IsAudioDeviceReady();
        if (a->audio_ready)
            for (i = 0; i < SND_COUNT; i++) {
                if(i==SND_RAIN || i==SND_THUNDER){a->sounds[i]=weather_sound(i==SND_THUNDER);continue;}
                Wave wave = {0};
                (void)snprintf(name, sizeof(name), "third_party/kenney/%s", sounds[i]);
                if (!game_join_path(path, sizeof(path), dir, name))
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
    if(a->life_icons.id)UnloadTexture(a->life_icons);
    if(a->rocks.id)UnloadTexture(a->rocks);
    for(int k=0;k<4;k++)if(a->uniforms[k].id)UnloadTexture(a->uniforms[k]);
    if(a->gates.id)UnloadTexture(a->gates);
    if(a->farms.id)UnloadTexture(a->farms);
    if(a->greenhouse_turns.id)UnloadTexture(a->greenhouse_turns);
    if(a->raider_attack.id)UnloadTexture(a->raider_attack);
    if(a->animal_babies.id)UnloadTexture(a->animal_babies);
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
    if(a->animal_walk.id)UnloadTexture(a->animal_walk);
    if(a->wall_links.id)UnloadTexture(a->wall_links);
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
