/* combat.c — 적·경비병·감시탑·투사체. 규칙의 적용은 반드시 공개 보고 API를 거칩니다. */
#include "session.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

enum { TARGET_NONE = 0, TARGET_ENEMY = 1, TARGET_BUILDING = 2, TARGET_VILLAGER = 3, TARGET_ANIMAL = 4 };
static int researched(const GameSession *s, int index) {
    CoreResearchView view;
    return core_get_research(s->world, index, &view) == CORE_OK && view.state == CORE_RSTATE_DONE;
}
void game_combat_clear(GameSession *s) {
    int i;
    for (i = 0; i < s->enemy_count; i++)
        game_path_free(&s->enemies[i].actor.path);
    s->enemy_count = 0;
    s->projectile_count = 0;
    s->raid_id = 0;
    s->tower_count = 0;
    memset(&s->raid, 0, sizeof(s->raid));
}
int game_combat_sync(GameSession *s) {
    CoreEnemyView views[GAME_MAX_ENEMIES];
    int32_t count = 0;
    int i, rc;
    rc = core_get_raid(s->world, &s->raid);
    if (rc != CORE_OK)
        return rc;
    if (s->raid.id != s->raid_id) {
        CoreRaidView raid = s->raid;
        game_combat_clear(s);
        s->raid = raid;
        s->raid_id = raid.id;
        /* 싸움의 연속 위치는 저장 대상이 아니므로 경비병도 코어가 아는 칸에서 다시 시작합니다. */
        for (i = 0; i < s->actor_count; i++) {
            CoreVillagerView *v = game_villager(s, s->actors[i].id);
            if (v && v->at_target)
                s->actors[i].position = (GamePoint){v->loc_x + 0.5f, v->loc_y + 0.5f};
        }
    }
    if (!s->raid_id)
        return CORE_OK;
    rc = core_copy_enemies(s->world, views, GAME_MAX_ENEMIES, &count);
    if (rc != CORE_OK)
        return rc;
    for (i = 0; i < count; i++) {
        int j;
        GameEnemy *enemy = NULL;
        for (j = 0; j < s->enemy_count; j++)
            if (s->enemies[j].actor.id == views[i].id) {
                enemy = &s->enemies[j];
                break;
            }
        if (views[i].state != CORE_ENEMY_ALIVE) {
            if (enemy)
                enemy->hp = 0;
            continue;
        }
        if (!enemy) {
            if (s->enemy_count >= GAME_MAX_ENEMIES)
                return CORE_ERR_BUFFER_TOO_SMALL;
            enemy = &s->enemies[s->enemy_count++];
            memset(enemy, 0, sizeof(*enemy));
            rc = core_get_enemy_def(s->world, views[i].type, &enemy->def);
            if (rc != CORE_OK)
                return rc;
            enemy->actor.id = views[i].id;
            enemy->hp = enemy->def.hp;
            enemy->actor.grid_revision = -1;
            enemy->actor.position = (GamePoint){s->raid.spawn_x + 0.5f, s->raid.spawn_y + 0.5f};
        }
    }
    return CORE_OK;
}
static int guard(const CoreVillagerView *v) {
    return v->state == CORE_VS_GUARD_POST && v->at_target && v->health > 0;
}
static int exposed(const GameSession *s,const CoreVillagerView *v) {
    return v->health > 0 && !v->inside_building && v->state != CORE_VS_LEAVING &&
           (s->summary.ruleset || !(v->state == CORE_VS_FLEE && v->at_target));
}
static GamePoint center(const CoreBuildingView *b) {
    return (GamePoint){b->x + b->w * 0.5f, b->y + b->h * 0.5f};
}
static float building_distance(GamePoint p, const CoreBuildingView *b) {
    float dx = fmaxf(fmaxf(b->x - p.x, 0), p.x - (b->x + b->w));
    float dy = fmaxf(fmaxf(b->y - p.y, 0), p.y - (b->y + b->h));
    return sqrtf(dx * dx + dy * dy);
}
static void hurt_enemy(GameSession *s, int id, int damage) {
    GameEnemy *e = game_enemy(s, id);
    if (!e)
        return;
    e->hp -= damage;
    if (e->hp <= 0) {
        e->hp = 0;
        if (core_report_enemy_killed(s->world, s->raid_id, id) == CORE_OK)
            s->kills++;
    }
}
static void impact(GameSession *s, const GameProjectile *p) {
    if (p->raid_id != s->raid_id)
        return;
    if (p->target_kind == TARGET_ENEMY) {
        GameEnemy *e = game_enemy(s, p->target_id);
        GamePoint where = e ? e->actor.position : p->to;
        int i;
        if (e) {
            e->incoming -= p->damage;
            if (e->incoming < 0)
                e->incoming = 0;
        }
        hurt_enemy(s, p->target_id, p->damage);
        if (p->radius > 0)
            for (i = 0; i < s->enemy_count; i++)
                if (s->enemies[i].actor.id != p->target_id && s->enemies[i].hp > 0 &&
                    game_distance(where, s->enemies[i].actor.position) <= p->radius)
                    hurt_enemy(s, s->enemies[i].actor.id, p->damage);
    } else if (p->target_kind == TARGET_BUILDING) {
        (void)core_report_building_damage(s->world, p->raid_id, p->target_id, p->damage);
    } else if (p->target_kind == TARGET_VILLAGER) {
        (void)core_report_villager_damage(s->world, p->raid_id, p->target_id, p->damage);
    } else if (p->target_kind == TARGET_ANIMAL) {
        int i;
        if (core_expansion_damage_animal(s->world, p->target_id, p->damage) == CORE_OK)
            for (i = 0; i < s->animal_count; i++)
                if (s->animals[i].id == p->target_id) {
                    s->animals[i].hp -= p->damage;
                    if (s->animals[i].hp <= 0)
                        s->animals[i].alive = 0;
                    break;
                }
    }
}
static void fire(GameSession *s, GamePoint from, GamePoint to, int kind, int id, int damage, float radius) {
    GameProjectile p;
    p.from = from;
    p.to = to;
    p.elapsed = 0;
    p.duration = fmaxf(0.5f, game_distance(from, to));
    p.raid_id = s->raid_id;
    p.target_kind = kind;
    p.target_id = id;
    p.damage = damage;
    p.radius = radius;
    if (s->projectile_count == GAME_MAX_PROJECTILES) {
        impact(s, &p);
        return;
    }
    s->projectiles[s->projectile_count++] = p;
    s->shots++;
    if (kind == TARGET_ENEMY) {
        GameEnemy *e = game_enemy(s, id);
        if (e)
            e->incoming += damage;
    }
}
static void projectiles_step(GameSession *s, float minutes) {
    int i = 0;
    while (i < s->projectile_count) {
        GameProjectile *p = &s->projectiles[i];
        p->elapsed += minutes;
        if (p->elapsed >= p->duration) {
            GameProjectile finished = *p;
            s->projectiles[i] = s->projectiles[--s->projectile_count];
            impact(s, &finished);
        } else
            i++;
    }
}
static GameEnemy *nearest_enemy(GameSession *s, GamePoint position, float range, int incoming) {
    GameEnemy *best = NULL;
    int i;
    for (i = 0; i < s->enemy_count; i++) {
        GameEnemy *e = &s->enemies[i];
        float distance;
        if (e->hp <= 0 || (incoming && e->incoming >= e->hp))
            continue;
        if (s->visibility) {
            int x = (int)e->actor.position.x, y = (int)e->actor.position.y;
            if (x < 0 || y < 0 || x >= s->summary.map_width || y >= s->summary.map_height ||
                s->visibility[y * s->summary.map_width + x] < 2)
                continue;
        }
        distance = game_distance(position, e->actor.position);
        if (distance <= range) {
            range = distance;
            best = e;
        }
    }
    return best;
}
static CoreAnimalView *nearest_animal(GameSession *s, GamePoint position, float range) {
    CoreAnimalView *best = NULL;
    int i;
    if (!s->summary.ruleset || !s->visibility)
        return NULL;
    for (i = 0; i < s->animal_count; i++) {
        CoreAnimalView *a = &s->animals[i];
        int x = a->x_milli / 1000, y = a->y_milli / 1000;
        float distance;
        if (!a->alive || a->owner || a->kind < 11 || s->visibility[y * s->summary.map_width + x] < 2)
            continue;
        distance = game_distance(position, (GamePoint){a->x_milli / 1000.0f, a->y_milli / 1000.0f});
        if (distance < range) {
            best = a;
            range = distance;
        }
    }
    return best;
}
static GameTower *tower_state(GameSession *s, int id) {
    int i;
    for (i = 0; i < s->tower_count; i++)
        if (s->towers[i].id == id)
            return &s->towers[i];
    if (s->tower_count == s->tower_capacity) {
        int cap = s->tower_capacity ? s->tower_capacity * 2 : 32;
        GameTower *p = (GameTower *)realloc(s->towers, (size_t)cap * sizeof(*p));
        if (!p) {
            s->last_error = CORE_ERR_OUT_OF_MEMORY;
            return NULL;
        }
        s->towers = p;
        s->tower_capacity = cap;
    }
    s->towers[s->tower_count] = (GameTower){id, 0};
    return &s->towers[s->tower_count++];
}
static void towers_step(GameSession *s, float minutes) {
    int i;
    for (i = 0; i < s->building_count; i++) {
        CoreBuildingView *b = &s->buildings[i];
        CoreBuildingDefView *d = &s->building_defs[b->type];
        GameTower *state;
        GameEnemy *enemy;
        if (b->state != CORE_BSTATE_ACTIVE || d->attack <= 0 || b->type == CORE_BLD_BARRACKS)
            continue;
        if (s->summary.ruleset && (d->category != CORE_CAT_TOWERS || !b->workers))
            continue;
        state = tower_state(s, b->id);
        if (!state)
            return;
        state->cooldown -= minutes;
        if (state->cooldown > 0)
            continue;
        enemy = nearest_enemy(s, center(b), d->attack_range + b->w * 0.5f, 1);
        if (enemy) {
            if (s->summary.ruleset && core_expansion_fire(s->world, b->id) != CORE_OK)
                continue;
            state->cooldown = (float)d->attack_cooldown;
            fire(s, center(b), enemy->actor.position, TARGET_ENEMY, enemy->actor.id, d->attack,
                 b->type == CORE_BLD_CANNON_TOWER ? (researched(s, 12) ? 2.0f : 1.0f) : 0.0f);
        } else {
            CoreAnimalView *animal = nearest_animal(s, center(b), (float)d->attack_range);
            if (animal && core_expansion_fire(s->world, b->id) == CORE_OK) {
                state->cooldown = (float)d->attack_cooldown;
                fire(s, center(b), (GamePoint){animal->x_milli / 1000.0f, animal->y_milli / 1000.0f},
                     TARGET_ANIMAL, animal->id, d->attack, 0);
            }
        }
    }
}
static void guards_step(GameSession *s, float minutes) {
    CoreBuildingDefView *d = &s->building_defs[CORE_BLD_BARRACKS];
    int i;
    for (i = 0; i < s->villager_count; i++) {
        CoreVillagerView *v = &s->villagers[i];
        GameActor *a;
        GameEnemy *enemy;
        CoreAnimalView *animal = NULL;
        GamePoint post, goal;
        int kind = TARGET_NONE, id = 0, barrier;
        if (!guard(v))
            continue;
        a = game_actor(s, v->id);
        if (!a)
            continue;
        post = (GamePoint){v->loc_x + 0.5f, v->loc_y + 0.5f};
        enemy = nearest_enemy(s, a->position, s->summary.ruleset ? 400.0f : 8.0f, 0);
        goal = post;
        if (enemy && (!s->summary.ruleset || s->dispatch_delay <= 0)) {
            goal = enemy->actor.position;
            kind = TARGET_ENEMY;
            id = enemy->actor.id;
        } else if ((animal = nearest_animal(s, a->position, 8.0f)) != NULL) {
            goal = (GamePoint){animal->x_milli / 1000.0f, animal->y_milli / 1000.0f};
            kind = TARGET_ANIMAL;
            id = animal->id;
        }
        if (kind) {
            a->chasing = 1;
            a->chase_goal = goal;
        } else if (a->chasing) {
            goal = a->chase_goal;
            if (game_distance(a->position, goal) < 0.6f) {
                a->chasing = 0;
                goal = post;
            }
        }
        a->cooldown -= minutes;
        if (kind && game_distance(a->position, goal) <= fmaxf(1, (float)d->attack_range)) {
            if (a->cooldown <= 0) {
                a->cooldown = (float)d->attack_cooldown;
                if (kind == TARGET_ENEMY)
                    hurt_enemy(s, id, d->attack);
                else {
                    GameProjectile hit = {0};
                    hit.raid_id = s->raid_id;
                    hit.target_kind = TARGET_ANIMAL;
                    hit.target_id = id;
                    hit.damage = d->attack;
                    impact(s, &hit);
                }
            }
        } else {
            if (a->target_x != (int)goal.x || a->target_y != (int)goal.y ||
                a->grid_revision != s->grid_revision || !a->path.count)
                (void)game_actor_plan(s, a, (int)goal.x, (int)goal.y, 0);
            if (game_actor_walk(s, a, minutes * v->walk_speed / 1000.0f / s->config.minutes_per_tick, 0,
                                &barrier) &&
                !kind)
                a->chasing = 0;
        }
    }
}
static int target_valid(GameSession *s, const GameEnemy *e) {
    if (e->target_kind == TARGET_VILLAGER) {
        CoreVillagerView *v = game_villager(s, e->target_id);
        return v && exposed(s,v);
    }
    if (e->target_kind == TARGET_BUILDING) {
        CoreBuildingView *b = game_building(s, e->target_id);
        return b && b->hp > 0 && b->state != CORE_BSTATE_RUINED;
    }
    return 0;
}
static void choose_target(GameSession *s, GameEnemy *e) {
    float best = 8;
    int i;
    e->target_kind = TARGET_NONE;
    e->target_id = 0;
    for (i = 0; i < s->villager_count; i++) {
        CoreVillagerView *v = &s->villagers[i];
        GameActor *a = game_actor(s, v->id);
        float distance;
        if (!a || !exposed(s,v) || (!guard(v) && e->def.behavior != CORE_BEHAVIOR_MONSTER))
            continue;
        distance = game_distance(e->actor.position, a->position);
        if (distance <= best) {
            best = distance;
            e->target_kind = TARGET_VILLAGER;
            e->target_id = v->id;
        }
    }
    if (e->target_kind != TARGET_NONE)
        return;
    best = 1e9f;
    for (i = 0; i < s->building_count; i++) {
        CoreBuildingView *b = &s->buildings[i];
        float distance;
        if (b->hp <= 0 || b->state == CORE_BSTATE_RUINED || b->type == CORE_BLD_WALL ||
            b->type == CORE_BLD_GATE)
            continue;
        if (e->def.behavior == CORE_BEHAVIOR_THIEF && b->type != CORE_BLD_KEEP &&
            b->type != CORE_BLD_WAREHOUSE)
            continue;
        distance = game_distance(e->actor.position, center(b));
        if (e->def.behavior == CORE_BEHAVIOR_RAIDER)
            distance +=
                game_distance(center(b), (GamePoint){s->summary.keep_x + 0.5f, s->summary.keep_y + 0.5f}) *
                0.3f;
        if (e->def.behavior == CORE_BEHAVIOR_MONSTER && b->type == CORE_BLD_HOUSE)
            distance *= 0.7f;
        if (distance < best) {
            best = distance;
            e->target_kind = TARGET_BUILDING;
            e->target_id = b->id;
        }
    }
}
static GamePoint building_approach(const GameSession *s, const CoreBuildingView *b, GamePoint from) {
    GamePoint best = {b->door_x + 0.5f, b->door_y + 0.5f};
    float distance = 1e9f;
    int x, y, w = s->summary.map_width, h = s->summary.map_height;
    for (y = b->y - 1; y <= b->y + b->h; y++)
        for (x = b->x - 1; x <= b->x + b->w; x++) {
            GamePoint p = {x + 0.5f, y + 0.5f};
            float d;
            if (x < 0 || y < 0 || x >= w || y >= h ||
                (x >= b->x && y >= b->y && x < b->x + b->w && y < b->y + b->h))
                continue;
            if (!game_path_cost(s->cost[y * w + x], 1))
                continue;
            d = game_distance(from, p);
            if (d < distance) {
                best = p;
                distance = d;
            }
        }
    return best;
}
static void attack(GameSession *s, GameEnemy *e, int kind, int id, GamePoint to) {
    if (e->actor.cooldown > 0)
        return;
    e->actor.cooldown = (float)e->def.cooldown;
    if (e->def.range > 1)
        fire(s, e->actor.position, to, kind, id, e->def.damage, 0);
    else if (kind == TARGET_BUILDING)
        (void)core_report_building_damage(s->world, s->raid_id, id, e->def.damage);
    else
        (void)core_report_villager_damage(s->world, s->raid_id, id, e->def.damage);
}
static void enemy_step(GameSession *s, GameEnemy *e, float minutes) {
    GamePoint goal = e->actor.position;
    float distance = 1e9f;
    int barrier = 0, arrived;
    e->actor.cooldown -= minutes;
    e->retarget -= minutes;
    if (e->fleeing) {
        goal = (GamePoint){s->raid.spawn_x + 0.5f, s->raid.spawn_y + 0.5f};
    } else {
        if (!target_valid(s, e) || e->retarget <= 0) {
            choose_target(s, e);
            e->retarget = 30;
        }
        if (e->target_kind == TARGET_VILLAGER) {
            GameActor *v = game_actor(s, e->target_id);
            if (!v)
                return;
            goal = v->position;
            distance = game_distance(e->actor.position, goal);
        } else if (e->target_kind == TARGET_BUILDING) {
            CoreBuildingView *b = game_building(s, e->target_id);
            if (!b)
                return;
            goal = building_approach(s, b, e->actor.position);
            distance = building_distance(e->actor.position, b);
            if (!e->looted && e->def.loot_amount > 0 && e->def.behavior != CORE_BEHAVIOR_RAIDER &&
                (b->type == CORE_BLD_KEEP || b->type == CORE_BLD_WAREHOUSE) && distance <= 1.0f) {
                int32_t amount = 0;
                if (core_report_loot(s->world, s->raid_id, e->actor.id, b->id, &amount) == CORE_OK) {
                    e->looted = 1;
                    e->fleeing = 1;
                    e->actor.grid_revision = -1;
                    s->loots++;
                    return;
                }
            }
        } else
            return;
        if (distance <= fmaxf(1, (float)e->def.range)) {
            attack(s, e, e->target_kind, e->target_id, goal);
            return;
        }
    }
    if (e->actor.grid_revision != s->grid_revision || e->actor.target_x != (int)goal.x ||
        e->actor.target_y != (int)goal.y || !e->actor.path.count) {
        if (game_actor_plan(s, &e->actor, (int)goal.x, (int)goal.y, 1) <= 0)
            return;
    }
    arrived = game_actor_walk(s, &e->actor, minutes * e->def.speed / 1000.0f / s->config.minutes_per_tick, 1,
                              &barrier);
    if (barrier) {
        CoreBuildingView *b = game_building(s, barrier);
        if (b && b->hp > 0 && e->actor.cooldown <= 0) {
            attack(s, e, TARGET_BUILDING, barrier, center(b));
            s->barrier_hits++;
        }
    }
    if (arrived && e->fleeing) {
        (void)core_report_enemy_escaped(s->world, s->raid_id, e->actor.id);
        e->hp = 0;
    }
}
void game_combat_step(GameSession *s, float minutes) {
    int i;
    if (s->summary.ruleset && s->summary.raid_id && s->alarm_seen_id != s->summary.raid_id) {
        s->alarm_seen_id = s->summary.raid_id;
        s->dispatch_delay = researched(s, 15) ? 16.0f : 20.0f;
    } else if (s->dispatch_delay > 0)
        s->dispatch_delay -= minutes;
    projectiles_step(s, minutes);
    if (!s->raid_id) {
        towers_step(s, minutes);
        guards_step(s, minutes);
        return;
    }
    for (i = 0; i < s->enemy_count; i++)
        if (s->enemies[i].hp > 0)
            enemy_step(s, &s->enemies[i], minutes);
    if (s->summary.ruleset && s->visibility)
        for (i = 0; i < s->enemy_count; i++)
            if (s->enemies[i].hp > 0) {
                int x = (int)s->enemies[i].actor.position.x, y = (int)s->enemies[i].actor.position.y;
                if (x >= 0 && y >= 0 && x < s->summary.map_width && y < s->summary.map_height &&
                    s->visibility[y * s->summary.map_width + x] == 2)
                    (void)core_expansion_spotted(s->world, x, y);
            }
    towers_step(s, minutes);
    guards_step(s, minutes);
}
