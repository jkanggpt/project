/* assets.h — 새 픽셀아트 PNG·한국어 TTF·OGG 리소스 관리자 */
#ifndef CASTLE_GAME_ASSETS_H
#define CASTLE_GAME_ASSETS_H
#include "raylib.h"
#include <castle_core.h>
#include "storage.h"
enum GameSound { SND_CLICK, SND_PLACE, SND_DENIED, SND_ALARM, SND_ARROW, SND_HIT, SND_DONE, SND_FANFARE, SND_RAIN, SND_THUNDER, SND_COUNT };
typedef struct GameAssets {
    Font font;
    Texture2D terrain, roads, snow, props, villagers, enemies, arrow, construction;
    Texture2D buildings[CORE_BLD_COUNT], resources[CORE_RES_COUNT];
    Texture2D turns[CORE_BLD_COUNT][4],status_icons,animals,biomes,animal_walk,wall_links;
    Texture2D gates, farms, greenhouse_turns, raider_attack, animal_babies;
    Texture2D trees, manors[6], biome_edges, shores, road_tiles, animal_swim, animal_attack;
    Sound sounds[SND_COUNT];
    double last_sound[SND_COUNT];
    int audio_ready, missing;
} GameAssets;
int game_assets_load(GameAssets *a,const char *directory,int audio);
void game_assets_free(GameAssets *a);
void game_sound(GameAssets *a,int sound,float volume);
int game_screenshot(const char *path);
#endif
