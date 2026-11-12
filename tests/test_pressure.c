/*
 * test_pressure.c — 마일스톤 3: 계절(겨울 농장, 난방), 자연 사건(예보, 가뭄, 폭풍, 역병), 수리, 치료소,
 *                   시장(차이, 가격 움직임), 떠돌이 상인, 장부, 저장
 */
#include <stdio.h>
#include <string.h>

#include "market.h"
#include "nature.h"
#include "placement.h"
#include "platform.h"
#include "test_town.h"
#include "test_util.h"
#include "villager_ai.h"

/* 시계를 day 일 minute 분으로 옮깁니다 (시험용) */
static void set_time(CoreWorld *world, int32_t day, int32_t minute)
{
    world->clock.tick = (day - 1) * CLOCK_TICKS_PER_DAY + minute / CLOCK_MINUTES_PER_TICK;
}

/* 쌓인 이벤트 중 type 이 몇 개인지 (모두 꺼냄) */
static int32_t count_events(CoreWorld *world, int32_t type)
{
    CoreEventView event;
    int32_t count = 0;
    while (core_poll_event(world, &event) == 1) {
        count += event.type == type;
    }
    return count;
}

/* 다음 06:00 결산 바로 뒤까지 진행합니다 */
static int run_to_next_daily(CoreWorld *world)
{
    do {
        if (!test_run(world, 1)) {
            return 0;
        }
    } while (clock_minute_of_day(&world->clock) != WORLD_DAILY_MIN);
    return 1;
}

static int32_t lived_in_houses(const CoreWorld *world)
{
    int32_t count = 0;
    int32_t i;
    for (i = 0; i < world->building_count; i++) {
        const Building *b = world->buildings[i];
        count += b->type == CORE_BLD_HOUSE && b->state == CORE_BSTATE_ACTIVE && b->resident_count > 0;
    }
    return count;
}

void test_winter_farms_stop_and_houses_burn_wood(void)
{
    CoreWorld *world = test_make_world(301, CORE_DIFF_NORMAL);
    Building *house;
    CoreSummaryView summary;
    int32_t terms[CORE_MOOD_TERM_COUNT];
    int32_t houses;
    int32_t wood;
    CHECK(world != NULL);
    test_give(world, 300);
    test_flatten(world, world->keep_x + 4, world->keep_y - 4, 8, 8);
    house = test_build_now(world, CORE_BLD_HOUSE, world->keep_x + 5, world->keep_y - 3);
    CHECK(house != NULL);
    CHECK(house->resident_count > 0);

    /* 계절별 농장 %: 겨울 0, 여름 120 (날씨가 맑을 때) */
    world->nature.weather = CORE_WEATHER_CLEAR;
    set_time(world, 25, 12 * 60);
    CHECK_EQ(nature_farm_percent(world), 0);
    CHECK_EQ(nature_heating_per_house(world), 2);
    set_time(world, 9, 12 * 60);
    CHECK_EQ(nature_farm_percent(world), 120);
    CHECK_EQ(nature_heating_per_house(world), 0);

    /* 가을: 겨울 준비량 = 주민 × 1.2 × 8일, 집마다 2 × 8일 */
    set_time(world, 17, 12 * 60);
    CHECK_EQ(core_get_summary(world, &summary), CORE_OK);
    CHECK_EQ(summary.winter_food_need, (world->villager_count * 12 * 8 + 9) / 10);
    CHECK_EQ(summary.winter_wood_need, lived_in_houses(world) * 2 * 8);
    set_time(world, 5, 12 * 60);
    CHECK_EQ(core_get_summary(world, &summary), CORE_OK);
    CHECK_EQ(summary.winter_food_need, 0);

    /* 겨울 아침: 사는 집마다 목재 2 */
    set_time(world, 25, WORLD_DAILY_MIN - CLOCK_MINUTES_PER_TICK);
    houses = lived_in_houses(world);
    CHECK(houses > 0);
    world->bank.amount[CORE_RES_WOOD] = 100;
    CHECK_EQ(core_tick(world, 1), CORE_OK);
    wood = 100 - 2 * houses;
    if (world->nature.event == CORE_NATURE_COLD_SNAP) {
        wood = 100 - 3 * houses; /* 혹한이 막 시작했으면 하나 더 (예보가 없으므로 일어나지 않아야 함) */
    }
    CHECK_EQ(world->bank.amount[CORE_RES_WOOD], wood);
    CHECK_EQ(world->ledger.today[CORE_RES_WOOD][CORE_LEDGER_HEATING], wood - 100);
    CHECK_EQ(house->flags & CORE_BF_COLD, 0);

    /* 목재가 없으면 춥고, 그 집 주민의 기분 항목에 추위가 들어감 */
    world->bank.amount[CORE_RES_WOOD] = 0;
    CHECK(run_to_next_daily(world));
    CHECK(house->flags & CORE_BF_COLD);
    CHECK_EQ(core_get_summary(world, &summary), CORE_OK);
    CHECK(summary.cold_houses >= 1);
    CHECK(house->resident_count > 0);
    (void)vil_mood_terms(world, house->residents[0], terms);
    CHECK_EQ(terms[CORE_MOOD_COLD], -15);
    CHECK(count_events(world, CORE_EVT_HOUSES_COLD) >= 1);
    CHECK_EQ(test_validate_verbose(world), 0);
    core_destroy(world);
}

