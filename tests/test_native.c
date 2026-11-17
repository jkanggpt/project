/* test_native.c — 실제 C 길찾기·이동·전투·설정·저장 교체를 화면 없이 검사합니다. */
#include "expansion.h"
#include "fortification_plan.h"
#include "platform.h"
#include "session.h"
#include "sprite_layout.h"
#include "storage.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failed = 0;
#define EXPECT(x)                                                                                            \
    do {                                                                                                     \
        if (!(x)) {                                                                                          \
            fprintf(stderr, "native FAIL %s:%d: %s\n", __FILE__, __LINE__, #x);                              \
            failed++;                                                                                        \
        }                                                                                                    \
    } while (0)
static void paths(void) {
    uint8_t cost[25];
    GamePathfinder finder = {0};
    GamePath path = {0};
    int i;
    memset(cost, 10, sizeof(cost));
    for (i = 0; i < 5; i++)
        cost[2 + i * 5] = CORE_COST_WALL;
    EXPECT(game_path_find(&finder, cost, 5, 5, 0, 2, 4, 2, 0, &path) == 0);
    EXPECT(game_path_find(&finder, cost, 5, 5, 0, 2, 4, 2, 1, &path) == 1);
    EXPECT(path.count == 5);
    cost[12] = CORE_COST_GATE;
    EXPECT(game_path_find(&finder, cost, 5, 5, 0, 2, 4, 2, 0, &path) == 1);
    EXPECT(game_path_find(&finder, cost, 5, 5, -1, 0, 1, 1, 0, &path) == 0);
    EXPECT(game_path_find(&finder, cost, 5, 5, 0, 0, 0, 0, 0, &path) == 1 && path.count == 1);
    memset(cost, 10, sizeof(cost));
    cost[1] = 30;
    cost[6] = 30;
    EXPECT(game_path_find(&finder, cost, 5, 5, 0, 0, 4, 0, 0, &path) == 1);
    EXPECT(path.count == 5); /* 일곱 칸 우회보다 네 번 움직이는 길이 쌉니다. */
    game_path_free(&path);
    game_pathfinder_free(&finder);
}
static void settings(void) {
    GameSettings a, b;
    const char *path = "native_settings.cfg";
    game_settings_default(&a);
    a.sfx_volume = 0.25f;
    a.fullscreen = 1;
    a.ui_scale = 2;
    EXPECT(game_settings_save(&a, path));
    EXPECT(game_settings_load(&b, path));
    EXPECT(fabsf(b.sfx_volume - 0.25f) < 0.001f && b.fullscreen == 1 && b.ui_scale == 2);
    EXPECT(game_write_bytes(path, "master=nan\nsfx=4\nui_scale=999\n",
                            strlen("master=nan\nsfx=4\nui_scale=999\n")));
    EXPECT(game_settings_load(&b, path));
    EXPECT(b.master_volume == 0.8f && b.sfx_volume == 0.8f && b.ui_scale == 1);
    (void)platform_remove_file(path);
}
static void session(const char *data) {
    GameSession *s = (GameSession *)calloc(1, sizeof(*s));
    uint64_t before = 0, after = 0;
    int line = 0, i;
    EXPECT(s != NULL);
    if (!s)
        return;
    EXPECT(game_session_init(s, data) == CORE_OK);
    EXPECT(game_session_new(s, u8"한글 성채", 42, CORE_DIFF_NORMAL) == CORE_OK);
    EXPECT(s->summary.population == 6 && s->cells == 96 * 96);
    EXPECT(game_session_step(s, 60) == CORE_OK);
    EXPECT(s->arrivals > 0 && core_validate(s->world) == 0);
    EXPECT(core_state_hash(s->world, &before) == CORE_OK);
    EXPECT(core_save(s->world, "native_roundtrip.sav") == CORE_OK);
    EXPECT(game_session_load(s, "native_missing.sav", &line) == CORE_ERR_FILE_OPEN);
    EXPECT(core_state_hash(s->world, &after) == CORE_OK && before == after);
    EXPECT(game_session_load(s, "native_roundtrip.sav", &line) == CORE_OK);
    EXPECT(core_state_hash(s->world, &after) == CORE_OK && before == after);
    EXPECT(game_session_step(s, NAN) == CORE_ERR_INVALID_ARG);
    EXPECT(core_debug_stress(s->world, 20, 2) == CORE_OK);
    EXPECT(game_session_refresh(s) == CORE_OK && s->enemy_count == 2);
    EXPECT(core_save(s->world, "native_raid.sav") == CORE_ERR_REJECTED);
    for (i = 0; i < s->enemy_count; i++) {
        s->enemies[i].actor.position = (GamePoint){s->summary.keep_x + 0.5f, s->summary.keep_y + 3.5f};
        s->enemies[i].hp = 1;
    }
    EXPECT(game_session_step(s, 100) == CORE_OK);
    EXPECT(s->shots > 0 && s->kills > 0);
    EXPECT(core_validate(s->world) == 0);
    game_session_free(s);
    free(s);
    (void)platform_remove_file("native_roundtrip.sav");
}
static void sprite_layout(void) {
    GameSpriteBox actor = game_actor_sprite(120, 160, GAME_ACTOR_HEIGHT);
    GameSpriteBox keep = game_building_sprite(CORE_BLD_KEEP, 10, 10, 3, 3, 256, 256);
    GameSpriteBox tower = game_building_sprite(CORE_BLD_TOWER, 10, 10, 1, 1, 128, 256);
    GameDrawItem order[4] = {{209, DRAW_VILLAGER, 0, 5},
                             {208, DRAW_BUILDING, 0, 1},
                             {150, DRAW_VILLAGER, 1, 2},
                             {160, DRAW_PROP, 0, 0}};
    EXPECT(actor.h == 1.5f * GAME_TILE_SIZE && actor.y + actor.h == 160);
    EXPECT(keep.y < 10 * GAME_TILE_SIZE && keep.y + keep.h == 13 * GAME_TILE_SIZE);
    EXPECT(fabsf((keep.x + keep.w / 2) - 11.5f * GAME_TILE_SIZE) < 0.001f);
    EXPECT(fabsf(keep.w / keep.h - 1) < 0.001f);
    EXPECT(tower.h >= 3 * GAME_TILE_SIZE && tower.y + tower.h == 11 * GAME_TILE_SIZE);
    qsort(order, 4, sizeof(order[0]), game_draw_compare);
    /* 뒤 주민 → 나무 → 건물 → 앞 주민: 큰 지붕에도 점유·정렬 기준은 발밑입니다. */
    EXPECT(order[0].key == 2 && order[1].kind == DRAW_PROP && order[2].kind == DRAW_BUILDING &&
           order[3].key == 5);
    {
        int i, seen = 0;
        for (i = 1; i <= 8; i++)
            seen |= 1 << game_villager_art(i);
        EXPECT(seen == 255 && game_villager_art(9) == game_villager_art(1));
        EXPECT(game_villager_art(-1) == 0);
    }
    EXPECT(game_walk_facing(0, 0, FACE_UP) == FACE_UP);
    EXPECT(game_walk_facing(-1, 0, FACE_DOWN) == FACE_LEFT);
    EXPECT(game_walk_facing(0, -1, FACE_DOWN) == FACE_UP);
    EXPECT(game_walk_facing(0, 1, FACE_UP) == FACE_DOWN);
    EXPECT(game_walk_frame(0.45f, 1) == 2 && game_walk_frame(0.45f, 0) == 1);
    {
        GameSpriteFrame right = game_villager_frame(8, FACE_RIGHT, 0.45f, 1);
        GameSpriteFrame left = game_villager_frame(8, FACE_LEFT, 0.45f, 1);
        GameSpriteFrame back = game_villager_frame(8, FACE_UP, 0, 0);
        EXPECT(right.column == 6 && right.row == 7 && !right.flip);
        EXPECT(left.column == right.column && left.row == right.row && left.flip);
        EXPECT(back.column == 9 && back.row == 7);
    }
}
static void walking_direction(void) {
    uint8_t cost[25];
    int cells[3] = {12, 13, 8}, barrier = 0;
    GameSession s = {0};
    GameActor actor = {0};
    float held;
    memset(cost, CORE_COST_NORMAL, sizeof(cost));
    s.cost = cost;
    s.summary.map_width = 5;
    actor.position = (GamePoint){2.5f, 2.5f};
    actor.path.cells = cells;
    actor.path.count = 3;
    actor.next = 1;
    EXPECT(!game_actor_walk(&s, &actor, 0.5f, 0, &barrier));
    EXPECT(actor.facing == FACE_RIGHT && fabsf(actor.position.x - 3.0f) < 0.001f);
    EXPECT(actor.walk_distance > 0 && game_walk_frame(actor.walk_distance, 1) == 2);
    EXPECT(!game_actor_walk(&s, &actor, 0.5f, 0, &barrier));
    EXPECT(!game_actor_walk(&s, &actor, 0.5f, 0, &barrier));
    EXPECT(actor.facing == FACE_UP && fabsf(actor.position.y - 2.0f) < 0.001f);
    held = actor.walk_distance;
    EXPECT(!game_actor_walk(&s, &actor, 0, 0, &barrier));
    EXPECT(actor.facing == FACE_UP && actor.walk_distance == held);
    cost[8] = CORE_COST_BLOCKED;
    EXPECT(!game_actor_walk(&s, &actor, 1, 0, &barrier));
    EXPECT(actor.facing == FACE_UP && actor.walk_distance == held);
}
static void indoor_transition(const char *data) {
    GameSession *s = (GameSession *)calloc(1, sizeof(*s));
    Villager *v;
    GameActor *a;
    Building *keep;
    EXPECT(s != NULL);
    if (!s)
        return;
    EXPECT(game_session_init(s, data) == CORE_OK);
    s->ruleset = 1;
    EXPECT(game_session_new(s, u8"실내 시험", 17, 1) == CORE_OK);
    v = s->world->villagers[0];
    keep = world_keep(s->world);
    a = game_actor(s, v->id);
    v->target = keep;
    v->at_target = 1;
    v->inside_building = keep->id;
    a->indoor = 0;
    EXPECT(game_session_refresh(s) == CORE_OK);
    EXPECT(game_session_step(s, 2) == CORE_OK);
    EXPECT(a->indoor > 0 && a->indoor < 1 && a->facing == FACE_UP);
    EXPECT(game_session_step(s, 3) == CORE_OK);
    EXPECT(a->indoor == 1);
    v->inside_building = 0;
    EXPECT(game_session_refresh(s) == CORE_OK);
    EXPECT(game_session_step(s, 2) == CORE_OK);
    EXPECT(a->indoor > 0 && a->indoor < 1 && a->facing == FACE_DOWN);
    game_session_free(s);
    free(s);
}
static void fortification_plan(void) {
    uint8_t terrain[64 * 64] = {0};
    BotFortificationPlan plan;
    int x, y, r, mask;
    for (y = 12; y <= 50; y++)
        for (x = 40; x <= 42; x++)
            terrain[y * 64 + x] = CORE_TERRAIN_WATER;
    EXPECT(bot_fortification_plan(terrain, 64, 64, 32, 32, &plan));
    EXPECT(plan.valid[0] && plan.valid[1]);
    for (r = 0; r < 2; r++)
        for (y = 0; y < 64; y++)
            for (x = 0; x < 64; x++)
                if (bot_fortification_boundary(&plan, x, y, r)) {
                    EXPECT(terrain[y * 64 + x] != CORE_TERRAIN_WATER);
                    EXPECT(bot_fortification_reserved(&plan, x, y, 1, 1));
                }
    EXPECT(bot_fortification_inside(&plan, 31, 31, 3, 3, 0));
    EXPECT(!bot_fortification_reserved(&plan, 36, 34, 1, 1));
    EXPECT(bot_fortification_inside(&plan, 32, 34, 1, 1, 0));
    for (mask = 0; mask < 16; mask++) {
        uint8_t cost[9] = {10, 10, 10, 10, 10, 10, 10, 10, 10};
        if (mask & 1)
            cost[1] = CORE_COST_WALL;
        if (mask & 2)
            cost[5] = CORE_COST_GATE;
        if (mask & 4)
            cost[7] = CORE_COST_WALL;
        if (mask & 8)
            cost[3] = CORE_COST_WALL;
        EXPECT(game_wall_mask(cost, 3, 3, 1, 1) == mask);
    }
    {
        CoreBuildingView b = {0};
        GameSpriteBox box;
        b.type = CORE_BLD_KEEP;
        b.x = b.y = 10;
        b.w = b.h = 3;
        box = game_expansion_building_sprite(&b, 512, 256);
        EXPECT(box.w <= 3 * 16 * 1.10f + 4.01f && box.w >= 3 * 16);
        EXPECT(box.h >= 3 * 16 + 24 && box.y + box.h == 13 * 16);
    }
}
static void animal_movement(const char *data) {
    GameSession *s = (GameSession *)calloc(1, sizeof(*s));
    float before, held;
    EXPECT(s != NULL);
    if (!s)
        return;
    EXPECT(game_session_init(s, data) == CORE_OK);
    s->ruleset = 1;
    EXPECT(game_session_new(s, u8"동물 걷기", 17, 1) == CORE_OK);
    before = s->animal_actors[0].position.x;
    s->world->expansion->animals[0].x_milli += 1000;
    s->exp_tick = -1;
    EXPECT(game_session_refresh(s) == CORE_OK);
    EXPECT(game_session_step(s, 2) == CORE_OK);
    EXPECT(s->animal_actors[0].position.x > before && s->animal_actors[0].position.x < before + 1);
    EXPECT(s->animal_actors[0].walk_distance > 0);
    held = s->animal_actors[0].position.x;
    EXPECT(game_session_step(s, 0) == CORE_OK);
    EXPECT(s->animal_actors[0].position.x == held);
    s->world->expansion->animals[0].x_milli -= 1000;
    s->exp_tick = -1;
    EXPECT(game_session_refresh(s) == CORE_OK);
    EXPECT(game_session_step(s, 2) == CORE_OK);
    EXPECT(s->animal_actors[0].facing == -1);
    game_session_free(s);
    free(s);
}
int main(int argc, char **argv) {
    if (argc != 2)
        return 2;
    paths();
    settings();
    sprite_layout();
    walking_direction();
    session(argv[1]);
    indoor_transition(argv[1]);
    fortification_plan();
    animal_movement(argv[1]);
    if (!failed)
        puts("C native runtime tests passed");
    return failed ? 1 : 0;
}
