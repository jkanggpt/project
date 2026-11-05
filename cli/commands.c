/*
 * commands.c — 대화형 메뉴 명령들 (설명은 commands.h)
 */
#include "commands.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "console.h"
#include "format.h"

#define CLI_MORNING_MINUTE (6 * 60)

/* 지도 글자: 성채 K, 주택 H, 농장 F, 채집 G, 벌목 L, 채석 Q, 광산 M, 창고 W, 시장 $, 병영 B, 탑 T, 성벽 #, 성문 +,
 * 치료소 &, 마탑 A, 도로 = */
static const char BUILDING_SYMBOLS[CORE_BLD_COUNT] = {'K', 'H', 'F', 'G', 'L', 'Q', 'M', 'W',
                                                      '$', 'B', 'T', '#', '+', '&', 'A', '='};

char cli_building_symbol(int32_t type)
{
    return (type >= 0 && type < CORE_BLD_COUNT) ? BUILDING_SYMBOLS[type] : '?';
}

void cli_print_events(CoreWorld *world)
{
    CoreEventView event;
    char text[200];
    int32_t produced[CORE_RES_COUNT];
    int32_t i;
    memset(produced, 0, sizeof(produced));
    while (core_poll_event(world, &event) == 1) {
        if (event.type == CORE_EVT_PRODUCED) {
            if (event.b >= 0 && event.b < CORE_RES_COUNT) {
                produced[event.b] += event.c; /* 생산은 너무 많아서 합계만 */
            }
            continue;
        }
        fmt_event(&event, text, sizeof(text));
        printf("  · %s\n", text);
    }
    for (i = 0; i < CORE_RES_COUNT; i++) {
        if (produced[i] > 0) {
            printf("  · 생산: %s +%d\n", fmt_resource_name(i), (int)produced[i]);
        }
    }
}

static void print_result(const char *action, int32_t rc)
{
    if (rc == CORE_OK) {
        printf("%s: %s\n", action, fmt_result_message(rc));
    } else {
        printf("%s 실패: %s (%s)\n", action, fmt_result_message(rc), core_result_name(rc));
    }
}

/* 기존 게임을 새 게임으로 바꿉니다 (기존 것은 해제) */
static void replace_world(CliContext *context, CoreWorld *world)
{
    core_destroy(context->world);
    context->world = world;
    walker_free(&context->walker);
    walker_init(&context->walker);
}

/* ticks 만큼 한 틱씩 진행하며 걷기를 흉내 냅니다 */
static int32_t advance(CliContext *context, int32_t ticks)
{
    int32_t i;
    for (i = 0; i < ticks; i++) {
        int32_t rc = walker_tick(&context->walker, context->world);
        if (rc != CORE_OK) {
            return rc;
        }
    }
    return CORE_OK;
}

/* ---- 명령 ------------------------------------------------------------ */

