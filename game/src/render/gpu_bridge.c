/* 화면 입력·게임 규칙을 바꾸지 않고 원화와 효과 자료를 모읍니다. */
#include "gpu_bridge.h"
#include "app.h"
#include "gpu.h"
#include "rlgl.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct MaterialPair {
    Texture2D albedo, normal;
} MaterialPair;
typedef struct Bridge {
    GameApp *app;
    CastleGpu *gpu;
    Texture2D albedo, normal, surface, scenery, contact_policy;
    RenderTexture2D overlay;
    unsigned char *terrain;
    size_t terrain_capacity;
    CastleGpuLight candidates[4];
    float candidate_distance[4];
    int candidate_count, active, material, error, reset_pending;
    uint32_t owner;
    float foot, time;
    const void *world;
    int world_tick;
    unsigned int frame;
    CastleGpuFrame config;
    CastleGpuStats checkpoint;
    size_t checkpoint_terrain;
    MaterialPair materials[512];
    char assets_directory[GAME_PATH_BYTES];
    size_t material_bytes, checkpoint_material_bytes;
    int material_count, checkpoint_material_count;
    uint64_t baked_sprites, calibrated_sprites, estimated_sprites;
} Bridge;
static Bridge bridge;

static Texture2D load_texture(const char *root, const char *relative) {
    char path[GAME_PATH_BYTES];
    int size = 0;
    if (!game_join_path(path, sizeof(path), root, relative))
        return (Texture2D){0};
    unsigned char *data = game_read_bytes(path, &size);
    Image image = data ? LoadImageFromMemory(".png", data, size) : (Image){0};
    free(data);
    Texture2D result = image.data ? LoadTextureFromImage(image) : (Texture2D){0};
    if (image.data)
        UnloadImage(image);
    if (result.id)
        SetTextureFilter(result, TEXTURE_FILTER_POINT);
    return result;
}
int game_gpu_initialize(GameApp *app, const char *assets_directory) {
    memset(&bridge, 0, sizeof(bridge));
    bridge.app = app;
    snprintf(bridge.assets_directory, sizeof(bridge.assets_directory), "%s", assets_directory);
    bridge.gpu = castle_gpu_create();
    bridge.albedo = load_texture(assets_directory, "lighting_lab/pixel_albedo.png");
    bridge.normal = load_texture(assets_directory, "lighting_lab/pixel_normals.png");
    bridge.surface = load_texture(assets_directory, "lighting_lab/pixel_surface.png");
    bridge.scenery = load_texture(assets_directory, "lighting_lab/scenery_masks.png");
    bridge.contact_policy = load_texture(assets_directory, "lighting_lab/contact_policy.png");
    if (!bridge.gpu || !bridge.albedo.id || !bridge.normal.id || !bridge.surface.id || !bridge.scenery.id ||
        !bridge.contact_policy.id) {
        game_gpu_shutdown();
        return 0;
    }
    castle_gpu_scenery(bridge.gpu, bridge.scenery);
    castle_gpu_contact_policy(bridge.gpu, bridge.contact_policy);
    bridge.reset_pending = 1;
    return 1;
}
void game_gpu_new_world(void) {
    /* 같은 시드·주소·틱으로 다시 시작해도 이전 굴뚝의 입자를 넘기지 않습니다. */
    bridge.reset_pending = 1;
    bridge.time = 0;
}
int game_gpu_failed(void) { return bridge.error; }
void game_gpu_shutdown(void) {
    if (bridge.active) {
        EndBlendMode();
        EndTextureMode();
        bridge.active = 0;
    }
    if (bridge.overlay.id)
        UnloadRenderTexture(bridge.overlay);
    if (bridge.albedo.id)
        UnloadTexture(bridge.albedo);
    if (bridge.normal.id)
        UnloadTexture(bridge.normal);
    if (bridge.surface.id)
        UnloadTexture(bridge.surface);
    if (bridge.scenery.id)
        UnloadTexture(bridge.scenery);
    if (bridge.contact_policy.id)
        UnloadTexture(bridge.contact_policy);
    for (int i = 0; i < 512; i++) {
        if (bridge.materials[i].albedo.id)
            UnloadTexture(bridge.materials[i].albedo);
        if (bridge.materials[i].normal.id)
            UnloadTexture(bridge.materials[i].normal);
    }
    castle_gpu_destroy(bridge.gpu);
    free(bridge.terrain);
    memset(&bridge, 0, sizeof(bridge));
}
int game_gpu_active(void) { return bridge.active; }
static CastleGpuFrame camera_frame(GameApp *app) {
    Vector2 origin = GetScreenToWorld2D((Vector2){0, 0}, app->camera);
    return (CastleGpuFrame){.width = GetScreenWidth(),
                            .height = GetScreenHeight(),
                            .pixel_scale = (int)fmaxf(1, fminf(6, floorf(app->camera.zoom + .5f))),
                            .origin = {origin.x, origin.y},
                            .scale = 1};
}
Camera2D game_gpu_screen_camera(GameApp *app) {
    if (!bridge.gpu || bridge.error)
        return app->camera;
    CastleGpuFrame f = camera_frame(app);
    CastleGpuViewport v = castle_gpu_viewport(&f);
    if (!v.scale)
        return app->camera;
    return (Camera2D){
        .target = v.origin, .offset = {v.destination.x, v.destination.y}, .zoom = (float)v.scale};
}
Camera2D game_gpu_camera(GameApp *app) {
    Camera2D camera = game_gpu_screen_camera(app);
    if (bridge.gpu && !bridge.error) {
        camera.offset = (Vector2){0, 0};
        camera.zoom = 1;
    }
    return camera;
}
Vector2 game_gpu_canvas_size(void) {
    return (Vector2){(float)bridge.overlay.texture.width, (float)bridge.overlay.texture.height};
}
void game_gpu_overlay_begin(void) {
    BeginTextureMode(bridge.overlay);
    ClearBackground(BLANK);
    rlSetBlendFactorsSeparate(0x0302, 0x0303, 1, 0x0303, 0x8006, 0x8006);
    BeginBlendMode(BLEND_CUSTOM_SEPARATE);
}
static void overlay_present(void) {
    CastleGpuViewport v = castle_gpu_viewport(&bridge.config);
    BeginBlendMode(BLEND_ALPHA_PREMULTIPLY);
    DrawTexturePro(bridge.overlay.texture, (Rectangle){0, 0, (float)v.width, -(float)v.height}, v.destination,
                   (Vector2){0, 0}, 0, WHITE);
    EndBlendMode();
}
void game_gpu_overlay_end(void) {
    EndBlendMode();
    EndTextureMode();
    overlay_present();
}
int game_gpu_begin(GameApp *app) {
    if (!bridge.gpu || bridge.error)
        return 0;
    int width = GetScreenWidth(), height = GetScreenHeight();
    if (width < 1 || height < 1)
        return 0;
    CastleGpuFrame camera = camera_frame(app);
    CastleGpuViewport viewport = castle_gpu_viewport(&camera);
    if (bridge.overlay.texture.width != viewport.width || bridge.overlay.texture.height != viewport.height) {
        if (bridge.overlay.id)
            UnloadRenderTexture(bridge.overlay);
        bridge.overlay = LoadRenderTexture(viewport.width, viewport.height);
        if (!bridge.overlay.id) {
            bridge.error = 1;
            return 0;
        }
        SetTextureFilter(bridge.overlay.texture, TEXTURE_FILTER_POINT);
    }
    GameSession *s = &app->session;
    size_t cells = (size_t)s->cells;
    if (cells > bridge.terrain_capacity) {
        unsigned char *replacement = realloc(bridge.terrain, cells);
        if (!replacement) {
            bridge.error = 1;
            return 0;
        }
        bridge.terrain = replacement;
        bridge.terrain_capacity = cells;
    }
    for (size_t i = 0; i < cells; i++) {
        int water = s->terrain[i] == CORE_TERRAIN_WATER;
        bridge.terrain[i] = (unsigned char)(water                                ? 2
                                            : s->roads[i] == 1                   ? 5
                                            : s->roads[i] == 4                   ? 20
                                            : s->roads[i]                        ? 4
                                            : s->terrain[i] == CORE_TERRAIN_ROCK ? 8
                                                                                 : 1);
        if (s->roads[i] == 3)
            bridge.terrain[i] |= 64u;
        if (s->summary.season == CORE_SEASON_WINTER && !water)
            bridge.terrain[i] = (unsigned char)((bridge.terrain[i] & ~1u) | 32u);
        if (s->visibility && !s->visibility[i])
            bridge.terrain[i] = 128;
    }
    float delta = app->automated ? 1.0f / 60 : fminf(.05f, GetFrameTime());
    if (app->automated || IsWindowFocused())
        bridge.time += delta;
    else
        delta = 0;
    float phase = fminf(1, fmaxf(0, (s->summary.minute_of_day - 360) / 840.0f));
    float azimuth = floorf(135 + phase * 90), elevation = floorf(18 + 44 * sinf(phase * PI));
    bridge.config = (CastleGpuFrame){.width = width,
                                     .height = height,
                                     .world_seed = (uint32_t)s->summary.seed,
                                     .reset_motion = bridge.reset_pending || bridge.world != s->world ||
                                                     bridge.world_tick > s->summary.tick,
                                     .pixel_scale = camera.pixel_scale,
                                     .natural = 1,
                                     .map_width = s->summary.map_width,
                                     .map_height = s->summary.map_height,
                                     .origin = {viewport.origin.x, viewport.origin.y},
                                     .scale = 1,
                                     .time = bridge.time,
                                     .delta = delta,
                                     .sun = {cosf(azimuth * DEG2RAD) * cosf(elevation * DEG2RAD),
                                             sinf(azimuth * DEG2RAD) * cosf(elevation * DEG2RAD),
                                             sinf(elevation * DEG2RAD), elevation},
                                     .ambient = s->summary.is_night ? .20f : .50f,
                                     .detail = 1.25f,
                                     .sun_enabled = !s->summary.is_night,
                                     .shadows = 1,
                                     .foliage = !app->settings.low_effects,
                                     .grass = !app->settings.low_effects,
                                     .water = 1,
                                     .smoke = !app->settings.low_effects};
    if (!castle_gpu_begin(bridge.gpu, &bridge.config)) {
        bridge.error = 1;
        return 0;
    }
    bridge.world = s->world;
    bridge.world_tick = s->summary.tick;
    bridge.reset_pending = 0;
    bridge.candidate_count = 0;
    bridge.material = 0;
    bridge.owner = 0;
    bridge.foot = 0;
    bridge.active = 1;
    bridge.frame++;
    game_gpu_overlay_begin();
    return 1;
}
void game_gpu_object(int material, uint32_t owner, float foot) {
    bridge.material = material;
    bridge.owner = owner;
    bridge.foot = foot;
}
static int same_rect(Rectangle a, Rectangle b) {
    return fabsf(a.x - b.x) < .1f && fabsf(a.y - b.y) < .1f && fabsf(fabsf(a.width) - b.width) < .1f &&
           fabsf(a.height - b.height) < .1f;
}
static const MaterialPair *material_pair(const GameMaterialBinding *m, Texture2D original) {
    size_t index = (size_t)(m - bridge.app->assets.materials);
    if (index >= 512 || m->divisor < 1)
        return NULL;
    MaterialPair *pair = &bridge.materials[index];
    if (pair->albedo.id && pair->normal.id)
        return pair;
    /* 읽기는 처음 사용할 때만 수행하며 메뉴 왕복 뒤에도 같은 자원을 재사용합니다. */
    pair->albedo = load_texture(bridge.assets_directory, m->albedo);
    pair->normal = load_texture(bridge.assets_directory, m->normal);
    if (!pair->albedo.id || !pair->normal.id || pair->albedo.width != original.width ||
        pair->albedo.height != original.height || pair->normal.width != original.width / m->divisor ||
        pair->normal.height != original.height / m->divisor) {
        fprintf(stderr, "스프라이트 재료를 읽지 못했습니다: %s\n", m->albedo);
        bridge.error = 1;
        return NULL;
    }
    bridge.material_count++;
    bridge.material_bytes += (size_t)pair->albedo.width * pair->albedo.height * 4 +
                             (size_t)pair->normal.width * pair->normal.height * 3;
    return pair;
}
static int sprite(Texture2D texture, Rectangle source, Rectangle destination, Color tint, int floor_sprite,
                  float angle) {
    if (!bridge.active || !texture.id)
        return 0;
    GameAssets *a = &bridge.app->assets;
    const GameMaterialBinding *binding = game_assets_material(a, texture);
    if (binding && !strcmp(binding->mode, "unlit"))
        return 0;
    for (int i = 0; i < 4; i++)
        if (texture.id == a->lighthouse_glow[i].id || texture.id == a->lighthouse_lamp[i].id)
            return 0;
    int kind = bridge.material;
    if (texture.id == a->trees.id)
        kind = GPU_MATERIAL_TREE;
    else if (texture.id == a->rocks.id)
        kind = GPU_MATERIAL_ROCK;
    else if (texture.id == a->biomes.id || texture.id == a->terrain.id || texture.id == a->snow.id ||
             texture.id == a->biome_edges.id || texture.id == a->shores.id ||
             texture.id == a->soil_markers.id) {
        int x = (int)floorf(destination.x / 16), y = (int)floorf(destination.y / 16),
            w = bridge.config.map_width;
        kind = x >= 0 && y >= 0 && x < w && y < bridge.config.map_height && (bridge.terrain[y * w + x] & 2)
                   ? GPU_MATERIAL_WATER
                   : GPU_MATERIAL_GRASS;
    } else if (texture.id == a->road_tiles.id || texture.id == a->roads.id ||
               texture.id == a->bridge_parts.id)
        kind = GPU_MATERIAL_ROAD;
    else if (texture.id == a->villagers.id || texture.id == a->enemies.id ||
             texture.id == a->animal_walk.id || texture.id == a->animal_swim.id ||
             texture.id == a->animal_attack.id)
        kind = GPU_MATERIAL_ACTOR;
    if (kind == GPU_MATERIAL_UNLIT)
        return 0;
    Rectangle calibrated = {0};
    float scale = 0;
    if (texture.id == a->turns[CORE_BLD_HOUSE][0].id && source.x == 0 && source.y == 0) {
        calibrated = (Rectangle){32, 30, 256, 282};
        scale = destination.width / 256;
    } else if (texture.id == a->trees.id && same_rect(source, (Rectangle){256, 0, 128, 128})) {
        calibrated = (Rectangle){352, 56, 256, 256};
        scale = destination.width / 256;
    } else if (texture.id == a->villagers.id && same_rect(source, (Rectangle){0, 0, 96, 96})) {
        calibrated = (Rectangle){656, 24, 288, 288};
        scale = destination.width / 288;
    } else if (texture.id == a->terrain.id && same_rect(source, (Rectangle){0, 0, 64, 64})) {
        calibrated = (Rectangle){32, 376, 256, 256};
        scale = 0;
    } else if (texture.id == a->roads.id && same_rect(source, (Rectangle){0, 0, 64, 64})) {
        calibrated = (Rectangle){352, 376, 256, 256};
        scale = 0;
    } else if (texture.id == a->rocks.id && same_rect(source, (Rectangle){128, 0, 128, 128})) {
        calibrated = (Rectangle){672, 376, 256, 256};
        scale = destination.width / 256;
    }
    int flip = source.width < 0;
    Texture2D normal = {0}, surface = {0};
    CastleGpuSprite s = {0};
    s.destination[0] = destination.x;
    s.destination[1] = destination.y;
    s.destination[2] = destination.width;
    s.destination[3] = destination.height;
    s.tint[0] = tint.r / 255.0f;
    s.tint[1] = tint.g / 255.0f;
    s.tint[2] = tint.b / 255.0f;
    s.tint[3] = tint.a / 255.0f;
    s.shape[0] = bridge.owner ? bridge.foot : destination.y + destination.height;
    s.shape[1] = destination.height * .72f;
    s.shape[2] = 1.25f;
    s.tags[0] = bridge.owner;
    s.tags[1] = (unsigned int)kind;
    s.tags[3] = bridge.owner * 7919u;
    if (texture.id == a->bridge_parts.id || texture.id == a->soil_markers.id)
        s.tags[2] = 2;
    if (calibrated.width > 0 && !floor_sprite) {
        texture = bridge.albedo;
        normal = bridge.normal;
        surface = bridge.surface;
        source = calibrated;
        if (flip)
            source.width = -source.width;
        s.tags[2] = 1;
        s.shape[1] = scale;
        bridge.calibrated_sprites++;
    } else if (binding && !strcmp(binding->mode, "baked")) {
        const MaterialPair *pair = material_pair(binding, texture);
        if (!pair)
            return 1;
        texture = pair->albedo;
        normal = pair->normal;
        s.tags[2] |= 4u;
        bridge.baked_sprites++;
    } else if (!binding) {
        bridge.estimated_sprites++;
    }
    if (floor_sprite) {
        s.tags[0] = 0;
        s.tags[2] |= 8u;
        s.shape[0] = destination.y + destination.height;
        s.shape[1] = 1;
        s.shape[2] = angle * DEG2RAD;
    }
    s.source[0] = (source.x + (source.width < 0 ? -source.width : 0)) / texture.width;
    s.source[1] = source.y / texture.height;
    s.source[2] = source.width / texture.width;
    s.source[3] = source.height / texture.height;
    memcpy(s.normal_source, s.source, sizeof(s.source));
    return castle_gpu_sprite(bridge.gpu, texture, normal, surface, &s);
}
int game_gpu_sprite(Texture2D texture, Rectangle source, Rectangle destination, Color tint) {
    return sprite(texture, source, destination, tint, 0, 0);
}
int game_gpu_floor_sprite(Texture2D texture, Rectangle source, Rectangle destination, Color tint,
                          float angle) {
    return sprite(texture, source, destination, tint, 1, angle);
}
static void add_caster(uint32_t owner, int dynamic, int kind, float x, float y, float z, float rx, float ry,
                       float rz, float opacity, int contact) {
    if (!bridge.active)
        return;
    CastleGpuCaster c = {{x, y, z, 0},
                         {fmaxf(.1f, rx), fmaxf(.1f, ry), fmaxf(.1f, rz), opacity},
                         {x, y, rx, fmaxf(1, ry * .5f)},
                         {owner, (unsigned int)dynamic, (unsigned int)kind, (unsigned int)contact},
                         {0}};
    (void)castle_gpu_caster(bridge.gpu, &c);
}
static void candidate(CastleGpuLight light) {
    CastleGpuViewport v = castle_gpu_viewport(&bridge.config);
    Vector2 center = {bridge.config.origin[0] + v.width * .5f, bridge.config.origin[1] + v.height * .5f};
    float dx = light.position[0] - center.x, dy = light.position[1] - light.position[2] * .9f - center.y;
    if (fabsf(dx) > v.width * .5f + light.position[3] || fabsf(dy) > v.height * .5f + light.position[3])
        return;
    float distance = dx * dx + dy * dy;
    int slot = bridge.candidate_count;
    if (slot == 4) {
        slot = 0;
        for (int i = 1; i < 4; i++)
            if (bridge.candidate_distance[i] > bridge.candidate_distance[slot])
                slot = i;
        if (distance >= bridge.candidate_distance[slot])
            return;
    } else
        bridge.candidate_count++;
    bridge.candidates[slot] = light;
    bridge.candidate_distance[slot] = distance;
}
void game_gpu_building(GameApp *app, const CoreBuildingView *b, Rectangle picture) {
    if (!bridge.active)
        return;
    int w = bridge.config.map_width;
    int known = 0;
    for (int y = b->y; y < b->y + b->h; y++)
        for (int x = b->x; x < b->x + b->w; x++)
            if (x >= 0 && y >= 0 && x < w && y < bridge.config.map_height &&
                (!app->session.visibility || app->session.visibility[y * w + x]))
                known = 1;
    if (!known)
        return;
    for (int y = b->y; y < b->y + b->h; y++)
        for (int x = b->x; x < b->x + b->w; x++)
            if (x >= 0 && y >= 0 && x < w && y < bridge.config.map_height)
                if (bridge.terrain[y * w + x] != 128)
                    bridge.terrain[y * w + x] = 16;
    if (b->kind_flags & CORE_EXP_PLOT)
        return;
    uint32_t owner = 0x10000000u | (uint32_t)b->id;
    float front = (b->y + b->h) * 16.0f, cx = (b->x + b->w * .5f) * 16,
          depth = fminf(b->h * 16.0f, picture.height * .26f);
    float height = picture.height * .78f, wall = height * .62f, rx = fminf(picture.width * .43f, b->w * 8.0f),
          cy = front - depth * .5f;
    if (b->kind_flags & (CORE_EXP_WALL | CORE_EXP_GATE)) {
        height = picture.height * .65f;
        wall = height;
    }
    add_caster(owner, 0, 0, cx, cy, wall * .5f, rx, depth * .5f, wall * .5f, .94f, 1);
    if (height > wall + 1)
        add_caster(owner, 0, 2, cx, cy, wall, rx + 1.5f, depth * .5f + 1.5f, height - wall, .94f, 0);
    int chimney = b->state == CORE_BSTATE_ACTIVE &&
                  (b->type == CORE_BLD_HOUSE || b->type == CORE_BLD_COTTAGE || b->type == CORE_BLD_MANSION ||
                   b->type == CORE_BLD_KILN || b->type == CORE_BLD_SMELTER || b->type == CORE_BLD_TAVERN);
    if (chimney) {
        CastleGpuViewport viewport = castle_gpu_viewport(&bridge.config);
        float ex = picture.x + picture.width * (b->rotation == 2 || b->rotation == 3 ? .26f : .78f),
              ez = picture.height * .92f;
        float ey = picture.y + picture.height * .04f + ez * .9f;
        add_caster(owner, 0, 0, ex, ey, ez - 3.0f, picture.width * .035f, 2.2f, 3.0f, .94f, 0);
        if (picture.x + picture.width > bridge.config.origin[0] - 24 &&
            picture.x < bridge.config.origin[0] + viewport.width + 24 &&
            picture.y + picture.height > bridge.config.origin[1] - 64 &&
            picture.y < bridge.config.origin[1] + viewport.height + 64) {
            CastleGpuEmitter emitter = {{ex, ey, ez, 0}, {0}};
            emitter.settings[0] = (float)b->id;
            (void)castle_gpu_emitter(bridge.gpu, &emitter);
        }
    }
    if (b->state == CORE_BSTATE_ACTIVE &&
        (b->resident_slots > 0 || b->worker_slots > 0 || b->type == CORE_BLD_KEEP)) {
        CastleGpuLight light = {{cx, front + 2, height * .32f, 54}, {1.12f, .67f, .28f, 0}, {owner, 0, 0, 0}};
        float strength = fminf(1, fmaxf(0, (.55f - bridge.config.ambient) / .4f));
        light.color[3] = .88f * strength * (.96f + .04f * sinf(bridge.time * 2.3f + b->id * .73f));
        if (light.color[3] > .01f)
            candidate(light);
    }
}
void game_gpu_prop(GameApp *app, int cell, int type) {
    if (!bridge.active)
        return;
    GameSession *s = &app->session;
    if (cell < 0 || cell >= s->cells || (s->visibility && !s->visibility[cell]))
        return;
    int x = cell % s->summary.map_width, y = cell / s->summary.map_width;
    float size = type < 4 ? 34.0f : type < 8 ? 22.0f : 13.0f, cx = (x + .5f) * 16, foot = (y + .8f) * 16;
    if (type == 20) {
        CoreExpansionTileView *t = &s->exp_tiles[cell];
        int mature = core_expansion_tree_mature_age(t->tree_kind);
        size = t->tree_age < mature * 3 / 10 ? 12.0f
               : t->tree_age < mature        ? 25.0f
               : t->tree_kind == 2           ? 43.0f
                                             : 38.0f;
    }
    if (type == 21)
        size = 22;
    uint32_t owner = 0x20000000u | (uint32_t)(cell + 1);
    if (type == 20 || type < 4) {
        float height = size * .72f;
        add_caster(owner, 1, 1, cx, foot, height * .72f, size * .32f, size * .16f, height * .30f, .78f, 3);
        add_caster(owner, 0, 1, cx, foot, height * .25f, size * .045f, size * .045f, height * .25f, .86f, 0);
    } else if (type < 8 || type == 21)
        add_caster(owner, 0, 1, cx, foot, size * .20f, size * .33f, size * .17f, size * .20f, .90f, 1);
}
void game_gpu_actor(uint32_t owner, float x, float foot, float width, float height) {
    if (bridge.active)
        add_caster(owner, 1, 1, x, foot, height * .36f, width * .16f, width * .11f, height * .36f, .84f, 1);
}
int game_gpu_finish(void) {
    if (!bridge.active)
        return 0;
    EndBlendMode();
    EndTextureMode();
    bridge.active = 0;
    for (int k = 0; k < bridge.candidate_count; k++)
        (void)castle_gpu_light(bridge.gpu, &bridge.candidates[k]);
    castle_gpu_terrain(bridge.gpu, bridge.terrain, bridge.config.map_width, bridge.config.map_height);
    if (!castle_gpu_render(bridge.gpu)) {
        bridge.error = 1;
        return 0;
    }
    castle_gpu_present(bridge.gpu, (Rectangle){0, 0, (float)GetScreenWidth(), (float)GetScreenHeight()});
    overlay_present();
    return 1;
}
void game_gpu_report(void) {
    if (!bridge.gpu)
        return;
    CastleGpuStats s = castle_gpu_stats(bridge.gpu);
    printf("GAME GPU: software=%d frames=%llu fields_ms=%.3f geometry_ms=%.3f shadows_ms=%.3f "
           "lighting_ms=%.3f smoke_ms=%.3f textures=%zu buffers=%zu live=%u static_updates=%llu "
           "logical=%dx%d pixel_scale=%d\n",
           s.software, (unsigned long long)s.frames, s.fields_ms, s.geometry_ms, s.shadows_ms, s.lighting_ms,
           s.smoke_ms, s.texture_bytes, s.buffer_bytes, s.live_objects, (unsigned long long)s.static_updates,
           s.logical_width, s.logical_height, s.pixel_scale);
    printf("GAME MATERIALS: pairs=%d bytes=%zu baked=%llu calibrated=%llu estimated=%llu\n",
           bridge.material_count, bridge.material_bytes, (unsigned long long)bridge.baked_sprites,
           (unsigned long long)bridge.calibrated_sprites, (unsigned long long)bridge.estimated_sprites);
}
int game_gpu_validate(void) { return castle_gpu_validate(bridge.gpu); }
unsigned int game_gpu_live_resources(void) { return castle_gpu_live_objects(); }
int game_gpu_transition_checkpoint(int first) {
    if (!bridge.gpu || bridge.error)
        return 1;
    CastleGpuStats current = castle_gpu_stats(bridge.gpu);
    int error = !first && (current.live_objects != bridge.checkpoint.live_objects ||
                           current.texture_bytes != bridge.checkpoint.texture_bytes ||
                           current.buffer_bytes != bridge.checkpoint.buffer_bytes ||
                           bridge.terrain_capacity != bridge.checkpoint_terrain ||
                           bridge.material_count != bridge.checkpoint_material_count ||
                           bridge.material_bytes != bridge.checkpoint_material_bytes);
    bridge.checkpoint = current;
    bridge.checkpoint_terrain = bridge.terrain_capacity;
    bridge.checkpoint_material_count = bridge.material_count;
    bridge.checkpoint_material_bytes = bridge.material_bytes;
    printf("GPU TRANSITION: objects=%u textures=%zu buffers=%zu terrain=%zu pairs=%d material_bytes=%zu "
           "error=%d\n",
           current.live_objects, current.texture_bytes, current.buffer_bytes, bridge.terrain_capacity,
           bridge.material_count, bridge.material_bytes, error);
    return error;
}
