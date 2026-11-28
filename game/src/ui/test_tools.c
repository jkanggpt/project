/* test_tools.c — 보통 세계를 생성하고 공개 검사 API로 자원·시간·진행을 조정합니다. */
#include "app.h"
#include "castle_expansion.h"
#include "format.h"
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static Rectangle box(float x, float y, float w, float h) { return (Rectangle){x, y, w, h}; }

static int integer(const char *text, int low, int high, int *out) {
    char *end;
    errno = 0;
    long n = strtol(text, &end, 10);
    if (!text[0] || *end || errno || n < low || n > high)
        return 0;
    *out = (int)n;
    return 1;
}

static void result(GameApp *a, int rc) {
    (void)snprintf(a->test_tools.result, sizeof(a->test_tools.result), "%s",
                   rc == CORE_OK ? u8"적용했습니다." : fmt_result_message(rc));
    if (rc == CORE_OK) {
        int refresh = game_session_refresh(&a->session);
        if (refresh != CORE_OK)
            game_app_result(a, refresh, 0, 0);
        a->shown_outcome = a->session.summary.outcome;
    }
}

static void sync_fields(GameApp *a) {
    GameTestTools *t = &a->test_tools;
    const CoreSummaryView *s = &a->session.summary;
    for (int r = 0; r < CORE_RES_COUNT; r++)
        (void)snprintf(t->resources[r], sizeof(t->resources[r]), "%d", s->resources[r]);
    (void)snprintf(t->day, sizeof(t->day), "%d", s->day);
    (void)snprintf(t->hour, sizeof(t->hour), "%02d", s->minute_of_day / 60);
    (void)snprintf(t->minute, sizeof(t->minute), "%02d", s->minute_of_day % 60);
    (void)snprintf(t->population, sizeof(t->population), "%d", a->session.villager_count);
}

void game_test_tools_enter(GameApp *a, int active) {
    memset(&a->test_tools, 0, sizeof(a->test_tools));
    GameTestTools *t = &a->test_tools;
    t->active = active;
    t->multiplier = 1;
    t->weather = -1;
    t->raid_type = CORE_THREAT_MONSTERS;
    strcpy(t->enemies, "12");
    if (active) {
        t->flags = CORE_TEST_ENABLED | CORE_TEST_FREE_BUILD | CORE_TEST_INSTANT_BUILD |
                   CORE_TEST_UNLOCK_BUILD | CORE_TEST_REVEAL | CORE_TEST_NO_RAIDS | CORE_TEST_SUSTAIN;
        result(a, core_test_configure(a->session.world, t->flags, t->weather));
        sync_fields(a);
    }
}

void game_test_tools_open(GameApp *a) {
    if (a->test_tools.active)
        sync_fields(a);
    game_app_open(a, PANEL_TEST_TOOLS);
}

static int button(GameApp *a, float x, float y, float w, const char *text) {
    return ui_button(&a->ui, box(x, y, w, 32), text, 1, 0, NULL);
}

static void toggle(GameApp *a, float x, float y, float width, int flag, const char *label) {
    GameTestTools *t = &a->test_tools;
    char text[160];
    (void)snprintf(text, sizeof(text), "%s  %s", (t->flags & flag) ? u8"[켜짐]" : u8"[꺼짐]", label);
    if (ui_button(&a->ui, box(x, y, width, 34), text, 1, !!(t->flags & flag), NULL)) {
        t->flags ^= flag;
        result(a, core_test_configure(a->session.world, t->flags, t->weather));
    }
}

static int resource_limit(const CoreSummaryView *s, int r) {
    return s->resource_capacity[r] > 0 ? s->resource_capacity[r] : 1000000000;
}