static int cmd_status(CliContext *context)
{
    CoreSummaryView summary;
    char date[96];
    int32_t i;
    int32_t rc = core_get_summary(context->world, &summary);
    if (rc != CORE_OK) {
        print_result("상태 읽기", rc);
        return 1;
    }
    fmt_date(&summary, date, sizeof(date));
    printf("\n=== %s ===\n", summary.castle_name);
    printf("날짜   : %s\n", date);
    printf("난이도 : %s   시드: %d\n", fmt_difficulty_name(summary.difficulty), (int)summary.seed);
    printf("주민   : %d명 (집 없음 %d, 일하는 사람 %d / 일자리 %d, 집 자리 %d), 평균 기분 %d\n",
           (int)summary.population, (int)summary.homeless, (int)summary.employed, (int)summary.job_slots,
           (int)summary.housing_capacity, (int)summary.average_mood);
    printf("자원   :");
    for (i = 0; i < CORE_RES_COUNT; i++) {
        printf(" %s %d", fmt_resource_name(i), (int)summary.resources[i]);
        if (summary.resource_capacity[i] > 0) {
            printf("/%d", (int)summary.resource_capacity[i]);
        }
        if (summary.resource_delta[i] != 0) {
            printf("(%+d)", (int)summary.resource_delta[i]);
        }
        printf(i + 1 < CORE_RES_COUNT ? "," : "\n");
    }
    printf("날씨   : %s, 농장 %d%%", fmt_weather_name(summary.weather), (int)summary.farm_percent);
    if (summary.nature_event != CORE_NATURE_NONE) {
        printf(", %s (%d일 남음)", fmt_nature_name(summary.nature_event), (int)summary.nature_days_left);
    }
    if (summary.forecast_event != CORE_NATURE_NONE) {
        printf(", 내일 %s 예보", fmt_nature_name(summary.forecast_event));
    }
    printf("\n");
    if (summary.winter_food_need > 0 || summary.winter_wood_need > 0) {
        printf("겨울   : 봄까지 식량 %d / %d, 목재 %d / %d (집 한 채 하룻밤 난방 %d)\n",
               (int)summary.resources[CORE_RES_FOOD], (int)summary.winter_food_need,
               (int)summary.resources[CORE_RES_WOOD], (int)summary.winter_wood_need, (int)summary.heating_per_house);
    }
    if (summary.sick_count > 0 || summary.cold_houses > 0) {
        printf("걱정   : 아픈 주민 %d명, 추운 집 %d채\n", (int)summary.sick_count, (int)summary.cold_houses);
    }
    if (summary.merchant_days_left > 0) {
        printf("상인   : 떠돌이 상인이 %d일 더 머뭅니다 (메뉴 17)\n", (int)summary.merchant_days_left);
    }
    return 1;
}

static int cmd_map(CliContext *context)
{
    static const char TERRAIN_SYMBOLS[CORE_TERRAIN_COUNT] = {'.', 'T', '^', '~', '*'};
    CoreSummaryView summary;
    CoreBuildingView *buildings = NULL;
    uint8_t *terrain;
    uint8_t *roads;
    char *canvas;
    int32_t count = 0;
    int32_t cells;
    int32_t x;
    int32_t y;
    int32_t i;
    if (core_get_summary(context->world, &summary) != CORE_OK) {
        return 1;
    }
    cells = summary.map_width * summary.map_height;
    /* 지도 크기와 건물 수는 실행 중에 정해지므로 동적으로 할당합니다 */
    terrain = (uint8_t *)malloc((size_t)cells);
    roads = (uint8_t *)malloc((size_t)cells);
    canvas = (char *)malloc((size_t)cells);
    buildings = (CoreBuildingView *)malloc((size_t)(summary.building_count_hint + 1) * sizeof(CoreBuildingView));
    if (terrain == NULL || roads == NULL || canvas == NULL || buildings == NULL ||
        core_copy_terrain(context->world, terrain, cells) != CORE_OK ||
        core_copy_roads(context->world, roads, cells) != CORE_OK ||
        core_copy_buildings(context->world, buildings, summary.building_count_hint + 1, &count) != CORE_OK) {
        printf("지도를 읽을 수 없습니다.\n");
        free(terrain);
        free(roads);
        free(canvas);
        free(buildings);
        return 1;
    }
    for (i = 0; i < cells; i++) {
        canvas[i] = roads[i] ? '=' : (terrain[i] < CORE_TERRAIN_COUNT ? TERRAIN_SYMBOLS[terrain[i]] : '?');
    }
    for (i = 0; i < count; i++) {
        const CoreBuildingView *b = &buildings[i];
        for (y = b->y; y < b->y + b->h; y++) {
            for (x = b->x; x < b->x + b->w; x++) {
                char symbol = cli_building_symbol(b->type);
                /* 짓는 중이면 소문자 */
                canvas[y * summary.map_width + x] =
                    (b->state == CORE_BSTATE_CONSTRUCTING && symbol >= 'A' && symbol <= 'Z') ? (char)(symbol + 32)
                                                                                              : symbol;
            }
        }
    }
    for (y = 0; y < summary.map_height; y++) {
        fwrite(&canvas[y * summary.map_width], 1, (size_t)summary.map_width, stdout);
        putchar('\n');
    }
    printf("지형: . %s  T %s  ^ %s  ~ %s  * %s   도로 =   (소문자 = 짓는 중)\n", fmt_terrain_name(0),
           fmt_terrain_name(1), fmt_terrain_name(2), fmt_terrain_name(3), fmt_terrain_name(4));
    printf("건물:");
    for (i = 0; i < CORE_BLD_COUNT - 1; i++) {
        printf(" %c %s", cli_building_symbol(i), fmt_building_name(i));
    }
    printf("\n");
    free(terrain);
    free(roads);
    free(canvas);
    free(buildings);
    return 1;
}