void test_nature_forecast_then_drought(void)
{
    CoreWorld *world = test_make_world(302, CORE_DIFF_NORMAL);
    CoreSummaryView summary;
    CHECK(world != NULL);
    /* 여름 첫날 새벽에 가뭄을 예보해 둔 상태 */
    set_time(world, 9, WORLD_DAILY_MIN - CLOCK_MINUTES_PER_TICK);
    world->nature.forecast = CORE_NATURE_DROUGHT;
    world->nature.forecast_days = 3;
    (void)count_events(world, CORE_EVT_NONE);
    CHECK_EQ(core_tick(world, 1), CORE_OK);
    CHECK_EQ(world->nature.event, CORE_NATURE_DROUGHT);
    CHECK_EQ(world->nature.event_days, 3);
    CHECK_EQ(world->nature.forecast, CORE_NATURE_NONE);
    CHECK_EQ(world->nature.weather, CORE_WEATHER_CLEAR); /* 가뭄에는 비가 오지 않음 */
    CHECK_EQ(nature_farm_percent(world), 60);            /* 여름 120% × 가뭄 50% */
    CHECK_EQ(core_get_summary(world, &summary), CORE_OK);
    CHECK_EQ(summary.nature_event, CORE_NATURE_DROUGHT);
    CHECK_EQ(summary.farm_percent, 60);
    CHECK_EQ(count_events(world, CORE_EVT_NATURE_STARTED), 1);

    /* 사흘 뒤 끝나고, 바로 다음 사건이 예보되지는 않음 (쉬는 날) */
    CHECK(run_to_next_daily(world));
    CHECK(run_to_next_daily(world));
    CHECK_EQ(world->nature.event, CORE_NATURE_DROUGHT);
    CHECK(run_to_next_daily(world));
    CHECK_EQ(world->nature.event, CORE_NATURE_NONE);
    CHECK_EQ(world->nature.forecast, CORE_NATURE_NONE);
    CHECK_EQ(count_events(world, CORE_EVT_NATURE_ENDED), 1);

    /* 풍년이면 +50% */
    world->nature.event = CORE_NATURE_BUMPER;
    world->nature.event_days = 2;
    world->nature.weather = CORE_WEATHER_CLEAR;
    CHECK_EQ(nature_farm_percent(world), 180);
    world->nature.weather = CORE_WEATHER_RAIN;
    CHECK_EQ(nature_farm_percent(world), 198); /* 120 × 1.1 × 1.5 */
    CHECK_EQ(test_validate_verbose(world), 0);
    core_destroy(world);
}

