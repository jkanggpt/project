/* main.c — 실제 게임과 분리한 2D 노멀·높이·차폐 조명 실험실 */
#include "lighting_lab.h"
#include "storage.h"
#include "raymath.h"
#include "rlgl.h"
#if !defined(_WIN32)
#include <GL/gl.h>
#endif
#define GLFW_INCLUDE_NONE
#include "GLFW/glfw3.h"
#include "gbuffer.h"
#include "lighting.h"
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef CASTLE_LIGHTING_ROOT
#define CASTLE_LIGHTING_ROOT "."
#endif
#define BUFFER_W 512
#define BUFFER_H 384
#define WORLD_LEFT (-16.0f)
#define WORLD_TOP (-8.0f)
#define WORLD_W 256.0f
#define WORLD_H 192.0f

typedef struct LabObject {
    int kind, flip;
    Vector2 position;
} LabObject;
typedef struct Lab {
    Texture2D albedo, normals, surface;
    RenderTexture2D buffer[3], result;
    Shader geometry, lighting;
    Font font;
    Rectangle bounds[LAB_KIND_COUNT], viewport;
    LabObject objects[16];
    int object_count;
    int mode, sun, torch, interior, cursor, paused;
    float azimuth, elevation, ambient, exposure, time;
    Vector3 cursor_position;
    double frame_ms;
} Lab;
static const char *const mode_names[] = {u8"알베도", u8"노멀 조명", u8"그림자 포함",
                                         u8"노멀",   u8"높이",      u8"차폐"};