static int cmd_advance_ticks(CliContext *context)
{
    CoreConfigView config;
    int ticks = 0;
    int32_t rc;
    (void)core_get_config(&config);
    printf("한 틱은 게임 속 %d분입니다.\n", (int)config.minutes_per_tick);
    if (!read_int("몇 틱 진행할까요?", 1, config.max_ticks_per_call, &ticks)) {
        return 0;
    }
    rc = advance(context, ticks);
    if (rc != CORE_OK) {
        print_result("시간 진행", rc);
    }
    cli_print_events(context->world);
    return cmd_status(context);
}

static int cmd_next_morning(CliContext *context)
{
    CoreSummaryView summary;
    CoreConfigView config;
    int32_t minutes;
    int32_t rc;
    (void)core_get_config(&config);
    rc = core_get_summary(context->world, &summary);
    if (rc != CORE_OK) {
        print_result("상태 읽기", rc);
        return 1;
    }
    /* 다음 날 06:00 까지 남은 분 */
    minutes = 24 * 60 - summary.minute_of_day + CLI_MORNING_MINUTE;
    if (summary.minute_of_day < CLI_MORNING_MINUTE) {
        minutes = CLI_MORNING_MINUTE - summary.minute_of_day;
    }
    rc = advance(context, minutes / config.minutes_per_tick);
    if (rc != CORE_OK) {
        print_result("시간 진행", rc);
    }
    cli_print_events(context->world);
    return cmd_status(context);
}

static void print_cost(const int32_t cost[CORE_RES_COUNT])
{
    int32_t i;
    int printed = 0;
    for (i = 0; i < CORE_RES_COUNT; i++) {
        if (cost[i] > 0) {
            printf("%s%s %d", printed ? ", " : "", fmt_resource_name(i), (int)cost[i]);
            printed = 1;
        }
    }
    if (!printed) {
        printf("무료");
    }
}

static int cmd_build(CliContext *context)
{
    CoreBuildingDefView def;
    CorePlacementView preview;
    int type = 0;
    int x = 0;
    int y = 0;
    int32_t id = 0;
    int32_t reason = 0;
    int32_t rc;
    int32_t i;

    printf("지을 수 있는 건물:\n");
    for (i = 1; i < CORE_BLD_COUNT; i++) {
        if (core_get_building_def(context->world, i, &def) != CORE_OK) {
            continue;
        }
        printf("%2d) %-10s %dx%d  비용: ", (int)i, fmt_building_name(i), (int)def.w, (int)def.h);
        print_cost(def.cost);
        printf(def.available ? "\n" : "  (등급 부족)\n");
    }
    if (!read_int("종류 번호", 1, CORE_BLD_COUNT - 1, &type) || !read_int("왼쪽 위 x", 0, 255, &x) ||
        !read_int("왼쪽 위 y", 0, 255, &y)) {
        return 0;
    }
    (void)core_can_place(context->world, type, x, y, &preview);
    if (preview.terrain_count >= 0) {
        printf("작업 반경 안 지형 %d칸 → 생산성 %d%%\n", (int)preview.terrain_count, (int)preview.productivity);
    }
    rc = core_place_building(context->world, type, x, y, &id, &reason);
    if (rc == CORE_OK) {
        printf("%s%s 짓기 시작했습니다 (번호 %d).\n", fmt_building_name(type), fmt_josa(fmt_building_name(type), "을", "를"),
               (int)id);
    } else if (rc == CORE_ERR_REJECTED) {
        printf("지을 수 없습니다: %s\n", fmt_place_result(reason));
    } else {
        print_result("건설", rc);
    }
    cli_print_events(context->world);
    return 1;
}