void test_nature_events_follow_seasons(void)
{
    /* 여러 시드로 두 해를 돌리며: 예보는 언제나 하루 전, 겨울 사건은 겨울에만, 가뭄은 여름에만 */
    int32_t seed;
    int32_t started = 0;
    for (seed = 1; seed <= 6; seed++) {
        CoreWorld *world = test_make_world(seed, CORE_DIFF_HARSH);
        int32_t forecast = CORE_NATURE_NONE;
        int32_t day;
        CHECK(world != NULL);
        for (day = 0; day < 64; day++) {
            CoreEventView event;
            test_keep_alive(world); /* 이 시험은 사건 일정만 봄 (아무것도 짓지 않아도 64일을 버티게) */
            CHECK(run_to_next_daily(world));
            while (core_poll_event(world, &event) == 1) {
                int32_t season = clock_season(&world->clock);
                if (event.type == CORE_EVT_NATURE_STARTED) {
                    CHECK_EQ(event.a, forecast); /* 어제 예보한 사건 */
                    CHECK(event.a != CORE_NATURE_COLD_SNAP || season == CORE_SEASON_WINTER);
                    CHECK(event.a != CORE_NATURE_DROUGHT || season == CORE_SEASON_SUMMER);
                    CHECK(event.a != CORE_NATURE_STORM || season != CORE_SEASON_WINTER);
                    started++;
                } else if (event.type == CORE_EVT_NATURE_FORECAST) {
                    forecast = event.a;
                }
            }
            /* 진행 중인 사건도 그 계절에만 (계절이 바뀌면 끝남) */
            {
                int32_t season = clock_season(&world->clock);
                int32_t active = world->nature.event;
                CHECK(active != CORE_NATURE_DROUGHT || season == CORE_SEASON_SUMMER);
                CHECK(active != CORE_NATURE_COLD_SNAP || season == CORE_SEASON_WINTER);
                CHECK(active != CORE_NATURE_BUMPER || season == CORE_SEASON_SUMMER || season == CORE_SEASON_AUTUMN);
                CHECK(active != CORE_NATURE_PLAGUE || season != CORE_SEASON_WINTER);
            }
            CHECK_EQ(test_validate_verbose(world), 0);
        }
        core_destroy(world);
    }
    CHECK(started >= 6); /* 혹독 난이도 두 해면 사건이 여럿 있어야 함 */
}