static char *shader_text(const char *const *lines) {
    size_t size = 1;
    for (int i = 0; lines[i]; i++)
        size += strlen(lines[i]);
    char *text = malloc(size);
    if (!text)
        return NULL;
    text[0] = 0;
    size_t at = 0;
    for (int i = 0; lines[i]; i++) {
        size_t n = strlen(lines[i]);
        memcpy(text + at, lines[i], n);
        at += n;
    }
    text[at] = 0;
    return text;
}
static Image image_file(const char *path) {
    int length = 0;
    unsigned char *bytes = game_read_bytes(path, &length);
    Image image = bytes ? LoadImageFromMemory(".png", bytes, length) : (Image){0};
    free(bytes);
    if (image.data)
        ImageFormat(&image, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    return image;
}
static Texture2D texture_file(const char *path) {
    Image image = image_file(path);
    Texture2D texture = image.data ? LoadTextureFromImage(image) : (Texture2D){0};
    if (image.data)
        UnloadImage(image);
    if (texture.id)
        SetTextureFilter(texture, TEXTURE_FILTER_POINT);
    return texture;
}
static int export_image(Image image, const char *path) {
    int length = 0;
    unsigned char *bytes = ExportImageToMemory(image, ".png", &length);
    int ok = bytes && game_write_bytes(path, bytes, (size_t)length);
    MemFree(bytes);
    return ok;
}
static int screenshot(const char *directory, const char *name, int scene_only, Lab *lab) {
    char path[GAME_PATH_BYTES];
    if (!game_make_dirs(directory) || !game_join_path(path, sizeof(path), directory, name))
        return 0;
    Image image = scene_only ? LoadImageFromTexture(lab->result.texture) : LoadImageFromScreen();
    if (!image.data)
        return 0;
    if (scene_only)
        ImageFlipVertical(&image);
    int ok = export_image(image, path);
    UnloadImage(image);
    return ok;
}
static Font korean_font(void) {
    const char *letters =
        u8"맵 · 조명 실험실 정사각 지면 높이 발밑 정렬 알베도 노멀 그림자 포함 차폐 낮 석양 밤 태양 방향 "
        u8"고도 토치 실내등 이동광 켜짐 꺼짐 인물 정지 재생 화살표 좌우 상하 드래그 월드 좌표 창문을 통과한 "
        u8"빛 벽 뒤 바닥 주의 프레임 소프트웨어 기준 구체 건물 나무 바위 상자 물 잔디 석재 원화 "
        u8"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz[]/.,:()-+%";
    int count = 0, unique_count = 0;
    int *points = LoadCodepoints(letters, &count), unique[512];
    for (int i = 0; i < count && unique_count < 512; i++) {
        int exists = 0;
        for (int j = 0; j < unique_count; j++)
            exists |= unique[j] == points[i];
        if (!exists && points[i] >= 32)
            unique[unique_count++] = points[i];
    }
    UnloadCodepoints(points);
    char path[GAME_PATH_BYTES];
    int length = 0;
    (void)snprintf(path, sizeof(path), "%s/game/assets/fonts/Galmuri11.ttf", CASTLE_LIGHTING_ROOT);
    unsigned char *bytes = game_read_bytes(path, &length);
    Font font = bytes ? LoadFontFromMemory(".ttf", bytes, length, 20, unique, unique_count) : (Font){0};
    free(bytes);
    if (font.texture.id)
        SetTextureFilter(font.texture, TEXTURE_FILTER_POINT);
    return font;
}
static int load_lab(Lab *lab) {
    char path[GAME_PATH_BYTES];
    (void)snprintf(path, sizeof(path), "%s/game/assets/lighting_lab/lighting_albedo.png",
                   CASTLE_LIGHTING_ROOT);
    Image albedo = image_file(path);
    if (!albedo.data)
        return 0;
    for (int i = 0; i < LAB_KIND_COUNT; i++) {
        Image cell =
            ImageFromImage(albedo, (Rectangle){(i % 4) * LAB_CELL, (i / 4) * LAB_CELL, LAB_CELL, LAB_CELL});
        lab->bounds[i] = GetImageAlphaBorder(cell, .5f);
        UnloadImage(cell);
    }
    lab->albedo = LoadTextureFromImage(albedo);
    UnloadImage(albedo);
    SetTextureFilter(lab->albedo, TEXTURE_FILTER_POINT);
    (void)snprintf(path, sizeof(path), "%s/game/assets/lighting_lab/lighting_normals.png",
                   CASTLE_LIGHTING_ROOT);
    lab->normals = texture_file(path);
    (void)snprintf(path, sizeof(path), "%s/game/assets/lighting_lab/lighting_surface.png",
                   CASTLE_LIGHTING_ROOT);
    lab->surface = texture_file(path);
    char *g = shader_text(LAB_GBUFFER_LINES), *l = shader_text(LAB_LIGHTING_LINES);
    if (!g || !l) {
        free(g);
        free(l);
        return 0;
    }
    lab->geometry = LoadShaderFromMemory(NULL, g);
    lab->lighting = LoadShaderFromMemory(NULL, l);
    free(g);
    free(l);
    if (!lab->geometry.id || !lab->lighting.id || lab->geometry.id == rlGetShaderIdDefault() ||
        lab->lighting.id == rlGetShaderIdDefault())
        return 0;
    for (int i = 0; i < 3; i++) {
        lab->buffer[i] = LoadRenderTexture(BUFFER_W, BUFFER_H);
        if (!lab->buffer[i].id)
            return 0;
        SetTextureFilter(lab->buffer[i].texture, TEXTURE_FILTER_POINT);
    }
    lab->result = LoadRenderTexture(BUFFER_W, BUFFER_H);
    if (!lab->result.id)
        return 0;
    SetTextureFilter(lab->result.texture, TEXTURE_FILTER_POINT);
    lab->font = korean_font();
    return lab->albedo.id && lab->normals.id && lab->surface.id && lab->font.texture.id && lab->result.id;
}
static void free_lab(Lab *lab) {
    if (lab->font.texture.id)
        UnloadFont(lab->font);
    if (lab->albedo.id)
        UnloadTexture(lab->albedo);
    if (lab->normals.id)
        UnloadTexture(lab->normals);
    if (lab->surface.id)
        UnloadTexture(lab->surface);
    for (int i = 0; i < 3; i++)
        if (lab->buffer[i].id)
            UnloadRenderTexture(lab->buffer[i]);
    if (lab->result.id)
        UnloadRenderTexture(lab->result);
    if (lab->geometry.id && lab->geometry.id != rlGetShaderIdDefault())
        UnloadShader(lab->geometry);
    if (lab->lighting.id && lab->lighting.id != rlGetShaderIdDefault())
        UnloadShader(lab->lighting);
}
static void preset(Lab *lab, int which) {
    lab->mode = 2;
    lab->torch = 1;
    lab->interior = 1;
    lab->cursor = 0;
    lab->sun = which != 2;
    lab->azimuth = which == 1 ? 215 : 135;
    lab->elevation = which == 1 ? 16 : 55;
    lab->ambient = which == 2 ? .025f : which == 1 ? .08f : .24f;
    lab->exposure = 1.1f;
}
static int compare_objects(const void *left, const void *right) {
    const LabObject *a = left, *b = right;
    return (a->position.y > b->position.y) - (a->position.y < b->position.y);
}
static void scene(Lab *lab) {
    float t = lab->time;
    int walk[] = {LAB_NPC, LAB_NPC_STEP_A, LAB_NPC_STEP_B, LAB_NPC_STEP_C};
    LabObject objects[] = {{LAB_HOUSE, 0, {64, 72}},
                           {LAB_OPEN_HOUSE, 0, {168, 72}},
                           {LAB_TREE, 0, {24, 120}},
                           {LAB_ROCK, 0, {144, 132}},
                           {LAB_CRATE, 0, {90, 124}},
                           {LAB_TORCH, 0, {112, 124}},
                           {LAB_WALL, 0, {168, 148}},
                           {LAB_BALL, 0, {212, 132}},
                           {walk[(int)(t * 5) % 4], 0, {128 + sinf(t * .45f) * 24, 110}},
                           {LAB_NPC, 1, {40, 148}}};
    lab->object_count = (int)(sizeof(objects) / sizeof(objects[0]));
    memcpy(lab->objects, objects, sizeof(objects));
    qsort(lab->objects, (size_t)lab->object_count, sizeof(lab->objects[0]), compare_objects);
}
static void maps(Lab *lab) {
    SetShaderValueTexture(lab->geometry, GetShaderLocation(lab->geometry, "normalAtlas"), lab->normals);
    SetShaderValueTexture(lab->geometry, GetShaderLocation(lab->geometry, "surfaceAtlas"), lab->surface);
}
static void draw_sprite(Lab *lab, int kind, Vector2 pos, int flip) {
    Rectangle bounds = lab->bounds[kind];
    Rectangle src = {kind % 4 * LAB_CELL + bounds.x, kind / 4 * LAB_CELL + bounds.y, bounds.width,
                     bounds.height};
    Rectangle dst = {pos.x + (flip ? LAB_ANCHOR_X - (bounds.x + bounds.width) / LAB_SCALE
                                   : bounds.x / LAB_SCALE - LAB_ANCHOR_X),
                     pos.y + bounds.y / LAB_SCALE - LAB_ANCHOR_Y, bounds.width / LAB_SCALE,
                     bounds.height / LAB_SCALE};
    if (flip)
        src.width = -src.width;
    DrawTexturePro(lab->albedo, src, dst, (Vector2){0}, 0, WHITE);
}
static void geometry_buffers(Lab *lab) {
    Camera2D camera = {{-WORLD_LEFT * 2, -WORLD_TOP * 2}, {0, 0}, 0, 2};
    for (int pass = 0; pass < 3; pass++) {
        BeginTextureMode(lab->buffer[pass]);
        ClearBackground(BLANK);
        BeginMode2D(camera);
        BeginShaderMode(lab->geometry);
        rlDisableColorBlend();
        SetShaderValue(lab->geometry, GetShaderLocation(lab->geometry, "bufferPass"), &pass,
                       SHADER_UNIFORM_INT);
        float flip = 1;
        SetShaderValue(lab->geometry, GetShaderLocation(lab->geometry, "normalFlip"), &flip,
                       SHADER_UNIFORM_FLOAT);
        maps(lab);
        for (int y = 0; y < 11; y++)
            for (int x = 0; x < 14; x++) {
                int kind = y == 6 || (x == 3 && y > 3 && y < 7) || (x == 10 && y > 3 && y < 7) ? LAB_STONE
                                                                                               : LAB_GRASS;
                if (x > 10 && y < 3)
                    kind = LAB_WATER;
                if (x < 3 && y > 8)
                    kind = LAB_DIRT;
                draw_sprite(lab, kind, (Vector2){x * 16 + 8, y * 16 + 8}, 0);
            }
        for (int i = 0; i < lab->object_count; i++) {
            LabObject *object = &lab->objects[i];
            float next_flip = object->flip ? -1.0f : 1.0f;
            if (next_flip != flip) {
                rlDrawRenderBatchActive();
                flip = next_flip;
                SetShaderValue(lab->geometry, GetShaderLocation(lab->geometry, "normalFlip"), &flip,
                               SHADER_UNIFORM_FLOAT);
                maps(lab);
            }
            draw_sprite(lab, object->kind, object->position, object->flip);
        }
        rlDrawRenderBatchActive();
        rlEnableColorBlend();
        EndShaderMode();
        EndMode2D();
        EndTextureMode();
    }
}
static void uniform_float(Shader shader, const char *name, float value) {
    SetShaderValue(shader, GetShaderLocation(shader, name), &value, SHADER_UNIFORM_FLOAT);
}
static void uniform_vec3(Shader shader, const char *name, Vector3 value) {
    SetShaderValue(shader, GetShaderLocation(shader, name), &value, SHADER_UNIFORM_VEC3);
}
static int scene_occluders(Lab *lab, float *centers, float *extents) {
    int count = 0;
    for (int i = 0; i < lab->object_count; i++) {
        LabPrimitive model[LAB_MAX_MODEL];
        LabObject *object = &lab->objects[i];
        int parts = lab_model(object->kind, model);
        for (int j = 0; j < parts; j++)
            if (model[j].occluder) {
                if (count == LAB_MAX_OCCLUDERS) {
                    fprintf(stderr, "Too many lighting occluders\n");
                    return -1;
                }
                LabPrimitive *p = &model[j];
                centers[count * 4] = object->position.x + (object->flip ? -p->center.x : p->center.x);
                centers[count * 4 + 1] = object->position.y + p->center.y;
                centers[count * 4 + 2] = p->center.z;
                centers[count * 4 + 3] = (float)p->shape;
                extents[count * 4] = p->extent.x;
                extents[count * 4 + 1] = p->extent.y;
                extents[count * 4 + 2] = p->extent.z;
                count++;
            }
    }
    return count;
}
static int lighting_pass(Lab *lab) {
    float centers[LAB_MAX_OCCLUDERS * 4] = {0}, extents[LAB_MAX_OCCLUDERS * 4] = {0};
    int count = scene_occluders(lab, centers, extents);
    if (count < 0)
        return 0;
    float az = lab->azimuth * DEG2RAD, elevation = lab->elevation * DEG2RAD;
    Vector3 sun = {cosf(az) * cosf(elevation), sinf(az) * cosf(elevation), sinf(elevation)};
    Vector3 sun_color =
        lab->sun ? (lab->elevation < 25 ? (Vector3){2.3f, 1.1f, .45f} : (Vector3){1.65f, 1.55f, 1.35f})
                 : (Vector3){0};
    Vector3 ambient = {lab->ambient * .68f, lab->ambient * .82f, lab->ambient};
    float positions[16] = {112,
                           124,
                           24,
                           100,
                           49.5f,
                           73,
                           18,
                           92,
                           168,
                           72,
                           20,
                           84,
                           lab->cursor_position.x,
                           lab->cursor_position.y,
                           lab->cursor_position.z,
                           90};
    float colors[16] = {2.1f, .82f,  .22f, lab->torch ? 3.2f + .15f * sinf(lab->time * 13) : 0,
                        2.1f, 1.05f, .40f, lab->interior ? 2.4f : 0,
                        1.8f, .85f,  .25f, lab->interior ? 2.2f : 0,
                        .48f, 1.1f,  2.0f, lab->cursor ? 3.0f : 0};
    Shader shader = lab->lighting;
    Vector2 origin = {WORLD_LEFT, WORLD_TOP}, size = {WORLD_W, WORLD_H};
    BeginTextureMode(lab->result);
    ClearBackground(BLACK);
    BeginShaderMode(shader);
    SetShaderValueTexture(shader, GetShaderLocation(shader, "normalBuffer"), lab->buffer[1].texture);
    SetShaderValueTexture(shader, GetShaderLocation(shader, "surfaceBuffer"), lab->buffer[2].texture);
    SetShaderValue(shader, GetShaderLocation(shader, "worldOrigin"), &origin, SHADER_UNIFORM_VEC2);
    SetShaderValue(shader, GetShaderLocation(shader, "worldSize"), &size, SHADER_UNIFORM_VEC2);
    SetShaderValue(shader, GetShaderLocation(shader, "displayMode"), &lab->mode, SHADER_UNIFORM_INT);
    SetShaderValue(shader, GetShaderLocation(shader, "occluderCount"), &count, SHADER_UNIFORM_INT);
    SetShaderValueV(shader, GetShaderLocation(shader, "occluderCenter[0]"), centers, SHADER_UNIFORM_VEC4,
                    count);
    SetShaderValueV(shader, GetShaderLocation(shader, "occluderExtent[0]"), extents, SHADER_UNIFORM_VEC4,
                    count);
    SetShaderValueV(shader, GetShaderLocation(shader, "lightPosition[0]"), positions, SHADER_UNIFORM_VEC4, 4);
    SetShaderValueV(shader, GetShaderLocation(shader, "lightColor[0]"), colors, SHADER_UNIFORM_VEC4, 4);
    uniform_vec3(shader, "sunDirection", sun);
    uniform_vec3(shader, "sunColor", sun_color);
    uniform_vec3(shader, "ambientColor", ambient);
    uniform_float(shader, "exposure", lab->exposure);
    uniform_float(shader, "torchEnabled", (float)lab->torch);
    uniform_float(shader, "interiorEnabled", (float)lab->interior);
    uniform_float(shader, "clockTime", lab->time);
    DrawTexturePro(lab->buffer[0].texture, (Rectangle){0, 0, BUFFER_W, -BUFFER_H},
                   (Rectangle){0, 0, BUFFER_W, BUFFER_H}, (Vector2){0}, 0, WHITE);
    EndShaderMode();
    EndTextureMode();
    return 1;
}
static void text(Lab *lab, float x, float y, Color color, const char *s) {
    DrawTextEx(lab->font, s, (Vector2){x, y}, 20, 0, color);
}
static int button(Lab *lab, Rectangle r, const char *label, int selected) {
    int hover = CheckCollisionPointRec(GetMousePosition(), r);
    DrawRectangleRec(r, selected ? (Color){113, 81, 43, 255}
                        : hover  ? (Color){63, 65, 75, 255}
                                 : (Color){34, 40, 49, 255});
    DrawRectangleLinesEx(r, 1, selected ? GOLD : (Color){83, 89, 98, 255});
    text(lab, r.x + 10, r.y + 8, RAYWHITE, label);
    return hover && IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
}
static void interface(Lab *lab, int input) {
    float w = (float)GetScreenWidth(), h = (float)GetScreenHeight(), side = 300;
    float available_w = w - side - 42, available_h = h - 70;
    float scale = fminf(available_w / WORLD_W, available_h / WORLD_H);
    lab->viewport = (Rectangle){18 + (available_w - WORLD_W * scale) / 2,
                                42 + (available_h - WORLD_H * scale) / 2, WORLD_W * scale, WORLD_H * scale};
    Rectangle v = lab->viewport;
    DrawTexturePro(lab->result.texture, (Rectangle){0, 0, BUFFER_W, -BUFFER_H}, v, (Vector2){0}, 0, WHITE);
    DrawRectangleLinesEx(v, 1, (Color){94, 101, 113, 255});
    text(lab, 18, 12, GOLD, u8"정사각 지면 · 높이 · 발밑 정렬");
    float x = w - side - 6, y = 18;
    text(lab, x, y, GOLD, u8"조명 실험실");
    y += 36;
    for (int i = 0; i < 6; i++) {
        char label[80];
        snprintf(label, sizeof(label), "[%d] %s", i + 1, mode_names[i]);
        if (button(lab, (Rectangle){x, y, side - 14, 34}, label, lab->mode == i) && input)
            lab->mode = i;
        y += 39;
    }
    const char *names[3] = {u8"낮", u8"석양", u8"밤"};
    for (int i = 0; i < 3; i++)
        if (button(lab, (Rectangle){x + i * 93, y, 86, 36}, names[i], 0) && input)
            preset(lab, i);
    y += 47;
    char label[180];
    snprintf(label, sizeof(label), u8"태양 방향 %.0f / 고도 %.0f", lab->azimuth, lab->elevation);
    text(lab, x, y, RAYWHITE, label);
    y += 29;
    text(lab, x, y, GRAY, u8"화살표: 좌우 방향 / 상하 고도");
    y += 35;
    int *flags[4] = {&lab->sun, &lab->torch, &lab->interior, &lab->cursor};
    const char *flag_names[4] = {u8"[S] 태양", u8"[T] 토치", u8"[I] 실내등", u8"[M] 이동광"};
    for (int i = 0; i < 4; i++) {
        snprintf(label, sizeof(label), "%s : %s", flag_names[i], *flags[i] ? u8"켜짐" : u8"꺼짐");
        if (button(lab, (Rectangle){x, y, side - 14, 34}, label, *flags[i]) && input)
            *flags[i] = !*flags[i];
        y += 39;
    }
    text(lab, x, y, GRAY, u8"맵 드래그: 이동광 월드 좌표");
    y += 28;
    text(lab, x, y, GRAY, u8"[P] 인물 정지 / 재생");
    y += 31;
    snprintf(label, sizeof(label), u8"프레임 %.1f ms", lab->frame_ms);
    text(lab, x, y, GRAY, label);
}
static void input(Lab *lab, float dt) {
    for (int i = 0; i < 6; i++)
        if (IsKeyPressed(KEY_ONE + i))
            lab->mode = i;
    if (IsKeyPressed(KEY_S))
        lab->sun = !lab->sun;
    if (IsKeyPressed(KEY_T))
        lab->torch = !lab->torch;
    if (IsKeyPressed(KEY_I))
        lab->interior = !lab->interior;
    if (IsKeyPressed(KEY_M))
        lab->cursor = !lab->cursor;
    if (IsKeyPressed(KEY_P))
        lab->paused = !lab->paused;
    if (IsKeyPressed(KEY_N))
        preset(lab, lab->sun ? 2 : 0);
    if (IsKeyDown(KEY_LEFT))
        lab->azimuth -= dt * 60;
    if (IsKeyDown(KEY_RIGHT))
        lab->azimuth += dt * 60;
    if (IsKeyDown(KEY_UP))
        lab->elevation = fminf(85, lab->elevation + dt * 35);
    if (IsKeyDown(KEY_DOWN))
        lab->elevation = fmaxf(5, lab->elevation - dt * 35);
    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(GetMousePosition(), lab->viewport)) {
        Rectangle v = lab->viewport;
        lab->cursor_position = (Vector3){WORLD_LEFT + (GetMouseX() - v.x) / v.width * WORLD_W,
                                         WORLD_TOP + (GetMouseY() - v.y) / v.height * WORLD_H + 24, 24};
        lab->cursor = 1;
    }
}
static int render(Lab *lab, int handle_input) {
    double start = GetTime();
    scene(lab);
    geometry_buffers(lab);
    if (!lighting_pass(lab))
        return 0;
    BeginDrawing();
    ClearBackground((Color){16, 22, 30, 255});
    interface(lab, handle_input);
    EndDrawing();
    lab->frame_ms = (GetTime() - start) * 1000;
    return 1;
}
static int vector_text(char *text, size_t capacity, const float *values, int count) {
    size_t at = 0;
    text[0] = 0;
    for (int i = 0; i < count; i++) {
        int n = snprintf(text + at, capacity - at, "%s%.6f", i ? "," : "", values[i]);
        if (n < 0 || (size_t)n >= capacity - at)
            return 0;
        at += (size_t)n;
    }
    return 1;
}
static int export_preview(Lab *lab, const char *directory) {
    if (!game_make_dirs(directory))
        return 1;
    scene(lab);
    geometry_buffers(lab);
    Image images[3] = {{0}};
    int errors = 0;
    const char *names[] = {"albedo.png", "normals.png", "surface.png"};
    for (int i = 0; i < 3; i++) {
        images[i] = LoadImageFromTexture(lab->buffer[i].texture);
        if (!images[i].data) {
            errors++;
            continue;
        }
        ImageFlipVertical(&images[i]);
    }
    /* 색 공간·알파 보정이 데이터 채널을 바꾸지 않게 거칠기를 별도 불투명 PNG로 보냅니다. */
    Image roughness = GenImageColor(BUFFER_W, BUFFER_H, WHITE);
    if (!roughness.data)
        errors++;
    if (!errors) {
        for (int i = 0; i < BUFFER_W * BUFFER_H; i++) {
            Color *surface = &((Color *)images[2].data)[i];
            ((Color *)roughness.data)[i] = (Color){surface->a, surface->a, surface->a, 255};
            surface->a = 255;
        }
        char path[GAME_PATH_BYTES];
        for (int i = 0; i < 3; i++)
            errors +=
                !(game_join_path(path, sizeof(path), directory, names[i]) && export_image(images[i], path));
        errors += !(game_join_path(path, sizeof(path), directory, "roughness.png") &&
                    export_image(roughness, path));
    }
    for (int i = 0; i < 3; i++)
        if (images[i].data)
            UnloadImage(images[i]);
    if (roughness.data)
        UnloadImage(roughness);
    float centers[LAB_MAX_OCCLUDERS * 4] = {0}, extents[LAB_MAX_OCCLUDERS * 4] = {0};
    int count = scene_occluders(lab, centers, extents);
    char center_text[6400], extent_text[6400], json[14000], path[GAME_PATH_BYTES];
    if (count < 0 || !vector_text(center_text, sizeof(center_text), centers, count * 4) ||
        !vector_text(extent_text, sizeof(extent_text), extents, count * 4))
        return 1;
    int length = snprintf(json, sizeof(json),
                          "{\"width\":%d,\"height\":%d,\"origin\":[%.1f,%.1f],\"size\":[%.1f,%.1f],"
                          "\"count\":%d,\"centers\":[%s],\"extents\":[%s]}\n",
                          BUFFER_W, BUFFER_H, (double)WORLD_LEFT, (double)WORLD_TOP, (double)WORLD_W,
                          (double)WORLD_H, count, center_text, extent_text);
    if (length < 0 || (size_t)length >= sizeof(json))
        return 1;
    errors += !(game_join_path(path, sizeof(path), directory, "scene.json") &&
                game_write_bytes(path, json, (size_t)length));
    printf("LIGHTING PREVIEW: occluders=%d errors=%d\n", count, errors);
    return errors ? 1 : 0;
}
static float pixel_light(Lab *lab, float world_x, float screen_y) {
    Image image = LoadImageFromTexture(lab->result.texture);
    ImageFlipVertical(&image);
    int x = (int)((world_x - WORLD_LEFT) / WORLD_W * BUFFER_W),
        y = (int)((screen_y - WORLD_TOP) / WORLD_H * BUFFER_H);
    Color c = GetImageColor(image, x, y);
    UnloadImage(image);
    return (.2126f * c.r + .7152f * c.g + .0722f * c.b) / 255;
}
static int verify_render(Lab *lab) {
    int errors = 0;
    preset(lab, 2);
    lab->time = 0;
    lab->interior = 0;
    lab->ambient = .015f;
    lab->mode = 1;
    if (!render(lab, 0))
        return 1;
    float no_shadow = pixel_light(lab, 160, 156);
    lab->mode = 2;
    if (!render(lab, 0))
        return 1;
    float shadow = pixel_light(lab, 160, 156);
    if (!(no_shadow > shadow + .025f))
        errors++;
    printf("GPU shadow: unoccluded=%.4f blocked=%.4f\n", no_shadow, shadow);
    lab->torch = 0;
    lab->interior = 1;
    lab->mode = 1;
    if (!render(lab, 0))
        return 1;
    float wall_no_shadow = pixel_light(lab, 36, 112);
    lab->mode = 2;
    if (!render(lab, 0))
        return 1;
    float wall = pixel_light(lab, 36, 112), window = pixel_light(lab, 49.5f, 116);
    if (!(wall_no_shadow > wall + .015f && window > wall + .015f))
        errors++;
    printf("GPU window: wall_without_occlusion=%.4f wall=%.4f opening=%.4f\n", wall_no_shadow, wall, window);
    lab->mode = 1;
    lab->interior = 0;
    lab->sun = 1;
    lab->elevation = 15;
    lab->azimuth = 0;
    if (!render(lab, 0))
        return 1;
    float east = pixel_light(lab, 216, 124);
    lab->azimuth = 180;
    if (!render(lab, 0))
        return 1;
    float west = pixel_light(lab, 216, 124);
    if (!(east > west + .04f))
        errors++;
    printf("GPU normal direction: east=%.4f west=%.4f\n", east, west);
    lab->elevation = 80;
    if (!render(lab, 0))
        return 1;
    float high_sun = pixel_light(lab, 112, 160);
    lab->elevation = 5;
    if (!render(lab, 0))
        return 1;
    float low_sun = pixel_light(lab, 112, 160);
    if (!(high_sun > low_sun + .08f))
        errors++;
    printf("GPU floor +Z: high_sun=%.4f low_sun=%.4f errors=%d\n", high_sun, low_sun, errors);
    lab->azimuth = 0;
    lab->elevation = 75;
    lab->mode = 1;
    if (!render(lab, 0))
        return 1;
    float high_clear = pixel_light(lab, 128, 148);
    lab->mode = 2;
    if (!render(lab, 0))
        return 1;
    float high_shadow = pixel_light(lab, 128, 148);
    lab->elevation = 10;
    lab->mode = 1;
    if (!render(lab, 0))
        return 1;
    float low_clear = pixel_light(lab, 128, 148);
    lab->mode = 2;
    if (!render(lab, 0))
        return 1;
    float low_shadow = pixel_light(lab, 128, 148);
    if (!(high_shadow >= high_clear - .02f && low_clear > low_shadow + .02f))
        errors++;
    printf("GPU shadow reach: high_clear=%.4f high_shadow=%.4f low_clear=%.4f low_shadow=%.4f errors=%d\n",
           high_clear, high_shadow, low_clear, low_shadow, errors);
    return errors ? 1 : 0;
}
static int check_assets(void) {
    const char *names[3] = {"lighting_albedo.png", "lighting_normals.png", "lighting_surface.png"};
    Image images[3] = {{0}};
    int errors = 0;
    unsigned counts[LAB_KIND_COUNT] = {0};
    for (int i = 0; i < 3; i++) {
        char path[GAME_PATH_BYTES];
        snprintf(path, sizeof(path), "%s/game/assets/lighting_lab/%s", CASTLE_LIGHTING_ROOT, names[i]);
        images[i] = image_file(path);
        if (!images[i].data || images[i].width != LAB_ATLAS || images[i].height != LAB_ATLAS)
            errors++;
    }
    if (!errors)
        for (int y = 0; y < LAB_ATLAS; y++)
            for (int x = 0; x < LAB_ATLAS; x++) {
                int i = y * LAB_ATLAS + x;
                Color a = ((Color *)images[0].data)[i], n = ((Color *)images[1].data)[i];
                if ((a.a >= 128) != (n.a >= 128)) {
                    errors++;
                    continue;
                }
                if (a.a < 128)
                    continue;
                counts[y / LAB_CELL * 4 + x / LAB_CELL]++;
                Vector3 normal = {n.r / 127.5f - 1, n.g / 127.5f - 1, n.b / 127.5f - 1};
                if (fabsf(Vector3Length(normal) - 1) > .015f)
                    errors++;
                Color s = ((Color *)images[2].data)[i];
                if (s.r > 64)
                    errors++;
                if (y / LAB_CELL == 0 && x / LAB_CELL < 3 && normal.z < .85f)
                    errors++;
            }
    for (int i = 0; i < LAB_KIND_COUNT; i++)
        if (counts[i] < 10)
            errors++;
    for (int i = 0; i < 3; i++)
        if (images[i].data)
            UnloadImage(images[i]);
    printf("LIGHTING MAPS: types=%d aligned=1 normalized=1 errors=%d\n", LAB_KIND_COUNT, errors);
    return errors ? 1 : 0;
}
static int number(const char *value, int *out) {
    char *end;
    errno = 0;
    long parsed = strtol(value, &end, 10);
    if (!*value || *end || errno || parsed < 0 || parsed > 1000000)
        return 0;
    *out = (int)parsed;
    return 1;
}
int main(int argc, char **argv) {
    if (argc > 1 && !strcmp(argv[1], "--check-geometry"))
        return lab_geometry_check();
    if (argc > 1 && !strcmp(argv[1], "--check-assets"))
        return check_assets();
    if (argc == 4 && !strcmp(argv[1], "--bake")) {
        if (!game_make_dirs(argv[3]))
            return 1;
        return art_lighting_bake(argv[2], argv[3]);
    }
    const char *capture = NULL, *preview = NULL;
    int verify = 0, frames = 0, width = 1280, height = 800, initial_preset = 2;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--verify-render"))
            verify = 1;
        else if (i + 1 < argc && !strcmp(argv[i], "--capture-dir"))
            capture = argv[++i];
        else if (i + 1 < argc && !strcmp(argv[i], "--export-preview"))
            preview = argv[++i];
        else if (i + 1 < argc && !strcmp(argv[i], "--frames")) {
            if (!number(argv[++i], &frames))
                return 2;
        } else if (i + 1 < argc && !strcmp(argv[i], "--width")) {
            if (!number(argv[++i], &width))
                return 2;
        } else if (i + 1 < argc && !strcmp(argv[i], "--height")) {
            if (!number(argv[++i], &height))
                return 2;
        } else if (i + 1 < argc && !strcmp(argv[i], "--preset")) {
            const char *name = argv[++i];
            if (!strcmp(name, "day"))
                initial_preset = 0;
            else if (!strcmp(name, "sunset"))
                initial_preset = 1;
            else if (!strcmp(name, "night"))
                initial_preset = 2;
            else
                return 2;
        } else {
            fprintf(stderr, "Usage: castle_lighting_lab [--capture-dir DIR] [--export-preview DIR] "
                            "[--verify-render] [--frames N] "
                            "[--preset day|sunset|night] [--width N --height N]\n");
            return 2;
        }
    }
    if (width < 1000 || height < 720 || width > 3840 || height > 2160 || frames < 0)
        return 2;
    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    /* raylib 초기화 전에 디스플레이 연결 실패를 정상 종료로 처리합니다. */
    if (!glfwInit()) {
        fprintf(stderr, "Lighting test could not connect to a graphical display\n");
        return 1;
    }
    glfwTerminate();
    InitWindow(width, height, u8"마법성채 조명 실험실");
    if (!IsWindowReady()) {
        fprintf(stderr, "Lighting test could not open a graphical display\n");
        return 1;
    }