static int cmd_build_line(CliContext *context)
{
    int kind = 0;
    int x0 = 0;
    int y0 = 0;
    int x1 = 0;
    int y1 = 0;
    int32_t placed = 0;
    int32_t reason = 0;
    int32_t type;
    printf("1) 도로  2) 성벽\n");
    if (!read_int("종류", 1, 2, &kind) || !read_int("시작 x", 0, 255, &x0) || !read_int("시작 y", 0, 255, &y0) ||
        !read_int("끝 x", 0, 255, &x1) || !read_int("끝 y", 0, 255, &y1)) {
        return 0;
    }
    type = kind == 1 ? CORE_BLD_ROAD : CORE_BLD_WALL;
    (void)core_place_line(context->world, type, x0, y0, x1, y1, &placed, &reason);
    printf("%s %d칸을 놓았습니다.", fmt_building_name(type), (int)placed);
    if (reason != CORE_PLACE_OK) {
        printf(" (놓지 못한 칸: %s)", fmt_place_result(reason));
    }
    printf("\n");
    return 1;
}

static int cmd_demolish(CliContext *context)
{
    int id = 0;
    int32_t rc;
    if (!read_int("철거할 건물 번호 (0 = 도로 한 칸)", 0, 2147483647, &id)) {
        return 0;
    }
    if (id == 0) {
        int x = 0;
        int y = 0;
        if (!read_int("도로 x", 0, 255, &x) || !read_int("도로 y", 0, 255, &y)) {
            return 0;
        }
        rc = core_remove_road(context->world, x, y);
    } else {
        rc = core_demolish(context->world, id);
    }
    print_result("철거", rc);
    cli_print_events(context->world);
    return 1;
}

/* 주민 뷰 전체를 새로 할당한 배열로 (호출한 쪽이 free) */
static CoreVillagerView *copy_villagers(CoreWorld *world, int32_t *out_count)
{
    CoreSummaryView summary;
    CoreVillagerView *views;
    (void)core_get_summary(world, &summary);
    views = (CoreVillagerView *)malloc((size_t)(summary.villager_count_hint + 1) * sizeof(CoreVillagerView));
    if (views == NULL || core_copy_villagers(world, views, summary.villager_count_hint + 1, out_count) != CORE_OK) {
        free(views);
        return NULL;
    }
    return views;
}

static int cmd_villagers(CliContext *context)
{
    int32_t count = 0;
    int32_t i;
    CoreVillagerView *views = copy_villagers(context->world, &count);
    if (views == NULL) {
        printf("주민 목록을 읽을 수 없습니다.\n");
        return 1;
    }
    printf("번호  이름          나이 상태           일터          기분 배고픔 피로 집\n");
    for (i = 0; i < count; i++) {
        const CoreVillagerView *v = &views[i];
        printf("%4d  %-12s  %3d  %-14s %-12s  %3d  %3d  %3d  %s\n", (int)v->id, v->name, (int)v->age,
               fmt_villager_state(v->state), v->job >= 0 ? fmt_building_name(v->job) : "-", (int)v->mood,
               (int)v->hunger, (int)v->rest, v->home_id != 0 ? "있음" : "없음");
    }
    free(views);
    return 1;
}

