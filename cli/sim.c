/*
 * sim.c — 시뮬레이터와 소크 테스트 (설명은 sim.h)
 */
#include "sim.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "castle_core.h"
#include "battle.h"
#include "format.h"
#include "walker.h"

/* ---- 봇 공통 도우미 ---------------------------------------------------- */

typedef struct BotContext {
    CoreWorld *world;
    CoreSummaryView summary;
    CoreBuildingView *buildings; /* 동적 배열 */
    int32_t building_count;
    int32_t building_capacity;
    int32_t keep_door_x;
    int32_t keep_door_y;
} BotContext;

static int refresh_buildings(BotContext *bot)
{
    int32_t rc;
    (void)core_get_summary(bot->world, &bot->summary);
    rc = core_copy_buildings(bot->world, bot->buildings, bot->building_capacity, &bot->building_count);
    if (rc == CORE_ERR_BUFFER_TOO_SMALL) {
        int32_t capacity = bot->building_count + 32;
        CoreBuildingView *grown =
            (CoreBuildingView *)realloc(bot->buildings, (size_t)capacity * sizeof(CoreBuildingView));
        if (grown == NULL) {
            return 0;
        }
        bot->buildings = grown;
        bot->building_capacity = capacity;
        rc = core_copy_buildings(bot->world, bot->buildings, bot->building_capacity, &bot->building_count);
    }
    if (rc != CORE_OK) {
        return 0;
    }
    bot->keep_door_x = bot->summary.keep_x;
    bot->keep_door_y = bot->summary.keep_y + 2;
    return 1;
}

static int32_t count_type(const BotContext *bot, int32_t type)
{
    int32_t count = 0;
    int32_t i;
    for (i = 0; i < bot->building_count; i++) {
        count += bot->buildings[i].type == type;
    }
    return count;
}

static int32_t count_constructing(const BotContext *bot)
{
    int32_t count = 0;
    int32_t i;
    for (i = 0; i < bot->building_count; i++) {
        count += bot->buildings[i].state == CORE_BSTATE_CONSTRUCTING && bot->buildings[i].type != CORE_BLD_WALL;
    }
    return count;
}

/* 성채 둘레에서 이 종류를 지을 가장 좋은 자리를 찾아 짓고, 문에서 성채까지 도로를 냅니다.
 * 생산성이 높은 자리를 고르되, 성채에서 멀수록 조금씩 깎습니다 */
static int build_best(BotContext *bot, int32_t type, int32_t radius)
{
    CorePlacementView preview;
    int32_t best_score = -1000000;
    int32_t best_x = 0;
    int32_t best_y = 0;
    int32_t dx;
    int32_t dy;
    int32_t id;
    int32_t reason;
    int32_t placed;
    for (dy = -radius; dy <= radius; dy++) {
        for (dx = -radius; dx <= radius; dx++) {
            int32_t x = bot->summary.keep_x + dx;
            int32_t y = bot->summary.keep_y + dy;
            int32_t score;
            (void)core_can_place(bot->world, type, x, y, &preview);
            if (preview.result != CORE_PLACE_OK) {
                continue;
            }
            score = preview.productivity * 4 - (abs(dx) + abs(dy)) * 3;
            if (score > best_score) {
                best_score = score;
                best_x = x;
                best_y = y;
            }
        }
    }
    if (best_score == -1000000 || core_place_building(bot->world, type, best_x, best_y, &id, &reason) != CORE_OK) {
        return 0;
    }
    (void)core_can_place(bot->world, type, best_x, best_y, &preview);
    /* 새 건물의 문에서 성채 문까지 도로 (모자라면 놓을 수 있는 만큼만) */
    {
        int32_t i;
        if (refresh_buildings(bot)) {
            for (i = 0; i < bot->building_count; i++) {
                if (bot->buildings[i].id == id && bot->buildings[i].door_x >= 0) {
                    (void)core_place_line(bot->world, CORE_BLD_ROAD, bot->buildings[i].door_x,
                                          bot->buildings[i].door_y, bot->keep_door_x, bot->keep_door_y, &placed,
                                          &reason);
                }
            }
        }
    }
    (void)refresh_buildings(bot);
    return 1;
}