void test_storm_damage_ruin_and_repair(void)
{
    CoreWorld *world = test_make_world(303, CORE_DIFF_NORMAL);
    Building *farm;
    Building *house;
    int32_t cost[CORE_RES_COUNT];
    int32_t reason = 0;
    int32_t i;
    CHECK(world != NULL);
    test_give(world, 500);
    test_flatten(world, world->keep_x + 4, world->keep_y - 4, 10, 10);
    farm = test_build_now(world, CORE_BLD_FARM, world->keep_x + 5, world->keep_y - 3);
    house = test_build_now(world, CORE_BLD_HOUSE, world->keep_x + 9, world->keep_y - 3);
    CHECK(farm != NULL && house != NULL);
    CHECK(farm->worker_count > 0 && house->resident_count > 0);

    /* 조금 부서짐: 수리 비용 = 건설 비용의 절반 × 잃은 비율 (올림) */
    placement_damage(world, farm, 30);
    CHECK_EQ(farm->state, CORE_BSTATE_ACTIVE);
    CHECK_EQ(farm->hp, 150 - 30);
    CHECK_EQ(core_get_repair_cost(world, farm->id, cost, CORE_RES_COUNT), CORE_OK);
    CHECK_EQ(cost[CORE_RES_GOLD], (15 * 30 + 299) / 300);
    CHECK_EQ(cost[CORE_RES_WOOD], (20 * 30 + 299) / 300);
    CHECK_EQ(count_events(world, CORE_EVT_BUILDING_DAMAGED), 1);

    /* 무너지면 일꾼과 거주민을 양쪽 모두 떼어 냄 */
    placement_damage(world, farm, 1000);
    placement_damage(world, house, 1000);
    CHECK_EQ(farm->state, CORE_BSTATE_RUINED);
    CHECK_EQ(farm->worker_count, 0);
    CHECK_EQ(house->resident_count, 0);
    for (i = 0; i < world->villager_count; i++) {
        CHECK(world->villagers[i]->workplace != farm);
        CHECK(world->villagers[i]->home != house);
    }
    CHECK_EQ(count_events(world, CORE_EVT_BUILDING_RUINED), 2);
    CHECK_EQ(test_validate_verbose(world), 0);

    /* 돈이 없으면 못 고침 → 돈이 있으면 고치고 다시 일꾼이 옴 */
    for (i = 0; i < CORE_RES_COUNT; i++) {
        world->bank.amount[i] = 0;
    }
    CHECK_EQ(core_repair(world, farm->id, &reason), CORE_ERR_REJECTED);
    CHECK_EQ(reason, CORE_DENY_NO_RESOURCES);
    test_give(world, 300);
    CHECK_EQ(core_get_repair_cost(world, farm->id, cost, CORE_RES_COUNT), CORE_OK);
    CHECK_EQ(cost[CORE_RES_WOOD], 10); /* 전부 무너짐: 건설 비용의 절반 */
    CHECK_EQ(core_repair(world, farm->id, &reason), CORE_OK);
    CHECK_EQ(farm->state, CORE_BSTATE_ACTIVE);
    CHECK_EQ(farm->hp, 150);
    CHECK(farm->worker_count > 0);
    CHECK_EQ(world->ledger.today[CORE_RES_WOOD][CORE_LEDGER_REPAIR], -10);
    CHECK_EQ(core_repair(world, farm->id, &reason), CORE_ERR_REJECTED);
    CHECK_EQ(reason, CORE_DENY_NOT_DAMAGED);
    CHECK_EQ(core_repair(world, house->id, &reason), CORE_OK);
    CHECK(house->resident_count > 0);
    CHECK_EQ(core_repair(world, 99999, &reason), CORE_ERR_NOT_FOUND);

    /* 폭풍 사건: 성채가 아닌 건물이 부서짐 */
    (void)count_events(world, CORE_EVT_NONE);
    set_time(world, 3, WORLD_DAILY_MIN - CLOCK_MINUTES_PER_TICK);
    world->nature.forecast = CORE_NATURE_STORM;
    world->nature.forecast_days = 1;
    CHECK_EQ(core_tick(world, 1), CORE_OK);
    CHECK_EQ(world->nature.weather, CORE_WEATHER_STORM);
    CHECK(farm->hp < 150 || house->hp < 200);
    CHECK_EQ(world_keep(world)->hp, world_building_def(world, CORE_BLD_KEEP)->max_hp);
    CHECK_EQ(test_validate_verbose(world), 0);
    core_destroy(world);
}

void test_sickness_and_healer(void)
{
    CoreWorld *world = test_make_world(304, CORE_DIFF_NORMAL);
    Building *healer;
    Villager *patient = NULL;
    CoreEventView event;
    int32_t herbs;
    int32_t health;
    int32_t ticks;
    int cured_by_healer = -1;
    int32_t i;
    CHECK(world != NULL);
    test_give(world, 300);
    test_flatten(world, world->keep_x + 4, world->keep_y - 4, 8, 8);
    healer = test_build_now(world, CORE_BLD_HEALER, world->keep_x + 5, world->keep_y - 3);
    CHECK(healer != NULL);
    CHECK_EQ(healer->worker_count, 1); /* 자동 배치 */
    for (i = 0; i < world->villager_count && patient == NULL; i++) {
        if (world->villagers[i]->workplace != healer) {
            patient = world->villagers[i];
        }
    }
    CHECK(patient != NULL);

    /* 근무 시간에 앓으면 일하지 않고 치료소로 가서, 치료사가 있으면 한 시간 뒤 약초 하나로 나음 */
    set_time(world, 2, 8 * 60);
    CHECK(test_run(world, 6)); /* 치료사가 출근하도록 */
    CHECK_EQ(nature_make_sick(world, patient), 1);
    CHECK_EQ(nature_make_sick(world, patient), 0); /* 이미 아픔 */
    CHECK(patient->flags & CORE_VF_SICK);
    herbs = world->bank.amount[CORE_RES_HERBS];
    for (ticks = 0; ticks < 30 && patient->sick_ticks > 0; ticks++) {
        CHECK(test_run(world, 1));
        CHECK(patient->state != CORE_VS_WORKING || patient->sick_ticks == 0);
    }
    CHECK_EQ(patient->sick_ticks, 0);
    CHECK_EQ(patient->flags & CORE_VF_SICK, 0);
    CHECK_EQ(world->bank.amount[CORE_RES_HERBS], herbs - 1);
    CHECK_EQ(world->ledger.today[CORE_RES_HERBS][CORE_LEDGER_HEALING], -1);
    while (core_poll_event(world, &event) == 1) {
        if (event.type == CORE_EVT_VILLAGER_CURED && event.a == patient->id) {
            cured_by_healer = event.b;
        }
    }
    CHECK_EQ(cured_by_healer, 1);

    /* 약초가 없으면 치료소가 열리지 않고, 시간이 지나면 저절로 나음 (건강은 조금 줄어듦) */
    world->bank.amount[CORE_RES_HERBS] = 0;
    CHECK(nature_open_healer(world, patient) == NULL);
    patient->health = 100;
    CHECK_EQ(nature_make_sick(world, patient), 1);
    health = patient->health;
    ticks = patient->sick_ticks;
    CHECK(test_run(world, ticks));
    CHECK_EQ(patient->sick_ticks, 0);
    CHECK(patient->health < health);
    CHECK_EQ(count_events(world, CORE_EVT_VILLAGER_CURED), 1);

    /* 역병 사건: 주민 몇 명이 앓기 시작함 */
    set_time(world, 7, WORLD_DAILY_MIN - CLOCK_MINUTES_PER_TICK);
    world->nature.forecast = CORE_NATURE_PLAGUE;
    world->nature.forecast_days = 4;
    CHECK_EQ(core_tick(world, 1), CORE_OK);
    CHECK_EQ(world->nature.event, CORE_NATURE_PLAGUE);
    CHECK(count_events(world, CORE_EVT_VILLAGER_SICK) >= 1);
    CHECK_EQ(test_validate_verbose(world), 0);
    core_destroy(world);
}