static void resources(GameApp *a, Rectangle b) {
    GameTestTools *t = &a->test_tools;
    float col = (b.width - 16) / 2;
    for (int r = 0; r < CORE_RES_COUNT; r++) {
        float x = b.x + (r / 8) * (col + 16), y = b.y + (r % 8) * 38;
        ui_small(&a->ui, x, y + 7, UI_TEXT, "%s", fmt_resource_name(r));
        ui_textbox(&a->ui, box(x + 85, y, col - 145, 30), 100 + r, t->resources[r], sizeof(t->resources[r]),
                   1);
        if (button(a, x + col - 54, y, 54, u8"적용")) {
            int value;
            if (!integer(t->resources[r], 0, resource_limit(&a->session.summary, r), &value))
                (void)snprintf(t->result, sizeof(t->result), u8"%s 범위: 0~%d", fmt_resource_name(r),
                               resource_limit(&a->session.summary, r));
            else
                result(a, core_test_resource(a->session.world, r, value));
        }
    }
    if (button(a, b.x + col + 16, b.y + 7 * 38, col, u8"자원 채우기 · 화폐 10만")) {
        int rc = CORE_OK;
        for (int r = 0; r < CORE_RES_COUNT && rc == CORE_OK; r++) {
            int limit = resource_limit(&a->session.summary, r);
            rc = core_test_resource(a->session.world, r, limit > 100000 ? 100000 : limit);
        }
        result(a, rc);
        sync_fields(a);
    }
    toggle(a, b.x, b.y + 310, b.width, CORE_TEST_RESOURCES, u8"자원을 계속 보충하기 (화폐 10만까지)");
}

static void set_clock(GameApp *a, int day, int minute) {
    int rc = core_test_clock(a->session.world, day, minute);
    if (rc == CORE_OK) {
        a->session.tick_minutes = 0;
        a->accumulator = 0;
        a->autosave_day = day;
        rc = game_session_checkpoint(&a->session, 0);
    }
    result(a, rc);
    if (rc == CORE_OK)
        sync_fields(a);
}

static void time_weather(GameApp *a, Rectangle b) {
    GameTestTools *t = &a->test_tools;
    float x = b.x, y = b.y, gap = 8, cell = (b.width - 5 * gap) / 6;
    static const int speeds[] = {0, 1, 3, 12, 30, 60};
    ui_small(&a->ui, x, y, UI_GOLD, u8"시간 배속 · 창을 닫으면 선택한 속도로 진행");
    for (int i = 0; i < 6; i++) {
        char label[32];
        (void)snprintf(label, sizeof(label), i ? u8"%d배" : u8"정지", speeds[i]);
        if (ui_button(&a->ui, box(x + i * (cell + gap), y + 26, cell, 32), label, 1,
                      speeds[i] == a->saved_speed * t->multiplier, NULL)) {
            t->multiplier = speeds[i] ? speeds[i] : 1;
            a->saved_speed = speeds[i] ? 1 : 0;
        }
    }
    if (button(a, x, y + 66, (b.width - gap) / 2, u8"10분 진행 (시뮬레이션)"))
        result(a, game_session_step(&a->session, 10));
    if (button(a, x + (b.width + gap) / 2, y + 66, (b.width - gap) / 2, u8"1시간 진행 (시뮬레이션)"))
        result(a, game_session_step(&a->session, 60));
    ui_small(&a->ui, x, y + 112, UI_GOLD, u8"날짜·시각 바로 이동 (생산·소비 생략, 앞으로만)");
    ui_textbox(&a->ui, box(x, y + 139, 110, 32), 201, t->day, sizeof(t->day), 1);
    ui_small(&a->ui, x + 116, y + 146, UI_TEXT, u8"일");
    ui_textbox(&a->ui, box(x + 155, y + 139, 65, 32), 202, t->hour, sizeof(t->hour), 1);
    ui_small(&a->ui, x + 226, y + 146, UI_TEXT, ":");
    ui_textbox(&a->ui, box(x + 247, y + 139, 65, 32), 203, t->minute, sizeof(t->minute), 1);
    if (button(a, x + 326, y + 139, 100, u8"이동")) {
        int day, hour, minute;
        if (integer(t->day, 1, 1000000, &day) && integer(t->hour, 0, 23, &hour) &&
            integer(t->minute, 0, 59, &minute) && minute % 10 == 0)
            set_clock(a, day, hour * 60 + minute);
        else
            (void)snprintf(t->result, sizeof(t->result),
                           u8"날짜 1~1000000일, 시각은 10분 단위로 입력하세요.");
    }
    static const int minutes[] = {300, 360, 720, 1080, 1260, 1380};
    static const char *const times[] = {u8"새벽", u8"아침", u8"정오", u8"저녁", u8"밤", u8"심야"};
    for (int i = 0; i < 6; i++)
        if (button(a, x + i * (cell + gap), y + 180, cell, times[i]))
            set_clock(a, a->session.summary.day + (a->session.summary.minute_of_day > minutes[i]),
                      minutes[i]);
    for (int i = 0; i < CORE_SEASON_COUNT; i++)
        if (button(a, x + i * (b.width + gap) / 4, y + 221, (b.width - 3 * gap) / 4, fmt_season_name(i))) {
            int day = ((a->session.summary.day - 1) / 48) * 48 + i * 12 + 1;
            if (day < a->session.summary.day ||
                (day == a->session.summary.day && a->session.summary.minute_of_day > 360))
                day += 48;
            set_clock(a, day, 360);
        }
    ui_small(&a->ui, x, y + 270, UI_GOLD, u8"날씨 고정");
    cell = (b.width - 4 * gap) / 5;
    for (int i = -1; i < CORE_WEATHER_COUNT; i++)
        if (ui_button(&a->ui, box(x + (i + 1) * (cell + gap), y + 297, cell, 34),
                      i < 0 ? u8"자연 날씨" : fmt_weather_name(i), 1, t->weather == i, NULL)) {
            t->weather = i;
            result(a, core_test_configure(a->session.world, t->flags, t->weather));
        }
}

