/* 소프트웨어 OpenGL도 기능 검사는 가능하지만 하드웨어 성능으로 기록하지 않습니다. */
#include "gpu.h"
#include "external/glad.h"
#include "rlgl.h"
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "common_glsl.h"
#include "shadows_glsl.h"
#include "fields_cs.h"
#include "geometry_vs.h"
#include "geometry_fs.h"
#include "grass_cs.h"
#include "particles_cs.h"
#include "smoke_vs.h"
#include "smoke_fs.h"
#include "bins_cs.h"
#include "cache_cs.h"
#include "lighting_cs.h"
#include "terrain_glsl.h"
#include "terrain_cs.h"
#include "terrain_vs.h"
#include "terrain_fs.h"
#include "meadow_cs.h"
#include "smoke_composite_cs.h"
#include "contacts_cs.h"
#include "light_bins_cs.h"
#include "fog_cs.h"

#define GPU_GRASS_LIMIT 16384
#define GPU_PARTICLES 2048
#define GPU_TILE_STRIDE 130
#define GPU_CHUNK 128
#define GPU_TIMER_COUNT 5
#define GPU_TIMER_FRAMES 4

#ifdef _WIN32
/* 전환형 노트북에서는 기본적으로 고성능 GPU를 요청하며 실제 선택은 로그로 확인합니다. */
__declspec(dllexport) unsigned long NvOptimusEnablement = 1;
__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
#endif

typedef struct GpuBatch {
    Texture2D texture, normal, surface;
    int first, count;
} GpuBatch;
typedef struct GpuBuffer {
    unsigned int id;
    size_t bytes;
} GpuBuffer;
typedef struct GpuProgram {
    unsigned int id;
} GpuProgram;
typedef struct GpuContactEntry {
    uint64_t key, used;
    unsigned int revision;
} GpuContactEntry;
typedef struct GpuOwnerEntry {
    uint32_t owner;
    int sprite;
} GpuOwnerEntry;
struct CastleGpu {
    CastleGpuFrame frame;
    CastleGpuStats stats;
    CastleGpuSprite *sprites;
    CastleGpuCaster *casters;
    CastleGpuLight *lights;
    CastleGpuEmitter *emitters;
    GpuBatch *batches;
    int sprite_count, caster_count, emitter_count, batch_count, light_count;
    int sprite_capacity, caster_capacity, emitter_capacity, batch_capacity, light_capacity;
    GpuBuffer sprite_buffer, caster_buffer, light_buffer, emitter_buffer, terrain_buffer;
    GpuBuffer grass_buffer, command_buffer, particle_buffer, tile_buffer;
    GpuBuffer page_buffer, light_tiles, static_tiles, fog_buffer;
    unsigned int *fog_words;
    size_t fog_capacity;
    Texture2D fog_layer;
    GpuBuffer grass_counts, grass_offsets, grass_sums, grass_group_offsets;
    GpuBuffer contact_columns;
    unsigned char *terrain;
    unsigned int *terrain_words;
    size_t terrain_bytes;
    Texture2D gbuffer[4], output, depth, wind, waves, sun_cache, point_cache, white;
    Texture2D scenery;
    Texture2D contact_policy, contact_atlas, contact_front;
    GpuContactEntry contact_entries[512];
    GpuOwnerEntry *owner_table;
    int owner_capacity;
    unsigned int contact_revision;
    float grass_density;
    uint32_t seed_value;
    int seed_valid;
    Texture2D smoke_layer;
    unsigned int smoke_framebuffer;
    Texture2D terrain_fields;
    uint32_t *page_table;
    int page_columns, page_rows, page_keys[64];
    unsigned char page_dirty[64];
    uint64_t page_used[64];
    unsigned int framebuffer, output_framebuffer, vao;
    GpuProgram fields, geometry, grass, particles, bins, cache, lighting, smoke;
    GpuProgram terrain_compute, terrain_graphics;
    GpuProgram meadow;
    GpuProgram smoke_composite;
    GpuProgram contacts, light_bins, fog;
    unsigned int timers[GPU_TIMER_FRAMES][GPU_TIMER_COUNT];
    uint64_t timer_frame_ids[GPU_TIMER_FRAMES];
    int timer_frame, width, height, world_width, world_height, chunks_x, chunks_y;
    int tile_width, tile_height;
    float tile_origin[2];
    unsigned char *valid_chunks;
    uint64_t static_key, terrain_key;
    int failed, reported;
};

_Static_assert(sizeof(CastleGpuSprite) == 112, "GPU sprite layout");
_Static_assert(sizeof(CastleGpuCaster) == 80, "GPU caster layout");
_Static_assert(sizeof(CastleGpuLight) == 48, "GPU light layout");
_Static_assert(sizeof(CastleGpuEmitter) == 32, "GPU emitter layout");

static unsigned int objects;
CastleGpuViewport castle_gpu_viewport(const CastleGpuFrame *frame) {
    CastleGpuViewport v = {0};
    if (!frame || frame->width < 1 || frame->height < 1)
        return v;
    v.scale = frame->pixel_scale > 0 ? frame->pixel_scale : 1;
    if (v.scale > 6)
        v.scale = 6;
    v.width = frame->width / v.scale;
    v.height = frame->height / v.scale;
    v.destination = (Rectangle){(float)((frame->width - v.width * v.scale) / 2),
                                (float)((frame->height - v.height * v.scale) / 2), (float)(v.width * v.scale),
                                (float)(v.height * v.scale)};
    v.origin = (Vector2){frame->origin[0], frame->origin[1]};
    if (frame->pixel_scale > 0) {
        v.origin.x = floorf(v.origin.x + .5f);
        v.origin.y = floorf(v.origin.y + .5f);
    }
    return v;
}
Vector2 castle_gpu_screen_to_world(const CastleGpuFrame *frame, Vector2 point) {
    CastleGpuViewport v = castle_gpu_viewport(frame);
    float scale = frame->pixel_scale > 0 ? (float)v.scale : frame->scale;
    return (Vector2){v.origin.x + (point.x - v.destination.x) / scale,
                     v.origin.y + (point.y - v.destination.y) / scale};
}
Vector2 castle_gpu_world_to_screen(const CastleGpuFrame *frame, Vector2 point) {
    CastleGpuViewport v = castle_gpu_viewport(frame);
    float scale = frame->pixel_scale > 0 ? (float)v.scale : frame->scale;
    return (Vector2){v.destination.x + (point.x - v.origin.x) * scale,
                     v.destination.y + (point.y - v.origin.y) * scale};
}
unsigned int castle_gpu_live_objects(void) { return objects; }
CastleGpuDevice castle_gpu_device(void) {
    CastleGpuDevice d = {0};
    snprintf(d.vendor, sizeof(d.vendor), "%s", glGetString(GL_VENDOR));
    snprintf(d.renderer, sizeof(d.renderer), "%s", glGetString(GL_RENDERER));
    snprintf(d.version, sizeof(d.version), "%s", glGetString(GL_VERSION));
    d.software = strstr(d.renderer, "llvmpipe") != NULL || strstr(d.renderer, "softpipe") != NULL ||
                 strstr(d.renderer, "Software") != NULL;
    return d;
}
void castle_gpu_scenery(CastleGpu *gpu, Texture2D atlas) {
    if (gpu)
        gpu->scenery = atlas;
}
void castle_gpu_contact_policy(CastleGpu *gpu, Texture2D policy) {
    if (gpu)
        gpu->contact_policy = policy;
}
int castle_gpu_available(char *message, size_t bytes) {
    GLint major = 0, minor = 0, images = 0, buffers = 0;
    glGetIntegerv(GL_MAJOR_VERSION, &major);
    glGetIntegerv(GL_MINOR_VERSION, &minor);
    glGetIntegerv(GL_MAX_COMPUTE_IMAGE_UNIFORMS, &images);
    glGetIntegerv(GL_MAX_SHADER_STORAGE_BUFFER_BINDINGS, &buffers);
    if (major < 4 || (major == 4 && minor < 3) || !glDispatchCompute || !glMemoryBarrier || images < 5 ||
        buffers < 8) {
        (void)snprintf(message, bytes, u8"OpenGL 4.3 컴퓨트와 이미지 5개·버퍼 바인딩 8개가 필요합니다.");
        return 0;
    }
    if (bytes)
        message[0] = 0;
    return 1;
}
int castle_gpu_check_compute(void) {
    char message[192];
    if (!castle_gpu_available(message, sizeof(message))) {
        fprintf(stderr, "%s\n", message);
        return 1;
    }
    const char *source = "#version 430\nlayout(local_size_x=64) in;"
                         "layout(std430,binding=0) buffer Values{uint values[];};"
                         "void main(){uint i=gl_GlobalInvocationID.x;values[i]=i*i+17u;}";
    unsigned int shader = rlCompileShader(source, RL_COMPUTE_SHADER);
    unsigned int program = shader ? rlLoadComputeShaderProgram(shader) : 0;
    if (shader)
        glDeleteShader(shader);
    if (!program)
        return 1;
    unsigned int buffer = rlLoadShaderBuffer(64 * sizeof(uint32_t), NULL, RL_DYNAMIC_COPY);
    rlEnableShader(program);
    rlBindShaderBuffer(buffer, 0);
    rlComputeShaderDispatch(1, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT);
    uint32_t values[64] = {0};
    rlReadShaderBuffer(buffer, values, sizeof(values), 0);
    rlDisableShader();
    int errors = 0;
    for (unsigned int i = 0; i < 64; i++)
        errors += values[i] != i * i + 17u;
    rlUnloadShaderBuffer(buffer);
    rlUnloadShaderProgram(program);
    printf("GPU COMPUTE: renderer=%s version=%s values=64 errors=%d\n", glGetString(GL_RENDERER),
           glGetString(GL_VERSION), errors);
    return errors ? 1 : 0;
}