void test_market_trade_rules(void)
{
    CoreWorld *world = test_make_world(305, CORE_DIFF_NORMAL);
    Building *market;
    int32_t gold = 0;
    int32_t reason = 0;
    int32_t gold_start;
    int32_t price;
    int32_t paid;
    int32_t earned;
    CoreMarketView view;
    int32_t i;
    CHECK(world != NULL);
    test_give(world, 400);
    CHECK_EQ(core_trade(world, CORE_RES_FOOD, 1, &gold, &reason), CORE_ERR_REJECTED);
    CHECK_EQ(reason, CORE_DENY_NO_MARKET);
    test_flatten(world, world->keep_x + 4, world->keep_y - 4, 8, 8);
    market = test_build_now(world, CORE_BLD_MARKET, world->keep_x + 5, world->keep_y - 3);
    CHECK(market != NULL);
    CHECK_EQ(market->worker_count, 1);

    /* 일꾼이 없으면 닫힘 */
    {
        int32_t worker = market->workers[0]->id;
        CHECK_EQ(core_unassign_worker(world, worker), CORE_OK);
        CHECK_EQ(core_trade(world, CORE_RES_FOOD, 1, &gold, &reason), CORE_ERR_REJECTED);
        CHECK_EQ(reason, CORE_DENY_NO_TRADER);
        CHECK_EQ(core_get_market(world, &view), CORE_OK);
        CHECK_EQ(view.open, 0);
        CHECK_EQ(view.closed_reason, CORE_DENY_NO_TRADER);
        CHECK_EQ(core_assign_worker(world, worker, market->id), CORE_OK);
    }
    CHECK_EQ(core_get_market(world, &view), CORE_OK);
    CHECK_EQ(view.open, 1);
    CHECK(view.buy_price[CORE_RES_FOOD] > view.sell_price[CORE_RES_FOOD]);
    CHECK_EQ(view.buy_price[CORE_RES_MANA], 0); /* 마정석은 시장에서 다루지 않음 */

    /* 사면 값이 오르고, 바로 되팔면 손해 (차이) */
    world->bank.amount[CORE_RES_FOOD] = 100;
    gold_start = world->bank.amount[CORE_RES_GOLD];
    price = world->market.price[CORE_RES_FOOD];
    CHECK_EQ(core_trade(world, CORE_RES_FOOD, 2, &gold, &reason), CORE_OK);
    paid = -gold;
    CHECK(paid > 0);
    CHECK_EQ(world->bank.amount[CORE_RES_FOOD], 120);
    CHECK_EQ(world->bank.amount[CORE_RES_GOLD], gold_start - paid);
    CHECK(world->market.price[CORE_RES_FOOD] > price);
    CHECK_EQ(world->ledger.today[CORE_RES_FOOD][CORE_LEDGER_TRADE], 20);
    price = world->market.price[CORE_RES_FOOD];
    CHECK_EQ(core_trade(world, CORE_RES_FOOD, -2, &gold, &reason), CORE_OK);
    earned = gold;
    CHECK(earned < paid);
    CHECK(world->market.price[CORE_RES_FOOD] < price);
    CHECK_EQ(world->bank.amount[CORE_RES_FOOD], 100);

    /* 거절하는 경우들 (아무것도 바뀌지 않음) */
    CHECK_EQ(core_trade(world, CORE_RES_MANA, 1, &gold, &reason), CORE_ERR_REJECTED);
    CHECK_EQ(reason, CORE_DENY_NOT_TRADABLE);
    world->bank.amount[CORE_RES_STONE] = 5;
    CHECK_EQ(core_trade(world, CORE_RES_STONE, -1, &gold, &reason), CORE_ERR_REJECTED);
    CHECK_EQ(reason, CORE_DENY_NO_GOODS);
    world->bank.amount[CORE_RES_FOOD] = world->bank.capacity - 5;
    CHECK_EQ(core_trade(world, CORE_RES_FOOD, 1, &gold, &reason), CORE_ERR_REJECTED);
    CHECK_EQ(reason, CORE_DENY_STORAGE_FULL);
    world->bank.amount[CORE_RES_GOLD] = 0;
    CHECK_EQ(core_trade(world, CORE_RES_WOOD, 1, &gold, &reason), CORE_ERR_REJECTED);
    CHECK_EQ(reason, CORE_DENY_NO_GOLD);
    CHECK_EQ(core_trade(world, CORE_RES_WOOD, 11, &gold, &reason), CORE_ERR_INVALID_ARG);
    CHECK_EQ(core_trade(world, CORE_RES_WOOD, 0, &gold, &reason), CORE_ERR_INVALID_ARG);
    CHECK_EQ(core_trade(NULL, CORE_RES_WOOD, 1, &gold, &reason), CORE_ERR_NULL_ARG);

    /* 값이 한도까지 오른 뒤에는 날마다 계절 목표 쪽으로 돌아감 */
    world->market.price[CORE_RES_WOOD] = market_base_price(CORE_RES_WOOD) * MARKET_MAX_PCT;
    price = world->market.price[CORE_RES_WOOD];
    for (i = 0; i < 10; i++) {
        market_daily(world);
    }
    CHECK(world->market.price[CORE_RES_WOOD] < price * 60 / 100);
    CHECK_EQ(test_validate_verbose(world), 0);
    core_destroy(world);
}

