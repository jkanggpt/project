/* session.c — 스냅숏 동기화, 고정 틱, 주민의 실제 경로 이동 */
#include "session.h"
#include "sprite_layout.h"
#include "storage.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

static void clear_world(GameSession *s) {
    int i;
    game_combat_clear(s);
    for (i = 0; i < s->actor_count; i++)
        game_path_free(&s->actors[i].path);
    free(s->actors);
    free(s->villagers);
    free(s->buildings);
    free(s->terrain);
    free(s->roads);
    free(s->cost);
    free(s->towers);
    game_pathfinder_free(&s->finder);
    core_destroy(s->world);
    free(s->exp_tiles);
    free(s->visibility);
    s->exp_tiles = NULL;
    s->visibility = NULL;
    s->animal_count = 0;
    memset(s->animal_actors, 0, sizeof(s->animal_actors));
    s->world = NULL;
    s->actors = NULL;
    s->villagers = NULL;
    s->buildings = NULL;
    s->terrain = NULL;
    s->roads = NULL;
    s->cost = NULL;
    s->towers = NULL;
    s->actor_count = s->actor_capacity = s->building_count = s->building_capacity = 0;
    s->villager_count = s->villager_capacity = s->tower_count = s->tower_capacity = s->cells = 0;
    s->grid_revision = -1;
    s->tick_minutes = 0;
}
int game_session_init(GameSession *s, const char *data_dir) {
    static const char *const files[CORE_DATA_COUNT] = {"buildings.def", "traits.def",   "names.txt",
                                                       "enemies.def",   "research.def", "ranks.def"};
    int i, rc;
    memset(s, 0, sizeof(*s));
    s->grid_revision = -1;
    rc = core_get_config(&s->config);
    if (rc != CORE_OK)
        return rc;
    rc = core_defs_create(&s->defs);
    if (rc != CORE_OK)
        return rc;
    for (i = 0; i < CORE_DATA_COUNT; i++) {
        char path[GAME_PATH_BYTES];
        int32_t line = 0;
        if (!game_join_path(path, sizeof(path), data_dir, files[i])) {
            game_session_free(s);
            return CORE_ERR_INVALID_ARG;
        }
        rc = core_defs_load_file(s->defs, i, path, &line);
        if (rc != CORE_OK && rc != CORE_ERR_FILE_OPEN && rc != CORE_ERR_FORMAT) {
            game_session_free(s);
            return rc;
        }
    }
    return CORE_OK;
}
void game_session_free(GameSession *s) {
    clear_world(s);
    core_defs_destroy(s->defs);
    s->defs = NULL;
}
static int adopt(GameSession *s, CoreWorld *world) {
    GameSession *next = (GameSession *)calloc(1, sizeof(*next));
    int rc;
    if (!next) {
        core_destroy(world);
        return CORE_ERR_OUT_OF_MEMORY;
    }
    next->world = world;
    next->defs = s->defs;
    next->config = s->config;
    next->grid_revision = -1;
    next->on_event = s->on_event;
    next->event_context = s->event_context;
    next->ruleset = s->ruleset;
    next->exp_tick = -1;
    rc = game_session_refresh(next);
    if (rc == CORE_OK) {
        clear_world(s);
        *s = *next;
    } else
        clear_world(next);
    free(next);
    return rc;
}
int game_session_new(GameSession *s, const char *name, int seed, int difficulty) {
    CoreNewGameParams p = {0};
    CoreWorld *world = NULL;
    int rc;
    if (strlen(name) >= sizeof(p.castle_name))
        return CORE_ERR_INVALID_ARG;
    strcpy(p.castle_name, name);
    p.seed = seed;
    p.difficulty = difficulty;
    p.ruleset = s->ruleset;
    rc = core_create(&p, s->defs, &world);
    return rc == CORE_OK ? adopt(s, world) : rc;
}
int game_session_load(GameSession *s, const char *path, int *error_line) {
    CoreWorld *world = NULL;
    int32_t line = 0;
    int rc = core_load(path, s->defs, &world, &line);
    if (error_line)
        *error_line = line;
    return rc == CORE_OK ? adopt(s, world) : rc;
}
CoreBuildingView *game_building(GameSession *s, int id) {
    int i;
    for (i = 0; i < s->building_count; i++)
        if (s->buildings[i].id == id)
            return &s->buildings[i];
    return NULL;
}
CoreVillagerView *game_villager(GameSession *s, int id) {
    int i;
    for (i = 0; i < s->villager_count; i++)
        if (s->villagers[i].id == id)
            return &s->villagers[i];
    return NULL;
}
GameActor *game_actor(GameSession *s, int id) {
    int i;
    for (i = 0; i < s->actor_count; i++)
        if (s->actors[i].id == id)
            return &s->actors[i];
    return NULL;
}
GameEnemy *game_enemy(GameSession *s, int id) {
    int i;
    for (i = 0; i < s->enemy_count; i++)
        if (s->enemies[i].actor.id == id && s->enemies[i].hp > 0)
            return &s->enemies[i];
    return NULL;
}
int game_building_at(const GameSession *s, int x, int y) {
    int i;
    for (i = 0; i < s->building_count; i++) {
        const CoreBuildingView *b = &s->buildings[i];
        if (x >= b->x && y >= b->y && x < b->x + b->w && y < b->y + b->h)
            return b->id;
    }
    return 0;
}
float game_distance(GamePoint a, GamePoint b) {
    float x = a.x - b.x, y = a.y - b.y;
    return sqrtf(x * x + y * y);
}
static int copy_views(GameSession *s) {
    int32_t n = 0;
    int rc = core_copy_buildings(s->world, s->buildings, s->building_capacity, &n);
    if (rc == CORE_ERR_BUFFER_TOO_SMALL) {
        CoreBuildingView *p = (CoreBuildingView *)realloc(s->buildings, (size_t)(n + 16) * sizeof(*p));
        if (!p)
            return CORE_ERR_OUT_OF_MEMORY;
        s->buildings = p;
        s->building_capacity = n + 16;
        rc = core_copy_buildings(s->world, s->buildings, s->building_capacity, &n);
    }
    if (rc != CORE_OK)
        return rc;
    s->building_count = n;
    rc = core_copy_villagers(s->world, s->villagers, s->villager_capacity, &n);
    if (rc == CORE_ERR_BUFFER_TOO_SMALL) {
        CoreVillagerView *p = (CoreVillagerView *)realloc(s->villagers, (size_t)(n + 16) * sizeof(*p));
        if (!p)
            return CORE_ERR_OUT_OF_MEMORY;
        s->villagers = p;
        s->villager_capacity = n + 16;
        rc = core_copy_villagers(s->world, s->villagers, s->villager_capacity, &n);
    }
    if (rc != CORE_OK)
        return rc;
    s->villager_count = n;
    return CORE_OK;
}
static int sync_actors(GameSession *s) {
    int i = 0;
    while (i < s->actor_count) {
        if (!game_villager(s, s->actors[i].id)) {
            game_path_free(&s->actors[i].path);
            s->actors[i] = s->actors[--s->actor_count];
        } else
            i++;
    }
    for (i = 0; i < s->villager_count; i++) {
        CoreVillagerView *v = &s->villagers[i];
        if (!game_actor(s, v->id)) {
            GameActor *a;
            if (s->actor_count == s->actor_capacity) {
                int cap = s->actor_capacity ? s->actor_capacity * 2 : 32;
                GameActor *p = (GameActor *)realloc(s->actors, (size_t)cap * sizeof(*p));
                if (!p)
                    return CORE_ERR_OUT_OF_MEMORY;
                s->actors = p;
                s->actor_capacity = cap;
            }
            a = &s->actors[s->actor_count++];
            memset(a, 0, sizeof(*a));
            a->id = v->id;
            a->revision = -1;
            a->grid_revision = -1;
            a->position = (GamePoint){v->loc_x + 0.5f, v->loc_y + 0.5f};
            if (s->summary.ruleset)
                a->position = (GamePoint){v->current_x_milli / 1000.0f, v->current_y_milli / 1000.0f};
            a->indoor = v->inside_building ? 1.0f : 0.0f;
            a->indoor_building = v->inside_building;
        }
    }
    return CORE_OK;
}
static void sync_animals(GameSession *s) {
    GameAnimalActor next[CORE_EXP_MAX_ANIMALS];
    int i, j;
    for (i = 0; i < s->animal_count; i++) {
        const CoreAnimalView *view = &s->animals[i];
        GameAnimalActor *old = NULL;
        GamePoint target = {view->x_milli / 1000.0f, view->y_milli / 1000.0f};
        if (s->animal_actors[i].id == view->id)
            old = &s->animal_actors[i];
        else
            for (j = 0; j < CORE_EXP_MAX_ANIMALS; j++)
                if (s->animal_actors[j].id == view->id) {
                    old = &s->animal_actors[j];
                    break;
                }
        if (old) {
            next[i] = *old;
            if (game_distance(target, old->goal) > 0.01f) {
                next[i].goal = target;
                next[i].remaining = 24.0f;
            }
        } else {
            memset(&next[i], 0, sizeof(next[i]));
            next[i].id = view->id;
            next[i].position = next[i].goal = target;
            next[i].facing = 1;
        }
    }
    memset(s->animal_actors, 0, sizeof(s->animal_actors));
    memcpy(s->animal_actors, next, (size_t)s->animal_count * sizeof(*next));
}
static void walk_animals(GameSession *s, float minutes) {
    int i;
    for (i = 0; i < s->animal_count; i++) {
        GameAnimalActor *a = &s->animal_actors[i];
        GamePoint before = a->position;
        float factor;
        if (a->remaining <= 0)
            continue;
        factor = fminf(1, minutes / a->remaining);
        a->position.x += (a->goal.x - a->position.x) * factor;
        a->position.y += (a->goal.y - a->position.y) * factor;
        a->remaining = fmaxf(0, a->remaining - minutes);
        if (fabsf(a->position.x - before.x) > 0.001f)
            a->facing = a->position.x > before.x ? 1 : -1;
        a->walk_distance = fmodf(a->walk_distance + game_distance(before, a->position), 0.8f);
    }
}
int game_session_refresh(GameSession *s) {
    int rc, i, cells;
    if (!s->world)
        return CORE_ERR_NULL_ARG;
    rc = core_get_summary(s->world, &s->summary);
    if (rc != CORE_OK)
        return rc;
    cells = s->summary.map_width * s->summary.map_height;
    if (cells != s->cells) {
        uint8_t *t = (uint8_t *)malloc((size_t)cells), *r = (uint8_t *)malloc((size_t)cells),
                *c = (uint8_t *)malloc((size_t)cells);
        if (!t || !r || !c) {
            free(t);
            free(r);
            free(c);
            return CORE_ERR_OUT_OF_MEMORY;
        }
        free(s->terrain);
        free(s->roads);
        free(s->cost);
        s->terrain = t;
        s->roads = r;
        s->cost = c;
        s->cells = cells;
        s->grid_revision = -1;
    }
    if (s->grid_revision != s->summary.cost_grid_rev) {
        rc = core_copy_cost_grid(s->world, s->cost, cells, &s->grid_revision);
        if (rc != CORE_OK)
            return rc;
        rc = core_copy_terrain(s->world, s->terrain, cells);
        if (rc != CORE_OK)
            return rc;
        rc = core_copy_roads(s->world, s->roads, cells);
        if (rc != CORE_OK)
            return rc;
    }
    for (i = 0; i < CORE_BLD_COUNT; i++) {
        rc = core_get_building_def(s->world, i, &s->building_defs[i]);
        if (rc != CORE_OK)
            return rc;
    }
    rc = copy_views(s);
    if (rc != CORE_OK)
        return rc;
    if (s->summary.ruleset && (!s->exp_tiles || s->exp_tick != s->summary.tick)) {
        if (!s->exp_tiles) {
            s->exp_tiles = (CoreExpansionTileView *)calloc((size_t)cells, sizeof(*s->exp_tiles));
            s->visibility = (uint8_t *)calloc((size_t)cells, 1);
        }
        if (!s->exp_tiles || !s->visibility)
            return CORE_ERR_OUT_OF_MEMORY;
        rc = core_expansion_copy_tiles(s->world, s->exp_tiles, cells);
        if (rc != CORE_OK)
            return rc;
        rc = core_expansion_copy_visibility(s->world, s->visibility, cells);
        if (rc != CORE_OK)
            return rc;
        rc = core_expansion_copy_animals(s->world, s->animals, CORE_EXP_MAX_ANIMALS, &s->animal_count);
        if (rc != CORE_OK)
            return rc;
        sync_animals(s);
        s->exp_tick = s->summary.tick;
    }
    rc = sync_actors(s);
    if (rc != CORE_OK)
        return rc;
    rc = game_combat_sync(s);
    if (rc == CORE_OK)
        game_effects_sync(s);
    return rc;
}
int game_actor_plan(GameSession *s, GameActor *a, int x, int y, int enemy) {
    s->finder.normal_cost = s->summary.ruleset ? 100 : 10;
    int rc = game_path_find(&s->finder, s->cost, s->summary.map_width, s->summary.map_height,
                            (int)a->position.x, (int)a->position.y, x, y, enemy, &a->path);
    a->next = 1;
    a->target_x = x;
    a->target_y = y;
    a->grid_revision = s->grid_revision;
    if (rc < 0)
        s->last_error = CORE_ERR_OUT_OF_MEMORY;
    return rc;
}
int game_actor_walk(GameSession *s, GameActor *a, float tiles, int enemy, int *barrier_id) {
    int w = s->summary.map_width;
    *barrier_id = 0;
    while (a->next < a->path.count && tiles > 0) {
        int cell = a->path.cells[a->next], c = game_path_cost(s->cost[cell], enemy);
        GamePoint goal = {cell % w + 0.5f, cell / w + 0.5f};
        float distance = game_distance(a->position, goal), factor;
        GamePoint before = a->position;
        if (!c) {
            a->grid_revision = -1;
            return 0;
        }
        if (enemy && (s->cost[cell] == CORE_COST_WALL || s->cost[cell] == CORE_COST_GATE)) {
            *barrier_id = game_building_at(s, cell % w, cell / w);
            return 0;
        }
        if (s->summary.ruleset && s->cost[cell] == CORE_COST_GATE)
            c = 67;
        factor = (s->summary.ruleset ? 100 : CORE_COST_NORMAL) / (float)c;
        if (distance <= tiles * factor + 0.0001f) {
            a->position = goal;
            a->next++;
            tiles -= distance / factor;
        } else {
            float t = tiles * factor / distance;
            a->position.x += (goal.x - a->position.x) * t;
            a->position.y += (goal.y - a->position.y) * t;
            tiles = 0;
        }
        a->facing = game_walk_facing(a->position.x - before.x, a->position.y - before.y, a->facing);
        /* 실제 움직인 거리로만 프레임을 진행해 정지·일시정지 때 발이 미끄러지지 않습니다. */
        a->walk_distance = fmodf(a->walk_distance + game_distance(before, a->position), 0.8f);
    }
    return a->path.count > 0 && a->next >= a->path.count;
}
static void walk_villagers(GameSession *s, float minutes) {
    int i;
    for (i = 0; i < s->villager_count; i++) {
        CoreVillagerView *v = &s->villagers[i];
        GameActor *a = game_actor(s, v->id);
        int barrier;
        if (!a)
            continue;
        if (s->summary.ruleset) {
            float before = a->indoor;
            if (v->inside_building) {
                a->indoor_building = v->inside_building;
                a->indoor = fminf(1, a->indoor + minutes / 5.0f);
            } else
                a->indoor = fmaxf(0, a->indoor - minutes / 5.0f);
            if (a->indoor != before) {
                CoreBuildingView *b = game_building(s, a->indoor_building);
                if (b) {
                    static const int entry[4] = {FACE_UP, FACE_LEFT, FACE_DOWN, FACE_RIGHT},
                                     exit[4] = {FACE_DOWN, FACE_RIGHT, FACE_UP, FACE_LEFT};
                    a->facing = v->inside_building ? entry[b->rotation] : exit[b->rotation];
                    a->walk_distance = fmodf(a->walk_distance + fabsf(a->indoor - before) * 0.5f, 0.8f);
                }
            }
            (void)core_expansion_report_position(s->world, v->id, (int)(a->position.x * 1000),
                                                 (int)(a->position.y * 1000));
            if (!v->inside_building && a->indoor > 0)
                continue;
        }
        if (v->at_target || (v->intent_flags & CORE_INTENT_HOLD))
            continue;
        if (a->revision != v->intent_rev || a->grid_revision != s->grid_revision) {
            int result = game_actor_plan(s, a, v->target_x, v->target_y, 0);
            a->revision = v->intent_rev;
            if (result == 0) {
                (void)core_villager_unreachable(s->world, v->id, v->intent_rev);
                continue;
            }
            if (result < 0)
                return;
        }
        if (game_actor_walk(s, a, minutes * v->walk_speed / 1000.0f / s->config.minutes_per_tick, 0,
                            &barrier)) {
            int rc = core_villager_arrived(s->world, v->id, v->intent_rev);
            if (rc == CORE_OK)
                s->arrivals++;
        }
    }
}
void game_session_events(GameSession *s) {
    CoreEventView event;
    while (core_poll_event(s->world, &event) > 0) {
        game_effects_event(s, &event);
        if (s->on_event)
            s->on_event(s->event_context, &event);
    }
}
int game_session_step(GameSession *s, float minutes) {
    int rc;
    if (!s->world || !isfinite(minutes) || minutes < 0 || minutes > 14400)
        return CORE_ERR_INVALID_ARG;
    if (s->summary.outcome == CORE_OUTCOME_LOST)
        return CORE_ERR_REJECTED;
    while (minutes > 0.0001f && s->summary.outcome != CORE_OUTCOME_LOST) {
        rc = game_session_refresh(s);
        if (rc != CORE_OK)
            return rc;
        float slice = minutes > 2 ? 2 : minutes;
        if (s->summary.ruleset)
            for (int i = 0; i < s->animal_count; i++)
                if (s->animals[i].attack_remaining_milli > 0 &&
                    s->animals[i].attack_remaining_milli / 1000.0f < slice)
                    slice = s->animals[i].attack_remaining_milli / 1000.0f;
        minutes -= slice;
        walk_villagers(s, slice);
        walk_animals(s, slice);
        game_combat_step(s, slice);
        s->visual_time += slice / 12.0f;
        if (s->summary.ruleset) {
            int elapsed = (int)floorf(slice * 1000 + .5f);
            for (int i = 0; i < s->animal_count; i++) {
                CoreAnimalView *animal = &s->animals[i];
                if (animal->attack_remaining_milli <= 0)
                    continue;
                if (animal->attack_remaining_milli <= elapsed) {
                    GameActor *target = game_actor(s, animal->target_id);
                    if (target)
                        (void)core_expansion_report_position(s->world, target->id,
                                                             (int)(target->position.x * 1000),
                                                             (int)(target->position.y * 1000));
                }
                animal->attack_remaining_milli =
                    animal->attack_remaining_milli > elapsed ? animal->attack_remaining_milli - elapsed : 0;
            }
            rc = core_expansion_advance_animal_attacks(s->world, elapsed);
            if (rc != CORE_OK)
                return rc;
        }
        if (s->last_error)
            return s->last_error;
        s->tick_minutes += slice;
        if (s->tick_minutes + 0.0001f >= s->config.minutes_per_tick) {
            s->tick_minutes -= s->config.minutes_per_tick;
            rc = core_tick(s->world, 1);
            if (rc != CORE_OK)
                return rc;
        }
        s->steps++;
    }
    rc = game_session_refresh(s);
    if (rc == CORE_OK)
        game_session_events(s);
    return rc;
}