/* ---- 봇 (이름 → 하루 행동 함수 표) -------------------------------------- */

/* 하루가 시작될 때 불립니다. 봇은 공개 API 명령만 씁니다 */
typedef void (*BotDailyFn)(BotContext *bot);

typedef struct Bot {
    const char *name;
    const char *description;
    BotDailyFn on_new_day;
} Bot;

static void bot_idle(BotContext *bot)
{
    (void)bot; /* 아무것도 하지 않는 기준선 */
}

/* "적당히 하는 플레이어": 먹을 것, 집, 목재, 석재, 약초, 창고 순으로 부족한 것을 채웁니다 */
/* 성채에서 위협이 오는 쪽으로 몇 칸 떨어진 곳에 가장 가까운, 지을 수 있는 자리 */
static int build_toward(BotContext *bot, int32_t type, int32_t direction, int32_t reach)
{
    static const int32_t DX[CORE_DIR_COUNT] = {0, 1, 0, -1};
    static const int32_t DY[CORE_DIR_COUNT] = {-1, 0, 1, 0};
    CorePlacementView preview;
    int32_t goal_x = bot->summary.keep_x + DX[direction] * reach;
    int32_t goal_y = bot->summary.keep_y + DY[direction] * reach;
    int32_t best = -1;
    int32_t best_x = 0;
    int32_t best_y = 0;
    int32_t dx;
    int32_t dy;
    int32_t id;
    int32_t reason;
    for (dy = -6; dy <= 6; dy++) {
        for (dx = -6; dx <= 6; dx++) {
            int32_t distance = abs(dx) + abs(dy);
            (void)core_can_place(bot->world, type, goal_x + dx, goal_y + dy, &preview);
            if (preview.result == CORE_PLACE_OK && (best < 0 || distance < best)) {
                best = distance;
                best_x = goal_x + dx;
                best_y = goal_y + dy;
            }
        }
    }
    if (best < 0 || core_place_building(bot->world, type, best_x, best_y, &id, &reason) != CORE_OK) {
        return 0;
    }
    (void)refresh_buildings(bot);
    return 1;
}

/* 방어: 위협이 예고되면 그쪽으로 감시탑을 (주민 열 명에 하나 더), 주민이 늘면 병영을 */
static int bot_defend(BotContext *bot)
{
    const CoreSummaryView *s = &bot->summary;
    int32_t towers = count_type(bot, CORE_BLD_TOWER);
    if (s->threat_type == CORE_THREAT_NONE && s->raid_id == 0) {
        return 0;
    }
    if (s->population >= 6 && towers < 1 + s->population / 10) {
        return build_toward(bot, CORE_BLD_TOWER, s->threat_direction, 5);
    }
    if (s->population >= 14 && count_type(bot, CORE_BLD_BARRACKS) == 0) {
        return build_best(bot, CORE_BLD_BARRACKS, 12);
    }
    return 0;
}

/* 부서진 건물은 바로 고칩니다 (돈이 되는 만큼) */
static void bot_repair(BotContext *bot)
{
    int32_t reason;
    int32_t i;
    for (i = 0; i < bot->building_count; i++) {
        const CoreBuildingView *b = &bot->buildings[i];
        if (b->state != CORE_BSTATE_CONSTRUCTING && b->hp < b->max_hp) {
            (void)core_repair(bot->world, b->id, &reason);
        }
    }
}

/* 모자란 만큼 사 봅니다: 큰 묶음부터 시도해 돈이 되는 만큼 */
static void bot_buy_up_to(BotContext *bot, int32_t resource, int32_t target)
{
    int32_t have = bot->summary.resources[resource];
    int32_t lots;
    int32_t gold;
    int32_t reason;
    if (have >= target || bot->summary.resources[CORE_RES_GOLD] < 60) {
        return;
    }
    lots = (target - have) / CORE_MARKET_LOT + 1;
    if (lots > CORE_MARKET_MAX_LOTS) {
        lots = CORE_MARKET_MAX_LOTS;
    }
    for (; lots > 0; lots--) {
        if (core_trade(bot->world, resource, lots, &gold, &reason) == CORE_OK) {
            return;
        }
    }
}

