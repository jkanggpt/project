/* 원화에 맞춘 건물의 차폐 체적·개구부·굴뚝 기준점입니다. */
#ifndef CASTLE_SCENE_PROFILES_H
#define CASTLE_SCENE_PROFILES_H
#include "raylib.h"
#include <castle_core.h>
enum GameSceneLightKind {
    GAME_SCENE_WINDOW,
    GAME_SCENE_DOOR,
    GAME_SCENE_TORCH,
    GAME_SCENE_FURNACE,
    GAME_SCENE_MAGIC,
    GAME_SCENE_BEACON,
    /* 인계 고정 불 원화를 땅 위에 세우는 드문 야영·검문·모임 자리입니다. */
    GAME_SCENE_BRAZIER,
    GAME_SCENE_CAMPFIRE
};
typedef struct GameSceneVolume {
    Vector3 center, extent;
    int kind, contact;
} GameSceneVolume;
typedef struct GameSceneLight {
    Vector3 position;
    Vector2 screen;
    Color color;
    float radius, strength, direction, cone;
    int kind;
} GameSceneLight;
typedef struct GameSceneChimney {
    Vector3 position, extent;
    int smoke;
} GameSceneChimney;
typedef struct GameSceneProfile {
    GameSceneVolume volumes[8];
    GameSceneLight lights[8];
    GameSceneChimney chimneys[3];
    int volume_count, light_count, chimney_count;
    Rectangle ground;
    float front, wall_height, roof_height;
} GameSceneProfile;
/* time은 등대 방향과 서로 다른 불빛 위상을 위한 시각 시간입니다. */
int game_scene_profile(const CoreBuildingView *building, Rectangle picture, float time,
                       GameSceneProfile *profile);
/* 원화에 그려진 창·불의 발광을 켤지 정합니다. 광원과 같은 점등 조건을 사용합니다. */
int game_scene_lit(const CoreBuildingView *building);
int game_scene_profile_validate(void);
#endif