static uint64_t hash_bytes(uint64_t hash, const void *data, size_t bytes) {
    const unsigned char *p = data;
    for (size_t i = 0; i < bytes; i++) {
        hash ^= p[i];
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}
static int reserve(void **memory, int *capacity, int count, size_t stride) {
    if (count <= *capacity)
        return 1;
    int next = *capacity ? *capacity * 2 : 128;
    if (next < count)
        next = count;
    if (next > 1000000)
        return 0;
    void *replacement = realloc(*memory, (size_t)next * stride);
    if (!replacement)
        return 0;
    *memory = replacement;
    *capacity = next;
    return 1;
}
static int buffer_reserve(CastleGpu *gpu, GpuBuffer *buffer, size_t bytes) {
    if (bytes <= buffer->bytes)
        return 1;
    if (buffer->id) {
        rlUnloadShaderBuffer(buffer->id);
        objects--;
        gpu->stats.buffer_bytes -= buffer->bytes;
    }
    buffer->id = rlLoadShaderBuffer((unsigned int)bytes, NULL, RL_DYNAMIC_COPY);
    buffer->bytes = buffer->id ? bytes : 0;
    if (buffer->id) {
        objects++;
        gpu->stats.buffer_bytes += bytes;
    }
    return buffer->id != 0;
}
static void buffer_free(CastleGpu *gpu, GpuBuffer *buffer) {
    if (buffer->id) {
        rlUnloadShaderBuffer(buffer->id);
        objects--;
        gpu->stats.buffer_bytes -= buffer->bytes;
    }
    memset(buffer, 0, sizeof(*buffer));
}
static Texture2D texture_new(CastleGpu *gpu, int width, int height, int format) {
    Texture2D texture = {0};
    texture.id = rlLoadTexture(NULL, width, height, format, 1);
    if (texture.id) {
        texture.width = width;
        texture.height = height;
        texture.mipmaps = 1;
        texture.format = format;
        SetTextureFilter(texture, TEXTURE_FILTER_POINT);
        SetTextureWrap(texture, TEXTURE_WRAP_CLAMP);
        objects++;
        gpu->stats.texture_bytes += (size_t)width * height * (size_t)GetPixelDataSize(1, 1, format);
    }
    return texture;
}
static void texture_free(CastleGpu *gpu, Texture2D *texture) {
    if (texture->id) {
        gpu->stats.texture_bytes -=
            (size_t)texture->width * texture->height * (size_t)GetPixelDataSize(1, 1, texture->format);
        rlUnloadTexture(texture->id);
        objects--;
    }
    memset(texture, 0, sizeof(*texture));
}
static size_t lines_size(const char *const *lines) {
    size_t n = 0;
    for (int i = 0; lines[i]; i++)
        n += strlen(lines[i]);
    return n;
}
static char *shader_source(const char *const *lines, int shadows) {
    size_t bytes = 32 + lines_size(GPU_common_glsl) + lines_size(lines) +
                   ((shadows & 1) ? lines_size(GPU_shadows_glsl) : 0) +
                   ((shadows & 2) ? lines_size(GPU_terrain_glsl) : 0);
    char *source = malloc(bytes);
    if (!source)
        return NULL;
    strcpy(source, "#version 430\n");
    const char *const *parts[4] = {GPU_common_glsl, (shadows & 1) ? GPU_shadows_glsl : NULL,
                                   (shadows & 2) ? GPU_terrain_glsl : NULL, lines};
    size_t at = strlen(source);
    for (int part = 0; part < 4; part++)
        if (parts[part])
            for (int i = 0; parts[part][i]; i++) {
                size_t n = strlen(parts[part][i]);
                memcpy(source + at, parts[part][i], n);
                at += n;
            }
    source[at] = 0;
    return source;
}
static unsigned int stage_compile(const char *const *lines, int type, int shadows) {
    char *source = shader_source(lines, shadows);
    if (!source)
        return 0;
    unsigned int shader = rlCompileShader(source, type);
    free(source);
    return shader;
}
static GpuProgram program_compute(const char *const *lines, int shadows) {
    unsigned int shader = stage_compile(lines, RL_COMPUTE_SHADER, shadows);
    unsigned int program = shader ? rlLoadComputeShaderProgram(shader) : 0;
    if (shader)
        glDeleteShader(shader);
    if (program)
        objects++;
    return (GpuProgram){program};
}
static GpuProgram program_graphics_flags(const char *const *vertex, const char *const *fragment, int flags) {
    unsigned int vs = stage_compile(vertex, RL_VERTEX_SHADER, 0),
                 fs = stage_compile(fragment, RL_FRAGMENT_SHADER, flags);
    unsigned int program = vs && fs ? rlLoadShaderProgram(vs, fs) : 0;
    if (vs)
        glDeleteShader(vs);
    if (fs)
        glDeleteShader(fs);
    if (program)
        objects++;
    return (GpuProgram){program};
}
static GpuProgram program_graphics(const char *const *vertex, const char *const *fragment) {
    return program_graphics_flags(vertex, fragment, 0);
}
static void program_free(GpuProgram *program) {
    if (program->id) {
        rlUnloadShaderProgram(program->id);
        objects--;
        program->id = 0;
    }
}
static void uniform_i(GpuProgram p, const char *name, int value) {
    glUniform1i(glGetUniformLocation(p.id, name), value);
}
static void uniform_f(GpuProgram p, const char *name, float value) {
    glUniform1f(glGetUniformLocation(p.id, name), value);
}
static void uniform_v2(GpuProgram p, const char *name, float x, float y) {
    glUniform2f(glGetUniformLocation(p.id, name), x, y);
}
static void sampler(GpuProgram p, const char *name, Texture2D texture, int slot) {
    glActiveTexture((GLenum)(GL_TEXTURE0 + slot));
    glBindTexture(GL_TEXTURE_2D, texture.id);
    uniform_i(p, name, slot);
}
static void seed_update(CastleGpu *gpu) {
    int changed = !gpu->seed_valid || gpu->seed_value != gpu->frame.world_seed;
    if (changed) {
        GpuProgram *programs[] = {&gpu->fields,           &gpu->geometry,   &gpu->grass,
                                  &gpu->particles,        &gpu->bins,       &gpu->cache,
                                  &gpu->lighting,         &gpu->smoke,      &gpu->terrain_compute,
                                  &gpu->terrain_graphics, &gpu->meadow,     &gpu->contacts,
                                  &gpu->smoke_composite,  &gpu->light_bins, &gpu->fog};
        for (size_t i = 0; i < sizeof(programs) / sizeof(programs[0]); i++) {
            glUseProgram(programs[i]->id);
            glUniform1ui(glGetUniformLocation(programs[i]->id, "uWorldSeed"), gpu->frame.world_seed);
        }
        gpu->seed_value = gpu->frame.world_seed;
        gpu->seed_valid = 1;
        memset(gpu->page_dirty, 1, sizeof(gpu->page_dirty));
        gpu->static_key = 0;
    }
    if (changed || gpu->frame.reset_motion) {
        uint32_t zero = 0;
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, gpu->particle_buffer.id);
        glClearBufferData(GL_SHADER_STORAGE_BUFFER, GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT, &zero);
        glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT);
    }
}
static void timer_start(CastleGpu *gpu, int phase) {
    if (phase == 0)
        gpu->timer_frame_ids[gpu->timer_frame] = gpu->stats.frames;
    glBeginQuery(GL_TIME_ELAPSED, gpu->timers[gpu->timer_frame][phase]);
}
static void timer_end(void) { glEndQuery(GL_TIME_ELAPSED); }
static void timer_poll(CastleGpu *gpu) {
    gpu->timer_frame = (gpu->timer_frame + 1) % GPU_TIMER_FRAMES;
    double *times[GPU_TIMER_COUNT] = {&gpu->stats.fields_ms, &gpu->stats.geometry_ms, &gpu->stats.shadows_ms,
                                      &gpu->stats.lighting_ms, &gpu->stats.smoke_ms};
    if (gpu->stats.frames < GPU_TIMER_FRAMES)
        return;
    for (int i = 0; i < GPU_TIMER_COUNT; i++) {
        GLint available = 0;
        glGetQueryObjectiv(gpu->timers[gpu->timer_frame][i], GL_QUERY_RESULT_AVAILABLE, &available);
        if (!available)
            return;
    }
    for (int i = 0; i < GPU_TIMER_COUNT; i++) {
        GLuint64 elapsed = 0;
        glGetQueryObjectui64v(gpu->timers[gpu->timer_frame][i], GL_QUERY_RESULT, &elapsed);
        *times[i] = (double)elapsed / 1000000.0;
    }
    gpu->stats.timed_frames++;
    gpu->stats.timed_frame_id = gpu->timer_frame_ids[gpu->timer_frame];
}
CastleGpu *castle_gpu_create(void) {
    char message[192];
    if (!castle_gpu_available(message, sizeof(message))) {
        fprintf(stderr, "%s\n", message);
        return NULL;
    }
    CastleGpu *gpu = calloc(1, sizeof(*gpu));
    if (!gpu)
        return NULL;
    gpu->stats.software = castle_gpu_device().software;
    gpu->fields = program_compute(GPU_fields_cs, 0);
    gpu->geometry = program_graphics(GPU_geometry_vs, GPU_geometry_fs);
    gpu->grass = program_compute(GPU_grass_cs, 0);
    gpu->particles = program_compute(GPU_particles_cs, 0);
    gpu->bins = program_compute(GPU_bins_cs, 1);
    gpu->cache = program_compute(GPU_cache_cs, 1);
    gpu->lighting = program_compute(GPU_lighting_cs, 1);
    gpu->smoke = program_graphics(GPU_smoke_vs, GPU_smoke_fs);
    gpu->terrain_compute = program_compute(GPU_terrain_cs, 2);
    gpu->terrain_graphics = program_graphics_flags(GPU_terrain_vs, GPU_terrain_fs, 2);
    gpu->meadow = program_compute(GPU_meadow_cs, 2);
    gpu->smoke_composite = program_compute(GPU_smoke_composite_cs, 0);
    gpu->contacts = program_compute(GPU_contacts_cs, 0);
    gpu->light_bins = program_compute(GPU_light_bins_cs, 0);
    gpu->fog = program_compute(GPU_fog_cs, 0);
    if (!gpu->fields.id || !gpu->geometry.id || !gpu->grass.id || !gpu->particles.id || !gpu->bins.id ||
        !gpu->cache.id || !gpu->lighting.id || !gpu->smoke.id || !gpu->terrain_compute.id ||
        !gpu->terrain_graphics.id || !gpu->meadow.id || !gpu->smoke_composite.id || !gpu->contacts.id ||
        !gpu->light_bins.id || !gpu->fog.id) {
        castle_gpu_destroy(gpu);
        return NULL;
    }
    glGenVertexArrays(1, &gpu->vao);
    if (gpu->vao)
        objects++;
    glGenQueries(GPU_TIMER_COUNT * GPU_TIMER_FRAMES, &gpu->timers[0][0]);
    objects += GPU_TIMER_COUNT * GPU_TIMER_FRAMES;
    gpu->wind = texture_new(gpu, 128, 128, PIXELFORMAT_UNCOMPRESSED_R16G16B16A16);
    gpu->waves = texture_new(gpu, 256, 256, PIXELFORMAT_UNCOMPRESSED_R16G16B16A16);
    SetTextureFilter(gpu->wind, TEXTURE_FILTER_BILINEAR);
    SetTextureFilter(gpu->waves, TEXTURE_FILTER_BILINEAR);
    gpu->white = texture_new(gpu, 1, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    unsigned char white[4] = {255, 255, 255, 255};
    UpdateTexture(gpu->white, white);
    int ready = gpu->vao && gpu->wind.id && gpu->waves.id && gpu->white.id &&
                reserve((void **)&gpu->lights, &gpu->light_capacity, 128, sizeof(*gpu->lights));
    ready = ready && buffer_reserve(gpu, &gpu->light_buffer, sizeof(CastleGpuLight) * 128) &&
            buffer_reserve(gpu, &gpu->grass_buffer, GPU_GRASS_LIMIT * sizeof(CastleGpuSprite));
    ready = ready && buffer_reserve(gpu, &gpu->command_buffer, 16) &&
            buffer_reserve(gpu, &gpu->particle_buffer, GPU_PARTICLES * 48);
    if (!ready) {
        castle_gpu_destroy(gpu);
        return NULL;
    }
    unsigned char *empty_particles = calloc(GPU_PARTICLES, 48);
    if (!empty_particles) {
        castle_gpu_destroy(gpu);
        return NULL;
    }
    rlUpdateShaderBuffer(gpu->particle_buffer.id, empty_particles, GPU_PARTICLES * 48, 0);
    free(empty_particles);
    printf("GPU RENDERER: vendor=%s renderer=%s software=%d OpenGL=%s\n", glGetString(GL_VENDOR),
           glGetString(GL_RENDERER), gpu->stats.software, glGetString(GL_VERSION));
    return gpu;
}
static void targets_free(CastleGpu *gpu) {
    for (int i = 0; i < 4; i++)
        texture_free(gpu, &gpu->gbuffer[i]);
    texture_free(gpu, &gpu->output);
    texture_free(gpu, &gpu->smoke_layer);
    if (gpu->smoke_framebuffer) {
        glDeleteFramebuffers(1, &gpu->smoke_framebuffer);
        gpu->smoke_framebuffer = 0;
        objects--;
    }
    if (gpu->depth.id) {
        rlUnloadTexture(gpu->depth.id);
        gpu->stats.texture_bytes -= (size_t)gpu->depth.width * gpu->depth.height * 4;
        objects--;
        memset(&gpu->depth, 0, sizeof(gpu->depth));
    }
    if (gpu->framebuffer) {
        glDeleteFramebuffers(1, &gpu->framebuffer);
        gpu->framebuffer = 0;
        objects--;
    }
    if (gpu->output_framebuffer) {
        glDeleteFramebuffers(1, &gpu->output_framebuffer);
        gpu->output_framebuffer = 0;
        objects--;
    }
}
void castle_gpu_destroy(CastleGpu *gpu) {
    if (!gpu)
        return;
    targets_free(gpu);
    texture_free(gpu, &gpu->fog_layer);
    texture_free(gpu, &gpu->wind);
    texture_free(gpu, &gpu->waves);
    texture_free(gpu, &gpu->white);
    texture_free(gpu, &gpu->sun_cache);
    texture_free(gpu, &gpu->point_cache);
    texture_free(gpu, &gpu->terrain_fields);
    texture_free(gpu, &gpu->contact_atlas);
    texture_free(gpu, &gpu->contact_front);
    GpuBuffer *buffers[] = {&gpu->sprite_buffer,  &gpu->caster_buffer,       &gpu->light_buffer,
                            &gpu->emitter_buffer, &gpu->terrain_buffer,      &gpu->grass_buffer,
                            &gpu->command_buffer, &gpu->particle_buffer,     &gpu->tile_buffer,
                            &gpu->page_buffer,    &gpu->grass_counts,        &gpu->grass_offsets,
                            &gpu->grass_sums,     &gpu->grass_group_offsets, &gpu->contact_columns,
                            &gpu->light_tiles,    &gpu->static_tiles,        &gpu->fog_buffer};
    for (size_t i = 0; i < sizeof(buffers) / sizeof(buffers[0]); i++)
        buffer_free(gpu, buffers[i]);
    GpuProgram *programs[] = {
        &gpu->fields, &gpu->geometry,        &gpu->grass,    &gpu->particles,       &gpu->bins,
        &gpu->cache,  &gpu->lighting,        &gpu->smoke,    &gpu->terrain_compute, &gpu->terrain_graphics,
        &gpu->meadow, &gpu->smoke_composite, &gpu->contacts, &gpu->light_bins,      &gpu->fog};
    for (size_t i = 0; i < sizeof(programs) / sizeof(programs[0]); i++)
        program_free(programs[i]);
    if (gpu->vao) {
        glDeleteVertexArrays(1, &gpu->vao);
        objects--;
    }
    if (gpu->timers[0][0]) {
        glDeleteQueries(GPU_TIMER_COUNT * GPU_TIMER_FRAMES, &gpu->timers[0][0]);
        objects -= GPU_TIMER_COUNT * GPU_TIMER_FRAMES;
    }
    free(gpu->lights);
    free(gpu->fog_words);
    free(gpu->sprites);
    free(gpu->casters);
    free(gpu->emitters);
    free(gpu->batches);
    free(gpu->terrain);
    free(gpu->terrain_words);
    free(gpu->valid_chunks);
    free(gpu->page_table);
    free(gpu->owner_table);
    free(gpu);
}
static int targets_resize(CastleGpu *gpu, int width, int height) {
    if (width == gpu->width && height == gpu->height)
        return 1;
    targets_free(gpu);
    gpu->width = width;
    gpu->height = height;
    for (int i = 0; i < 4; i++)
        gpu->gbuffer[i] =
            texture_new(gpu, width, height,
                        i == 1 ? PIXELFORMAT_UNCOMPRESSED_R16G16B16A16 : PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    gpu->output = texture_new(gpu, width, height, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    gpu->smoke_layer = texture_new(gpu, width, height, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    gpu->depth = (Texture2D){rlLoadTextureDepth(width, height, false), width, height, 1, 0};
    if (gpu->depth.id) {
        objects++;
        gpu->stats.texture_bytes += (size_t)width * height * 4;
    }
    glGenFramebuffers(1, &gpu->framebuffer);
    if (gpu->framebuffer)
        objects++;
    glBindFramebuffer(GL_FRAMEBUFFER, gpu->framebuffer);
    for (int i = 0; i < 4; i++)
        glFramebufferTexture2D(GL_FRAMEBUFFER, (GLenum)(GL_COLOR_ATTACHMENT0 + i), GL_TEXTURE_2D,
                               gpu->gbuffer[i].id, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, gpu->depth.id, 0);
    const GLenum attachments[4] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2,
                                   GL_COLOR_ATTACHMENT3};
    glDrawBuffers(4, attachments);
    int okay = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
    glGenFramebuffers(1, &gpu->output_framebuffer);
    if (gpu->output_framebuffer)
        objects++;
    glBindFramebuffer(GL_FRAMEBUFFER, gpu->output_framebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gpu->output.id, 0);
    glDrawBuffer(GL_COLOR_ATTACHMENT0);
    okay = okay && glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
    glGenFramebuffers(1, &gpu->smoke_framebuffer);
    if (gpu->smoke_framebuffer)
        objects++;
    glBindFramebuffer(GL_FRAMEBUFFER, gpu->smoke_framebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gpu->smoke_layer.id, 0);
    glDrawBuffer(GL_COLOR_ATTACHMENT0);
    okay = okay && glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return okay;
}
static int world_resize(CastleGpu *gpu, int width, int height) {
    if (width == gpu->world_width && height == gpu->world_height)
        return 1;
    texture_free(gpu, &gpu->sun_cache);
    texture_free(gpu, &gpu->point_cache);
    free(gpu->valid_chunks);
    gpu->valid_chunks = NULL;
    gpu->world_width = width;
    gpu->world_height = height;
    gpu->chunks_x = (width + GPU_CHUNK - 1) / GPU_CHUNK;
    gpu->chunks_y = (height + GPU_CHUNK - 1) / GPU_CHUNK;
    gpu->valid_chunks = calloc((size_t)gpu->chunks_x * gpu->chunks_y, 1);
    gpu->sun_cache = texture_new(gpu, width, height, PIXELFORMAT_UNCOMPRESSED_GRAYSCALE);
    gpu->point_cache = texture_new(gpu, width, height, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    gpu->static_key = 0;
    return gpu->valid_chunks && gpu->sun_cache.id && gpu->point_cache.id;
}
int castle_gpu_begin(CastleGpu *gpu, const CastleGpuFrame *frame) {
    if (!gpu || frame->width < 1 || frame->height < 1 || frame->map_width < 1 || frame->map_height < 1)
        return 0;
    gpu->frame = *frame;
    if (frame->pixel_scale > 0) {
        CastleGpuViewport v = castle_gpu_viewport(frame);
        gpu->frame.origin[0] = v.origin.x;
        gpu->frame.origin[1] = v.origin.y;
        gpu->frame.scale = 1;
    }
    gpu->sprite_count = 0;
    gpu->caster_count = 0;
    gpu->emitter_count = 0;
    gpu->batch_count = 0;
    gpu->light_count = 0;
    gpu->failed = 0;
    return 1;
}
int castle_gpu_sprite(CastleGpu *gpu, Texture2D texture, Texture2D normal, Texture2D surface,
                      const CastleGpuSprite *sprite) {
    if (!gpu || !texture.id)
        return 0;
    if (gpu->frame.natural && (sprite->tags[2] & 2u) == 0 &&
        (sprite->tags[1] == GPU_MATERIAL_GRASS || sprite->tags[1] == GPU_MATERIAL_ROAD ||
         sprite->tags[1] == GPU_MATERIAL_WATER))
        return 1;
    if (!reserve((void **)&gpu->sprites, &gpu->sprite_capacity, gpu->sprite_count + 1, sizeof(*sprite))) {
        gpu->failed = 1;
        return 0;
    }
    GpuBatch *batch = gpu->batch_count ? &gpu->batches[gpu->batch_count - 1] : NULL;
    if (!batch || batch->texture.id != texture.id || batch->normal.id != normal.id ||
        batch->surface.id != surface.id) {
        if (!reserve((void **)&gpu->batches, &gpu->batch_capacity, gpu->batch_count + 1, sizeof(*batch))) {
            gpu->failed = 1;
            return 0;
        }
        batch = &gpu->batches[gpu->batch_count++];
        *batch = (GpuBatch){texture, normal, surface, gpu->sprite_count, 0};
    }
    CastleGpuSprite copy = *sprite;
    if (gpu->frame.pixel_scale > 0) {
        float center = copy.destination[0] + copy.destination[2] * .5f;
        copy.destination[0] += floorf(center + .5f) - center;
        copy.destination[1] += floorf(copy.shape[0] + .5f) - copy.shape[0];
        copy.shape[0] = floorf(copy.shape[0] + .5f);
    }
    gpu->sprites[gpu->sprite_count++] = copy;
    batch->count++;
    return 1;
}
int castle_gpu_caster(CastleGpu *gpu, const CastleGpuCaster *caster) {
    if (!gpu ||
        !reserve((void **)&gpu->casters, &gpu->caster_capacity, gpu->caster_count + 1, sizeof(*caster))) {
        if (gpu)
            gpu->failed = 1;
        return 0;
    }
    gpu->casters[gpu->caster_count++] = *caster;
    return 1;
}
int castle_gpu_light(CastleGpu *gpu, const CastleGpuLight *light) {
    if (!gpu || !light ||
        !reserve((void **)&gpu->lights, &gpu->light_capacity, gpu->light_count + 1, sizeof(*light))) {
        if (gpu)
            gpu->failed = 1;
        return 0;
    }
    gpu->lights[gpu->light_count++] = *light;
    return 1;
}
int castle_gpu_emitter(CastleGpu *gpu, const CastleGpuEmitter *emitter) {
    if (!gpu ||
        !reserve((void **)&gpu->emitters, &gpu->emitter_capacity, gpu->emitter_count + 1, sizeof(*emitter))) {
        if (gpu)
            gpu->failed = 1;
        return 0;
    }
    gpu->emitters[gpu->emitter_count++] = *emitter;
    return 1;
}
void castle_gpu_terrain(CastleGpu *gpu, const unsigned char *cells, int width, int height) {
    if (!gpu || !cells || width < 1 || height < 1)
        return;
    size_t bytes = (size_t)width * height;
    uint64_t key = hash_bytes(UINT64_C(1469598103934665603), cells, bytes);
    if (key == gpu->terrain_key && bytes == gpu->terrain_bytes)
        return;
    if (bytes != gpu->terrain_bytes) {
        memset(gpu->page_dirty, 1, sizeof(gpu->page_dirty));
        unsigned char *replacement = malloc(bytes);
        unsigned int *words = malloc(bytes * sizeof(*words));
        if (!replacement || !words) {
            free(replacement);
            free(words);
            gpu->failed = 1;
            return;
        }
        free(gpu->terrain);
        free(gpu->terrain_words);
        gpu->terrain = replacement;
        gpu->terrain_words = words;
        gpu->terrain_bytes = bytes;
    } else if (gpu->page_table) {
        for (size_t i = 0; i < bytes; i++) {
            if (gpu->terrain[i] == cells[i])
                continue;
            int x = (int)(i % (size_t)width) * 16, y = (int)(i / (size_t)width) * 16;
            for (int page = 0; page < 64; page++) {
                int page_key = gpu->page_keys[page] - 1;
                if (page_key < 0)
                    continue;
                int px = page_key % gpu->page_columns * GPU_CHUNK,
                    py = page_key / gpu->page_columns * GPU_CHUNK;
                if (x + 16 >= px - 32 && x <= px + GPU_CHUNK + 32 && y + 16 >= py - 32 &&
                    y <= py + GPU_CHUNK + 32)
                    gpu->page_dirty[page] = 1;
            }
        }
    }
    memcpy(gpu->terrain, cells, bytes);
    for (size_t i = 0; i < bytes; i++)
        gpu->terrain_words[i] = cells[i];
    if (!buffer_reserve(gpu, &gpu->terrain_buffer, bytes * sizeof(unsigned int))) {
        gpu->failed = 1;
        return;
    }
    rlUpdateShaderBuffer(gpu->terrain_buffer.id, gpu->terrain_words,
                         (unsigned int)(bytes * sizeof(unsigned int)), 0);
    gpu->terrain_key = key;
}
static void terrain_uniforms(CastleGpu *gpu, GpuProgram program) {
    glUseProgram(program.id);
    rlBindShaderBuffer(gpu->terrain_buffer.id, 3);
    rlBindShaderBuffer(gpu->page_buffer.id, 4);
    glUniform2i(glGetUniformLocation(program.id, "uMapCells"), gpu->frame.map_width, gpu->frame.map_height);
    glUniform2i(glGetUniformLocation(program.id, "uFieldChunks"), gpu->page_columns, gpu->page_rows);
    sampler(program, "uTerrainFields", gpu->terrain_fields, 5);
}
static int terrain_update(CastleGpu *gpu) {
    int columns = (gpu->world_width + 127) / 128, rows = (gpu->world_height + 127) / 128;
    if (columns != gpu->page_columns || rows != gpu->page_rows) {
        uint32_t *table = calloc((size_t)columns * rows, sizeof(*table));
        if (!table)
            return 0;
        free(gpu->page_table);
        gpu->page_table = table;
        gpu->page_columns = columns;
        gpu->page_rows = rows;
        memset(gpu->page_keys, 0, sizeof(gpu->page_keys));
        memset(gpu->page_used, 0, sizeof(gpu->page_used));
    }
    if (!gpu->terrain_fields.id)
        gpu->terrain_fields = texture_new(gpu, 1536, 1536, PIXELFORMAT_UNCOMPRESSED_R16G16B16A16);
    if (!gpu->terrain_fields.id ||
        !buffer_reserve(gpu, &gpu->page_buffer, (size_t)columns * rows * sizeof(uint32_t)))
        return 0;
    terrain_uniforms(gpu, gpu->terrain_compute);
    rlBindImageTexture(gpu->terrain_fields.id, 0, gpu->terrain_fields.format, false);
    float right = gpu->frame.origin[0] + gpu->width / gpu->frame.scale;
    float bottom = gpu->frame.origin[1] + gpu->height / gpu->frame.scale;
    int x0 = (int)fmaxf(0, floorf((gpu->frame.origin[0] - 24) / 128));
    int y0 = (int)fmaxf(0, floorf((gpu->frame.origin[1] - 24) / 128));
    int x1 = (int)fminf((float)columns - 1, floorf((right + 24) / 128));
    int y1 = (int)fminf((float)rows - 1, floorf((bottom + 24) / 128));
    int cx = (x0 + x1) / 2, cy = (y0 + y1) / 2, selected = 0;
    uint64_t stamp = gpu->stats.frames + 1;
    /* 가까운 청크를 먼저 고정해 확대 1배에서도 상주 한도를 지킵니다. */
    for (int ring = 0; ring <= columns + rows && selected < 64; ring++)
        for (int y = y0; y <= y1 && selected < 64; y++)
            for (int x = x0; x <= x1 && selected < 64; x++) {
                if (abs(x - cx) > ring || abs(y - cy) > ring || (abs(x - cx) != ring && abs(y - cy) != ring))
                    continue;
                int key = y * columns + x, slot = (int)gpu->page_table[key] - 1;
                if (slot < 0) {
                    uint64_t oldest = UINT64_MAX;
                    for (int i = 0; i < 64; i++)
                        if (gpu->page_used[i] < oldest) {
                            oldest = gpu->page_used[i];
                            slot = i;
                        }
                    if (gpu->page_keys[slot])
                        gpu->page_table[gpu->page_keys[slot] - 1] = 0;
                    gpu->page_keys[slot] = key + 1;
                    gpu->page_table[key] = (uint32_t)slot + 1;
                    gpu->page_dirty[slot] = 1;
                }
                gpu->page_used[slot] = stamp;
                selected++;
                if (!gpu->page_dirty[slot])
                    continue;
                glUniform2i(glGetUniformLocation(gpu->terrain_compute.id, "uChunk"), x, y);
                glUniform2i(glGetUniformLocation(gpu->terrain_compute.id, "uPageOffset"), slot % 8 * 192,
                            slot / 8 * 192);
                rlComputeShaderDispatch(24, 24, 1);
                gpu->page_dirty[slot] = 0;
            }
    rlUpdateShaderBuffer(gpu->page_buffer.id, gpu->page_table,
                         (unsigned int)((size_t)columns * rows * sizeof(uint32_t)), 0);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT |
                    GL_SHADER_STORAGE_BARRIER_BIT);
    return 1;
}
static int contacts_update(CastleGpu *gpu) {
    if (!gpu->contact_policy.id)
        return 1;
    if (!gpu->contact_atlas.id)
        gpu->contact_atlas = texture_new(gpu, 4096, 4096, PIXELFORMAT_UNCOMPRESSED_GRAYSCALE);
    if (!gpu->contact_front.id)
        gpu->contact_front = texture_new(gpu, 256, 512, PIXELFORMAT_UNCOMPRESSED_R16G16B16A16);
    if (!gpu->contact_atlas.id || !gpu->contact_front.id ||
        !buffer_reserve(gpu, &gpu->contact_columns, 257 * 2 * sizeof(float)))
        return 0;
    int capacity = 16;
    while (capacity < gpu->sprite_count * 2)
        capacity *= 2;
    if (capacity > gpu->owner_capacity) {
        GpuOwnerEntry *table = realloc(gpu->owner_table, (size_t)capacity * sizeof(*table));
        if (!table)
            return 0;
        gpu->owner_table = table;
        gpu->owner_capacity = capacity;
    }
    memset(gpu->owner_table, 0, (size_t)gpu->owner_capacity * sizeof(*gpu->owner_table));
    GpuProgram p = gpu->contacts;
    glUseProgram(p.id);
    rlBindShaderBuffer(gpu->contact_columns.id, 0);
    rlBindImageTexture(gpu->contact_atlas.id, 0, gpu->contact_atlas.format, false);
    rlBindImageTexture(gpu->contact_front.id, 1, gpu->contact_front.format, false);
    sampler(p, "uContactPolicy", gpu->contact_policy, 1);
    for (int batch_index = 0; batch_index < gpu->batch_count; batch_index++) {
        GpuBatch *batch = &gpu->batches[batch_index];
        for (int i = batch->first; i < batch->first + batch->count; i++) {
            CastleGpuSprite *s = &gpu->sprites[i];
            uint32_t owner = s->tags[0], kind = s->tags[1];
            if (!owner || (kind != 1 && kind != 2 && kind != 3 && kind != 6))
                continue;
            float source[4];
            memcpy(source, s->source, sizeof(source));
            if (source[2] < 0) {
                source[0] += source[2];
                source[2] = -source[2];
            }
            uint64_t key =
                hash_bytes(UINT64_C(1469598103934665603), &batch->texture.id, sizeof(unsigned int));
            key = hash_bytes(key, source, sizeof(source));
            key = hash_bytes(key, &kind, sizeof(kind));
            if (!key)
                key = 1;
            int slot = -1, free_slot = -1;
            uint64_t oldest = UINT64_MAX;
            for (int entry = 0; entry < 512; entry++) {
                if (gpu->contact_entries[entry].key == key) {
                    slot = entry;
                    break;
                }
                if (gpu->contact_entries[entry].used < oldest) {
                    oldest = gpu->contact_entries[entry].used;
                    free_slot = entry;
                }
            }
            if (slot < 0) {
                if (free_slot < 0 || oldest == gpu->stats.frames + 1)
                    continue;
                slot = free_slot;
                gpu->contact_entries[slot].key = key;
                gpu->contact_entries[slot].revision = ++gpu->contact_revision;
                sampler(p, "uSourceTexture", batch->texture, 0);
                glUniform4fv(glGetUniformLocation(p.id, "uSource"), 1, source);
                uniform_i(p, "uKind", (int)kind);
                glUniform2i(glGetUniformLocation(p.id, "uPage"), slot % 16 * 256, slot / 16 * 128);
                for (int pass = 0; pass < 3; pass++) {
                    uniform_i(p, "uPass", pass);
                    rlComputeShaderDispatch(pass == 2 ? 512 : pass == 1 ? 1 : 4, 1, 1);
                    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_SHADER_IMAGE_ACCESS_BARRIER_BIT |
                                    GL_TEXTURE_FETCH_BARRIER_BIT);
                }
            }
            gpu->contact_entries[slot].used = gpu->stats.frames + 1;
            s->contact_profile[0] = (float)(slot + 1);
            s->contact_profile[1] = s->source[2] < 0 ? 1.0f : 0.0f;
            s->contact_profile[2] = 2;
            s->contact_profile[3] = (float)gpu->contact_entries[slot].revision;
            unsigned int index = (owner * 2654435761u) & (unsigned int)(gpu->owner_capacity - 1);
            while (gpu->owner_table[index].owner && gpu->owner_table[index].owner != owner)
                index = (index + 1) & (unsigned int)(gpu->owner_capacity - 1);
            GpuOwnerEntry *entry = &gpu->owner_table[index];
            if (entry->owner) {
                CastleGpuSprite *previous = &gpu->sprites[entry->sprite];
                if (previous->destination[2] * previous->destination[3] >
                    s->destination[2] * s->destination[3])
                    continue;
            }
            *entry = (GpuOwnerEntry){owner, i};
        }
    }
    for (int i = 0; i < gpu->caster_count; i++) {
        CastleGpuCaster *c = &gpu->casters[i];
        unsigned int index = (c->tags[0] * 2654435761u) & (unsigned int)(gpu->owner_capacity - 1);
        while (gpu->owner_table[index].owner && gpu->owner_table[index].owner != c->tags[0])
            index = (index + 1) & (unsigned int)(gpu->owner_capacity - 1);
        if (!gpu->owner_table[index].owner)
            continue;
        int elevated = c->center[2] - c->extent[2] > 1.0f;
        if (c->tags[2] == 2 || (elevated && c->tags[2] != 1)) {
            c->tags[3] &= ~1u;
            continue;
        }
        CastleGpuSprite *s = &gpu->sprites[gpu->owner_table[index].sprite];
        memcpy(c->contact_profile, s->contact_profile, sizeof(c->contact_profile));
        c->contact[0] = s->destination[0] + s->destination[2] * .5f;
        c->contact[1] = s->destination[1] + s->destination[3];
        c->contact[2] = c->contact[3] = s->destination[2] / 64.0f;
        if (elevated) {
            c->contact_profile[2] = 0;
            c->tags[3] &= ~1u;
        } else
            c->tags[3] |= 1u;
    }
    return 1;
}
static void shadow_uniforms(CastleGpu *gpu, GpuProgram program) {
    glUseProgram(program.id);
    rlBindShaderBuffer(gpu->caster_buffer.id, 0);
    rlBindShaderBuffer(gpu->light_buffer.id, 1);
    rlBindShaderBuffer(gpu->tile_buffer.id, 2);
    rlBindShaderBuffer(gpu->static_tiles.id, 4);
    uniform_i(program, "uCasterCount", gpu->caster_count);
    uniform_i(program, "uLightCount", gpu->light_count);
    uniform_i(program, "uSunEnabled", gpu->frame.sun_enabled);
    glUniform3fv(glGetUniformLocation(program.id, "uSunDirection"), 1, gpu->frame.sun);
    uniform_v2(program, "uTileOrigin", gpu->tile_origin[0], gpu->tile_origin[1]);
    glUniform2i(glGetUniformLocation(program.id, "uTileSize"), gpu->tile_width, gpu->tile_height);
    sampler(program, "uShadowWind", gpu->wind, 6);
    uniform_v2(program, "uShadowMap", (float)gpu->world_width, (float)gpu->world_height);
    uniform_f(program, "uShadowTime", gpu->frame.time);
    uniform_i(program, "uShadowFoliage", gpu->frame.foliage);
    sampler(program, "uContacts", gpu->contact_atlas.id ? gpu->contact_atlas : gpu->white, 7);
    sampler(program, "uContactFront", gpu->contact_front.id ? gpu->contact_front : gpu->white, 8);
}
static void geometry_uniforms(CastleGpu *gpu) {
    GpuProgram p = gpu->geometry;
    glUseProgram(p.id);
    uniform_v2(p, "uOrigin", gpu->frame.origin[0], gpu->frame.origin[1]);
    uniform_v2(p, "uViewport", (float)gpu->width, (float)gpu->height);
    uniform_v2(p, "uMap", (float)gpu->world_width, (float)gpu->world_height);
    uniform_f(p, "uScale", gpu->frame.scale);
    uniform_f(p, "uTime", gpu->frame.time);
    uniform_f(p, "uDetail", gpu->frame.detail);
    uniform_i(p, "uFoliage", gpu->frame.foliage);
    uniform_i(p, "uWater", gpu->frame.water);
    uniform_i(p, "uNatural", gpu->frame.natural);
    sampler(p, "uWind", gpu->wind, 3);
    sampler(p, "uWaves", gpu->waves, 4);
    glBindVertexArray(gpu->vao);
}
static int inside_box(const CastleGpuCaster *c, const float *point) {
    float x = point[0] - c->center[0], y = point[1] - c->center[1], z = point[2] - c->center[2];
    if (c->tags[2] == 1) {
        float q = x * x / (c->extent[0] * c->extent[0]) + y * y / (c->extent[1] * c->extent[1]) +
                  z * z / (c->extent[2] * c->extent[2]);
        return q < 1;
    }
    if (fabsf(x) > c->extent[0] || fabsf(y) > c->extent[1])
        return 0;
    return c->tags[2] == 2 ? z >= 0 && z <= c->extent[2] * (1 - fabsf(y) / c->extent[1])
                           : fabsf(z) <= c->extent[2];
}
static void cache_update(CastleGpu *gpu) {
    uint64_t key = UINT64_C(1469598103934665603);
    key = hash_bytes(key, gpu->frame.sun, 3 * sizeof(float));
    key = hash_bytes(key, &gpu->frame.sun_enabled, sizeof(int));
    for (int i = 0; i < gpu->caster_count; i++)
        if (!gpu->casters[i].tags[1])
            key = hash_bytes(key, &gpu->casters[i], sizeof(gpu->casters[i]));
    int cached = gpu->light_count < 4 ? gpu->light_count : 4;
    for (int i = 0; i < cached; i++) {
        if ((gpu->lights[i].tags[2] & 1u) != 0)
            continue;
        key = hash_bytes(key, &i, sizeof(i));
        key = hash_bytes(key, gpu->lights[i].position, sizeof(gpu->lights[i].position));
        key = hash_bytes(key, gpu->lights[i].tags, sizeof(gpu->lights[i].tags));
    }
    if (key != gpu->static_key) {
        memset(gpu->valid_chunks, 0, (size_t)gpu->chunks_x * gpu->chunks_y);
        gpu->static_key = key;
    }
    int x0 = (int)floorf(gpu->frame.origin[0] / GPU_CHUNK),
        y0 = (int)floorf(gpu->frame.origin[1] / GPU_CHUNK);
    int x1 = (int)floorf((gpu->frame.origin[0] + gpu->width / gpu->frame.scale) / GPU_CHUNK),
        y1 = (int)floorf((gpu->frame.origin[1] + gpu->height / gpu->frame.scale) / GPU_CHUNK);
    if (x0 < 0)
        x0 = 0;
    if (y0 < 0)
        y0 = 0;
    if (x1 >= gpu->chunks_x)
        x1 = gpu->chunks_x - 1;
    if (y1 >= gpu->chunks_y)
        y1 = gpu->chunks_y - 1;
    shadow_uniforms(gpu, gpu->cache);
    rlBindImageTexture(gpu->sun_cache.id, 0, gpu->sun_cache.format, false);
    rlBindImageTexture(gpu->point_cache.id, 1, gpu->point_cache.format, false);
    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++)
            if (!gpu->valid_chunks[y * gpu->chunks_x + x]) {
                glUniform4i(glGetUniformLocation(gpu->cache.id, "uRegion"), x * GPU_CHUNK, y * GPU_CHUNK,
                            GPU_CHUNK, GPU_CHUNK);
                rlComputeShaderDispatch(GPU_CHUNK / 8, GPU_CHUNK / 8, 1);
                gpu->valid_chunks[y * gpu->chunks_x + x] = 1;
                gpu->stats.static_updates++;
            }
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
}
int castle_gpu_render(CastleGpu *gpu) {
    if (!gpu || gpu->failed)
        return 0;
    rlDrawRenderBatchActive();
    GLint old_fbo = 0, old_viewport[4], old_vao = 0, old_program = 0, old_active = 0, old_texture = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &old_fbo);
    glGetIntegerv(GL_VIEWPORT, old_viewport);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &old_vao);
    glGetIntegerv(GL_CURRENT_PROGRAM, &old_program);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &old_active);
    glActiveTexture(GL_TEXTURE0);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &old_texture);
    GLboolean old_blend = glIsEnabled(GL_BLEND), old_depth = glIsEnabled(GL_DEPTH_TEST);
    GLboolean old_cull = glIsEnabled(GL_CULL_FACE), old_dither = glIsEnabled(GL_DITHER);
    GLint old_src_rgb, old_dst_rgb, old_src_alpha, old_dst_alpha, old_depth_func;
    GLboolean old_depth_mask;
    glGetIntegerv(GL_BLEND_SRC_RGB, &old_src_rgb);
    glGetIntegerv(GL_BLEND_DST_RGB, &old_dst_rgb);
    glGetIntegerv(GL_BLEND_SRC_ALPHA, &old_src_alpha);
    glGetIntegerv(GL_BLEND_DST_ALPHA, &old_dst_alpha);
    glGetIntegerv(GL_DEPTH_FUNC, &old_depth_func);
    glGetBooleanv(GL_DEPTH_WRITEMASK, &old_depth_mask);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DITHER);
    CastleGpuViewport viewport = castle_gpu_viewport(&gpu->frame);
    int okay = targets_resize(gpu, viewport.width, viewport.height) &&
               world_resize(gpu, gpu->frame.map_width * 16, gpu->frame.map_height * 16);
    okay =
        okay && buffer_reserve(gpu, &gpu->sprite_buffer,
                               (size_t)(gpu->sprite_count ? gpu->sprite_count : 1) * sizeof(CastleGpuSprite));
    okay =
        okay && buffer_reserve(gpu, &gpu->caster_buffer,
                               (size_t)(gpu->caster_count ? gpu->caster_count : 1) * sizeof(CastleGpuCaster));
    okay = okay &&
           buffer_reserve(gpu, &gpu->emitter_buffer,
                          (size_t)(gpu->emitter_count ? gpu->emitter_count : 1) * sizeof(CastleGpuEmitter));
    float right = gpu->frame.origin[0] + gpu->width / gpu->frame.scale,
          bottom = gpu->frame.origin[1] + gpu->height / gpu->frame.scale;
    gpu->tile_origin[0] = floorf(gpu->frame.origin[0] / GPU_CHUNK) * GPU_CHUNK;
    gpu->tile_origin[1] = floorf(gpu->frame.origin[1] / GPU_CHUNK) * GPU_CHUNK;
    gpu->tile_width = ((int)ceilf((right - gpu->tile_origin[0]) / GPU_CHUNK) + 1) * 8;
    gpu->tile_height = ((int)ceilf((bottom - gpu->tile_origin[1]) / GPU_CHUNK) + 1) * 8;
    okay = okay &&
           buffer_reserve(gpu, &gpu->tile_buffer,
                          (size_t)gpu->tile_width * gpu->tile_height * GPU_TILE_STRIDE * sizeof(uint32_t));
    okay = okay &&
           buffer_reserve(gpu, &gpu->static_tiles,
                          (size_t)gpu->tile_width * gpu->tile_height * GPU_TILE_STRIDE * sizeof(uint32_t));
    okay = okay &&
           buffer_reserve(gpu, &gpu->light_buffer,
                          (size_t)(gpu->light_capacity ? gpu->light_capacity : 1) * sizeof(*gpu->lights));
    okay = okay && buffer_reserve(gpu, &gpu->light_tiles,
                                  (size_t)gpu->tile_width * gpu->tile_height *
                                      (size_t)(gpu->light_capacity + 1) * sizeof(uint32_t));
    if (!okay) {
        gpu->failed = 1;
        goto restore;
    }
    timer_poll(gpu);
    seed_update(gpu);
    timer_start(gpu, 0);
    if (gpu->frame.natural && !contacts_update(gpu))
        gpu->failed = 1;
    if (gpu->sprite_count)
        rlUpdateShaderBuffer(gpu->sprite_buffer.id, gpu->sprites,
                             (unsigned int)((size_t)gpu->sprite_count * sizeof(*gpu->sprites)), 0);
    if (gpu->caster_count)
        rlUpdateShaderBuffer(gpu->caster_buffer.id, gpu->casters,
                             (unsigned int)((size_t)gpu->caster_count * sizeof(*gpu->casters)), 0);
    for (int k = 0; k < gpu->light_count; k++) {
        gpu->lights[k].tags[1] = 0;
        for (int i = 0; i < gpu->caster_count; i++)
            if (inside_box(&gpu->casters[i], gpu->lights[k].position)) {
                gpu->lights[k].tags[1] = gpu->casters[i].tags[0];
                break;
            }
    }
    if (gpu->light_count)
        rlUpdateShaderBuffer(gpu->light_buffer.id, gpu->lights,
                             (unsigned int)((size_t)gpu->light_count * sizeof(*gpu->lights)), 0);
    if (gpu->emitter_count)
        rlUpdateShaderBuffer(gpu->emitter_buffer.id, gpu->emitters,
                             (unsigned int)((size_t)gpu->emitter_count * sizeof(*gpu->emitters)), 0);
    if (gpu->frame.natural && !terrain_update(gpu))
        gpu->failed = 1;
    glUseProgram(gpu->fields.id);
    uniform_v2(gpu->fields, "uMap", (float)gpu->world_width, (float)gpu->world_height);
    uniform_f(gpu->fields, "uTime", gpu->frame.time);
    rlBindShaderBuffer(gpu->terrain_buffer.id, 3);
    glUniform2i(glGetUniformLocation(gpu->fields.id, "uMapCells"), gpu->frame.map_width,
                gpu->frame.map_height);
    uniform_i(gpu->fields, "uTerrainReady", gpu->terrain_buffer.id != 0);
    uniform_i(gpu->fields, "uWaterEnabled", gpu->frame.water);
    uniform_i(gpu->fields, "uWindEnabled", gpu->frame.foliage || gpu->frame.grass || gpu->frame.smoke);
    uniform_i(gpu->fields, "uNatural", gpu->frame.natural);
    uniform_i(gpu->fields, "uWindCasterCount", gpu->caster_count);
    rlBindShaderBuffer(gpu->caster_buffer.id, 0);
    rlBindImageTexture(gpu->wind.id, 0, gpu->wind.format, false);
    rlBindImageTexture(gpu->waves.id, 1, gpu->waves.format, false);
    rlComputeShaderDispatch(32, 32, 1);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
    unsigned int command[4] = {48, 0, 0, 0};
    rlUpdateShaderBuffer(gpu->command_buffer.id, command, sizeof(command), 0);
    if (gpu->frame.grass && gpu->frame.natural && gpu->terrain_buffer.id) {
        unsigned int cells = (unsigned int)(gpu->frame.map_width * gpu->frame.map_height);
        unsigned int groups = (cells + 63) / 64;
        int ready = buffer_reserve(gpu, &gpu->grass_counts, cells * 2 * sizeof(uint32_t)) &&
                    buffer_reserve(gpu, &gpu->grass_offsets, cells * 2 * sizeof(uint32_t)) &&
                    buffer_reserve(gpu, &gpu->grass_sums, (groups * 2 + 258) * sizeof(uint32_t)) &&
                    buffer_reserve(gpu, &gpu->grass_group_offsets, groups * 2 * sizeof(uint32_t));
        if (!ready)
            gpu->failed = 1;
        else {
            GpuProgram p = gpu->meadow;
            terrain_uniforms(gpu, p);
            rlBindShaderBuffer(gpu->grass_counts.id, 0);
            rlBindShaderBuffer(gpu->grass_offsets.id, 1);
            rlBindShaderBuffer(gpu->command_buffer.id, 2);
            rlBindShaderBuffer(gpu->grass_sums.id, 5);
            rlBindShaderBuffer(gpu->grass_group_offsets.id, 6);
            rlBindShaderBuffer(gpu->grass_buffer.id, 7);
            uniform_v2(p, "uOrigin", gpu->frame.origin[0], gpu->frame.origin[1]);
            uniform_v2(p, "uViewport", (float)gpu->width, (float)gpu->height);
            uniform_f(p, "uScale", gpu->frame.scale);
            float area = (float)gpu->width * gpu->height / (gpu->frame.scale * gpu->frame.scale * 256);
            float density =
                gpu->frame.grass_density > 0 ? gpu->frame.grass_density : 12000 / fmaxf(1, area * 24);
            density = fminf(1, density);
            if (gpu->grass_density <= 0 || fabsf(gpu->grass_density - density) > .04f)
                gpu->grass_density = density;
            uniform_f(p, "uDensity", gpu->grass_density);
            uniform_i(
                p, "uLimit",
                gpu->frame.grass_limit > 0
                    ? (gpu->frame.grass_limit < GPU_GRASS_LIMIT ? gpu->frame.grass_limit : GPU_GRASS_LIMIT)
                    : GPU_GRASS_LIMIT);
            for (int pass = 0; pass < 7; pass++) {
                uniform_i(p, "uPass", pass);
                unsigned int dispatch = pass == 0   ? 5
                                        : pass == 2 ? 1
                                        : pass == 5 ? (groups + 63) / 64
                                                    : groups;
                rlComputeShaderDispatch(dispatch, 1, 1);
                glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_COMMAND_BARRIER_BIT);
            }
        }
    } else if (gpu->frame.grass && gpu->terrain_buffer.id) {
        glUseProgram(gpu->grass.id);
        rlBindShaderBuffer(gpu->terrain_buffer.id, 0);
        rlBindShaderBuffer(gpu->grass_buffer.id, 1);
        rlBindShaderBuffer(gpu->command_buffer.id, 2);
        glUniform2i(glGetUniformLocation(gpu->grass.id, "uMapCells"), gpu->frame.map_width,
                    gpu->frame.map_height);
        uniform_v2(gpu->grass, "uOrigin", gpu->frame.origin[0], gpu->frame.origin[1]);
        uniform_v2(gpu->grass, "uViewport", (float)gpu->width, (float)gpu->height);
        uniform_f(gpu->grass, "uScale", gpu->frame.scale);
        float visible_width =
            fmaxf(0, fminf(right, (float)gpu->world_width) - fmaxf(0, gpu->frame.origin[0]));
        float visible_height =
            fmaxf(0, fminf(bottom, (float)gpu->world_height) - fmaxf(0, gpu->frame.origin[1]));
        float area = visible_width * visible_height / 256.0f;
        float density =
            gpu->frame.grass_density > 0 ? gpu->frame.grass_density : 12000 / (fmaxf(1, area) * 4);
        uniform_f(gpu->grass, "uDensity", fminf(1, density));
        uniform_i(gpu->grass, "uLimit", GPU_GRASS_LIMIT);
        rlComputeShaderDispatch((unsigned int)(gpu->frame.map_width * gpu->frame.map_height + 63) / 64, 1, 1);
    }
    glUseProgram(gpu->particles.id);
    rlBindShaderBuffer(gpu->particle_buffer.id, 0);
    rlBindShaderBuffer(gpu->emitter_buffer.id, 1);
    uniform_i(gpu->particles, "uEmitterCount", gpu->frame.smoke ? gpu->emitter_count : 0);
    uniform_i(gpu->particles, "uParticleCount", GPU_PARTICLES);
    uniform_i(gpu->particles, "uNatural", gpu->frame.natural);
    uniform_i(gpu->particles, "uProbe", gpu->frame.smoke_probe);
    uniform_f(gpu->particles, "uDelta", fmaxf(0, fminf(.05f, gpu->frame.delta)));
    uniform_f(gpu->particles, "uTime", gpu->frame.time);
    uniform_v2(gpu->particles, "uMap", (float)gpu->world_width, (float)gpu->world_height);
    sampler(gpu->particles, "uWind", gpu->wind, 0);
    rlComputeShaderDispatch(GPU_PARTICLES / 64, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_COMMAND_BARRIER_BIT);
    timer_end();
    timer_start(gpu, 1);
    glBindFramebuffer(GL_FRAMEBUFFER, gpu->framebuffer);
    glViewport(0, 0, gpu->width, gpu->height);
    const GLenum attachments[4] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2,
                                   GL_COLOR_ATTACHMENT3};
    glDrawBuffers(4, attachments);
    glDisable(GL_BLEND);
    /* 색만 합성하고 노멀·높이·소유자 번호는 가장 앞의 픽셀을 그대로 저장합니다. */
    glEnablei(GL_BLEND, 0);
    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_TRUE);
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    if (gpu->frame.natural && !gpu->failed) {
        GpuProgram p = gpu->terrain_graphics;
        terrain_uniforms(gpu, p);
        uniform_v2(p, "uOrigin", gpu->frame.origin[0], gpu->frame.origin[1]);
        uniform_v2(p, "uViewport", (float)gpu->width, (float)gpu->height);
        uniform_v2(p, "uMap", (float)gpu->world_width, (float)gpu->world_height);
        uniform_f(p, "uScale", gpu->frame.scale);
        uniform_f(p, "uTime", gpu->frame.time);
        uniform_i(p, "uWater", gpu->frame.water);
        uniform_i(p, "uGroundCasterCount", gpu->caster_count);
        rlBindShaderBuffer(gpu->caster_buffer.id, 6);
        sampler(p, "uContacts", gpu->contact_atlas.id ? gpu->contact_atlas : gpu->white, 7);
        sampler(p, "uWaves", gpu->waves, 4);
        glBindVertexArray(gpu->vao);
        glDrawArrays(GL_TRIANGLES, 0, 3);
    }
    geometry_uniforms(gpu);
    rlBindShaderBuffer(gpu->sprite_buffer.id, 0);
    rlBindShaderBuffer(gpu->grass_buffer.id, 1);
    uniform_i(gpu->geometry, "uGrass", 0);
    for (int i = 0; i < gpu->batch_count; i++) {
        GpuBatch *batch = &gpu->batches[i];
        sampler(gpu->geometry, "uAlbedo", batch->texture, 0);
        sampler(gpu->geometry, "uNormals", batch->normal.id ? batch->normal : gpu->white, 1);
        sampler(gpu->geometry, "uSurface", batch->surface.id ? batch->surface : gpu->white, 2);
        uniform_i(gpu->geometry, "uBase", batch->first);
        glDrawArraysInstanced(GL_TRIANGLES, 0, 48, batch->count);
    }
    if (gpu->frame.grass && gpu->terrain_buffer.id) {
        uniform_i(gpu->geometry, "uGrass", 1);
        sampler(gpu->geometry, "uAlbedo", gpu->scenery.id ? gpu->scenery : gpu->white, 0);
        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, gpu->command_buffer.id);
        glDrawArraysIndirect(GL_TRIANGLES, NULL);
        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
    }
    glMemoryBarrier(GL_FRAMEBUFFER_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
    timer_end();
    timer_start(gpu, 2);
    shadow_uniforms(gpu, gpu->bins);
    rlComputeShaderDispatch((unsigned int)(gpu->tile_width + 7) / 8, (unsigned int)(gpu->tile_height + 7) / 8,
                            1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    glUseProgram(gpu->light_bins.id);
    rlBindShaderBuffer(gpu->light_buffer.id, 1);
    rlBindShaderBuffer(gpu->light_tiles.id, 3);
    uniform_i(gpu->light_bins, "uLightCount", gpu->light_count);
    uniform_i(gpu->light_bins, "uLightTileStride", gpu->light_capacity + 1);
    uniform_v2(gpu->light_bins, "uTileOrigin", gpu->tile_origin[0], gpu->tile_origin[1]);
    glUniform2i(glGetUniformLocation(gpu->light_bins.id, "uTileSize"), gpu->tile_width, gpu->tile_height);
    rlComputeShaderDispatch((unsigned int)(gpu->tile_width + 7) / 8, (unsigned int)(gpu->tile_height + 7) / 8,
                            1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    if (gpu->frame.shadows)
        cache_update(gpu);
    timer_end();
    timer_start(gpu, 3);
    shadow_uniforms(gpu, gpu->lighting);
    rlBindShaderBuffer(gpu->light_tiles.id, 3);
    uniform_i(gpu->lighting, "uLightTileStride", gpu->light_capacity + 1);
    uniform_f(gpu->lighting, "uTime", gpu->frame.time);
    uniform_f(gpu->lighting, "uOvercast", gpu->frame.overcast);
    uniform_i(gpu->lighting, "uWater", gpu->frame.water);
    sampler(gpu->lighting, "uAlbedo", gpu->gbuffer[0], 0);
    sampler(gpu->lighting, "uNormalHeight", gpu->gbuffer[1], 1);
    sampler(gpu->lighting, "uMaterial", gpu->gbuffer[2], 2);
    sampler(gpu->lighting, "uOwner", gpu->gbuffer[3], 3);
    sampler(gpu->lighting, "uSunCache", gpu->sun_cache, 4);
    sampler(gpu->lighting, "uPointCache", gpu->point_cache, 5);
    uniform_v2(gpu->lighting, "uOrigin", gpu->frame.origin[0], gpu->frame.origin[1]);
    uniform_v2(gpu->lighting, "uViewport", (float)gpu->width, (float)gpu->height);
    uniform_v2(gpu->lighting, "uMap", (float)gpu->world_width, (float)gpu->world_height);
    uniform_f(gpu->lighting, "uScale", gpu->frame.scale);
    uniform_f(gpu->lighting, "uAmbient", gpu->frame.ambient);
    uniform_f(gpu->lighting, "uElevation", gpu->frame.sun[3]);
    uniform_i(gpu->lighting, "uShadows", gpu->frame.shadows);
    uniform_i(gpu->lighting, "uView", gpu->frame.view);
    rlBindImageTexture(gpu->output.id, 0, gpu->output.format, false);
    rlComputeShaderDispatch((unsigned int)(gpu->width + 7) / 8, (unsigned int)(gpu->height + 7) / 8, 1);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT |
                    GL_FRAMEBUFFER_BARRIER_BIT);
    timer_end();
    timer_start(gpu, 4);
    if (gpu->frame.smoke && gpu->emitter_count && gpu->frame.view == 0) {
        glBindFramebuffer(GL_FRAMEBUFFER,
                          gpu->frame.natural ? gpu->smoke_framebuffer : gpu->output_framebuffer);
        glDrawBuffer(GL_COLOR_ATTACHMENT0);
        if (gpu->frame.natural) {
            glClearColor(0, 0, 0, 0);
            glClear(GL_COLOR_BUFFER_BIT);
        }
        glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        glUseProgram(gpu->smoke.id);
        rlBindShaderBuffer(gpu->particle_buffer.id, 0);
        rlBindShaderBuffer(gpu->light_buffer.id, 1);
        rlBindShaderBuffer(gpu->light_tiles.id, 3);
        uniform_i(gpu->smoke, "uLightCount", gpu->light_count);
        uniform_i(gpu->smoke, "uLightTileStride", gpu->light_capacity + 1);
        uniform_i(gpu->smoke, "uSunEnabled", gpu->frame.sun_enabled);
        glUniform3fv(glGetUniformLocation(gpu->smoke.id, "uSunDirection"), 1, gpu->frame.sun);
        uniform_v2(gpu->smoke, "uTileOrigin", gpu->tile_origin[0], gpu->tile_origin[1]);
        glUniform2i(glGetUniformLocation(gpu->smoke.id, "uTileSize"), gpu->tile_width, gpu->tile_height);
        sampler(gpu->smoke, "uNormalHeight", gpu->gbuffer[1], 0);
        sampler(gpu->smoke, "uPuffs", gpu->scenery.id ? gpu->scenery : gpu->white, 1);
        uniform_v2(gpu->smoke, "uOrigin", gpu->frame.origin[0], gpu->frame.origin[1]);
        uniform_v2(gpu->smoke, "uViewport", (float)gpu->width, (float)gpu->height);
        uniform_f(gpu->smoke, "uScale", gpu->frame.scale);
        uniform_f(gpu->smoke, "uAmbient", gpu->frame.ambient);
        uniform_i(gpu->smoke, "uNatural", gpu->frame.natural);
        uniform_f(gpu->smoke, "uPuffScale",
                  gpu->frame.smoke_scale > 0 ? gpu->frame.smoke_scale
                  : gpu->frame.natural       ? 4.0f
                                             : 1.0f);
        glDrawArraysInstanced(GL_TRIANGLES, 0, 6,
                              gpu->emitter_count * 32 < GPU_PARTICLES ? gpu->emitter_count * 32
                                                                      : GPU_PARTICLES);
        if (gpu->frame.natural) {
            glMemoryBarrier(GL_FRAMEBUFFER_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
            glUseProgram(gpu->smoke_composite.id);
            sampler(gpu->smoke_composite, "uSmokeLayer", gpu->smoke_layer, 0);
            rlBindImageTexture(gpu->output.id, 0, gpu->output.format, false);
            rlComputeShaderDispatch((unsigned int)(gpu->width + 7) / 8, (unsigned int)(gpu->height + 7) / 8,
                                    1);
            glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
        }
    }
    timer_end();
    gpu->stats.frames++;
    gpu->stats.sprites = gpu->sprite_count;
    gpu->stats.casters = gpu->caster_count;
    gpu->stats.lights = gpu->light_count;
    gpu->stats.emitters = gpu->emitter_count;
    gpu->stats.smoke_particles =
        gpu->frame.smoke ? (gpu->emitter_count * 32 < GPU_PARTICLES ? gpu->emitter_count * 32 : GPU_PARTICLES)
                         : 0;
    gpu->stats.live_objects = objects;
    gpu->stats.logical_width = gpu->width;
    gpu->stats.logical_height = gpu->height;
    gpu->stats.pixel_scale = gpu->frame.pixel_scale;
restore:
    glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)old_fbo);
    glViewport(old_viewport[0], old_viewport[1], old_viewport[2], old_viewport[3]);
    glUseProgram((GLuint)old_program);
    glBindVertexArray((GLuint)old_vao);
    if (old_blend)
        glEnable(GL_BLEND);
    else
        glDisable(GL_BLEND);
    if (old_depth)
        glEnable(GL_DEPTH_TEST);
    else
        glDisable(GL_DEPTH_TEST);
    if (old_cull)
        glEnable(GL_CULL_FACE);
    else
        glDisable(GL_CULL_FACE);
    if (old_dither)
        glEnable(GL_DITHER);
    else
        glDisable(GL_DITHER);
    glBlendFuncSeparate((GLenum)old_src_rgb, (GLenum)old_dst_rgb, (GLenum)old_src_alpha,
                        (GLenum)old_dst_alpha);
    glDepthFunc((GLenum)old_depth_func);
    glDepthMask(old_depth_mask);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, (GLuint)old_texture);
    glActiveTexture((GLenum)old_active);
    return !gpu->failed;
}
void castle_gpu_present(CastleGpu *gpu, Rectangle destination) {
    if (gpu && gpu->output.id) {
        if (gpu->frame.pixel_scale > 0) {
            CastleGpuViewport v = castle_gpu_viewport(&gpu->frame);
            destination.x += v.destination.x;
            destination.y += v.destination.y;
            destination.width = v.destination.width;
            destination.height = v.destination.height;
        }
        DrawTexturePro(gpu->output, (Rectangle){0, 0, (float)gpu->width, -(float)gpu->height}, destination,
                       (Vector2){0, 0}, 0, WHITE);
    }
}
CastleGpuStats castle_gpu_stats(const CastleGpu *gpu) { return gpu ? gpu->stats : (CastleGpuStats){0}; }
Texture2D castle_gpu_output(const CastleGpu *gpu) { return gpu ? gpu->output : (Texture2D){0}; }
/* 세계 표시 여유와 실제 탐험 상태는 서로 다른 입력으로 유지합니다. */
Texture2D castle_gpu_clouds(CastleGpu *gpu, const unsigned char *visibility, int width, int height) {
    if (!gpu || !visibility || width < 1 || height < 1)
        return (Texture2D){0};
    rlDrawRenderBatchActive();
    GLint old_program = 0, old_active = 0, old_texture = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &old_program);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &old_active);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &old_texture);
    CastleGpuViewport view = castle_gpu_viewport(&gpu->frame);
    if (gpu->fog_layer.width != view.width || gpu->fog_layer.height != view.height) {
        texture_free(gpu, &gpu->fog_layer);
        gpu->fog_layer = texture_new(gpu, view.width, view.height, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    }
    size_t count = (size_t)width * height;
    if (count > gpu->fog_capacity) {
        unsigned int *replacement = realloc(gpu->fog_words, count * sizeof(*replacement));
        if (!replacement) {
            gpu->failed = 1;
            goto restore_fog;
        }
        gpu->fog_words = replacement;
        gpu->fog_capacity = count;
    }
    if (!gpu->fog_layer.id || !buffer_reserve(gpu, &gpu->fog_buffer, count * sizeof(unsigned int))) {
        gpu->failed = 1;
        goto restore_fog;
    }
    for (size_t i = 0; i < count; i++)
        gpu->fog_words[i] = visibility[i];
    rlUpdateShaderBuffer(gpu->fog_buffer.id, gpu->fog_words, (unsigned int)(count * sizeof(unsigned int)), 0);
    glUseProgram(gpu->fog.id);
    rlBindShaderBuffer(gpu->fog_buffer.id, 3);
    rlBindImageTexture(gpu->fog_layer.id, 0, gpu->fog_layer.format, false);
    glUniform2i(glGetUniformLocation(gpu->fog.id, "uMapCells"), width, height);
    glUniform1ui(glGetUniformLocation(gpu->fog.id, "uWorldSeed"), gpu->frame.world_seed);
    uniform_v2(gpu->fog, "uViewport", (float)view.width, (float)view.height);
    uniform_v2(gpu->fog, "uOrigin", gpu->frame.origin[0], gpu->frame.origin[1]);
    uniform_f(gpu->fog, "uScale", gpu->frame.scale);
    uniform_f(gpu->fog, "uTime", gpu->frame.time);
    uniform_f(gpu->fog, "uAmbient", gpu->frame.ambient);
    uniform_f(gpu->fog, "uElevation", gpu->frame.sun[3]);
    uniform_i(gpu->fog, "uSunEnabled", gpu->frame.sun_enabled);
    rlComputeShaderDispatch((unsigned int)(view.width + 7) / 8, (unsigned int)(view.height + 7) / 8, 1);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
restore_fog:
    glUseProgram((unsigned int)old_program);
    glActiveTexture((GLenum)old_active);
    glBindTexture(GL_TEXTURE_2D, (unsigned int)old_texture);
    return gpu->failed ? (Texture2D){0} : gpu->fog_layer;
}
Texture2D castle_gpu_data_texture(const CastleGpu *gpu, int index) {
    if (!gpu)
        return (Texture2D){0};
    if (index >= 0 && index < 4)
        return gpu->gbuffer[index];
    return index == 4   ? gpu->contact_atlas
           : index == 5 ? gpu->terrain_fields
           : index == 6 ? gpu->smoke_layer
                        : (Texture2D){0};
}
static float half_value(uint16_t bits) {
    int exponent = (bits >> 10) & 31, fraction = bits & 1023;
    float value =
        exponent ? ldexpf(1.0f + fraction / 1024.0f, exponent - 15) : ldexpf(fraction / 1024.0f, -14);
    return bits & 0x8000 ? -value : value;
}
size_t castle_gpu_grass_snapshot(CastleGpu *gpu, uint32_t *seeds, size_t capacity) {
    if (!gpu || !gpu->command_buffer.id)
        return 0;
    uint32_t command[4];
    glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);
    rlReadShaderBuffer(gpu->command_buffer.id, command, sizeof(command), 0);
    if (command[1] > GPU_GRASS_LIMIT || command[1] > capacity || !seeds)
        return 0;
    CastleGpuSprite *plants = calloc(command[1] ? command[1] : 1, sizeof(*plants));
    if (!plants)
        return 0;
    if (command[1])
        rlReadShaderBuffer(gpu->grass_buffer.id, plants, command[1] * sizeof(*plants), 0);
    for (uint32_t i = 0; i < command[1]; i++)
        seeds[i] = plants[i].tags[3];
    free(plants);
    return command[1];
}
int castle_gpu_validate_scenery(CastleGpu *gpu) {
    if (!gpu || !gpu->frame.natural || castle_gpu_validate(gpu))
        return 1;
    int errors = 0, contact_columns = 0, species_mask = 0, root_errors = 0, halo_errors = 0;
    float contact_error = 0, meadow_min = 100, meadow_max = 0;
    Image contacts = LoadImageFromTexture(gpu->contact_atlas);
    Image fields = LoadImageFromTexture(gpu->terrain_fields);
    if (!contacts.data || !fields.data) {
        if (contacts.data)
            UnloadImage(contacts);
        if (fields.data)
            UnloadImage(fields);
        return 1;
    }
    ImageFormat(&contacts, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    for (int b = 0; b < gpu->batch_count; b++) {
        GpuBatch *batch = &gpu->batches[b];
        Image source = LoadImageFromTexture(batch->texture);
        if (!source.data) {
            errors++;
            continue;
        }
        ImageFormat(&source, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
        for (int i = batch->first; i < batch->first + batch->count; i++) {
            CastleGpuSprite *s = &gpu->sprites[i];
            int page = (int)s->contact_profile[0] - 1;
            if (page < 0)
                continue;
            int height = (int)(s->source[3] * source.height + .5f);
            int width = (int)(fabsf(s->source[2]) * source.width + .5f);
            int left = (int)((s->source[0] + (s->source[2] < 0 ? s->source[2] : 0)) * source.width + .5f);
            int top = (int)(s->source[1] * source.height + .5f);
            float strip = s->tags[1] == 2 ? 31 / 255.0f : s->tags[1] == 6 ? 102 / 255.0f : 51 / 255.0f;
            int source_fronts[128];
            float lowest = -1024;
            for (int x = 64; x < 192; x++) {
                float local = (x + .5f) * .5f - 64;
                int sx = left + (int)floorf((local / 64 + .5f) * width), front = -1;
                for (int y = height - 1; y >= (int)(height * (1 - strip)); y--)
                    if (((Color *)source.data)[(top + y) * source.width + sx].a >= 128) {
                        front = y + 1;
                        break;
                    }
                source_fronts[x - 64] = front;
                if (front >= 0)
                    lowest = fmaxf(lowest, (front - height) * 64.0f / width);
            }
            for (int x = 64; x < 192; x++) {
                int front = source_fronts[x - 64];
                if (s->tags[1] == 3 && front >= 0 && (front - height) * 64.0f / width < lowest - 2.0f)
                    front = -1;
                int actual = -1;
                for (int y = 0; y < 128; y++)
                    if (((Color *)contacts.data)[(page / 16 * 128 + y) * contacts.width + page % 16 * 256 + x]
                            .r < 128)
                        actual = y;
                errors += (front < 0) != (actual < 0);
                if (front >= 0 && actual >= 0) {
                    float expected = (front - height) * 64.0f / width;
                    float found = (actual + .5f) * .5f - 48;
                    float error = fabsf(expected - found) * s->destination[2] / 64.0f;
                    contact_error = fmaxf(contact_error, error);
                    errors += error > 1.001f;
                    contact_columns++;
                }
            }
        }
        UnloadImage(source);
    }
    unsigned int command[4];
    glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);
    rlReadShaderBuffer(gpu->command_buffer.id, command, sizeof(command), 0);
    CastleGpuSprite *plants = calloc(command[1] ? command[1] : 1, sizeof(*plants));
    if (!plants || command[1] > GPU_GRASS_LIMIT)
        errors++;
    else if (command[1]) {
        rlReadShaderBuffer(gpu->grass_buffer.id, plants, command[1] * sizeof(*plants), 0);
        for (unsigned int i = 0; i < command[1]; i++) {
            CastleGpuSprite *s = &plants[i];
            float x = s->destination[0] + floorf(s->destination[2] * .5f), y = s->shape[0];
            int cx = (int)floorf(x / 16), cy = (int)floorf(y / 16), species = (int)s->shape[2];
            if (cx < 0 || cy < 0 || cx >= gpu->frame.map_width || cy >= gpu->frame.map_height ||
                (gpu->terrain[cy * gpu->frame.map_width + cx] & 1) == 0 ||
                fabsf(x - floorf(x + .5f)) > .001f || fabsf(y - floorf(y + .5f)) > .001f)
                root_errors++;
            if (species < 0 || species > 4)
                errors++;
            else
                species_mask |= 1 << species;
            if (species == 1) {
                meadow_min = fminf(meadow_min, s->destination[3]);
                meadow_max = fmaxf(meadow_max, s->destination[3]);
            }
            int chunk_x = (int)x / 128, chunk_y = (int)y / 128;
            int page = (int)gpu->page_table[chunk_y * gpu->page_columns + chunk_x] - 1;
            if (page >= 0) {
                int px = page % 8 * 192 + (int)x - chunk_x * 128 + 32;
                int py = page / 8 * 192 + (int)y - chunk_y * 128 + 32;
                const uint16_t *sample = (const uint16_t *)fields.data + ((size_t)py * fields.width + px) * 4;
                root_errors += half_value(sample[0]) < 1.499f || half_value(sample[1]) < 2.499f;
            }
        }
    }
    free(plants);
    int pixel_bytes = GetPixelDataSize(1, 1, fields.format);
    for (int page = 0; page < 64; page++) {
        int key = gpu->page_keys[page] - 1;
        if (key < 0 || gpu->page_dirty[page])
            continue;
        for (int direction = 0; direction < 2; direction++) {
            int next_x = key % gpu->page_columns + (direction == 0);
            int next_y = key / gpu->page_columns + (direction == 1);
            if (next_x >= gpu->page_columns || next_y >= gpu->page_rows)
                continue;
            int next = (int)gpu->page_table[next_y * gpu->page_columns + next_x] - 1;
            if (next < 0 || gpu->page_dirty[next])
                continue;
            for (int y = 0; y < (direction ? 64 : 192); y++)
                for (int x = 0; x < (direction ? 192 : 64); x++) {
                    size_t a = (size_t)(page / 8 * 192 + y + (direction ? 128 : 0)) * fields.width +
                               page % 8 * 192 + x + (direction ? 0 : 128);
                    size_t b = (size_t)(next / 8 * 192 + y) * fields.width + next % 8 * 192 + x;
                    halo_errors +=
                        memcmp((unsigned char *)fields.data + a * pixel_bytes,
                               (unsigned char *)fields.data + b * pixel_bytes, (size_t)pixel_bytes) != 0;
                }
        }
    }
    errors += root_errors + halo_errors;
    int ao_proxy_errors = 0;
    for (int i = 0; i < gpu->caster_count; i++)
        if ((gpu->casters[i].tags[3] & 1u) && gpu->casters[i].contact_profile[0] < .5f)
            for (int j = 0; j < gpu->caster_count; j++)
                if (gpu->casters[j].tags[0] == gpu->casters[i].tags[0] &&
                    gpu->casters[j].contact_profile[0] > .5f) {
                    ao_proxy_errors++;
                    break;
                }
    errors += ao_proxy_errors;
    printf("GPU CONTACT AO: approximate_base_overrides=%d\n", ao_proxy_errors);
    errors += contact_columns == 0;
    if (gpu->frame.grass)
        errors += command[1] == 0 || (species_mask & 2) == 0 || (species_mask & (species_mask - 1)) == 0 ||
                  meadow_min < 5.6f || meadow_max > 13.2f;
    printf("GPU NATURAL: contact_columns=%d contact_error=%.4f roots=%u root_errors=%d species_mask=%d "
           "meadow=%.1f..%.1f halo_errors=%d errors=%d\n",
           contact_columns, contact_error, command[1], root_errors, species_mask, meadow_min, meadow_max,
           halo_errors, errors);
    UnloadImage(contacts);
    UnloadImage(fields);
    return errors != 0;
}
int castle_gpu_validate(CastleGpu *gpu) {
    if (!gpu || gpu->failed)
        return 1;
    size_t words = (size_t)gpu->tile_width * gpu->tile_height * GPU_TILE_STRIDE;
    uint32_t *data = malloc(words * sizeof(*data));
    if (!data)
        return 1;
    glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);
    uint32_t command[4];
    rlReadShaderBuffer(gpu->command_buffer.id, command, sizeof(command), 0);
    gpu->stats.grass_instances = (int)command[1];
    rlReadShaderBuffer(gpu->tile_buffer.id, data, (unsigned int)(words * sizeof(*data)), 0);
    int overflow = 0;
    for (size_t i = 0; i < words; i += GPU_TILE_STRIDE)
        overflow += data[i + 1] != 0;
    free(data);
    gpu->stats.tile_overflows = overflow;
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        fprintf(stderr, "GPU OpenGL error: %u\n", error);
        return 1;
    }
    printf("GPU VALIDATE: tile_overflows=%d fallback=exact objects=%u\n", overflow, objects);
    return 0;
}
