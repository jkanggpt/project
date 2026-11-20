/* session.h — C 코어의 스냅숏과 실시간 이동·전투를 연결하는 C 실행 계층 */
#ifndef CASTLE_GAME_SESSION_H
#define CASTLE_GAME_SESSION_H
#include <castle_core.h>
#include <castle_expansion.h>
#include "path.h"

#define GAME_MAX_ENEMIES 64
#define GAME_MAX_PROJECTILES 512
typedef struct GamePoint {
    float x, y;
} GamePoint;
typedef struct GameActor {
    int id, revision, grid_revision, next, target_x, target_y;
    GamePoint position;
    GamePath path;
    float cooldown, walk_distance;
    int facing;
    float indoor;
    int indoor_building;
    int chasing;
    GamePoint chase_goal;
} GameActor;
typedef struct GameEnemy {
    GameActor actor;
    CoreEnemyDefView def;
    int hp, incoming, target_id, target_kind, fleeing, looted;
    float retarget, special_clock, attack_elapsed;
    int attack_serial, attack_applied, attack_kind, attack_target, summoner_id;
    GamePoint attack_to;
    CoreRaiderInfo info;
} GameEnemy;
typedef struct GameProjectile {
    GamePoint from, to;
    float elapsed, duration, radius;
    int raid_id, target_id, target_kind, damage, source_type;
} GameProjectile;
typedef struct GameTower {
    int id;
    float cooldown;
} GameTower;
typedef struct GameAnimalActor {
    int id, facing;
    GamePoint position, goal;
    float remaining, walk_distance;
} GameAnimalActor;

typedef struct GameEffect {
    int id, category, skin, hp, max_hp, facing, seen, dead, attack_tick, owner;
    GamePoint position;
    float hurt_started, hurt_until, bar_until, death_started, attack_started;
} GameEffect;
typedef void (*GameEventFn)(void *context, const CoreEventView *event);
typedef struct GameSession {
    CoreWorld *world;
    CoreDefs *defs;
    CoreConfigView config;
    CoreSummaryView summary;
    CoreBuildingDefView building_defs[CORE_BLD_COUNT];
    CoreBuildingView *buildings;
    CoreVillagerView *villagers;
    int building_count, building_capacity, villager_count, villager_capacity;
    uint8_t *terrain, *roads, *cost;
    CoreExpansionTileView *exp_tiles;
    uint8_t *visibility;
    CoreAnimalView animals[CORE_EXP_MAX_ANIMALS];
    GameAnimalActor animal_actors[CORE_EXP_MAX_ANIMALS];
    int animal_count, ruleset, exp_tick;
    int cells, grid_revision;
    GameActor *actors;
    int actor_count, actor_capacity;
    GamePathfinder finder;
    CoreRaidView raid;
    GameEnemy *enemies;
    int enemy_count, enemy_capacity, raid_id;
    int alarm_seen_id;
    float dispatch_delay;
    GameTower *towers;
    int tower_count, tower_capacity;
    GameProjectile projectiles[GAME_MAX_PROJECTILES];
    int projectile_count;
    float tick_minutes;
    int last_error;
    unsigned long steps, arrivals, shots, kills, barrier_hits, loots;
    GameEventFn on_event;
    void *event_context;
    GameEffect effects[2048];
    int effect_revision;
    float visual_time;
} GameSession;

int game_session_init(GameSession *s, const char *data_dir);
void game_session_free(GameSession *s);
int game_session_new(GameSession *s, const char *name, int seed, int difficulty);
int game_session_load(GameSession *s, const char *path, int *error_line);
int game_session_refresh(GameSession *s);
/* 게임 속 분 단위. 프레임과 무관하게 최대 2분씩 잘라 이동·전투를 풉니다. */
int game_session_step(GameSession *s, float minutes);
void game_session_events(GameSession *s);
CoreBuildingView *game_building(GameSession *s, int id);
CoreVillagerView *game_villager(GameSession *s, int id);
GameActor *game_actor(GameSession *s, int id);
GameEnemy *game_enemy(GameSession *s, int id);
int game_building_at(const GameSession *s, int x, int y);
float game_distance(GamePoint a, GamePoint b);
int game_actor_plan(GameSession *s, GameActor *actor, int x, int y, int enemy);
int game_actor_walk(GameSession *s, GameActor *actor, float tiles, int enemy, int *barrier_id);
void game_combat_clear(GameSession *s);
int game_combat_sync(GameSession *s);
void game_combat_step(GameSession *s, float minutes);
GameEffect *game_effect(GameSession *s, int category, int id);
void game_effects_sync(GameSession *s);
void game_effects_event(GameSession *s, const CoreEventView *event);
#endif