void test_merchant_visits_and_deals(void)
{
    CoreWorld *world = test_make_world(306, CORE_DIFF_NORMAL);
    CoreMerchantView view;
    int32_t reason = 0;
    int32_t mana;
    int32_t gold;
    int32_t i;
    CHECK(world != NULL);
    CHECK_EQ(core_merchant_accept(world, 0, &reason), CORE_ERR_REJECTED);
    CHECK_EQ(reason, CORE_DENY_NO_MERCHANT);
    world->merchant.next_visit_day = clock_day(&world->clock);
    market_daily(world);
    CHECK_EQ(world->merchant.days_left, MERCHANT_STAY_DAYS);
    CHECK(world->merchant.deal_count >= 3);
    CHECK(world->merchant.next_visit_day >= clock_day(&world->clock) + MERCHANT_MIN_GAP);
    CHECK_EQ(count_events(world, CORE_EVT_MERCHANT_ARRIVED), 1);
    CHECK_EQ(core_get_merchant(world, &view), CORE_OK);
    CHECK_EQ(view.deal_count, world->merchant.deal_count);

    /* 첫 제안은 언제나 금화로 마정석 사기 */
    CHECK_EQ(view.deals[0].give_resource, CORE_RES_GOLD);
    CHECK_EQ(view.deals[0].get_resource, CORE_RES_MANA);
    world->bank.amount[CORE_RES_GOLD] = view.deals[0].give_amount - 1;
    CHECK_EQ(core_merchant_accept(world, 0, &reason), CORE_ERR_REJECTED);
    CHECK_EQ(reason, CORE_DENY_NO_GOLD);
    world->bank.amount[CORE_RES_GOLD] = 1000;
    mana = world->bank.amount[CORE_RES_MANA];
    gold = world->bank.amount[CORE_RES_GOLD];
    CHECK_EQ(core_merchant_accept(world, 0, &reason), CORE_OK);
    CHECK_EQ(world->bank.amount[CORE_RES_MANA], mana + view.deals[0].get_amount);
    CHECK_EQ(world->bank.amount[CORE_RES_GOLD], gold - view.deals[0].give_amount);
    CHECK_EQ(world->ledger.today[CORE_RES_MANA][CORE_LEDGER_TRADE], view.deals[0].get_amount);
    CHECK_EQ(core_merchant_accept(world, 0, &reason), CORE_ERR_REJECTED);
    CHECK_EQ(reason, CORE_DENY_DEAL_TAKEN);
    CHECK_EQ(core_merchant_accept(world, CORE_MERCHANT_MAX_DEALS, &reason), CORE_ERR_INVALID_ARG);

    /* 이틀 뒤 떠남 */
    market_daily(world);
    CHECK_EQ(world->merchant.days_left, 1);
    market_daily(world);
    CHECK_EQ(world->merchant.days_left, 0);
    CHECK_EQ(world->merchant.deal_count, 0);
    CHECK_EQ(count_events(world, CORE_EVT_MERCHANT_LEFT), 1);
    CHECK_EQ(core_merchant_accept(world, 1, &reason), CORE_ERR_REJECTED);
    CHECK_EQ(reason, CORE_DENY_NO_MERCHANT);
    for (i = 0; i < CORE_RES_COUNT; i++) {
        CHECK(world->bank.amount[i] >= 0);
    }
    CHECK_EQ(test_validate_verbose(world), 0);
    core_destroy(world);
}

