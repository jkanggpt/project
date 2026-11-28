/* 광원은 원화의 창·문·불꽃 위치에 붙이고 체적은 그림자 계산에만 사용합니다. */
#include "scene_profiles.h"
#include "fire_atlas.h"
#include <castle_expansion.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

enum Shape {
    SHAPE_FLAT,
    SHAPE_GABLE,
    SHAPE_TOWER,
    SHAPE_WALL,
    SHAPE_GATE,
    SHAPE_KEEP,
    SHAPE_TENT,
    SHAPE_OPEN,
    SHAPE_PIT
};
enum Policy {
    POLICY_NONE,
    POLICY_HOME,
    POLICY_WORK,
    POLICY_SERVICE,
    POLICY_GUARD,
    POLICY_ALWAYS,
    POLICY_MAGIC
};
typedef struct Anchor {
    float u, v;
} Anchor;
typedef struct SourceProfile {
    int type, shape, policy, light_kind;
    float base[4], half_width[4], depth[4], wall_top[4], roof_top[4];
    Anchor opening[4];
    unsigned pair_mask;
    Anchor chimney[4], feature[4];
    int feature_kind;
} SourceProfile;
#define F(n) n##f
#define V(a, b, c, d) {F(a), F(b), F(c), F(d)}
#define A(a, b, c, d, e, f, g, h)                                                                            \
    {                                                                                                        \
        {F(a), F(b)}, {F(c), F(d)}, {F(e), F(f)}, { F(g), F(h) }                                             \
    }