/* 시장에 일꾼이 없으면 농부 한 명을 보냅니다 (겨울에는 농장이 놀므로) */
static void bot_staff_market(BotContext *bot)
{
    CoreVillagerView *views;
    int32_t count = 0;
    int32_t market = 0;
    int32_t i;
    for (i = 0; i < bot->building_count; i++) {
        if (bot->buildings[i].type == CORE_BLD_MARKET && bot->buildings[i].state == CORE_BSTATE_ACTIVE &&
            bot->buildings[i].workers == 0) {
            market = bot->buildings[i].id;
        }
    }
    if (market == 0) {
        return;
    }
    views = (CoreVillagerView *)malloc((size_t)(bot->summary.villager_count_hint + 1) * sizeof(CoreVillagerView));
    if (views != NULL &&
        core_copy_villagers(bot->world, views, bot->summary.villager_count_hint + 1, &count) == CORE_OK) {
        for (i = 0; i < count; i++) {
            if (views[i].job == CORE_BLD_FARM || (i == count - 1 && views[i].job >= 0)) {
                (void)core_assign_worker(bot->world, views[i].id, market);
                break;
            }
        }
    }
    free(views);
}

/* 시장: 겨울 준비가 모자라거나 식량이 바닥나면 사고, 창고가 넘칠 것 같은 물건은 팝니다 */
static void bot_trade(BotContext *bot)
{
    const CoreSummaryView *s = &bot->summary;
    int32_t gold;
    int32_t reason;
    int32_t r;
    if (!s->market_open) {
        bot_staff_market(bot);
        (void)core_get_summary(bot->world, &bot->summary);
        if (!s->market_open) {
            return;
        }
    }
    bot_buy_up_to(bot, CORE_RES_FOOD, s->winter_food_need > 0 ? s->winter_food_need : s->population * 3);
    (void)core_get_summary(bot->world, &bot->summary);
    bot_buy_up_to(bot, CORE_RES_WOOD, s->winter_wood_need > 0 ? s->winter_wood_need : s->housing_capacity / 2);
    (void)core_get_summary(bot->world, &bot->summary);
    for (r = CORE_RES_STONE; r < CORE_RES_COUNT; r++) {
        if (s->resource_capacity[r] > 0 && s->resources[r] * 100 >= s->resource_capacity[r] * 90) {
            (void)core_trade(bot->world, r, -3, &gold, &reason);
        }
    }
}

/* 상인: 남는 돈으로 마정석을 사고, 겨울 전에 식량 제안이 있으면 받습니다 */
static void bot_merchant(BotContext *bot)
{
    CoreMerchantView merchant;
    int32_t reason;
    int32_t i;
    if (core_get_merchant(bot->world, &merchant) != CORE_OK || merchant.days_left <= 0) {
        return;
    }
    for (i = 0; i < merchant.deal_count; i++) {
        const CoreDealView *deal = &merchant.deals[i];
        int wanted = deal->get_resource == CORE_RES_MANA ||
                     (deal->get_resource == CORE_RES_FOOD && bot->summary.winter_food_need > 0);
        if (!deal->taken && wanted && deal->give_resource == CORE_RES_GOLD &&
            bot->summary.resources[CORE_RES_GOLD] > deal->give_amount + 100) {
            (void)core_merchant_accept(bot->world, i, &reason);
        }
    }
}

