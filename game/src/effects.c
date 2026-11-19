/* effects.c — 로직에서 제거된 대상은 충돌 없는 그림 잔상만 남깁니다. */
#include "session.h"
#include <string.h>
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
        e->hurt_started = s->visual_time;
        e->hurt_until = s->visual_time + 0.18f;
        e->bar_until = s->visual_time + 2.5f;
    }
    if (attack && attack != e->attack_tick) {
        e->attack_started = s->visual_time;
        e->attack_tick = attack;
    }
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
            observe(s, 0, v->id, v->health, 100, v->id, a->position, a->facing, 0);
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
