/* lighting_lab.h — 실험 스프라이트와 그림자가 공유하는 월드 모양 */
#ifndef CASTLE_LIGHTING_LAB_H
#define CASTLE_LIGHTING_LAB_H
#include "raylib.h"
#define LAB_CELL 384
#define LAB_SCALE 3.0f
#define LAB_ATLAS 1536
#define LAB_ANCHOR_X 64.0f
#define LAB_ANCHOR_Y 96.0f
#define LAB_MAX_MODEL 24
#define LAB_MAX_OCCLUDERS 64
#define LAB_KIND_COUNT 16
/* RGB 법선은 월드 +X(오른쪽), +Y(지면 남쪽), +Z(높이)입니다. */
typedef enum LabKind {
    LAB_GRASS,
    LAB_STONE,
    LAB_DIRT,
    LAB_WATER,
    LAB_HOUSE,
    LAB_OPEN_HOUSE,
    LAB_NPC,
    LAB_TREE,
    LAB_ROCK,
    LAB_CRATE,
    LAB_TORCH,
    LAB_WALL,
    LAB_NPC_STEP_A,
    LAB_NPC_STEP_B,
    LAB_NPC_STEP_C,
    LAB_BALL
} LabKind;
typedef struct LabPrimitive {
    Vector3 center, extent;
    int shape, material, occluder; /* 0 상자, 1 타원체, 2 박공지붕 */
} LabPrimitive;
typedef struct LabHit {
    Vector3 position, normal;
    float distance;
    int material;
} LabHit;
int lab_model(int kind, LabPrimitive out[LAB_MAX_MODEL]);
int lab_intersect(const LabPrimitive *shape, Vector3 origin, Vector3 direction, float maximum, LabHit *hit);
int art_lighting_bake(const char *materials, const char *output);
int lab_geometry_check(void);
#endif