static void bot_reasonable(BotContext *bot)
{
    const CoreSummaryView *s;
    int32_t farms;
    int32_t houses;
    int32_t built = 0;
    int32_t i;
    if (!refresh_buildings(bot)) {
        return;
    }
    s = &bot->summary;
    if (count_constructing(bot) >= 3) {
        return; /* 한꺼번에 너무 많이 짓지 않음 */
    }
    farms = count_type(bot, CORE_BLD_FARM);
    houses = count_type(bot, CORE_BLD_HOUSE);
    /* 0) 창고: 물건 하나라도 한도의 70% 를 넘으면 가장 먼저 (겨울 대비 비축 공간) */
    for (i = CORE_RES_FOOD; i < CORE_RES_COUNT; i++) {
        if (s->resource_capacity[i] > 0 && s->resources[i] * 100 >= s->resource_capacity[i] * 70) {
            built += build_best(bot, CORE_BLD_WAREHOUSE, 10);
            break;
        }
    }
    /* 1) 식량: 주민 4명당 농장 하나 (여름·가을에는 겨울을 대비해 3명당 하나), 첫날은 무조건 하나 */
    if (farms == 0 || (farms < s->population / (s->season == CORE_SEASON_SUMMER || s->season == CORE_SEASON_AUTUMN
                                                     ? 3
                                                     : 4) +
                                    1 &&
                       s->resources[CORE_RES_FOOD] < s->population * 12)) {
        built += build_best(bot, CORE_BLD_FARM, 10);
    }
    /* 2) 집: 집 없는 주민이 있거나 빈자리가 2 보다 적으면 */
    if (built < 2 && (s->homeless > 0 || s->housing_capacity - (s->population - s->homeless) < 2)) {
        built += build_best(bot, CORE_BLD_HOUSE, 8);
    }
    /* 3) 목재: 벌목장은 주민 8명당 하나 */
    if (built < 2 && count_type(bot, CORE_BLD_LUMBER) < 1 + s->population / 8) {
        built += build_best(bot, CORE_BLD_LUMBER, 14);
    }
    /* 3-1) 겨울 대비: 여름·가을에 난방 목재가 모자랄 것 같으면 벌목장을 더 */
    if (built < 2 && (s->season == CORE_SEASON_SUMMER || s->season == CORE_SEASON_AUTUMN) &&
        s->resources[CORE_RES_WOOD] < s->housing_capacity && count_type(bot, CORE_BLD_LUMBER) < 2 + s->population / 6) {
        built += build_best(bot, CORE_BLD_LUMBER, 14);
    }
    /* 4) 석재와 약초 */
    if (built < 2 && houses >= 2 && count_type(bot, CORE_BLD_QUARRY) < 1 + s->population / 12) {
        built += build_best(bot, CORE_BLD_QUARRY, 16);
    }
    if (built < 2 && s->population >= 8 && count_type(bot, CORE_BLD_GATHERER) < 1 + s->population / 15) {
        built += build_best(bot, CORE_BLD_GATHERER, 14);
    }
    /* 5) 치료소와 시장 */
    if (built < 2 && (s->population >= 10 || s->sick_count > 0) && count_type(bot, CORE_BLD_HEALER) == 0) {
        built += build_best(bot, CORE_BLD_HEALER, 18);
    }
    if (built < 2 && s->population >= 14 && count_type(bot, CORE_BLD_MARKET) == 0) {
        built += build_best(bot, CORE_BLD_MARKET, 20);
    }
    if (built < 2) {
        built += bot_defend(bot);
    }
    bot_repair(bot);
    bot_trade(bot);
    bot_merchant(bot);
}

static const Bot BOTS[] = {
    {"idle", "아무것도 하지 않음 (기준선)", bot_idle},
    {"reasonable", "먹을 것, 집, 자원, 창고를 차례로 짓는 보통 플레이어", bot_reasonable},
};
#define BOT_COUNT ((int)(sizeof(BOTS) / sizeof(BOTS[0])))

static const Bot *find_bot(const char *name)
{
    int i;
    for (i = 0; i < BOT_COUNT; i++) {
        if (strcmp(BOTS[i].name, name) == 0) {
            return &BOTS[i];
        }
    }
    return NULL;
}

int sim_bot_exists(const char *bot_name)
{
    return find_bot(bot_name) != NULL;
}

void sim_print_bots(void)
{
    int i;
    for (i = 0; i < BOT_COUNT; i++) {
        printf("  %-12s %s\n", BOTS[i].name, BOTS[i].description);
    }
}

static int32_t create_world(int32_t seed, int difficulty, const char *name, const CoreDefs *defs, CoreWorld **out)
{
    CoreNewGameParams params;
    memset(&params, 0, sizeof(params));
    strncpy(params.castle_name, name, sizeof(params.castle_name) - 1);
    params.seed = seed;
    params.difficulty = difficulty;
    return core_create(&params, defs, out);
}