static void construction(GameApp *a, Rectangle b) {
    float y = b.y, col = (b.width - 12) / 2;
    toggle(a, b.x, y, col, CORE_TEST_FREE_BUILD, u8"건설 비용 면제");
    toggle(a, b.x + col + 12, y, col, CORE_TEST_INSTANT_BUILD, u8"즉시 완공");
    toggle(a, b.x, y + 44, b.width, CORE_TEST_UNLOCK_BUILD, u8"건물 등급·도로 연결 제한 해제");
    ui_wrap(&a->ui, box(b.x, y + 90, b.width, 42), UI_MUTED,
            u8"빈 공간·문·지형 조건은 지킵니다. 공사 중인 건물은 아래 단추로 마칩니다.");
    if (button(a, b.x, y + 140, col, u8"모든 공사 완공"))
        result(a, core_test_finish_buildings(a->session.world));
    if (button(a, b.x + col + 12, y + 140, col, u8"모든 연구 완료"))
        result(a, core_test_research(a->session.world, -1));
    ui_small(&a->ui, b.x, y + 198, UI_GOLD, u8"영지 등급 바로 지정");
    for (int i = 0; i < CORE_RANK_COUNT; i++) {
        float w = (b.width - 16) / 3;
        if (ui_button(&a->ui, box(b.x + (i % 3) * (w + 8), y + 225 + (i / 3) * 42, w, 34),
                      core_expansion_rank_name(i), 1, a->session.summary.rank == i, NULL))
            result(a, core_test_rank(a->session.world, i));
    }
}