void test_ledger_matches_bank(void)
{
    /* 며칠 동안의 장부(어제들 + 오늘)의 합이 실제 자원 변화와 정확히 같아야 함 */
    CoreWorld *world = test_make_world(307, CORE_DIFF_NORMAL);
    int64_t start[CORE_RES_COUNT];
    int64_t today_start[CORE_RES_COUNT];
    int64_t counted[CORE_RES_COUNT];
    int32_t gold;
    int32_t reason;
    int32_t r;
    int32_t c;
    int32_t t;
    CHECK(world != NULL);
    test_give(world, 280);
    test_flatten(world, world->keep_x + 4, world->keep_y - 4, 12, 10);
    /* 짓는 동안에는 장부가 넘어가는 것을 세지 않으므로, 다 지은 뒤부터 셉니다 */
    CHECK(test_build_now(world, CORE_BLD_FARM, world->keep_x + 5, world->keep_y - 3) != NULL);
    CHECK(test_build_now(world, CORE_BLD_MARKET, world->keep_x + 9, world->keep_y - 3) != NULL);
    CHECK(test_build_now(world, CORE_BLD_HOUSE, world->keep_x + 5, world->keep_y + 3) != NULL);
    for (r = 0; r < CORE_RES_COUNT; r++) {
        start[r] = world->bank.amount[r];
        today_start[r] = 0;
        counted[r] = 0;
        for (c = 0; c < CORE_LEDGER_COUNT; c++) {
            today_start[r] += world->ledger.today[r][c];
        }
    }
    (void)core_trade(world, CORE_RES_WOOD, 2, &gold, &reason);
    {
        int32_t id = 0;
        (void)core_place_building(world, CORE_BLD_HOUSE, world->keep_x + 9, world->keep_y + 3, &id, &reason);
    }
    for (t = 0; t < 6 * CLOCK_TICKS_PER_DAY; t++) {
        CHECK(test_run(world, 1));
        if (clock_minute_of_day(&world->clock) == WORLD_DAILY_MIN) {
            for (r = 0; r < CORE_RES_COUNT; r++) {
                for (c = 0; c < CORE_LEDGER_COUNT; c++) {
                    counted[r] += world->ledger.yesterday[r][c];
                }
            }
        }
        if (t == 300) {
            (void)core_trade(world, CORE_RES_FOOD, -1, &gold, &reason);
            placement_damage(world, world->buildings[1], 20);
            (void)core_repair(world, world->buildings[1]->id, &reason);
        }
    }
    for (r = 0; r < CORE_RES_COUNT; r++) {
        int64_t today_end = 0;
        for (c = 0; c < CORE_LEDGER_COUNT; c++) {
            today_end += world->ledger.today[r][c];
        }
        CHECK_EQ(world->bank.amount[r] - start[r], counted[r] + today_end - today_start[r]);
    }
    CHECK_EQ(test_validate_verbose(world), 0);
    core_destroy(world);
}

