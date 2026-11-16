/*
 * sim.c — 시뮬레이터와 소크 테스트 (설명은 sim.h)
 */
#include "sim.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "castle_core.h"
#include "battle.h"
#include "format.h"
#include "walker.h"
#include "platform.h"

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
    /* 새 건물의 문에서 성채 문까지 도로 (금화가 넉넉할 때만: 초반에는 도로보다 집이 급함) */
    if (bot->summary.resources[CORE_RES_GOLD] >= 60) {
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

/* 가을·겨울에 봄까지 버틸 식량·목재가 모자란가 */
static int winter_short(const CoreSummaryView *s)
{
    return s->resources[CORE_RES_FOOD] < s->winter_food_need || s->resources[CORE_RES_WOOD] < s->winter_wood_need;
}

static void bot_staff(BotContext *bot, int32_t type, int32_t want);

/* 성장: 조건을 채우면 승급, 등급이 풀어 주는 마정석 광산·마탑을 짓고, 마탑에서 늘 연구 하나를 돌림 */
static int bot_progress(BotContext *bot)
{
    const CoreSummaryView *s = &bot->summary;
    int32_t reason;
    int32_t built = 0;
    if (s->rank_ready && core_rank_up(bot->world, &reason) == CORE_OK) {
        (void)refresh_buildings(bot);
    }
    if (built == 0 && s->rank >= CORE_RANK_VILLAGE &&
        count_type(bot, CORE_BLD_MANA_MINE) < (s->resources[CORE_RES_MANA] < 30 ? 2 : 1) + s->population / 20) {
        built += build_best(bot, CORE_BLD_MANA_MINE, 18);
    }
    if (built == 0 && s->rank >= CORE_RANK_FORTRESS &&
        count_type(bot, CORE_BLD_MAGE_TOWER) < (s->population >= 30 ? 2 : 1)) {
        built += build_best(bot, CORE_BLD_MAGE_TOWER, 14);
    }
    /* 마법사가 없으면 연구가 안 됨 (식량이 넉넉할 때만 농부를 데려옴) */
    if (s->resources[CORE_RES_FOOD] > s->population * 8 && s->season != CORE_SEASON_WINTER) {
        bot_staff(bot, CORE_BLD_MAGE_TOWER, 2);
    }
    /* 연구: 시작할 수 있는 것 중 점수가 가장 적은 것 (같으면 번호 순) — 금화를 40 남겨 둠 */
    if (s->research_active < 0 && count_type(bot, CORE_BLD_MAGE_TOWER) > 0 && !winter_short(s)) {
        CoreResearchView view;
        int32_t best = -1;
        int32_t best_points = 0;
        int32_t i;
        for (i = 0; core_get_research(bot->world, i, &view) == CORE_OK; i++) {
            if (view.state == CORE_RSTATE_AVAILABLE && s->resources[CORE_RES_GOLD] >= view.cost_gold + 40 &&
                s->resources[CORE_RES_MANA] >= view.cost_mana && (best < 0 || view.points < best_points)) {
                best = i;
                best_points = view.points;
            }
        }
        if (best >= 0) {
            (void)core_start_research(bot->world, best, &reason);
        }
    }
    return built;
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
/* type 건물 가운데 일꾼이 want 명보다 적은 것에 농부(없으면 아무 일꾼) 한 명을 옮깁니다.
 * 모두 일하고 있으면 자동 배치가 놀고 있는 주민을 보낼 수 없기 때문 (시장, 마탑) */
static void bot_staff(BotContext *bot, int32_t type, int32_t want)
{
    CoreVillagerView *views;
    int32_t count = 0;
    int32_t market = 0;
    int32_t i;
    for (i = 0; i < bot->building_count; i++) {
        if (bot->buildings[i].type == type && bot->buildings[i].state == CORE_BSTATE_ACTIVE &&
            bot->buildings[i].workers < want) {
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
            if ((views[i].job == CORE_BLD_FARM || (i == count - 1 && views[i].job >= 0)) && views[i].job != type) {
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
        bot_staff(bot, CORE_BLD_MARKET, 1);
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

/* 짓거나, 금화만 모자라서 못 지었으면 2(금화를 모음)를 돌려줍니다. toward >= 0 이면 그 방향에 (감시탑) */
static int build_or_save(BotContext *bot, int32_t type, int32_t radius, int32_t toward, int32_t *built)
{
    CoreBuildingDefView def;
    int32_t i;
    if (toward >= 0 ? build_toward(bot, type, toward, radius) : build_best(bot, type, radius)) {
        (*built)++;
        return 0;
    }
    if (core_get_building_def(bot->world, type, &def) != CORE_OK) {
        return 0;
    }
    for (i = 0; i < CORE_RES_COUNT; i++) {
        if (i != CORE_RES_GOLD && bot->summary.resources[i] < def.cost[i]) {
            return 0; /* 다른 자원이 모자람: 모으지 않고 그 자원을 만드는 건물을 지음 */
        }
    }
    return bot->summary.resources[CORE_RES_GOLD] < def.cost[CORE_RES_GOLD] ? 2 : 0;
}

/* 다음 등급에 모자란 것을 채웁니다. 금화를 모아야 하면 2, 마정석만 모자라면 1 */
static int bot_goal(BotContext *bot, int32_t *built)
{
    const CoreSummaryView *s = &bot->summary;
    CoreRankView rank;
    if (core_get_rank(bot->world, &rank) != CORE_OK || rank.next_rank < 0) {
        return 0;
    }
    /* 먹을 것과 목재가 먼저: 농장·벌목장이 없으면 승급 목표는 미룸 */
    if (count_type(bot, CORE_BLD_FARM) == 0 || count_type(bot, CORE_BLD_LUMBER) == 0) {
        return 0;
    }
    /* 마정석은 천천히 모이므로 마을이 되면 광산부터 (공물·마탑·연구에 모두 마정석이 듦) */
    if (s->rank >= CORE_RANK_VILLAGE && count_type(bot, CORE_BLD_MANA_MINE) == 0) {
        int saving = build_or_save(bot, CORE_BLD_MANA_MINE, 18, -1, built);
        if (saving || *built > 0) {
            return saving;
        }
    }
    if (rank.have_defense < rank.need_defense) {
        int32_t direction = s->threat_type != CORE_THREAT_NONE ? s->threat_direction : 0;
        return build_or_save(bot, CORE_BLD_TOWER, 5, direction, built);
    }
    if (rank.have_research < rank.need_research || s->rank >= CORE_RANK_FORTRESS) {
        if (count_type(bot, CORE_BLD_MAGE_TOWER) == 0) {
            return build_or_save(bot, CORE_BLD_MAGE_TOWER, 14, -1, built);
        }
        if (s->research_active < 0 && rank.have_research < rank.need_research) {
            (void)bot_progress(bot); /* 연구 시작 (금화가 모자라면 모음) */
            (void)core_get_summary(bot->world, &bot->summary);
            return bot->summary.research_active < 0 ? 2 : 0;
        }
    }
    if (rank.have_population >= rank.need_population && rank.have_mood >= rank.need_mood &&
        rank.have_defense >= rank.need_defense && rank.have_research >= rank.need_research) {
        if (s->resources[CORE_RES_GOLD] < rank.tribute_gold) {
            return 2;
        }
        if (s->resources[CORE_RES_MANA] < rank.tribute_mana) {
            return 1;
        }
    }
    return 0;
}

static void bot_reasonable(BotContext *bot)
{
    const CoreSummaryView *s;
    int essentials_ok;
    int32_t farms;
    int32_t houses;
    int32_t built = 0;
    int32_t i;
    int saving = 0;
    if (!refresh_buildings(bot)) {
        return;
    }
    s = &bot->summary;
    {
        int32_t reason;
        if (s->rank_ready && core_rank_up(bot->world, &reason) == CORE_OK) {
            (void)refresh_buildings(bot);
            s = &bot->summary;
        }
    }
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
    /* 1-1) 첫 벌목장과 첫 채석장은 집보다 먼저 (목재·석재가 없으면 아무것도 못 지음). 목재가 바닥이면 벌목장을 더 */
    if (built < 2 && (count_type(bot, CORE_BLD_LUMBER) == 0 ||
                      (s->resources[CORE_RES_WOOD] < 30 && count_type(bot, CORE_BLD_LUMBER) < 1 + s->population / 8))) {
        built += build_best(bot, CORE_BLD_LUMBER, 14);
    }
    if (built < 2 && houses >= 1 && count_type(bot, CORE_BLD_QUARRY) == 0 && s->resources[CORE_RES_STONE] < 15) {
        built += build_best(bot, CORE_BLD_QUARRY, 16);
    }
    /* 2) 집: 집 없는 주민이 있거나 빈자리가 2 보다 적으면 (이주민이 곧 세금이므로 집은 아끼지 않음) */
    if (built < 2 && (s->homeless > 0 || s->housing_capacity - (s->population - s->homeless) < 2)) {
        built += build_best(bot, CORE_BLD_HOUSE, 8);
    }
    /* 2-0) 승급에 꼭 필요한 건물(첫 광산, 모자란 감시탑, 첫 마탑)은 겨울 준비와 상관없이 짓되,
     *      먹을 것이 넉넉할 때만 (숲이 적은 지도에서 목재를 써 버려 굶지 않게) */
    essentials_ok = s->resources[CORE_RES_FOOD] >= s->population * 6;
    if (built < 2 && essentials_ok && s->rank >= CORE_RANK_VILLAGE && count_type(bot, CORE_BLD_MANA_MINE) == 0) {
        built += build_best(bot, CORE_BLD_MANA_MINE, 18);
    }
    if (built < 2 && essentials_ok && s->rank >= CORE_RANK_VILLAGE && s->resources[CORE_RES_WOOD] >= 30) {
        CoreRankView rank;
        if (core_get_rank(bot->world, &rank) == CORE_OK && rank.next_rank >= 0 && rank.have_defense < rank.need_defense) {
            built += build_toward(bot, CORE_BLD_TOWER, s->threat_type != CORE_THREAT_NONE ? s->threat_direction : 0, 5);
        }
    }
    if (built < 2 && essentials_ok && s->rank >= CORE_RANK_FORTRESS && count_type(bot, CORE_BLD_MAGE_TOWER) == 0 &&
        s->resources[CORE_RES_WOOD] >= 40) {
        built += build_best(bot, CORE_BLD_MAGE_TOWER, 14);
    }
    /* 2-1) 주민이 열 명이 되면 시장 (겨울 식량·목재를 살 곳) */
    if (built < 2 && s->population >= 10 && count_type(bot, CORE_BLD_MARKET) == 0) {
        built += build_best(bot, CORE_BLD_MARKET, 20);
    }
    /* 먹을 것과 집 다음에는 승급 목표부터 (방어 → 마정석 광산 → 마탑과 연구 → 공물).
     * 겨울 준비가 모자라면 목표는 미루고 금화를 식량·목재에 씀 */
    if (!winter_short(s) && s->resources[CORE_RES_FOOD] >= s->population * 6 && s->resources[CORE_RES_WOOD] >= 30) {
        saving = bot_goal(bot, &built);
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
    if (built < 2 && houses >= 1 && count_type(bot, CORE_BLD_QUARRY) < 1 + s->population / 12) {
        built += build_best(bot, CORE_BLD_QUARRY, 16);
    }
    if (saving) {
        /* 공물을 모으는 중: 금화가 넉넉하고 마정석만 모자라면 광산은 계속 지음 (마정석을 버는 길) */
        if (saving == 1) {
            (void)bot_progress(bot);
        }
        bot_repair(bot);
        bot_trade(bot);
        bot_merchant(bot);
        return;
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
    if (built < 2) {
        built += bot_progress(bot);
    }
    bot_repair(bot);
    bot_trade(bot);
    bot_merchant(bot);
}

static const Bot BOTS[] = {
    {"idle", u8"아무것도 하지 않음 (기준선)", bot_idle},
    {"reasonable", u8"먹을 것, 집, 자원, 창고를 차례로 짓는 보통 플레이어", bot_reasonable},
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

int sim_bot_daily(CoreWorld *world, const char *bot_name)
{
    const Bot *bot = find_bot(bot_name);
    BotContext context;
    if (world == NULL || bot == NULL) return CORE_ERR_INVALID_ARG;
    memset(&context, 0, sizeof(context));
    context.world = world;
    if (core_get_summary(world, &context.summary) != CORE_OK) return CORE_ERR_INVALID_ARG;
    if(context.summary.ruleset && strcmp(bot_name,"idle")!=0)return sim_expansion_bot_daily(world);
    bot->on_new_day(&context);
    free(context.buildings);
    return CORE_OK;
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
    fprintf(out, ",%d,%d,%d,%d,%d,%d,%d", (int)summary->weather, (int)summary->nature_event, (int)summary->sick_count,
            (int)summary->cold_houses, (int)summary->merchant_days_left, (int)summary->threat_type,
            (int)summary->guards);
    fprintf(out, ",%d,%d,%d\n", (int)summary->defense, (int)summary->research_done, (int)summary->outcome);
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
        fprintf(stderr, u8"모르는 봇입니다: %s\n", bot_name);
        return 1;
    }
    memset(&context, 0, sizeof(context));
    (void)core_get_config(&config);
    if (create_world(seed, difficulty, "Simulation", defs, &context.world) != CORE_OK) {
        fprintf(stderr, u8"월드를 만들 수 없습니다\n");
        return 1;
    }
    if (csv_path != NULL) {
        out = platform_fopen_utf8(csv_path, "wb");
        if (out == NULL) {
            fprintf(stderr, u8"CSV 파일을 열 수 없습니다: %s\n", csv_path);
            core_destroy(context.world);
            return 1;
        }
    }
    walker_init(&walker);
    battle_init(&battle);
    fprintf(out, "day,year,season,gold,mana,food,wood,stone,herbs,capacity,population,mood,homeless,employed,"
                 "buildings,rank,threat_strength,weather,nature_event,sick,cold_houses,merchant,threat_type,guards,"
                 "defense,research_done,outcome\n");
    (void)core_get_summary(context.world, &summary);
    write_csv_row(out, &summary);
    for (day = 0; day < days && status == 0; day++) {
        CoreEventView event;
        bot->on_new_day(&context);
        for (tick = 0; tick < config.ticks_per_day; tick++) {
            int32_t rc = walker_tick(&walker, context.world);
            if (rc == CORE_ERR_REJECTED) {
                break; /* 패배: 시간이 더는 흐르지 않음 (아래에서 기록하고 끝냄) */
            }
            if (rc != CORE_OK || battle_tick(&battle, context.world) != CORE_OK || core_validate(context.world) != 0) {
                fprintf(stderr, u8"시뮬레이션 %d일째에 문제가 생겼습니다\n", day + 1);
                status = 1;
                break;
            }
        }
        while (core_poll_event(context.world, &event) == 1) {
            if (print_events && event.type != CORE_EVT_PRODUCED) {
                char text[200];
                fmt_event(&event, text, sizeof(text));
                fprintf(stderr, u8"[%d일] %s\n", day + 2, text);
            }
        }
        (void)core_get_summary(context.world, &summary);
        write_csv_row(out, &summary);
        if (summary.outcome == CORE_OUTCOME_LOST) {
            fprintf(stderr, u8"패배: %s (%d일째)\n", fmt_lose_reason(summary.lose_reason), (int)summary.day);
            break;
        }
    }
    if (out != stdout && fclose(out) != 0) {
        status = 1;
    }
    walker_free(&walker);
    free(context.buildings);
    core_destroy(context.world);
    if (status == 0 && csv_path != NULL) {
        printf(u8"시뮬레이션 완료: %d일, 시드 %d, 봇 %s → %s (주민 %d명, 기분 %d, 등급 %s, 연구 %d%s)\n", days, (int)seed,
               bot_name, csv_path, (int)summary.population, (int)summary.average_mood, fmt_rank_name(summary.rank),
               (int)summary.research_done,
               summary.outcome == CORE_OUTCOME_WON ? u8", 승리" : summary.outcome == CORE_OUTCOME_LOST ? u8", 패배" : "");
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
        (void)core_start_research(bot->world, soak_pick(random_state, -2, CORE_MAX_RESEARCH + 2), &reason);
        (void)core_rank_up(bot->world, &reason);
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
    long budget = (long)days * 144; /* 이 시드에서 돌릴 틱 수 (패배하면 새 월드로 나머지를 돌림) */
    long done = 0;
    int32_t last_day;
    int32_t restarts = 0;
    int failures = 0;

    memset(&bot, 0, sizeof(bot));
    if (create_world(seed, seed % CORE_DIFF_COUNT, "Soak", defs, &bot.world) != CORE_OK) {
        fprintf(stderr, u8"  시드 %d: 월드를 만들 수 없습니다\n", (int)seed);
        return 1;
    }
    walker_init(&walker);
    battle_init(&battle);
    walker.instant = (seed % 2) == 0; /* 시드의 반은 바로 도착, 반은 실제 걷는 시간 */
    snprintf(path, sizeof(path), "soak_%d.sav", (int)seed);
    snprintf(backup, sizeof(backup), "%s.bak", path);
    (void)core_get_summary(bot.world, &summary);
    last_day = summary.day;

    while (done < budget && failures == 0) {
        CoreEventView event;
        uint32_t roll = soak_random(&random_state);
        int32_t violations;
        int32_t rc;

        /* 매 틱 진행 후 불변 조건 검사 */
        rc = walker_tick(&walker, bot.world);
        if (rc == CORE_ERR_REJECTED && summary.outcome == CORE_OUTCOME_LOST) {
            /* 패배한 월드도 저장·불러오기·검사가 맞아야 함. 그다음 새 월드로 남은 틱을 돌림 */
            if (!save_load_roundtrip(&bot.world, path, defs) || core_validate(bot.world) != 0) {
                fprintf(stderr, u8"  시드 %d: 패배한 월드의 저장/불러오기가 어긋났습니다\n", (int)seed);
                failures++;
                break;
            }
            core_destroy(bot.world);
            bot.world = NULL;
            restarts++;
            if (create_world(seed + 100000 * restarts, seed % CORE_DIFF_COUNT, "Soak", defs, &bot.world) != CORE_OK) {
                fprintf(stderr, u8"  시드 %d: 새 월드를 만들 수 없습니다\n", (int)seed);
                failures++;
                break;
            }
            walker_free(&walker);
            walker_init(&walker);
            walker.instant = (seed % 2) == 0;
            battle_init(&battle);
            (void)core_get_summary(bot.world, &summary);
            last_day = summary.day;
            continue;
        }
        if (rc != CORE_OK) {
            fprintf(stderr, u8"  시드 %d: core_tick 실패\n", (int)seed);
            failures++;
            break;
        }
        done++;
        /* 습격: 시드 셋 중 둘은 숫자 전투, 하나는 아무렇게나 보낸 보고 (끝은 C 의 시간 초과에 맡김) */
        if (seed % 3 != 0) {
            if (battle_tick(&battle, bot.world) != CORE_OK) {
                fprintf(stderr, u8"  시드 %d: 전투 실패\n", (int)seed);
                failures++;
                break;
            }
        } else {
            random_reports(bot.world, &random_state);
        }
        (*ticks_run)++;
        violations = core_validate(bot.world);
        if (violations != 0) {
            fprintf(stderr, u8"  시드 %d: 불변 조건 위반 %d개 (%d일)\n", (int)seed, (int)violations, (int)summary.day);
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
                fprintf(stderr, u8"  시드 %d: 저장/불러오기 왕복이 어긋났습니다 (%d일)\n", (int)seed, (int)summary.day);
                failures++;
            }
            if (roll % 13 == 0 && !poke_invalid_calls(bot.world)) {
                fprintf(stderr, u8"  시드 %d: 잘못된 호출이 상태를 바꾸었습니다\n", (int)seed);
                failures++;
            }
        }
    }
    walker_free(&walker);
    free(bot.buildings);
    core_destroy(bot.world);
    remove(path);
    remove(backup);
    if (restarts > 0 && failures == 0) {
        printf(u8"  시드 %d: 패배 %d번 (새 월드로 계속)\n", (int)seed, (int)restarts);
    }
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
        printf(u8"소크 테스트 통과: 시드 %d개 × %d일, 틱 %ld번, 불변 조건 위반 0\n", seeds, days, ticks_run);
        return 0;
    }
    printf(u8"소크 테스트 실패: 시드 %d개 중 %d개에서 문제\n", seeds, failed_seeds);
    return 1;
}

/* ---- 성능 측정 (--bench) ---------------------------------------------------- */

/* 지금 시각 (ms). C11 의 timespec_get 이 없는 C 라이브러리(오래된 MinGW)에서는 clock() 으로 (정밀도가 낮음) */
static double now_ms(void)
{
#if defined(TIME_UTC)
    struct timespec ts;
    if (timespec_get(&ts, TIME_UTC) != 0) {
        return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1000000.0;
    }
#endif
    return (double)clock() * 1000.0 / CLOCKS_PER_SEC;
}

int sim_run_bench(int villagers, int enemies, int32_t seed, const CoreDefs *defs)
{
    CoreWorld *world = NULL;
    Walker walker;
    CoreConfigView config;
    CoreSummaryView summary;
    CoreEventView event;
    double total = 0.0;
    double slowest = 0.0;
    int32_t measured = 0;
    int32_t tick;
    int32_t rc;
    (void)core_get_config(&config);
    if (create_world(seed, CORE_DIFF_NORMAL, "Bench", defs, &world) != CORE_OK) {
        fprintf(stderr, u8"월드를 만들 수 없습니다\n");
        return 1;
    }
    rc = core_debug_stress(world, villagers, enemies);
    if (rc != CORE_OK) {
        fprintf(stderr, u8"큰 마을을 만들 수 없습니다: %s\n", core_result_name(rc));
        core_destroy(world);
        return 1;
    }
    walker_init(&walker);
    /* 하루는 적응 (집·일터를 찾아 걷기 시작), 그다음 닷새를 잼. 걷기 흉내(C 그래픽 프런트엔드 몫)는 재지 않음 */
    for (tick = 0; tick < config.ticks_per_day * 6; tick++) {
        double start;
        double spent;
        if (tick >= config.ticks_per_day) {
            start = now_ms();
            rc = core_tick(world, 1);
            spent = now_ms() - start;
            total += spent;
            slowest = spent > slowest ? spent : slowest;
            measured++;
        } else {
            rc = core_tick(world, 1);
        }
        if (rc != CORE_OK || walker_step(&walker, world) != 0) {
            fprintf(stderr, u8"%d틱째에 멈췄습니다: %s\n", (int)tick, core_result_name(rc));
            break;
        }
        while (core_poll_event(world, &event) == 1) {
        }
    }
    (void)core_get_summary(world, &summary);
    printf(u8"주민 %d명 (요청 %d), 건물 %d채, 적 %d명: 틱 %d번, 평균 %.3f ms, 가장 느린 틱 %.3f ms (목표: 1 ms 미만)\n",
           (int)summary.population, villagers, (int)summary.building_count, enemies, (int)measured,
           measured > 0 ? total / measured : 0.0, slowest);
    walker_free(&walker);
    core_destroy(world);
    return 0;
}