static void write_csv_row(FILE *out, const CoreSummaryView *summary)
{
    int i;
    fprintf(out, "%d,%d,%d", (int)summary->day, (int)summary->year, (int)summary->season);
    for (i = 0; i < CORE_RES_COUNT; i++) {
        fprintf(out, ",%d", (int)summary->resources[i]);
    }
    fprintf(out, ",%d,%d,%d,%d,%d,%d,%d,%d", (int)summary->resource_capacity[CORE_RES_FOOD],
            (int)summary->population, (int)summary->average_mood, (int)summary->homeless, (int)summary->employed,
            (int)summary->building_count, (int)summary->rank, (int)summary->threat_strength);
    fprintf(out, ",%d,%d,%d,%d,%d,%d,%d\n", (int)summary->weather, (int)summary->nature_event, (int)summary->sick_count,
            (int)summary->cold_houses, (int)summary->merchant_days_left, (int)summary->threat_type,
            (int)summary->guards);
}

int sim_run_simulation(int days, int32_t seed, int difficulty, const char *bot_name, const char *csv_path,
                       const CoreDefs *defs, int print_events)
{
    const Bot *bot = find_bot(bot_name);
    BotContext context;
    Walker walker;
    Battle battle;
    CoreConfigView config;
    CoreSummaryView summary;
    FILE *out = stdout;
    int day;
    int32_t tick;
    int status = 0;

    if (bot == NULL) {
        fprintf(stderr, "모르는 봇입니다: %s\n", bot_name);
        return 1;
    }
    memset(&context, 0, sizeof(context));
    (void)core_get_config(&config);
    if (create_world(seed, difficulty, "Simulation", defs, &context.world) != CORE_OK) {
        fprintf(stderr, "월드를 만들 수 없습니다\n");
        return 1;
    }
    if (csv_path != NULL) {
        out = fopen(csv_path, "w");
        if (out == NULL) {
            fprintf(stderr, "CSV 파일을 열 수 없습니다: %s\n", csv_path);
            core_destroy(context.world);
            return 1;
        }
    }
    walker_init(&walker);
    battle_init(&battle);
    fprintf(out, "day,year,season,gold,mana,food,wood,stone,herbs,capacity,population,mood,homeless,employed,"
                 "buildings,rank,threat_strength,weather,nature_event,sick,cold_houses,merchant,threat_type,guards\n");
    (void)core_get_summary(context.world, &summary);
    write_csv_row(out, &summary);
    for (day = 0; day < days && status == 0; day++) {
        CoreEventView event;
        bot->on_new_day(&context);
        for (tick = 0; tick < config.ticks_per_day; tick++) {
            if (walker_tick(&walker, context.world) != CORE_OK || battle_tick(&battle, context.world) != CORE_OK ||
                core_validate(context.world) != 0) {
                fprintf(stderr, "시뮬레이션 %d일째에 문제가 생겼습니다\n", day + 1);
                status = 1;
                break;
            }
        }
        while (core_poll_event(context.world, &event) == 1) {
            if (print_events && event.type != CORE_EVT_PRODUCED) {
                char text[200];
                fmt_event(&event, text, sizeof(text));
                fprintf(stderr, "[%d일] %s\n", day + 2, text);
            }
        }
        (void)core_get_summary(context.world, &summary);
        write_csv_row(out, &summary);
    }
    if (out != stdout && fclose(out) != 0) {
        status = 1;
    }
    walker_free(&walker);
    free(context.buildings);
    core_destroy(context.world);
    if (status == 0 && csv_path != NULL) {
        printf("시뮬레이션 완료: %d일, 시드 %d, 봇 %s → %s (주민 %d명, 기분 %d)\n", days, (int)seed, bot_name,
               csv_path, (int)summary.population, (int)summary.average_mood);
    }
    return status;
}

/* ---- 소크 테스트 ------------------------------------------------------ */

