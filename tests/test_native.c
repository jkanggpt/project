/* test_native.c — 실제 C 길찾기·이동·전투·설정·저장 교체를 화면 없이 검사합니다. */
#include "expansion.h"
#include "fortification_plan.h"
#include "platform.h"
#include "session.h"
#include "sim.h"
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
    memset(cost,10,sizeof(cost));
    cost[7]=cost[11]=cost[13]=CORE_COST_WALL;
    EXPECT(game_path_find(&finder,cost,5,5,2,2,-1,-1,3,&path)==1);
    EXPECT(path.count>=3 && path.cells[path.count-1]/5==4);
    cost[17]=CORE_COST_WALL;
    EXPECT(game_path_find(&finder,cost,5,5,2,2,-1,-1,3,&path)==0);
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
        for (i = 1; i <= 12; i++)
            seen |= 1 << game_villager_art(i);
        EXPECT(seen == 4095 && game_villager_art(49) == game_villager_art(1));
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
        EXPECT(box.h >= 3 * 16 && box.h <= 3 * 16 + 24 && box.y + box.h == 13 * 16);
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
    {
        int id = s->villagers[0].id, x = s->summary.keep_x * 1000 + 250,
            y = (s->summary.keep_y + 4) * 1000 + 750;
        EXPECT(core_expansion_report_position(s->world, id, x, y) == CORE_OK);
        EXPECT(core_save(s->world, "native_position.sav") == CORE_OK);
        EXPECT(game_session_load(s, "native_position.sav", NULL) == CORE_OK);
        GameActor *actor = game_actor(s, id);
        EXPECT(actor && fabsf(actor->position.x - x / 1000.0f) < .00001f &&
               fabsf(actor->position.y - y / 1000.0f) < .00001f);
        (void)platform_remove_file("native_position.sav");
        (void)platform_remove_file("native_position.sav.bak");
    }
    game_session_free(s);
    free(s);
}
static void raid_attack_frames(const char *data) {
    GameSession *s=calloc(1,sizeof(*s));EXPECT(s!=NULL);if(!s)return;
    EXPECT(game_session_init(s,data)==CORE_OK);s->ruleset=1;
    EXPECT(game_session_new(s,u8"공격 프레임",42,1)==CORE_OK);
    EXPECT(threat_debug_raid(s->world,CORE_THREAT_BANDITS,CORE_DIR_NORTH,3)==CORE_OK);
    EXPECT(game_session_refresh(s)==CORE_OK);
    for(int i=0;i<s->world->villager_count;i++)s->world->villagers[i]->inside_building=world_keep(s->world)->id;
    Villager *v=s->world->villagers[0];v->inside_building=0;v->health=100;v->state=CORE_VS_IDLE;v->at_target=1;
    GameActor *a=game_actor(s,v->id);a->position=(GamePoint){100.5f,100.5f};
    v->current_x_milli=100500;v->current_y_milli=100500;
    for(int i=0;i<s->enemy_count;i++)s->enemies[i].actor.position=(GamePoint){160.5f,160.5f};
    s->enemies[0].actor.position=(GamePoint){101.0f,100.5f};
    EXPECT(game_session_step(s,1.2f)==CORE_OK);EXPECT(v->health==100);
    EXPECT(game_session_step(s,1.2f)==CORE_OK);EXPECT(v->health==100);
    EXPECT(game_session_step(s,1.2f)==CORE_OK);EXPECT(v->health==95);
    EXPECT(game_session_step(s,1.2f)==CORE_OK);EXPECT(v->health==95);
    Building *keep=world_keep(s->world);uint64_t before=0,after=0;
    EXPECT(core_report_building_damage(s->world,s->raid_id,keep->id,100000)==CORE_OK);
    EXPECT(game_session_step(s,10)==CORE_OK);
    EXPECT(s->summary.outcome==CORE_OUTCOME_LOST);
    EXPECT(core_state_hash(s->world,&before)==CORE_OK);
    EXPECT(core_save(s->world,"native_defeat.sav")==CORE_OK);
    {
        CoreWorld *copy=NULL;
        EXPECT(core_load("native_defeat.sav",NULL,&copy,NULL)==CORE_OK);
        if(copy) {
            uint64_t copied=0;core_state_hash(copy,&copied);EXPECT(copied==before);
            int differences=0;
            for(int p=0;p<s->cells;p++)if(s->world->grid.cost[p]!=copy->grid.cost[p]) {
                if(differences++<4)fprintf(stderr,"defeat cost mismatch at %d: %d != %d\n",p,s->world->grid.cost[p],copy->grid.cost[p]);
            }
            EXPECT(differences==0);core_destroy(copy);
        }
    }
    EXPECT(game_session_load(s,"native_defeat.sav",NULL)==CORE_OK);
    EXPECT(core_state_hash(s->world,&after)==CORE_OK);EXPECT(before==after);
    (void)platform_remove_file("native_defeat.sav");(void)platform_remove_file("native_defeat.sav.bak");
    game_session_free(s);free(s);
}
static void bot_trade_recovery(const char *data) {
    GameSession *s=calloc(1,sizeof(*s));EXPECT(s!=NULL);if(!s)return;
    EXPECT(game_session_init(s,data)==CORE_OK);s->ruleset=1;
    EXPECT(game_session_new(s,u8"거래 시설 복구",42,1)==CORE_OK);
    CoreExpansionBuildingInfo info;core_expansion_building_info(CORE_BLD_STALL,&info);
    Building *b=entity_add_building(s->world,CORE_BLD_STALL);EXPECT(b!=NULL);
    if(b) {
        b->x=110;b->y=110;b->w=info.w;b->h=info.h;b->door_x=b->x+b->w/2;b->door_y=b->y+b->h;
        b->state=CORE_BSTATE_RUINED;b->hp=0;
        for(int r=0;r<15;r++)s->world->bank.amount[r]=100;
        s->world->bank.amount[CORE_RES_GOLD]=1000;
        expansion_rebuild(s->world);
        EXPECT(core_validate(s->world)==0);
        EXPECT(sim_bot_daily(s->world,"reasonable")==CORE_OK);
        EXPECT(b->state==CORE_BSTATE_ACTIVE);EXPECT(b->hp==info.hp);
        int trade=0;for(int i=0;i<s->world->building_count;i++)trade+=s->world->buildings[i]->type==CORE_BLD_STALL || s->world->buildings[i]->type==CORE_BLD_MARKET || s->world->buildings[i]->type==CORE_BLD_TRADING_POST;
        EXPECT(trade==1);EXPECT(core_validate(s->world)==0);
    }
    game_session_free(s);free(s);
}
static void ability_during_attack(const char *data) {
    for(int kind=0;kind<2;kind++) {
        GameSession *s=calloc(1,sizeof(*s));EXPECT(s!=NULL);if(!s)return;
        EXPECT(game_session_init(s,data)==CORE_OK);s->ruleset=1;
        EXPECT(game_session_new(s,u8"공격 중 주기 능력",42,1)==CORE_OK);
        EXPECT(threat_debug_raid(s->world,CORE_THREAT_MONSTERS,CORE_DIR_NORTH,3)==CORE_OK);
        s->world->threat.raid.enemies[0].type=kind?16:13;
        EXPECT(game_session_refresh(s)==CORE_OK);
        GameEnemy *caster=&s->enemies[0],*ally=&s->enemies[1];
        caster->actor.position=(GamePoint){100.5f,100.5f};ally->actor.position=(GamePoint){101.0f,100.5f};
        caster->special_clock=119;caster->attack_serial=1;caster->attack_elapsed=0;
        int before=s->world->expansion->summons_total,hp=ally->def.hp-20;
        ally->hp=hp;
        EXPECT(game_session_step(s,1)==CORE_OK);
        if(kind)EXPECT(s->enemies[1].hp==hp+15);
        else EXPECT(s->world->expansion->summons_total==before+1);
        game_session_free(s);free(s);
    }
}
static void actual_retreat(const char *data) {
    GameSession *s=calloc(1,sizeof(*s));EXPECT(s!=NULL);if(!s)return;
    EXPECT(game_session_init(s,data)==CORE_OK);s->ruleset=1;
    EXPECT(game_session_new(s,u8"철수 우회 검사",42,0)==CORE_OK);
    EXPECT(threat_debug_raid(s->world,CORE_THREAT_BANDITS,CORE_DIR_EAST,1)==CORE_OK);
    EXPECT(game_session_refresh(s)==CORE_OK);
    GameEnemy *e=&s->enemies[0];int x=s->summary.keep_x+14,y=s->summary.keep_y+14;
    while(x<s->summary.map_width-2 && !game_path_cost(s->cost[y*s->summary.map_width+x],3))x++;
    e->actor.position=(GamePoint){x+.5f,y+.5f};e->actor.grid_revision=-1;
    e->entry_x=s->summary.map_width-1;e->entry_y=y;
    e->inbound_count=1;e->inbound[0]=s->summary.keep_x;e->inbound[1]=s->summary.keep_y;
    s->world->threat.raid.phase=CORE_RAID_ASSAULT;
    s->world->threat.raid.first_discovered=1;
    s->world->threat.raid.started_tick=0;s->world->threat.raid.assault_end_tick=1;
    EXPECT(core_tick(s->world,1)==CORE_OK);
    EXPECT(s->world->threat.raid.phase==CORE_RAID_RETREAT);
    EXPECT(core_expansion_build_locked(s->world));EXPECT(game_session_refresh(s)==CORE_OK);
    float before=e->actor.position.x;GamePoint retreat_from=e->actor.position;e->retarget=30;
    memset(s->visibility,2,(size_t)s->cells);
    EXPECT(game_session_step(s,1)==CORE_OK);EXPECT(e->actor.position.x<before+2);
    EXPECT(game_distance(e->actor.position,retreat_from)>.001f);
    EXPECT(s->raid_id!=0);
    memset(s->visibility,0,(size_t)s->cells);
    EXPECT(game_session_step(s,1)==CORE_OK);
    EXPECT(s->raid_id==0);EXPECT(!core_expansion_build_locked(s->world));
    EXPECT(s->world->threat.raid.escaped==1);EXPECT(s->world->threat.raid.killed==0);
    GameActor *resident=game_actor(s,s->villagers[0].id);EXPECT(resident!=NULL);
    if(resident){resident->chasing=1;resident->attack_serial=7;resident->attack_elapsed=1;resident->outfit=4;EXPECT(game_session_step(s,1)==CORE_OK);EXPECT(!resident->chasing && !resident->attack_serial && !resident->outfit);}
    EXPECT(core_validate(s->world)==0);game_session_free(s);free(s);
}
static void blocked_raid_target(const char *data) {
    GameSession *s=calloc(1,sizeof(*s));EXPECT(s!=NULL);if(!s)return;
    EXPECT(game_session_init(s,data)==CORE_OK);s->ruleset=1;
    EXPECT(game_session_new(s,u8"막힌 목표와 지속 공격",42,0)==CORE_OK);
    Building *keep=world_keep(s->world),*pocket=entity_add_building(s->world,CORE_BLD_WAREHOUSE);
    EXPECT(pocket!=NULL);if(!pocket){game_session_free(s);free(s);return;}
    pocket->x=keep->x-5;pocket->y=keep->y;pocket->w=pocket->h=1;pocket->door_x=pocket->x;pocket->door_y=pocket->y+1;pocket->hp=200;pocket->state=CORE_BSTATE_ACTIVE;
    expansion_rebuild(s->world);
    EXPECT(threat_debug_raid(s->world,CORE_THREAT_BANDITS,CORE_DIR_WEST,1)==CORE_OK);
    expansion_raid_discover(s->world);EXPECT(game_session_refresh(s)==CORE_OK);
    memset(s->cost,100,(size_t)s->cells);
    for(int y=keep->y;y<keep->y+keep->h;y++)for(int x=keep->x;x<keep->x+keep->w;x++)s->cost[y*s->summary.map_width+x]=0;
    for(int y=pocket->y-2;y<=pocket->y+2;y++)for(int x=pocket->x-2;x<=pocket->x+2;x++)s->cost[y*s->summary.map_width+x]=0;
    for(int v=0;v<s->villager_count;v++)s->villagers[v].inside_building=keep->id;
    GameEnemy *e=&s->enemies[0];e->actor.position=(GamePoint){keep->x-9.5f,keep->y+.5f};e->hp=100000;e->def.damage=1;
    e->def.range=1;e->def.cooldown=20;e->def.speed=1000;e->def.behavior=CORE_BEHAVIOR_MONSTER;
    int hp=keep->hp;
    for(int n=0;n<240;n++)game_combat_step(s,2);
    if(keep->hp==hp || e->attack_serial<4)fprintf(stderr,"blocked attack: hp=%d/%d pos=%.2f,%.2f target=%d/%d path=%d/%d goal=%d,%d cd=%.1f retarget=%.1f range=%d pocket_hp=%d raid=%d\n",keep->hp,hp,e->actor.position.x,e->actor.position.y,e->target_kind,e->target_id,e->actor.next,e->actor.path.count,e->actor.target_x,e->actor.target_y,e->actor.cooldown,e->retarget,e->def.range,pocket->hp,s->raid_id);
    EXPECT(keep->hp<hp);EXPECT(e->attack_serial>=4);EXPECT(e->target_id!=pocket->id);
    GamePoint hit={(float)keep->x+1,(float)keep->y+1};s->visibility[((int)hit.y)*s->summary.map_width+(int)hit.x]=2;
    game_hit_fx_direction(s,(GamePoint){hit.x,hit.y+1},hit,0);
    GameHitFx *fx=&s->hit_fx[(s->hit_cursor+159)%160];
    EXPECT(fx->active);EXPECT(fabsf(fx->angle+90)<.01f);
    GameDrawItem depth[2]={{97*16,DRAW_VILLAGER,0,1},{96*16,DRAW_HIT_FX,0,2}};
    qsort(depth,2,sizeof(*depth),game_draw_compare);EXPECT(depth[0].kind==DRAW_HIT_FX);
    game_session_free(s);free(s);
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
    raid_attack_frames(argv[1]);
    bot_trade_recovery(argv[1]);
    ability_during_attack(argv[1]);
    actual_retreat(argv[1]);
    blocked_raid_target(argv[1]);
    if (!failed)
        puts("C native runtime tests passed");
    return failed ? 1 : 0;
}