#if !defined(_WIN32)
    printf("LIGHTING GPU: vendor=%s renderer=%s version=%s\n", (const char *)glGetString(GL_VENDOR),
           (const char *)glGetString(GL_RENDERER), (const char *)glGetString(GL_VERSION));
#endif
    SetWindowMinSize(1000, 720);
    SetTargetFPS(capture || preview || verify || frames ? 0 : 60);
    Lab lab = {0};
    int result = 0;
    if (!IsWindowReady() || !load_lab(&lab)) {
        fprintf(stderr, "Lighting resources or GLSL 120 shader failed\n");
        free_lab(&lab);
        if (IsWindowReady())
            CloseWindow();
        return 1;
    }
    preset(&lab, initial_preset);
    lab.cursor_position = (Vector3){112, 48, 24};
    if (preview)
        result = export_preview(&lab, preview);
    else if (verify)
        result = verify_render(&lab);
    else if (capture) {
        const char *names[] = {"albedo.png",  "day.png",    "day-normal-only.png",
                               "sunset.png",  "night.png",  "night-normal-only.png",
                               "normals.png", "height.png", "occlusion.png"};
        for (int i = 0; i < 9; i++) {
            preset(&lab, i == 0 || i == 1 || i == 2 ? 0 : i == 3 ? 1 : 2);
            if (i == 0)
                lab.mode = 0;
            if (i == 2 || i == 5)
                lab.mode = 1;
            if (i >= 6)
                lab.mode = i - 3;
            lab.time = 0;
            for (int f = 0; f < 2; f++)
                if (!render(&lab, 0))
                    result = 1;
            if (!screenshot(capture, names[i], 0, &lab))
                result = 1;
        }
    } else {
        int frame = 0, measured = 0;
        double total_ms = 0, maximum_ms = 0;
        while (!WindowShouldClose() && (!frames || frame < frames)) {
            float dt = fminf(GetFrameTime(), .1f);
            input(&lab, dt);
            if (!lab.paused)
                lab.time += dt;
            if (!render(&lab, 1)) {
                result = 1;
                break;
            }
            /* 초기 셰이더 준비 프레임을 빼고 표시까지의 벽시계 시간을 기록합니다. */
            if (frame >= 10) {
                measured++;
                total_ms += lab.frame_ms;
                if (lab.frame_ms > maximum_ms)
                    maximum_ms = lab.frame_ms;
            }
            frame++;
        }
        printf("LIGHTING FRAMES: frames=%d measured=%d mean_ms=%.2f max_ms=%.2f last_ms=%.2f\n", frame,
               measured, measured ? total_ms / measured : 0, maximum_ms, lab.frame_ms);
    }
    free_lab(&lab);
    CloseWindow();
    return result;
}