void test_save_roundtrip_world_pressure(void)
{
    const char *path = "pressure.sav";
    CoreWorld *world = test_make_world(308, CORE_DIFF_HARSH);
    CoreWorld *loaded = NULL;
    Building *house;
    uint64_t a = 0;
    uint64_t b = 1;
    int32_t reason;
    int32_t gold;
    CHECK(world != NULL);
    test_give(world, 500);
    test_flatten(world, world->keep_x + 4, world->keep_y - 4, 12, 10);
    house = test_build_now(world, CORE_BLD_HOUSE, world->keep_x + 5, world->keep_y - 3);
    CHECK(house != NULL);
    CHECK(test_build_now(world, CORE_BLD_MARKET, world->keep_x + 9, world->keep_y - 3) != NULL);
    /* 진행 중인 사건, 예보, 상인과 한 거래, 아픈 주민, 추운 집, 부서진 건물, 움직인 가격 */
    world->nature.forecast = CORE_NATURE_PLAGUE;
    world->nature.forecast_days = 5;
    world->merchant.next_visit_day = clock_day(&world->clock) + 1;
    CHECK(run_to_next_daily(world));
    CHECK_EQ(world->nature.event, CORE_NATURE_PLAGUE);
    CHECK(world->merchant.days_left > 0);
    world->bank.amount[CORE_RES_GOLD] = 1000;
    CHECK_EQ(core_merchant_accept(world, 0, &reason), CORE_OK);
    world->bank.amount[CORE_RES_STONE] = 100;
    CHECK_EQ(core_trade(world, CORE_RES_STONE, 3, &gold, &reason), CORE_OK);
    (void)nature_make_sick(world, world->villagers[0]);
    house->flags |= CORE_BF_COLD;
    placement_damage(world, house, 40);
    CHECK(test_run(world, 50));
    CHECK_EQ(core_tick(world, 3), CORE_OK); /* 걷는 도중 */
    CHECK_EQ(test_validate_verbose(world), 0);

    CHECK_EQ(core_save(world, path), CORE_OK);
    CHECK_EQ(core_load(path, NULL, &loaded, NULL), CORE_OK);
    (void)core_state_hash(world, &a);
    (void)core_state_hash(loaded, &b);
    CHECK(a == b);
    CHECK_EQ(loaded->nature.event, world->nature.event);
    CHECK_EQ(loaded->merchant.deal_count, world->merchant.deal_count);
    CHECK_EQ(loaded->merchant.deals[0].taken, 1);
    CHECK_EQ(loaded->market.price[CORE_RES_STONE], world->market.price[CORE_RES_STONE]);
    CHECK_EQ(loaded->villagers[0]->sick_ticks, world->villagers[0]->sick_ticks);
    /* 같은 보고를 하면 같은 미래 */
    CHECK(test_run(world, 400));
    CHECK(test_run(loaded, 400));
    (void)core_state_hash(world, &a);
    (void)core_state_hash(loaded, &b);
    CHECK(a == b);
    CHECK_EQ(test_validate_verbose(loaded), 0);
    core_destroy(world);
    core_destroy(loaded);
    (void)platform_remove_file(path);
    (void)platform_remove_file("pressure.sav.bak");
}