static void population_combat(GameApp *a, Rectangle b) {
    GameTestTools *t = &a->test_tools;
    float x = b.x, y = b.y, col = (b.width - 12) / 2;
    toggle(a, x, y, col, CORE_TEST_SUSTAIN, u8"주민 욕구·건강 유지");
    toggle(a, x + col + 12, y, col, CORE_TEST_NO_RAIDS, u8"자연 습격 멈춤");
    ui_small(&a->ui, x, y + 52, UI_TEXT, u8"주민 수 (1~300명, 주거는 별도로 건설)");
    ui_textbox(&a->ui, box(x, y + 78, 110, 32), 301, t->population, sizeof(t->population), 1);
    if (button(a, x + 122, y + 78, 110, u8"인구 적용")) {
        int value;
        result(a, integer(t->population, 1, 300, &value) ? core_test_population(a->session.world, value)
                                                         : CORE_ERR_INVALID_ARG);
        sync_fields(a);
    }
    if (button(a, x + col + 12, y + 78, col, u8"주민 치료·건물 수리·패배 복구"))
        result(a, core_test_restore(a->session.world));
    ui_small(&a->ui, x, y + 132, UI_GOLD, u8"수동 습격 (자연 습격 정지 중에도 가능)");
    static const char *const factions[] = {u8"도적", u8"마물", u8"적대 영주"};
    for (int i = 1; i < CORE_THREAT_COUNT; i++)
        if (ui_button(&a->ui, box(x + (i - 1) * (b.width + 8) / 3, y + 158, (b.width - 16) / 3, 32),
                      factions[i - 1], 1, t->raid_type == i, NULL))
            t->raid_type = i;
    static const char *const directions[] = {u8"북쪽", u8"동쪽", u8"남쪽", u8"서쪽"};
    for (int i = 0; i < CORE_DIR_COUNT; i++)
        if (ui_button(&a->ui, box(x + i * (b.width + 8) / 4, y + 200, (b.width - 24) / 4, 32), directions[i],
                      1, t->direction == i, NULL))
            t->direction = i;
    ui_textbox(&a->ui, box(x, y + 244, 110, 32), 302, t->enemies, sizeof(t->enemies), 1);
    if (button(a, x + 122, y + 244, 150, u8"습격 생성 (1~64)")) {
        int count;
        result(a, integer(t->enemies, 1, 64, &count)
                      ? core_test_raid(a->session.world, t->raid_type, t->direction, count)
                      : CORE_ERR_INVALID_ARG);
    }
    if (button(a, x + col + 12, y + 244, col, u8"현재 습격 종료")) {
        result(a, core_test_raid(a->session.world, t->raid_type, t->direction, 0));
        a->raid_alert_id = 0;
    }
}

static void world_saves(GameApp *a, Rectangle b) {
    toggle(a, b.x, b.y, b.width, CORE_TEST_REVEAL, u8"지도 전체 시야");
    ui_wrap(&a->ui, box(b.x, b.y + 50, b.width, 70), UI_TEXT,
            u8"테스트 저장은 저장 폴더 안의 test-tools에 보관합니다.\n"
            "일반 이어하기·불러오기에는 나타나지 않습니다.\n"
            "불러온 뒤에는 검사 설정을 기본값으로 다시 켭니다.");
    if (button(a, b.x, b.y + 138, (b.width - 12) / 2, u8"테스트 슬롯 저장"))
        game_app_open(a, PANEL_SAVE);
    if (button(a, b.x + (b.width + 12) / 2, b.y + 138, (b.width - 12) / 2, u8"테스트 슬롯 불러오기"))
        game_app_open(a, PANEL_LOAD);
    if (button(a, b.x, b.y + 188, b.width, u8"같은 이름·시드로 새 테스트 영지 만들기"))
        (void)game_app_start_test(a);
    ui_wrap(&a->ui, box(b.x, b.y + 244, b.width, 75), UI_MUTED,
            u8"일반 게임은 메인 화면의 새 게임·이어하기로 시작하세요.\n"
            "창작 도구는 난이도나 저장 형식에 새 게임 모드를 추가하지 않습니다.");
}

