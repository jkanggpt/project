/* assets.h — PNG·TTF·OGG를 기존 에셋에서 불러오는 C 리소스 관리자 */
#ifndef CASTLE_GAME_ASSETS_H
#define CASTLE_GAME_ASSETS_H
#include "raylib.h"
#include "castle_core.h"
#include "storage.h"
enum GameSound { SND_CLICK, SND_PLACE, SND_DENIED, SND_ALARM, SND_ARROW, SND_HIT, SND_DONE, SND_FANFARE, SND_COUNT };
typedef struct GameAssets {
    Font font;
    Texture2D terrain, roads, snow, villagers, enemies, arrow;
    Texture2D buildings[CORE_BLD_COUNT], resources[CORE_RES_COUNT];
    Sound sounds[SND_COUNT];
    double last_sound[SND_COUNT];
    int audio_ready, missing;
} GameAssets;
int game_assets_load(GameAssets *a,const char *directory,int audio);
void game_assets_free(GameAssets *a);
void game_sound(GameAssets *a,int sound,float volume);
int game_screenshot(const char *path);
#endif