static int cmd_buildings(CliContext *context)
{
    static const char *const STATES[3] = {"짓는 중", "운영 중", "무너짐"};
    CoreSummaryView summary;
    CoreBuildingView *views;
    int32_t count = 0;
    int32_t i;
    (void)core_get_summary(context->world, &summary);
    views = (CoreBuildingView *)malloc((size_t)(summary.building_count_hint + 1) * sizeof(CoreBuildingView));
    if (views == NULL ||
        core_copy_buildings(context->world, views, summary.building_count_hint + 1, &count) != CORE_OK) {
        free(views);
        printf("건물 목록을 읽을 수 없습니다.\n");
        return 1;
    }
    printf("번호  종류          위치       상태     일꾼   거주   생산성\n");
    for (i = 0; i < count; i++) {
        const CoreBuildingView *b = &views[i];
        if (b->type == CORE_BLD_WALL) {
            continue; /* 성벽은 너무 많아서 생략 */
        }
        printf("%4d  %-12s (%2d,%2d)   %-8s %d/%d    %d/%d    %d%%", (int)b->id, fmt_building_name(b->type),
               (int)b->x, (int)b->y, STATES[b->state % 3], (int)b->workers, (int)b->worker_slots, (int)b->residents,
               (int)b->resident_slots, (int)b->productivity);
        if (b->state == CORE_BSTATE_CONSTRUCTING && b->build_total > 0) {
            printf("  (%d%%)", (int)(b->build_progress * 100 / b->build_total));
        }
        printf("\n");
    }
    free(views);
    return 1;
}

static int cmd_assign(CliContext *context)
{
    int villager = 0;
    int building = 0;
    if (!read_int("주민 번호", 1, 2147483647, &villager) || !read_int("건물 번호", 1, 2147483647, &building)) {
        return 0;
    }
    print_result("일꾼 배치", core_assign_worker(context->world, villager, building));
    cli_print_events(context->world);
    return 1;
}

static int cmd_unassign(CliContext *context)
{
    int villager = 0;
    if (!read_int("주민 번호", 1, 2147483647, &villager)) {
        return 0;
    }
    print_result("일꾼 빼기", core_unassign_worker(context->world, villager));
    return 1;
}

static int cmd_ledger(CliContext *context)
{
    CoreLedgerView ledger;
    int32_t r;
    int32_t c;
    if (core_get_ledger(context->world, &ledger) != CORE_OK) {
        printf("장부를 읽을 수 없습니다.\n");
        return 1;
    }
    printf("어제 하루 장부 (0 인 항목은 생략)\n");
    for (r = 0; r < CORE_RES_COUNT; r++) {
        int32_t net = 0;
        printf("  %-6s:", fmt_resource_name(r));
        for (c = 0; c < CORE_LEDGER_COUNT; c++) {
            if (ledger.yesterday[r][c] != 0) {
                printf(" %s %+d", fmt_ledger_name(c), (int)ledger.yesterday[r][c]);
                net += ledger.yesterday[r][c];
            }
        }
        printf("  = %+d\n", (int)net);
    }
    return 1;
}

static int cmd_market(CliContext *context)
{
    CoreMarketView market;
    int resource = 0;
    int lots = 0;
    int32_t gold = 0;
    int32_t reason = 0;
    int32_t r;
    int32_t rc;
    (void)core_get_market(context->world, &market);
    printf("시장 가격 (한 묶음 = %d개)%s\n", CORE_MARKET_LOT, market.open ? "" : " — 지금은 거래할 수 없음");
    if (!market.open) {
        printf("  이유: %s\n", fmt_deny_reason(market.closed_reason));
    }
    for (r = 0; r < CORE_RES_COUNT; r++) {
        if (market.base_price[r] > 0) {
            printf("  %d) %-6s 살 때 %3d  팔 때 %3d  (기준 %d, 지금 %d%%)\n", (int)r, fmt_resource_name(r),
                   (int)market.buy_price[r], (int)market.sell_price[r], (int)market.base_price[r],
                   (int)market.trend[r]);
        }
    }
    if (!market.open) {
        return 1;
    }
    if (!read_int("자원 번호", 0, CORE_RES_COUNT - 1, &resource) ||
        !read_int("묶음 수 (사면 +, 팔면 -)", -CORE_MARKET_MAX_LOTS, CORE_MARKET_MAX_LOTS, &lots)) {
        return 0;
    }
    if (lots == 0) {
        return 1;
    }
    rc = core_trade(context->world, resource, lots, &gold, &reason);
    if (rc == CORE_OK) {
        printf("거래했습니다: 금화 %+d\n", (int)gold);
    } else if (rc == CORE_ERR_REJECTED) {
        printf("거래할 수 없습니다: %s\n", fmt_deny_reason(reason));
    } else {
        print_result("거래", rc);
    }
    return 1;
}