/* 소크 테스트용 작은 난수 (코어의 난수와 섞이지 않도록 따로 둡니다) */
static uint32_t soak_random(uint32_t *state)
{
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

static int32_t soak_pick(uint32_t *state, int32_t lo, int32_t hi)
{
    return lo + (int32_t)(soak_random(state) % (uint32_t)(hi - lo + 1));
}

/* 잘못된 호출을 섞어 보냅니다. 모두 오류 코드를 돌려줘야 하고 상태를 바꾸면 안 됩니다 */
static int poke_invalid_calls(CoreWorld *world)
{
    uint8_t tiny[4];
    uint64_t before = 0;
    uint64_t after = 1;
    int32_t id = 0;
    int32_t reason = 0;
    int ok = 1;
    (void)core_state_hash(world, &before);
    ok &= core_tick(world, 0) == CORE_ERR_INVALID_ARG;
    ok &= core_tick(world, -3) == CORE_ERR_INVALID_ARG;
    ok &= core_copy_terrain(world, tiny, (int32_t)sizeof(tiny)) == CORE_ERR_BUFFER_TOO_SMALL;
    ok &= core_get_summary(world, NULL) == CORE_ERR_NULL_ARG;
    ok &= core_save(world, "") == CORE_ERR_INVALID_ARG;
    ok &= core_place_building(world, CORE_BLD_KEEP, 10, 10, &id, &reason) == CORE_ERR_REJECTED;
    ok &= core_place_building(world, 999, 10, 10, &id, &reason) == CORE_ERR_REJECTED;
    ok &= core_place_building(world, CORE_BLD_HOUSE, -50, 9999, &id, &reason) == CORE_ERR_REJECTED;
    ok &= core_demolish(world, -1) == CORE_ERR_NOT_FOUND;
    ok &= core_assign_worker(world, 999999, 1) == CORE_ERR_NOT_FOUND;
    ok &= core_villager_arrived(world, 999999, 1) == CORE_ERR_NOT_FOUND;
    ok &= core_set_auto_assign(world, 1, 7) == CORE_ERR_INVALID_ARG;
    ok &= core_report_building_damage(world, 1, 1, -5) == CORE_ERR_INVALID_ARG;
    ok &= core_report_enemy_killed(world, 99999, 1) == CORE_IGNORED;
    ok &= core_report_raid_ended(world, -1) == CORE_IGNORED;
    ok &= core_report_loot(world, 99999, 1, 1, NULL) == CORE_IGNORED;
    (void)core_state_hash(world, &after);
    return ok && before == after;
}

/* 저장했다가 다시 불러와서, 불러온 월드로 계속 진행합니다. 해시가 같아야 합니다 */
static int save_load_roundtrip(CoreWorld **world, const char *path, const CoreDefs *defs)
{
    CoreWorld *loaded = NULL;
    uint64_t before = 0;
    uint64_t after = 1;
    int32_t rc = core_save(*world, path);
    if (rc == CORE_ERR_REJECTED) {
        return 1; /* 습격 중에는 저장할 수 없음: 규칙대로이므로 실패가 아님 */
    }
    if (rc != CORE_OK || core_load(path, defs, &loaded, NULL) != CORE_OK) {
        return 0;
    }
    (void)core_state_hash(*world, &before);
    (void)core_state_hash(loaded, &after);
    core_destroy(*world);
    *world = loaded;
    return before == after;
}

/* 하루에 한 번: 무작위 명령 (대부분은 규칙에 막혀 거절됨, 그것도 확인할 것) */
static void random_commands(BotContext *bot, uint32_t *random_state)
{
    CoreVillagerView *views;
    int32_t villager_count = 0;
    int32_t i;
    int32_t id;
    int32_t reason;
    int32_t placed;
    if (!refresh_buildings(bot)) {
        return;
    }
    for (i = 0; i < 6; i++) {
        int32_t type = soak_pick(random_state, 1, CORE_BLD_COUNT - 1);
        int32_t x = bot->summary.keep_x + soak_pick(random_state, -16, 16);
        int32_t y = bot->summary.keep_y + soak_pick(random_state, -16, 16);
        (void)core_place_building(bot->world, type, x, y, &id, &reason);
    }
    (void)core_place_line(bot->world, soak_pick(random_state, 0, 1) ? CORE_BLD_ROAD : CORE_BLD_WALL,
                          bot->summary.keep_x + soak_pick(random_state, -12, 12),
                          bot->summary.keep_y + soak_pick(random_state, -12, 12),
                          bot->summary.keep_x + soak_pick(random_state, -12, 12),
                          bot->summary.keep_y + soak_pick(random_state, -12, 12), &placed, &reason);
    if (bot->building_count > 1 && soak_pick(random_state, 0, 3) == 0) {
        (void)core_demolish(bot->world, bot->buildings[soak_pick(random_state, 0, bot->building_count - 1)].id);
    }
    (void)core_remove_road(bot->world, bot->summary.keep_x + soak_pick(random_state, -12, 12),
                           bot->summary.keep_y + soak_pick(random_state, -12, 12));
    views = (CoreVillagerView *)malloc((size_t)(bot->summary.villager_count_hint + 1) * sizeof(CoreVillagerView));
    if (views != NULL &&
        core_copy_villagers(bot->world, views, bot->summary.villager_count_hint + 1, &villager_count) == CORE_OK &&
        villager_count > 0 && bot->building_count > 0) {
        const CoreVillagerView *v = &views[soak_pick(random_state, 0, villager_count - 1)];
        int32_t target = bot->buildings[soak_pick(random_state, 0, bot->building_count - 1)].id;
        int32_t gold;
        (void)core_trade(bot->world, soak_pick(random_state, 0, CORE_RES_COUNT - 1),
                         soak_pick(random_state, 0, 1) ? soak_pick(random_state, 1, 4) : -soak_pick(random_state, 1, 4),
                         &gold, &reason);
        (void)core_merchant_accept(bot->world, soak_pick(random_state, 0, CORE_MERCHANT_MAX_DEALS - 1), &reason);
        (void)core_repair(bot->world, target, &reason);
        switch (soak_pick(random_state, 0, 4)) {
        case 0:
            (void)core_assign_worker(bot->world, v->id, target);
            break;
        case 1:
            (void)core_unassign_worker(bot->world, v->id);
            break;
        case 2:
            (void)core_release_worker(bot->world, v->id);
            break;
        case 3:
            (void)core_villager_arrived(bot->world, v->id, v->intent_rev - 1); /* 오래된 보고 */
            break;
        default:
            (void)core_villager_unreachable(bot->world, v->id, v->intent_rev);
            break;
        }
    }
    free(views);
}

/* 아무렇게나 고른 습격 보고 (틀린 id, 늦은 보고 포함). 무엇을 보내도 불변 조건이 지켜져야 합니다 */
static void random_reports(CoreWorld *world, uint32_t *random_state)
{
    CoreRaidView raid;
    int32_t raid_id;
    int32_t amount;
    if (core_get_raid(world, &raid) != CORE_OK || raid.id == 0 || soak_pick(random_state, 0, 3) != 0) {
        return;
    }
    raid_id = soak_pick(random_state, 0, 9) == 0 ? raid.id + 1 : raid.id; /* 가끔 틀린 습격 id */
    switch (soak_pick(random_state, 0, 6)) {
    case 0:
        (void)core_report_building_damage(world, raid_id, soak_pick(random_state, 1, 40), soak_pick(random_state, 0, 120));
        break;
    case 1:
        (void)core_report_villager_damage(world, raid_id, soak_pick(random_state, 1, 60), soak_pick(random_state, 0, 40));
        break;
    case 2:
        (void)core_report_enemy_killed(world, raid_id, soak_pick(random_state, 0, raid.enemy_count + 1));
        break;
    case 3:
        (void)core_report_loot(world, raid_id, soak_pick(random_state, 0, raid.enemy_count + 1),
                               soak_pick(random_state, 1, 10), &amount);
        break;
    case 4:
        (void)core_report_enemy_escaped(world, raid_id, soak_pick(random_state, 1, raid.enemy_count));
        break;
    case 5:
        if (soak_pick(random_state, 0, 20) == 0) {
            (void)core_report_raid_ended(world, raid_id);
        }
        break;
    default:
        break;
    }
}

static int soak_one_seed(int days, int32_t seed, const CoreDefs *defs, long *ticks_run)
{
    BotContext bot;
    Walker walker;
    Battle battle;
    CoreSummaryView summary;
    char path[64];
    char backup[72];
    uint32_t random_state = (uint32_t)seed * 2654435761u + 1u;
    int32_t end_day;
    int32_t last_day;
    int failures = 0;

    memset(&bot, 0, sizeof(bot));
    if (create_world(seed, seed % CORE_DIFF_COUNT, "Soak", defs, &bot.world) != CORE_OK) {
        fprintf(stderr, "  시드 %d: 월드를 만들 수 없습니다\n", (int)seed);
        return 1;
    }
    walker_init(&walker);
    battle_init(&battle);
    walker.instant = (seed % 2) == 0; /* 시드의 반은 바로 도착, 반은 실제 걷는 시간 */
    snprintf(path, sizeof(path), "soak_%d.sav", (int)seed);
    snprintf(backup, sizeof(backup), "%s.bak", path);
    (void)core_get_summary(bot.world, &summary);
    end_day = summary.day + days;
    last_day = summary.day;

    while (summary.day < end_day && failures == 0) {
        CoreEventView event;
        uint32_t roll = soak_random(&random_state);
        int32_t violations;

        /* 매 틱 진행 후 불변 조건 검사 */
        if (walker_tick(&walker, bot.world) != CORE_OK) {
            fprintf(stderr, "  시드 %d: core_tick 실패\n", (int)seed);
            failures++;
            break;
        }
        /* 습격: 시드 셋 중 둘은 숫자 전투, 하나는 아무렇게나 보낸 보고 (끝은 C 의 시간 초과에 맡김) */
        if (seed % 3 != 0) {
            if (battle_tick(&battle, bot.world) != CORE_OK) {
                fprintf(stderr, "  시드 %d: 전투 실패\n", (int)seed);
                failures++;
                break;
            }
        } else {
            random_reports(bot.world, &random_state);
        }
        (*ticks_run)++;
        violations = core_validate(bot.world);
        if (violations != 0) {
            fprintf(stderr, "  시드 %d: 불변 조건 위반 %d개 (%d일)\n", (int)seed, (int)violations, (int)summary.day);
            failures++;
            break;
        }
        /* 이벤트는 가끔만 꺼냅니다 (꺼내지 않아도 문제없어야 함) */
        if (roll % 7 == 0) {
            while (core_poll_event(bot.world, &event) == 1) {
            }
        }
        (void)core_get_summary(bot.world, &summary);
        if (summary.day != last_day) {
            last_day = summary.day;
            if (roll % 3 == 0) {
                bot_reasonable(&bot);
            } else {
                random_commands(&bot, &random_state);
            }
            if (roll % 10 == 0 && !save_load_roundtrip(&bot.world, path, defs)) {
                fprintf(stderr, "  시드 %d: 저장/불러오기 왕복이 어긋났습니다 (%d일)\n", (int)seed, (int)summary.day);
                failures++;
            }
            if (roll % 13 == 0 && !poke_invalid_calls(bot.world)) {
                fprintf(stderr, "  시드 %d: 잘못된 호출이 상태를 바꾸었습니다\n", (int)seed);
                failures++;
            }
        }
    }
    walker_free(&walker);
    free(bot.buildings);
    core_destroy(bot.world);
    remove(path);
    remove(backup);
    return failures;
}

int sim_run_soak(int days, int seeds, int32_t first_seed, const CoreDefs *defs)
{
    int i;
    int failed_seeds = 0;
    long ticks_run = 0;
    for (i = 0; i < seeds; i++) {
        int32_t seed = first_seed + i;
        if (soak_one_seed(days, seed, defs, &ticks_run) != 0) {
            failed_seeds++;
        }
    }
    if (failed_seeds == 0) {
        printf("소크 테스트 통과: 시드 %d개 × %d일, 틱 %ld번, 불변 조건 위반 0\n", seeds, days, ticks_run);
        return 0;
    }
    printf("소크 테스트 실패: 시드 %d개 중 %d개에서 문제\n", seeds, failed_seeds);
    return 1;
}