#define PROFILE(type, shape, policy, kind, base, width, depth, wall, roof, opening, pairs, chimney, feature, \
                feature_kind)                                                                                \
    {CORE_BLD_##type,                                                                                        \
     SHAPE_##shape,                                                                                          \
     POLICY_##policy,                                                                                        \
     GAME_SCENE_##kind,                                                                                      \
     base,                                                                                                   \
     width,                                                                                                  \
     depth,                                                                                                  \
     wall,                                                                                                   \
     roof,                                                                                                   \
     opening,                                                                                                \
     pairs,                                                                                                  \
     chimney,                                                                                                \
     feature,                                                                                                \
     GAME_SCENE_##feature_kind},
static const SourceProfile sources[] = {
#include "../../../art/sources/scene_profiles/buildings.inc"
};
#undef PROFILE
#undef A
#undef V
#undef F

static float bounded(float n, float low, float high) { return fminf(high, fmaxf(low, n)); }
static int eligible(const CoreBuildingView *b, int policy) {
    if (b->state != CORE_BSTATE_ACTIVE)
        return 0;
    switch (policy) {
    case POLICY_HOME:
        return b->residents > 0;
    case POLICY_WORK:
        return b->workers > 0 && b->working_now > 0 && b->operating && b->blocked_reason == 0;
    case POLICY_SERVICE:
        return b->workers > 0 && b->operating;
    case POLICY_GUARD:
        return b->workers > 0;
    case POLICY_ALWAYS:
        return 1;
    case POLICY_MAGIC:
        return b->operating != 0;
    default:
        return 0;
    }
}
int game_scene_lit(const CoreBuildingView *b) {
    if (!b || b->type < 0 || b->type >= CORE_BLD_COUNT)
        return 0;
    return eligible(b, sources[b->type].policy);
}
static void volume(GameSceneProfile *p, int kind, Vector3 center, Vector3 extent, int contact) {
    if (p->volume_count < 8)
        p->volumes[p->volume_count++] = (GameSceneVolume){center, extent, kind, contact};
}
static void light(GameSceneProfile *p, Rectangle picture, Anchor a, int kind, float depth, float strength,
                  float time, int id) {
    if (a.u < 0 || p->light_count >= 8)
        return;
    Vector2 screen = {picture.x + a.u * picture.width, picture.y + a.v * picture.height};
    /* 앞 벽보다 한 화소 밖에서 시작해 벽을 통과하는 광원을 만들지 않습니다. */
    float y = p->front + 1.0f - depth;
    float z = fmaxf(1, (y - screen.y) / .9f);
    float size = bounded(picture.width * .70f, 22, 72);
    Color color = {255, 172, 84, 255};
    if (kind == GAME_SCENE_TORCH) {
        size = bounded(picture.width * .75f, 26, 72);
        color = (Color){255, 156, 66, 255};
    }
    if (kind == GAME_SCENE_FURNACE) {
        size = bounded(picture.width * .95f, 30, 88);
        color = (Color){255, 111, 40, 255};
    }
    if (kind == GAME_SCENE_MAGIC) {
        size = bounded(picture.width * .90f, 30, 96);
        color = (Color){84, 199, 255, 255};
    }
    if (kind == GAME_SCENE_BEACON) {
        size = 144;
        color = (Color){255, 225, 150, 255};
    }
    float flicker = kind == GAME_SCENE_FURNACE || kind == GAME_SCENE_TORCH
                        ? .94f + .04f * sinf(time * 7.7f + (float)id * .73f) +
                              .02f * sinf(time * 13.1f + (float)id * 1.13f)
                        : 1.0f;
    p->lights[p->light_count++] = (GameSceneLight){.position = {screen.x, y, z},
                                                   .screen = screen,
                                                   .color = color,
                                                   .radius = size,
                                                   .strength = strength * flicker,
                                                   .direction = time * .47f + (float)id * .618f,
                                                   .cone = kind == GAME_SCENE_BEACON ? .30f : 0,
                                                   .kind = kind};
}
/* 받침이 땅(ground_y)에 닿는 고정 불. 화면 위치는 아틀라스의 불꽃 중심입니다. */
static void ground_fire(GameSceneProfile *p, int kind, float x, float ground_y, float time, int id) {
    if (p->light_count >= 8)
        return;
    const GameFireRow *row = &GAME_STATIONARY_ROWS[kind == GAME_SCENE_BRAZIER ? GAME_STATIONARY_BRAZIER
                                                                              : GAME_STATIONARY_CAMPFIRE];
    Vector2 screen = {x - (float)row->base_x + row->flame_x, ground_y - (float)(row->base_y + 1) + row->flame_y};
    float z = fmaxf(1, (ground_y - screen.y) / .9f);
    float flicker = .93f + .05f * sinf(time * 8.3f + (float)id * .91f) + .02f * sinf(time * 13.7f + (float)id);
    p->lights[p->light_count++] = (GameSceneLight){.position = {screen.x, ground_y, z},
                                                   .screen = screen,
                                                   .color = kind == GAME_SCENE_BRAZIER ? (Color){255, 150, 62, 255}
                                                                                       : (Color){255, 140, 54, 255},
                                                   .radius = kind == GAME_SCENE_BRAZIER ? 58.0f : 66.0f,
                                                   .strength = .92f * flicker,
                                                   .direction = 0,
                                                   .cone = 0,
                                                   .kind = kind};
}
/* 모든 건물이 아니라 야영지·검문·모임 장소에만 고정 불을 둡니다. */
static int fire_site(int type) {
    switch (type) {
    case CORE_BLD_GATHERER:
    case CORE_BLD_LOGGING_SITE:
        return GAME_SCENE_CAMPFIRE;
    case CORE_BLD_GATE:
    case CORE_BLD_PALISADE_GATE:
    case CORE_BLD_STONE_GATE:
    case CORE_BLD_IRON_GATE:
    case CORE_BLD_TOWER:
    case CORE_BLD_MARKET:
        return GAME_SCENE_BRAZIER;
    default:
        return -1;
    }
}
int game_scene_profile(const CoreBuildingView *b, Rectangle picture, float time, GameSceneProfile *p) {
    if (!p)
        return 0;
    memset(p, 0, sizeof(*p));
    if (!b || b->type < 0 || b->type >= CORE_BLD_COUNT || picture.width <= 0 || picture.height <= 0)
        return 0;
    const SourceProfile *s = &sources[b->type];
    int r = b->rotation & 3;
    if (s->shape == SHAPE_FLAT || b->kind_flags & CORE_EXP_PLOT)
        return 0;
    float x = picture.x + picture.width * .5f, front = picture.y + picture.height * s->base[r];
    float half = picture.width * s->half_width[r];
    float depth = fminf((float)b->h * 16.0f * .84f, picture.height * s->depth[r]);
    float center_y = front - depth * .5f;
    float wall = fmaxf(2, (center_y - (picture.y + picture.height * s->wall_top[r])) / .9f);
    float top = fmaxf(wall, (center_y - (picture.y + picture.height * s->roof_top[r])) / .9f);
    p->front = front;
    p->wall_height = wall;
    p->roof_height = top;
    p->ground = (Rectangle){x - half, front - depth, half * 2, depth};
    if (b->type == CORE_BLD_LIGHTHOUSE) {
        volume(p, 1, (Vector3){x, center_y, wall * .5f}, (Vector3){half, depth * .5f, wall * .5f}, 1);
        float cap_base = fmaxf(wall, (center_y - (picture.y + picture.height * .18f)) / .9f);
        volume(p, 2, (Vector3){x, center_y, cap_base},
               (Vector3){half * .90f, depth * .52f, fmaxf(1, top - cap_base)}, 0);
        /* 등실은 빈 공간으로 남기고 가는 기둥만 빛을 가립니다. */
        for (int side = -1; side <= 1; side += 2)
            volume(p, 0, (Vector3){x + (float)side * half * .72f, center_y, (wall + cap_base) * .5f},
                   (Vector3){fmaxf(.5f, half * .045f), depth * .40f, (cap_base - wall) * .5f}, 0);
    } else if (s->shape == SHAPE_GATE) {
        float post = half * .28f;
        volume(p, 0, (Vector3){x - half + post, center_y, wall * .5f},
               (Vector3){post, depth * .5f, wall * .5f}, 1);
        volume(p, 0, (Vector3){x + half - post, center_y, wall * .5f},
               (Vector3){post, depth * .5f, wall * .5f}, 1);
        volume(p, 0, (Vector3){x, center_y, wall * .87f}, (Vector3){half, depth * .5f, wall * .13f}, 0);
    } else if (s->shape == SHAPE_KEEP) {
        volume(p, 0, (Vector3){x, center_y, wall * .5f}, (Vector3){half * .67f, depth * .5f, wall * .5f}, 1);
        volume(p, 2, (Vector3){x, center_y, wall}, (Vector3){half * .71f, depth * .54f, top - wall}, 0);
        for (int side = -1; side <= 1; side += 2)
            volume(p, 1, (Vector3){x + (float)side * half * .79f, center_y, top * .43f},
                   (Vector3){half * .22f, depth * .43f, top * .43f}, 1);
    } else if (s->shape == SHAPE_OPEN) {
        for (int side = -1; side <= 1; side += 2)
            volume(p, 0, (Vector3){x + (float)side * half * .88f, center_y, wall * .5f},
                   (Vector3){fmaxf(.8f, half * .07f), depth * .45f, wall * .5f}, 1);
    } else if (s->shape == SHAPE_PIT) {
        volume(p, 1, (Vector3){x, center_y, wall * .40f}, (Vector3){half, depth * .5f, wall * .4f}, 1);
        volume(p, 1, (Vector3){x - half * .24f, center_y - depth * .12f, top * .48f},
               (Vector3){half * .58f, depth * .35f, top * .48f}, 0);
    } else {
        int kind = s->shape == SHAPE_TOWER ? 1 : 0;
        volume(p, kind, (Vector3){x, center_y, wall * .5f}, (Vector3){half, depth * .5f, wall * .5f}, 1);
    }
    if (b->type != CORE_BLD_LIGHTHOUSE && s->shape != SHAPE_KEEP && s->shape != SHAPE_WALL &&
        s->shape != SHAPE_PIT && top > wall + .5f)
        volume(p, 2, (Vector3){x, center_y, wall}, (Vector3){half * 1.02f, depth * .54f, top - wall}, 0);
    Anchor chimney = s->chimney[r];
    if (chimney.u >= 0) {
        float sy = picture.y + picture.height * chimney.v, cy = front - depth * .60f;
        float z = fmaxf(3, (cy - sy) / .9f);
        int smoke = eligible(b, s->policy);
        if (s->policy == POLICY_HOME && (b->flags & CORE_BF_COLD))
            smoke = 0;
        p->chimneys[p->chimney_count++] =
            (GameSceneChimney){.position = {picture.x + picture.width * chimney.u, cy, z},
                               .extent = {fmaxf(.8f, picture.width * .025f), fmaxf(.8f, depth * .055f),
                                          fmaxf(1.5f, picture.height * .045f)},
                               .smoke = smoke};
    }
    if (!eligible(b, s->policy))
        return 1;
    float strength = s->light_kind == GAME_SCENE_WINDOW ? .44f : .68f;
    light(p, picture, s->opening[r], s->light_kind, 0, strength, time, b->id);
    if ((s->pair_mask & (1u << (unsigned)r)) && fabsf(s->opening[r].u - .5f) > .08f) {
        Anchor second = {1.0f - s->opening[r].u, s->opening[r].v};
        light(p, picture, second, s->light_kind, 0, strength, time, b->id + 1);
    }
    if (s->feature_kind == GAME_SCENE_FURNACE && (b->type == CORE_BLD_KILN || b->type == CORE_BLD_SMELTER) &&
        !eligible(b, POLICY_WORK))
        return 1;
    light(p, picture, s->feature[r], s->feature_kind, s->feature_kind == GAME_SCENE_BEACON ? depth * .50f : 0,
          s->feature_kind == GAME_SCENE_FURNACE ? .85f
          : s->feature_kind == GAME_SCENE_MAGIC ? .65f
                                                : .90f,
          time, b->id + 2);
    if (b->type == CORE_BLD_KEEP && r == 0) {
        Anchor second = {1.0f - s->feature[r].u, s->feature[r].v};
        light(p, picture, second, GAME_SCENE_TORCH, 0, .90f, time, b->id + 3);
    }
    int site = fire_site(b->type);
    if (site >= 0) {
        /* 정문 쪽 오른편 바닥에 둡니다. 그림 사각형 안에 받침이 들어오도록 제한합니다. */
        float offset = site == GAME_SCENE_CAMPFIRE ? -.30f : .40f;
        float fx = bounded(x + picture.width * offset, picture.x + 8, picture.x + picture.width - 8);
        ground_fire(p, site, fx, fminf(front, picture.y + picture.height - 1), time, b->id + 5);
    }
    return 1;
}
int game_scene_profile_validate(void) {
    int errors = 0, count = (int)(sizeof(sources) / sizeof(sources[0]));
    errors += count != CORE_BLD_COUNT;
    for (int type = 0; type < count; type++) {
        const SourceProfile *s = &sources[type];
        errors += s->type != type;
        for (int rotation = 0; rotation < 4; rotation++) {
            CoreBuildingView b = {.id = type + 1,
                                  .type = type,
                                  .w = 3,
                                  .h = 2,
                                  .state = CORE_BSTATE_ACTIVE,
                                  .workers = 2,
                                  .working_now = 1,
                                  .residents = 3,
                                  .operating = 1,
                                  .rotation = rotation};
            Rectangle picture = {40, 30, 72, 80};
            GameSceneProfile p;
            int present = game_scene_profile(&b, picture, 2, &p);
            errors += (s->shape != SHAPE_FLAT) != present;
            for (int v = 0; v < p.volume_count; v++) {
                Vector3 e = p.volumes[v].extent;
                errors +=
                    !isfinite(e.x) || !isfinite(e.y) || !isfinite(e.z) || e.x <= 0 || e.y <= 0 || e.z <= 0;
            }
            for (int l = 0; l < p.light_count; l++) {
                GameSceneLight *a = &p.lights[l];
                errors += fabsf(a->position.x - a->screen.x) > .001f ||
                          fabsf(a->position.y - a->position.z * .9f - a->screen.y) > .001f;
                errors += a->screen.x < picture.x || a->screen.x > picture.x + picture.width ||
                          a->screen.y < picture.y || a->screen.y > picture.y + picture.height;
                errors += !isfinite(a->position.z) || a->radius <= 0 || a->strength <= 0;
            }
            b.state = CORE_BSTATE_RUINED;
            (void)game_scene_profile(&b, picture, 2, &p);
            errors += p.light_count != 0;
            for (int c = 0; c < p.chimney_count; c++)
                errors += p.chimneys[c].smoke != 0;
            b.state = CORE_BSTATE_ACTIVE;
            b.workers = b.working_now = b.residents = 0;
            b.operating = 0;
            (void)game_scene_profile(&b, picture, 2, &p);
            if (s->policy != POLICY_ALWAYS)
                errors += p.light_count != 0;
            for (int c = 0; c < p.chimney_count; c++)
                errors += p.chimneys[c].smoke != 0;
        }
    }
    printf("SCENE PROFILES: types=%d rotations=%d errors=%d\n", count, count * 4, errors);
    return errors ? 1 : 0;
}