static int cmd_merchant(CliContext *context)
{
    CoreMerchantView merchant;
    int choice = 0;
    int32_t reason = 0;
    int32_t i;
    int32_t rc;
    (void)core_get_merchant(context->world, &merchant);
    if (merchant.days_left <= 0) {
        printf("지금은 떠돌이 상인이 없습니다.\n");
        return 1;
    }
    printf("떠돌이 상인 (%d일 더 머묾)\n", (int)merchant.days_left);
    for (i = 0; i < merchant.deal_count; i++) {
        const CoreDealView *deal = &merchant.deals[i];
        printf("  %d) %s %d 내고 → %s %d 받기%s\n", (int)(i + 1), fmt_resource_name(deal->give_resource),
               (int)deal->give_amount, fmt_resource_name(deal->get_resource), (int)deal->get_amount,
               deal->taken ? " (함)" : "");
    }
    if (!read_int("받아들일 제안 (0 = 그만)", 0, merchant.deal_count, &choice)) {
        return 0;
    }
    if (choice == 0) {
        return 1;
    }
    rc = core_merchant_accept(context->world, choice - 1, &reason);
    if (rc == CORE_OK) {
        printf("거래했습니다.\n");
    } else if (rc == CORE_ERR_REJECTED) {
        printf("거래할 수 없습니다: %s\n", fmt_deny_reason(reason));
    } else {
        print_result("거래", rc);
    }
    return 1;
}

static int cmd_repair(CliContext *context)
{
    int id = 0;
    int32_t cost[CORE_RES_COUNT];
    int32_t reason = 0;
    int32_t rc;
    int32_t i;
    if (!read_int("고칠 건물 번호", 1, 2147483647, &id)) {
        return 0;
    }
    if (core_get_repair_cost(context->world, id, cost, CORE_RES_COUNT) == CORE_OK) {
        printf("수리 비용:");
        for (i = 0; i < CORE_RES_COUNT; i++) {
            if (cost[i] > 0) {
                printf(" %s %d", fmt_resource_name(i), (int)cost[i]);
            }
        }
        printf("\n");
    }
    rc = core_repair(context->world, id, &reason);
    if (rc == CORE_ERR_REJECTED) {
        printf("고칠 수 없습니다: %s\n", fmt_deny_reason(reason));
    } else {
        print_result("수리", rc);
    }
    cli_print_events(context->world);
    return 1;
}

static int cmd_save(CliContext *context)
{
    char path[CLI_PATH_BYTES];
    int32_t rc;
    if (!read_text("저장할 파일 이름", context->save_path, path, sizeof(path))) {
        return 0;
    }
    rc = core_save(context->world, path);
    print_result("저장", rc);
    if (rc == CORE_OK) {
        strcpy(context->save_path, path);
    }
    return 1;
}

int cli_load_game(CliContext *context, const char *path)
{
    CoreWorld *loaded = NULL;
    int32_t line = 0;
    int32_t rc = core_load(path, context->defs, &loaded, &line);
    if (rc != CORE_OK) {
        print_result("불러오기", rc);
        if (line > 0) {
            printf("  문제가 있는 줄: %d\n", (int)line);
        }
        return 0;
    }
    replace_world(context, loaded);
    if (strlen(path) < sizeof(context->save_path)) {
        strcpy(context->save_path, path);
    }
    printf("'%s' 에서 불러왔습니다.\n", path);
    return 1;
}

