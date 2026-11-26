/* effects.c — 로직에서 제거된 대상은 충돌 없는 그림 잔상만 남깁니다. */
#include "session.h"
#include <math.h>
#include <string.h>
static GamePoint wildlife_origin(const GameSession *s, int id, GamePoint fallback) {
    for (int i = 0; i < s->animal_count; i++)
        if (s->animals[i].id == id)
            return s->animal_actors[i].position;
    return (GamePoint){fallback.x - 1, fallback.y};
}
GameEffect *game_effect(GameSession *s, int category, int id) {
    unsigned start = ((unsigned)id * 2654435761u + (unsigned)category * 2246822519u) & 2047u;
    for (unsigned n = 0; n < 2048; n++) {
        GameEffect *e = &s->effects[(start + n) & 2047u];
        if (e->id == id && e->category == category)
            return e;
        if (!e->id ||
            (e->seen < s->effect_revision - 2 && (!e->dead || s->visual_time - e->death_started > 1))) {
            memset(e, 0, sizeof(*e));
            e->id = id;
            e->category = category;
            e->attack_started = -1;
            e->hurt_until = -1;
            e->bar_until = -1;
            return e;
        }
    }
    return NULL;
}
static void observe(GameSession *s, int category, int id, int hp, int max, int skin, GamePoint p, int facing,
                    int attack) {
    GameEffect *e = game_effect(s, category, id);
    if (!e)
        return;
    if (e->seen && hp < e->hp) {
        if (category == 0) {
            CoreVillagerView *v = game_villager(s, id);
            if (v && v->mood_source_kind == 2)
                game_hit_fx_direction(s, wildlife_origin(s, v->mood_event_source, p), p, 3);
        }
        e->hurt_started = s->visual_time;
        e->hurt_until = s->visual_time + 0.18f;
        e->bar_until = s->visual_time + 3.0f;
    }
    if (attack && attack != e->attack_tick) {
        e->attack_started = s->visual_time;
        e->attack_tick = attack;
    }
    if (!e->seen && hp < max)
        e->bar_until = s->visual_time + 3.0f;
    e->hp = hp;
    e->max_hp = max;
    e->skin = skin;
    e->position = p;
    e->facing = facing;
    e->seen = s->effect_revision;
    if (hp <= 0 && !e->dead) {
        e->dead = 1;
        e->death_started = s->visual_time;
    }
}
void game_effects_sync(GameSession *s) {
    s->effect_revision++;
    for (int i = 0; i < s->villager_count; i++) {
        CoreVillagerView *v = &s->villagers[i];
        GameActor *a = game_actor(s, v->id);
        if (a)
            observe(s, 0, v->id, v->health, 100, v->appearance + 1, a->position, a->facing, 0);
    }
    for (int i = 0; i < s->animal_count; i++) {
        CoreAnimalView *a = &s->animals[i];
        CoreAnimalInfo info;
        core_expansion_animal_info(a->kind, &info);
        observe(s, 1, a->id, a->hp, info.hp, a->kind, s->animal_actors[i].position,
                s->animal_actors[i].facing, a->attack_remaining_milli > 0 ? a->attack_tick : 0);
        GameEffect *fx = game_effect(s, 1, a->id);
        if (fx)
            fx->owner = a->owner;
        if (fx && a->attack_remaining_milli > 0)
            fx->attack_started = s->visual_time - (2400 - a->attack_remaining_milli) / 12000.0f;
    }
    for (int i = 0; i < s->enemy_count; i++) {
        GameEnemy *e = &s->enemies[i];
        observe(s, 2, e->actor.id, e->hp, e->def.hp, e->def.type, e->actor.position, e->actor.facing, 0);
    }
    /* 사냥·아사한 야생 개체는 새 스냅숏에서 빠져도 마지막 그림으로 사망 연출합니다. */
    for (int i = 0; i < 2048; i++) {
        GameEffect *e = &s->effects[i];
        if (e->id && e->category == 1 && e->seen < s->effect_revision && !e->dead) {
            e->dead = 1;
            e->death_started = s->visual_time;
        }
    }
}
void game_effects_event(GameSession *s, const CoreEventView *event) {
    if (event->type == CORE_EVT_BUILDING_DAMAGED && (event->cause_kind == 2 || event->cause_kind == 3)) {
        CoreBuildingView *b = game_building(s, event->a);
        if (b) {
            GamePoint p = {b->x + b->w * .5f, b->y + b->h * .5f};
            game_hit_fx_direction(s, wildlife_origin(s, event->source_id, p), p,
                                  event->cause_kind == 2 ? 3 : 2);
        }
    }
    if (event->type == CORE_EVT_VILLAGER_DIED && event->b == CORE_DEATH_WILDLIFE) {
        GameEffect *fx = game_effect(s, 0, event->a);
        if (fx)
            game_hit_fx_direction(s, wildlife_origin(s, event->source_id, fx->position), fx->position, 3);
    }

    int category = event->type == CORE_EVT_VILLAGER_DIED ? 0 : event->type == CORE_EVT_ENEMY_KILLED ? 2 : -1;
    if (category >= 0) {
        GameEffect *e = game_effect(s, category, event->a);
        if (e && !e->dead) {
            e->dead = 1;
            e->hp = 0;
            e->death_started = e->hurt_started > s->visual_time ? e->hurt_started : s->visual_time;
        }
    }
}

int game_hit_style(int source) {
    if (source == 50)
        return 1;
    if (source == 100 + CORE_BLD_CANNON_TOWER)
        return 7;
    if (source == 100 + CORE_BLD_ARCANE_TOWER)
        return 6;
    if (source >= 100)
        return 1;
    if (source == 25)
        return 4;
    if (source == 12 || source == 21)
        return 5;
    if (source == 6 || source == 13 || source == 16 || source == 27)
        return 6;
    return 1;
}
void game_hit_fx(GameSession *s, GamePoint position, int style) {
    game_hit_fx_direction(s, (GamePoint){position.x - 1, position.y}, position, style);
}
void game_hit_fx_direction(GameSession *s, GamePoint origin, GamePoint position, int style) {
    int x = (int)position.x, y = (int)position.y;
    if (x < 0 || y < 0 || x >= s->summary.map_width || y >= s->summary.map_height ||
        (s->visibility && s->visibility[y * s->summary.map_width + x] < 2))
        return;
    int n = s->hit_cursor;
    s->hit_cursor = (n + 1) % 160;
    s->hit_fx[n] = (GameHitFx){position, s->visual_time,
                               atan2f(position.y - origin.y, position.x - origin.x) * 57.2957795f, style, 1};
}
