/* GPU 자원과 장면 제출은 화면·게임 규칙에서 분리합니다. */
#ifndef CASTLE_GPU_H
#define CASTLE_GPU_H
#include "raylib.h"
#include <stddef.h>
#include <stdint.h>

typedef struct CastleGpu CastleGpu;
enum CastleGpuMaterial {
    GPU_MATERIAL_UNLIT,
    GPU_MATERIAL_BUILDING,
    GPU_MATERIAL_TREE,
    GPU_MATERIAL_ACTOR,
    GPU_MATERIAL_GRASS,
    GPU_MATERIAL_ROAD,
    GPU_MATERIAL_ROCK,
    GPU_MATERIAL_WATER,
    GPU_MATERIAL_TUFT
};
typedef struct CastleGpuSprite {
    float destination[4];
    float source[4];
    float tint[4];
    float normal_source[4];
    float shape[4];
    uint32_t tags[4];
    float contact_profile[4];
} CastleGpuSprite;
typedef struct CastleGpuCaster {
    float center[4];
    float extent[4];
    float contact[4];
    uint32_t tags[4];
    float contact_profile[4];
} CastleGpuCaster;
/* 광원 tags[2]의 하위 비트는 간략 차폐, tags[3]은 안정된 발광체 번호입니다. */
typedef struct CastleGpuLight {
    float position[4];
    float color[4];
    uint32_t tags[4];
} CastleGpuLight;
/* 건물 받침의 닳은 흙. rect는 세계 좌표 x,y,w,h, settings는 세기·문 x·문 y·종류(0 건물, 1 밭)입니다.
   점유 칸이 아니라 실제 받침 모양을 따르며 지형·길·물·눈 자료를 바꾸지 않습니다. */
typedef struct CastleGpuFootprint {
    float rect[4];
    float settings[4];
} CastleGpuFootprint;
typedef struct CastleGpuEmitter {
    float position[4];
    float settings[4];
} CastleGpuEmitter;
typedef struct CastleGpuFrame {
    int width, height, map_width, map_height;
    uint32_t world_seed;
    /* 0은 이전 비교 장면, 1~6은 세계 1단위를 한 논리 화소로 표시합니다. */
    int pixel_scale, natural;
    float origin[2], scale, time, delta;
    float sun[4], ambient, detail, grass_density;
    float smoke_scale;
    int sun_enabled, shadows, foliage, grass, water, smoke, view;
    int smoke_probe;
    int grass_limit;
    int reset_motion;
    /* 0은 맑음, 1은 짙은 구름·비입니다. 물의 햇빛 반짝임을 줄입니다. */
    float overcast;
} CastleGpuFrame;
typedef struct CastleGpuViewport {
    int width, height, scale;
    Rectangle destination;
    Vector2 origin;
} CastleGpuViewport;
CastleGpuViewport castle_gpu_viewport(const CastleGpuFrame *frame);
Vector2 castle_gpu_screen_to_world(const CastleGpuFrame *frame, Vector2 point);
Vector2 castle_gpu_world_to_screen(const CastleGpuFrame *frame, Vector2 point);
typedef struct CastleGpuStats {
    double fields_ms, geometry_ms, shadows_ms, lighting_ms, smoke_ms;
    size_t texture_bytes, buffer_bytes;
    unsigned int live_objects;
    int software, sprites, casters, lights, emitters, grass_instances, smoke_particles, tile_overflows;
    int logical_width, logical_height, pixel_scale;
    uint64_t frames, timed_frames, timed_frame_id, static_updates;
} CastleGpuStats;
typedef struct CastleGpuDevice {
    char vendor[96], renderer[192], version[128];
    int software;
} CastleGpuDevice;

int castle_gpu_check_compute(void);
int castle_gpu_available(char *message, size_t bytes);
CastleGpuDevice castle_gpu_device(void);
CastleGpu *castle_gpu_create(void);
void castle_gpu_scenery(CastleGpu *gpu, Texture2D atlas);
void castle_gpu_contact_policy(CastleGpu *gpu, Texture2D policy);
void castle_gpu_destroy(CastleGpu *gpu);
int castle_gpu_begin(CastleGpu *gpu, const CastleGpuFrame *frame);
int castle_gpu_sprite(CastleGpu *gpu, Texture2D texture, Texture2D normals, Texture2D surface,
                      const CastleGpuSprite *sprite);
int castle_gpu_caster(CastleGpu *gpu, const CastleGpuCaster *caster);
int castle_gpu_light(CastleGpu *gpu, const CastleGpuLight *light);
int castle_gpu_emitter(CastleGpu *gpu, const CastleGpuEmitter *emitter);
int castle_gpu_footprint(CastleGpu *gpu, const CastleGpuFootprint *footprint);
void castle_gpu_terrain(CastleGpu *gpu, const unsigned char *cells, int width, int height);
int castle_gpu_render(CastleGpu *gpu);
Texture2D castle_gpu_clouds(CastleGpu *gpu, const unsigned char *visibility, int width, int height);
void castle_gpu_present(CastleGpu *gpu, Rectangle destination);
CastleGpuStats castle_gpu_stats(const CastleGpu *gpu);
Texture2D castle_gpu_output(const CastleGpu *gpu);
unsigned int castle_gpu_live_objects(void);
int castle_gpu_validate(CastleGpu *gpu);
/* 아래 읽기 기능은 검사·진단 전용이며 일반 프레임에서 호출하지 않습니다. */
Texture2D castle_gpu_data_texture(const CastleGpu *gpu, int index);
int castle_gpu_validate_scenery(CastleGpu *gpu);
size_t castle_gpu_grass_snapshot(CastleGpu *gpu, uint32_t *seeds, size_t capacity);
#endif