static int cmd_load(CliContext *context)
{
    char path[CLI_PATH_BYTES];
    if (!read_text("불러올 파일 이름", context->save_path, path, sizeof(path))) {
        return 0;
    }
    if (cli_load_game(context, path)) {
        return cmd_status(context);
    }
    return 1;
}

static int cmd_new_game(CliContext *context)
{
    CoreNewGameParams params;
    CoreWorld *world = NULL;
    int seed = 0;
    int difficulty = 0;
    int32_t rc;

    memset(&params, 0, sizeof(params));
    for (;;) {
        if (!read_text("성채 이름", "바람언덕", params.castle_name, sizeof(params.castle_name))) {
            return 0;
        }
        if (!read_int("지도 시드", 0, 2147483647, &seed)) {
            return 0;
        }
        printf("난이도: 0) 평화  1) 보통  2) 혹독\n");
        if (!read_int("난이도", 0, CORE_DIFF_COUNT - 1, &difficulty)) {
            return 0;
        }
        params.seed = seed;
        params.difficulty = difficulty;
        rc = core_create(&params, context->defs, &world);
        if (rc == CORE_OK) {
            break;
        }
        print_result("새 게임", rc);
        if (rc != CORE_ERR_INVALID_ARG) {
            return 1; /* 메모리 부족 등: 메뉴로 돌아감 */
        }
        printf("이름에 쓸 수 없는 글자가 있습니다. 다시 입력하세요.\n");
    }
    replace_world(context, world);
    printf("새 게임을 시작했습니다.\n");
    cli_print_events(context->world);
    return cmd_status(context);
}

static int cmd_quit(CliContext *context)
{
    context->running = 0;
    return 1;
}

/* 메뉴 명령 표. 0번은 언제나 종료입니다 */
static const CliCommand COMMANDS[] = {
    {"종료", cmd_quit, 0},
    {"상태 보기", cmd_status, 1},
    {"지도 보기", cmd_map, 1},
    {"시간 진행 (틱 단위)", cmd_advance_ticks, 1},
    {"다음 날 아침까지 진행", cmd_next_morning, 1},
    {"건물 짓기", cmd_build, 1},
    {"도로·성벽 줄 놓기", cmd_build_line, 1},
    {"철거하기", cmd_demolish, 1},
    {"주민 목록", cmd_villagers, 1},
    {"건물 목록", cmd_buildings, 1},
    {"일꾼 배치", cmd_assign, 1},
    {"일꾼 빼기", cmd_unassign, 1},
    {"저장하기", cmd_save, 1},
    {"불러오기", cmd_load, 0},
    {"새 게임", cmd_new_game, 0},
    {"장부 보기 (어제 하루)", cmd_ledger, 1},
    {"시장 거래", cmd_market, 1},
    {"떠돌이 상인과 거래", cmd_merchant, 1},
    {"건물 수리", cmd_repair, 1},
};
#define COMMAND_COUNT ((int)(sizeof(COMMANDS) / sizeof(COMMANDS[0])))

static void print_menu(const CliContext *context)
{
    int i;
    printf("\n--- 메뉴 ---\n");
    for (i = 1; i < COMMAND_COUNT; i++) {
        if (COMMANDS[i].needs_world && context->world == NULL) {
            continue;
        }
        printf("%d) %s\n", i, COMMANDS[i].label);
    }
    printf("0) %s\n", COMMANDS[0].label);
}

void cli_run_menu(CliContext *context)
{
    context->running = 1;
    while (context->running) {
        int choice = 0;
        const CliCommand *command;
        print_menu(context);
        if (!read_int("번호를 고르세요", 0, COMMAND_COUNT - 1, &choice)) {
            break; /* 입력이 끝남 */
        }
        command = &COMMANDS[choice];
        if (command->needs_world && context->world == NULL) {
            printf("먼저 새 게임을 시작하거나 불러오세요.\n");
            continue;
        }
        if (!command->run(context)) {
            break;
        }
    }
}