static void setup(GameApp *a, Rectangle b) {
    float x = b.x, y = b.y;
    ui_wrap(&a->ui, box(x, y, b.width, 60), UI_TEXT,
            u8"자원·시간·날씨·건설·주민·습격을 직접 조절하며 시험합니다.\n"
            "일반 세계를 사용하며, 테스트 저장은 별도 폴더에 보관합니다.");
    ui_small(&a->ui, x, y + 75, UI_GOLD, u8"영지 이름");
    ui_textbox(&a->ui, box(x, y + 101, b.width, 36), 401, a->castle_name, sizeof(a->castle_name), 0);
    ui_small(&a->ui, x, y + 152, UI_GOLD, u8"지도 시드");
    ui_textbox(&a->ui, box(x, y + 178, b.width, 36), 402, a->seed_text, sizeof(a->seed_text), 1);
    for (int i = 0; i < CORE_DIFF_COUNT; i++)
        if (ui_button(&a->ui, box(x + i * (b.width + 8) / 3, y + 235, (b.width - 16) / 3, 34),
                      fmt_difficulty_name(i), 1, a->difficulty == i, NULL))
            a->difficulty = i;
    if (button(a, x, y + 292, (b.width - 12) / 2, u8"테스트 영지 만들기"))
        (void)game_app_start_test(a);
    if (button(a, x + (b.width + 12) / 2, y + 292, (b.width - 12) / 2, u8"최근 테스트 이어하기"))
        (void)game_app_load_test(a);
    if (button(a, x, y + 334, b.width, u8"최근 일반 저장을 복사해서 시험하기"))
        (void)game_app_copy_test(a);
}

void game_test_tools_draw(GameApp *a) {
    a->ui.tooltip[0] = '\0'; /* 뒤에 있는 HUD의 설명이 도구를 가리지 않게 합니다. */
    GameTestTools *t = &a->test_tools;
    float width = (float)GetScreenWidth() - 40, height = (float)GetScreenHeight() - 40;
    if (width > 1080)
        width = 1080;
    if (height > 650)
        height = 650;
    Rectangle b = box((GetScreenWidth() - width) / 2, (GetScreenHeight() - height) / 2, width, height);
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), (Color){8, 13, 19, 200});
    ui_box(b, UI_PANEL);
    ui_text(&a->ui, b.x + 24, b.y + 20, UI_GOLD, u8"창작 도구 · 테스트 작업실");
    if (button(a, b.x + b.width - 164, b.y + 16, 140, t->active ? u8"닫고 플레이 (F10)" : u8"메뉴로")) {
        game_app_close(a);
        return;
    }
    float old_size = a->ui.font_size;
    a->ui.font_size = 16;
    Rectangle content = box(b.x + 24, b.y + 112, b.width - 48, b.height - 172);
    if (!t->active)
        setup(a, box(content.x, b.y + 76, content.width, content.height));
    else {
        static const char *const tabs[] = {u8"자원", u8"시간·날씨", u8"건설·진행", u8"주민·전투",
                                           u8"세계·저장"};
        for (int i = 0; i < 5; i++)
            if (ui_button(
                    &a->ui,
                    box(content.x + i * (content.width + 8) / 5, b.y + 66, (content.width - 32) / 5, 32),
                    tabs[i], 1, t->tab == i, NULL)) {
                t->tab = i;
                a->ui.focus = 0;
                sync_fields(a);
            }
        switch (t->tab) {
        case 0:
            resources(a, content);
            break;
        case 1:
            time_weather(a, content);
            break;
        case 2:
            construction(a, content);
            break;
        case 3:
            population_combat(a, content);
            break;
        default:
            world_saves(a, content);
            break;
        }
    }
    ui_wrap(&a->ui, box(content.x, b.y + b.height - 55, content.width, 44), UI_GOLD,
            t->result[0] ? t->result : u8"F10: 도구 열기/닫기 · 창을 열어 둔 동안 시간은 멈춥니다.");
    a->ui.font_size = old_size;
}

void game_test_tools_badge(GameApp *a) {
    if (!a->test_tools.active || a->panel != PANEL_NONE)
        return;
    Rectangle badge = box(12, a->status_height + 8.0f, 174, 30);
    if (a->toast_area_count < 8)
        a->toast_areas[a->toast_area_count++] = badge;
    if (ui_button(&a->ui, badge, u8"창작 도구 열기 · F10", 1, 0, NULL))
        game_test_tools_open(a);
}
