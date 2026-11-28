/* assets.h — 새 픽셀아트 PNG·한국어 TTF·OGG 리소스 관리자 */
#ifndef CASTLE_GAME_ASSETS_H
#define CASTLE_GAME_ASSETS_H
#include "raylib.h"
#include "storage.h"
#include <castle_core.h>
enum GameSound {
    SND_CLICK,
    SND_PLACE,
    SND_DENIED,
    SND_ALARM,
    SND_ARROW,
    SND_HIT,
    SND_DONE,
    SND_FANFARE,
    SND_RAIN,
    SND_THUNDER,
    SND_COUNT
};
typedef struct GameAlphaMask {
    unsigned int texture_id;
    int width, height;
    unsigned char *bits;
} GameAlphaMask;
typedef struct GameMaterialBinding {
    unsigned int texture_id;
    const char *albedo, *normal, *surface, *mode;
    int divisor;
} GameMaterialBinding;
typedef struct GameAssets {
    Font font;
    GameAlphaMask alpha[512];
    int alpha_count;
    GameMaterialBinding materials[512];
    int material_count;
    Texture2D carried_lights, stationary_fires;
    Texture2D terrain, roads, snow, props, villagers, enemies, arrow, construction;
    Texture2D buildings[CORE_BLD_COUNT], resources[CORE_RES_COUNT];
    Texture2D turns[CORE_BLD_COUNT][4], status_icons, animals, biomes, animal_walk, wall_links;
    Texture2D gates, farms, greenhouse_turns, raider_attack, animal_babies;
    Texture2D life_icons, rocks, uniforms[4];
    Texture2D windmill_blades[4], windmill_previews[4], irrigated_wheel[4], irrigated_previews;
    Texture2D lighthouse_lamp[4], lighthouse_glow[4], boats[2][4], nets[2][4], boat_wake[4];
    Texture2D harvest_phases, territory_icon, clinic_icon, fishing_icons;
    Texture2D portraits, combat_fx, factions, merchant, fishing_boats, bridge_parts, ore_soil,
        observatory_motion, soil_markers, merchant_portrait;
    Rectangle observatory_sources[64];
    Rectangle farm_sources[84];
    Rectangle greenhouse_sources[16];
    Texture2D trees, manors[6], biome_edges, shores, road_tiles, animal_swim, animal_attack;
    Sound sounds[SND_COUNT];
    double last_sound[SND_COUNT];
    int audio_ready, missing;
} GameAssets;
int game_texture_hit(const GameAssets *assets, Texture2D texture, Rectangle source, float u, float v);
const GameMaterialBinding *game_assets_material(const GameAssets *assets, Texture2D texture);
int game_assets_load(GameAssets *a, const char *directory, int audio);
void game_assets_free(GameAssets *a);
void game_sound(GameAssets *a, int sound, float volume);
int game_screenshot(const char *path);
#endif
