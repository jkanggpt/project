/* main_menu.c — 제목 배경과 새 게임 창. 규칙과 저장 형식은 공개 API만 읽습니다. */
#include "app.h"
#include "format.h"
#include "render/gpu_bridge.h"
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static Rectangle area(float x, float y, float w, float h) { return (Rectangle){x, y, w, h}; }

static void refresh_save(GameApp *a) {
    GameMenuState *m = &a->menu;
    char path[GAME_PATH_BYTES];
    m->latest_slot = game_app_latest(a);
    m->latest_backup = 0;
    memset(&m->latest, 0, sizeof(m->latest));
    if (m->latest_slot >= 0) {
        game_app_slot_path(a, m->latest_slot, 0, path);
        if (core_peek_save(path, a->session.defs, &m->latest) != CORE_OK) {
            game_app_slot_path(a, m->latest_slot, 1, path);
            m->latest_backup = 1;
            if (core_peek_save(path, a->session.defs, &m->latest) != CORE_OK)
                m->latest_slot = -1;
        }
    }
    m->save_age = 0;
    m->saves_checked = 1;
}

/* 성능 검사 세계는 동물·폭풍도 추가하므로, 배경은 정상 건설 명령으로 작게 만듭니다. */
static int background(GameApp *a) {
    static const int types[] = {CORE_BLD_SHACK,        CORE_BLD_WELL,   CORE_BLD_GARDEN,
                                CORE_BLD_LOGGING_SITE, CORE_BLD_GARDEN, CORE_BLD_SHACK};
    GameSession *s = &a->session;
    int rc = game_session_new(s, u8"바람언덕", 42, CORE_DIFF_PEACEFUL);
    if (rc != CORE_OK)
        return rc;
    game_gpu_new_world();
    for (size_t i = 0; i < sizeof(types) / sizeof(types[0]); i++) {
        int placed = 0;
        for (int radius = 4; radius <= 13 && !placed; radius += 3)
            for (int dy = -radius; dy <= radius && !placed; dy += 3)
                for (int dx = -radius; dx <= radius && !placed; dx += 3) {
                    int32_t id = 0, why = 0;
                    placed = core_place_building(s->world, types[i], s->summary.keep_x + dx,
                                                 s->summary.keep_y + dy, &id, &why) == CORE_OK;
                }
    }
    /* 초기 건설을 마친 모습으로 시작하되, 이 세계는 저장하지 않습니다. */
    rc = game_session_step(s, 360);
    if (rc != CORE_OK)
        return rc;
    a->menu.background_ready = 1;
    a->menu.simulation_time = 0;
    a->menu.saves_checked = 0;
    a->speed = 0;
    if (a->panel == PANEL_TITLE && a->return_panel != PANEL_TITLE) {
        a->panel_depth = 0;
        a->return_panel = PANEL_TITLE;
        a->notice_count = 0;
        a->message[0] = '\0';
        if (!a->automated)
            ui_transition(&a->ui);
    }
    return CORE_OK;
}

void game_main_menu_enter(GameApp *a) {
    a->menu.background_ready = 0;
    a->menu.credits = 0;
    a->menu.saves_checked = 0;
    a->menu.preview_ready = 0;
    a->menu.pending_action = 0;
    a->ui.focus = 0;
    if (!a->automated)
        ui_transition(&a->ui);
}

static int read_seed(const char *text, int *seed) {
    char *end;
    long value;
    if (!text[0])
        return 0;
    for (const char *p = text; *p; p++)
        if (*p < '0' || *p > '9')
            return 0;
    errno = 0;
    value = strtol(text, &end, 10);
    if (errno || *end || value < 0 || value > INT_MAX)
        return 0;
    *seed = (int)value;
    return 1;
}

static void preview(GameApp *a) {
    GameMenuState *m = &a->menu;
    CoreWorld *world = NULL;
    CoreNewGameParams params = {0};
    unsigned char *terrain = NULL;
    int32_t width = 0, height = 0;
    m->preview_ready = -1;
    if (!read_seed(a->seed_text, &m->preview_seed))
        return;
    (void)snprintf(params.castle_name, sizeof(params.castle_name), u8"지도 미리보기");
    params.seed = m->preview_seed;
    params.difficulty = CORE_DIFF_PEACEFUL;
    params.ruleset = CORE_RULES_EXPANSION;
    if (core_create(&params, a->session.defs, &world) == CORE_OK &&
        core_get_map_size(world, &width, &height) == CORE_OK && width > 0 && height > 0 &&
        width <= INT_MAX / height) {
        int cells = width * height;
        terrain = (unsigned char *)malloc((size_t)cells);
        if (terrain && core_copy_terrain(world, terrain, cells) == CORE_OK) {
            for (int y = 0; y < 64; y++)
                for (int x = 0; x < 64; x++)
                    m->preview[y * 64 + x] = terrain[(y * height / 64) * width + x * width / 64];
            m->map_width = width;
            m->map_height = height;
            m->preview_ready = 1;
        }
    }
    free(terrain);
    core_destroy(world);
}

void game_main_menu_update(GameApp *a, float delta) {
    GameMenuState *m = &a->menu;
    if (a->in_game)
        return;
    if (a->test_tools.active)
        game_test_tools_enter(a, 0);
    if (a->panel != PANEL_CONFIRM)
        a->test_tools.loading = 0;
    if (!m->background_ready) {
        int rc = background(a);
        if (rc != CORE_OK) {
            game_app_result(a, rc, 0, 0);
            return;
        }
    }
    /* 공용 루프가 설정한 게임용 화면 중심은 비활성 창에서도 제목용으로 복원합니다. */
    a->camera.offset = (Vector2){GetScreenWidth() * .70f, GetScreenHeight() * .48f};
    /* 비활성 창은 시간·주민 이동·카메라를 모두 멈춥니다. 자동 캡처는 같은 경로로 진행합니다. */
    if (!a->automated && !IsWindowFocused())
        return;
    m->time += delta;
    m->save_age += delta;
    if (a->panel == PANEL_TITLE && (!m->saves_checked || m->save_age > 5))
        refresh_save(a);
    m->simulation_time += delta;
    if (m->simulation_time >= .1f) {
        int rc = game_session_step(&a->session, m->simulation_time * 12);
        m->simulation_time = 0;
        if (rc != CORE_OK)
            game_app_result(a, rc, 0, 0);
        /* 특별 이주가 시작되기 전에 다시 만들어 주민 수와 작업량을 제한합니다. */
        if (a->session.summary.day >= 4)
            m->background_ready = 0;
    }
    a->camera.target = (Vector2){a->session.summary.keep_x * 16.0f + sinf(m->time * .035f) * 40,
                                 a->session.summary.keep_y * 16.0f + cosf(m->time * .028f) * 28};
    a->camera.zoom = floorf(fmaxf(1.7f, fminf(3, GetScreenHeight() / 300.0f)) + .5f);
}

static Rectangle menu_modal(GameApp *a, const char *title, float width, float height) {
    width = fminf(width, GetScreenWidth() - 40.0f);
    height = fminf(height, GetScreenHeight() - 40.0f);
    Rectangle b = area((GetScreenWidth() - width) / 2, (GetScreenHeight() - height) / 2, width, height);
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), (Color){8, 13, 19, 195});
    ui_box(b, UI_PANEL);
    DrawRectangle((int)b.x, (int)b.y, (int)b.width, 3, UI_GOLD);
    ui_text(&a->ui, b.x + 26, b.y + 24, UI_GOLD, "%s", title);
    return b;
}

static void crest(GameApp *a, float x, float y, float size) {
    Texture2D manor = a->assets.manors[3];
    DrawCircle((int)(x + size / 2), (int)(y + size / 2), size / 2, (Color){24, 33, 39, 240});
    DrawCircleLines((int)(x + size / 2), (int)(y + size / 2), size / 2, UI_GOLD);
    DrawTexturePro(manor, area(0, 0, (float)manor.width, (float)manor.height),
                   area(x + 5, y + 5, size - 10, size - 10), (Vector2){0, 0}, 0, WHITE);
    DrawTriangle((Vector2){x + size / 2 - 6, y + size + 1}, (Vector2){x + size / 2, y + size + 8},
                 (Vector2){x + size / 2 + 6, y + size + 1}, UI_GOLD);
}

static void credits(GameApp *a) {
    Rectangle b = menu_modal(a, u8"함께 만든 사람들", 790, 525);
    float x = b.x + 28, y = b.y + 82, width = b.width - 56;
    static const char *const names[] = {u8"게임 제작", u8"그림",       u8"효과음",
                                        u8"글꼴",      u8"라이브러리", u8"주민 이름"};
    static const char *const details[] = {u8"C11 · raylib 5.5 / Claude Code · Codex",
                                          u8"image_gen 원화와 프로젝트 파생 그림 · 보유 권리 CC0 1.0",
                                          u8"Kenney · UI와 전투 효과음 8개 · CC0",
                                          u8"Galmuri11 · Lee Minseo (quiple) · SIL OFL 1.1",
                                          u8"raylib · Ramon Santamaria 및 기여자 / GLFW · zlib/libpng 계열",
                                          u8"학생이 제공한 이름 324개"};
    float old_size = a->ui.font_size;
    a->ui.font_size = 18;
    for (int i = 0; i < 6; i++) {
        ui_small(&a->ui, x, y, UI_GOLD, "%s", names[i]);
        ui_wrap(&a->ui, area(x + 110, y, width - 110, 46), UI_TEXT, details[i]);
        y += 53;
    }
    a->ui.font_size = old_size;
    if (ui_button(&a->ui, area(x, b.y + b.height - 64, width, 40), u8"메뉴로 돌아가기", 1, 0, NULL) ||
        IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_ENTER)) {
        a->menu.credits = 0;
        a->ui.consumed = 1;
    }
}

static void menu_action(GameApp *a, int item) {
    switch (item) {
    case 0:
        refresh_save(a);
        if (a->menu.latest_slot >= 0)
            (void)game_app_load(a, a->menu.latest_slot, 0);
        break;
    case 1:
        game_app_open(a, PANEL_NEW);
        break;
    case 2:
        (void)game_tutorial_start(a);
        break; /* 튜토리얼 작업과의 고정 연결점입니다. */
    case 3:
        game_app_open(a, PANEL_LOAD);
        break;
    case 4:
        game_app_open(a, PANEL_SETTINGS);
        break;
    case 5:
        game_app_open(a, PANEL_HELP);
        break;
    case 6:
        a->menu.credits = 1;
        break;
    case 7:
        a->quit = 1;
        break;
    case 8:
        (void)game_app_start(a);
        break;
    case 9:
        game_test_tools_open(a);
        break;
    }
}

static void request_action(GameApp *a, int item) {
    if (!a->automated && !a->in_game && (item == 0 || item == 2 || item == 8)) {
        a->menu.pending_action = item + 1;
        a->menu.pending_panel = (int)a->panel;
        ui_transition_out(&a->ui);
    } else
        menu_action(a, item);
}

static int finish_transition(GameApp *a) {
    if (a->menu.pending_action && a->menu.pending_panel != (int)a->panel) {
        a->menu.pending_action = 0;
        ui_transition(&a->ui);
        return 0;
    }
    if (!a->menu.pending_action || a->ui.transition < 1)
        return 0;
    int item = a->menu.pending_action - 1;
    a->menu.pending_action = 0;
    menu_action(a, item);
    ui_transition(&a->ui); /* 실패했을 때도 알림과 메뉴가 다시 보여야 합니다. */
    return 1;
}

void game_main_menu_title(GameApp *a) {
    if (finish_transition(a))
        return;
    static const char *const labels[] = {u8"이어하기", u8"새 게임", u8"튜토리얼", u8"불러오기", u8"창작 도구",
                                         u8"설정",     u8"도움말",  u8"제작진",   u8"종료"};
    static const int actions[] = {0, 1, 2, 3, 9, 4, 5, 6, 7};
    GameMenuState *m = &a->menu;
    GameUi *u = &a->ui;
    m->preview_ready = 0;
    m->preview_delay = 0;
    float w = (float)GetScreenWidth(), h = (float)GetScreenHeight();
    float x = fmaxf(32, w * .06f), width = fminf(360, w * .39f);
    float top = fmaxf(32, h * .10f), row = fminf(48, h * .06f), gap = 6;
    float y = top + 112;
    Vector2 mouse = GetMousePosition();
    int moved = mouse.x != m->mouse_x || mouse.y != m->mouse_y;
    int action = -1;
    m->mouse_x = mouse.x;
    m->mouse_y = mouse.y;
    DrawRectangleGradientH(0, 0, (int)(w * .70f), (int)h, (Color){12, 19, 25, 250}, (Color){12, 19, 25, 0});
    DrawRectangleGradientV(0, (int)(h * .65f), (int)w, (int)(h * .35f), BLANK, (Color){8, 15, 21, 220});
    crest(a, x, top, 56);
    float logo_size = fminf(32, (width - 72) / 7.5f);
    Vector2 logo = {x + 72, top + 12};
    DrawTextEx(a->assets.font, u8"마법성채 키우기", (Vector2){logo.x + 2, logo.y + 3}, logo_size, 0, BLACK);
    DrawTextEx(a->assets.font, u8"마법성채 키우기", logo, logo_size, 0, UI_GOLD);
    ui_small(u, x, top + 78, UI_TEXT, u8"작은 개척지에서, 당신의 왕성까지.");
    DrawLine((int)x, (int)(top + 101), (int)(x + width), (int)(top + 101), Fade(UI_GOLD, .45f));
    if (!m->saves_checked)
        refresh_save(a);
    if (m->selected == 0 && m->latest_slot < 0)
        m->selected = 1;
    if (m->credits) {
        credits(a);
        return;
    }
    if (IsKeyPressed(KEY_DOWN) || IsKeyPressedRepeat(KEY_DOWN))
        m->selected = (m->selected + 1) % 9;
    if (IsKeyPressed(KEY_UP) || IsKeyPressedRepeat(KEY_UP))
        m->selected = (m->selected + 8) % 9;
    if (!m->selected && m->latest_slot < 0)
        m->selected = IsKeyPressed(KEY_UP) || IsKeyPressedRepeat(KEY_UP) ? 8 : 1;
    for (int i = 0; i < 9; i++) {
        Rectangle r = area(x, y + i * (row + gap), width, row);
        int enabled = i != 0 || m->latest_slot >= 0;
        int hover = CheckCollisionPointRec(mouse, r);
        if (moved && hover && enabled)
            m->selected = i;
        int selected = m->selected == i;
        DrawRectangleRec(r, selected ? (Color){105, 82, 49, 224} : (Color){18, 28, 35, 205});
        DrawRectangleLinesEx(r, 1, selected ? UI_GOLD : (Color){69, 78, 77, 180});
        ui_text(u, r.x + 20, r.y + (r.height - u->font_size) / 2, enabled ? UI_TEXT : UI_MUTED, "%s",
                labels[i]);
        if (selected)
            ui_small(u, r.x + r.width - 32, r.y + (r.height - u->font_size + 3) / 2, UI_GOLD, ">");
        if (enabled && hover && !u->consumed && IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
            action = i;
    }
    if (!u->consumed && (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)))
        action = m->selected;
    if (action >= 0 && !m->pending_action) {
        u->consumed = 1;
        game_sound(&a->assets, SND_CLICK, u->volume);
        request_action(a, actions[action]);
    }
    Rectangle card =
        area(fmaxf(x + width + 38, w - 410), h - 175, fminf(362, w - (x + width + 38) - 32), 118);
    ui_box(card, UI_PANEL);
    ui_small(u, card.x + 16, card.y + 12, UI_GOLD, u8"가장 최근의 영지%s",
             m->latest_backup ? u8" · 예비 저장" : "");
    if (m->latest_slot >= 0) {
        float old_size = u->font_size;
        u->font_size = 18;
        ui_wrap(u, area(card.x + 16, card.y + 38, card.width - 32, 24), UI_TEXT, m->latest.castle_name);
        ui_small(u, card.x + 16, card.y + 72, UI_TEXT, u8"%d년 %s · %d일 · %s", m->latest.year,
                 fmt_season_name(m->latest.season), m->latest.day, core_expansion_rank_name(m->latest.rank));
        ui_small(u, card.x + 16, card.y + 94, UI_MUTED, u8"주민 %d명", m->latest.population);
        u->font_size = old_size;
    } else {
        ui_small(u, card.x + 16, card.y + 45, UI_TEXT, u8"아직 저장된 영지가 없습니다.");
        ui_small(u, card.x + 16, card.y + 79, UI_MUTED, u8"새 게임이나 튜토리얼로 시작하세요.");
    }
    ui_small(u, x, h - 29, UI_MUTED, u8"위/아래 선택 · Enter 시작");
    ui_small(u, w - 114, h - 29, UI_TEXT, u8"버전 1.2.4");
}

static void update_preview(GameApp *a) {
    GameMenuState *m = &a->menu;
    if (!m->preview_ready) {
        if (!a->seed_text[0])
            (void)snprintf(a->seed_text, sizeof(a->seed_text), "%d",
                           (int)((unsigned long)time(NULL) & INT_MAX));
        (void)snprintf(m->preview_key, sizeof(m->preview_key), "%s", a->seed_text);
        preview(a);
    } else if (strcmp(m->preview_key, a->seed_text)) {
        (void)snprintf(m->preview_key, sizeof(m->preview_key), "%s", a->seed_text);
        m->preview_delay = .3f;
    } else if (m->preview_delay > 0) {
        m->preview_delay -= fminf(GetFrameTime(), .1f);
        if (m->preview_delay <= 0)
            preview(a);
    }
}

void game_main_menu_new_game(GameApp *a) {
    if (finish_transition(a))
        return;
    update_preview(a);
    static const Color terrain_colors[] = {{100, 143, 94, 255},
                                           {44, 91, 67, 255},
                                           {137, 139, 142, 255},
                                           {68, 116, 157, 255},
                                           {105, 73, 145, 255}};
    static const char *const descriptions[] = {u8"습격 없이 여유롭게.\n넉넉한 자원으로 시작.",
                                               u8"살림과 방어를 함께.\n첫 영지에 권합니다.",
                                               u8"강한 습격과 적은 자원.\n익숙한 영주의 도전."};
    GameMenuState *m = &a->menu;
    GameUi *u = &a->ui;
    Rectangle b = menu_modal(a, u8"새로운 영지", 1000, 690);
    float x = b.x + 28, width = b.width - 56, left = width * .49f, right = x + left + 26;
    float cards_y = b.y + b.height - 190,
          map_size = fminf(260, fminf(width - left - 26, cards_y - b.y - 135));
    float map_x = right + (width - left - 26 - map_size) / 2, map_y = b.y + 86;
    if (ui_button(u, area(b.x + b.width - 88, b.y + 19, 60, 34), u8"닫기", 1, 0, NULL)) {
        game_app_close(a);
        return;
    }
    if (IsKeyPressed(KEY_TAB))
        u->focus = u->focus % 3 + 1;
    ui_small(u, x, b.y + 86, UI_MUTED, u8"성채 이름");
    ui_textbox(u, area(x, b.y + 112, left, 42), 1, a->castle_name, sizeof(a->castle_name), 0);
    ui_small(u, x, b.y + 178, UI_MUTED, u8"지도 시드");
    ui_textbox(u, area(x, b.y + 204, left - 102, 42), 2, a->seed_text, sizeof(a->seed_text), 1);
    if (ui_button(u, area(x + left - 94, b.y + 204, 94, 42), u8"새 시드", 1, 0, NULL)) {
        (void)snprintf(a->seed_text, sizeof(a->seed_text), "%d", GetRandomValue(0, INT_MAX - 1));
        m->preview_ready = 0;
        u->focus = 0;
    }
    ui_wrap(u, area(x, b.y + 266, left, 76), UI_MUTED,
            u8"같은 시드, 같은 지형.\n한글 이름은 붙여넣기도 됩니다.");
    ui_box(area(map_x - 4, map_y - 4, map_size + 8, map_size + 8), UI_EDGE);
    if (m->preview_ready == 1) {
        for (int y = 0; y < 64; y++)
            for (int xx = 0; xx < 64; xx++) {
                unsigned type = m->preview[y * 64 + xx];
                DrawRectangleRec(
                    area(map_x + xx * map_size / 64, map_y + y * map_size / 64, ceilf(map_size / 64),
                         ceilf(map_size / 64)),
                    terrain_colors[type < sizeof(terrain_colors) / sizeof(terrain_colors[0]) ? type : 0]);
            }
        ui_small(u, map_x, map_y + map_size + 13, UI_TEXT, u8"%d×%d 지도 · 시드 %d", m->map_width,
                 m->map_height, m->preview_seed);
    } else
        ui_wrap(u, area(map_x + 12, map_y + 24, map_size - 24, map_size - 48), UI_TEXT,
                m->preview_ready < 0 ? u8"지도 시드는 0~2147483647의 정수로 입력하세요."
                                     : u8"지도를 살펴보고 있습니다.");
    ui_small(u, x, cards_y - 28, UI_GOLD, u8"어떤 영주가 되시겠어요?");
    if (u->focus == 3) {
        if (IsKeyPressed(KEY_LEFT))
            a->difficulty = (a->difficulty + CORE_DIFF_COUNT - 1) % CORE_DIFF_COUNT;
        if (IsKeyPressed(KEY_RIGHT))
            a->difficulty = (a->difficulty + 1) % CORE_DIFF_COUNT;
    }
    float old_size = u->font_size;
    u->font_size = 18;
    for (int i = 0; i < CORE_DIFF_COUNT; i++) {
        CoreDifficultyView difficulty;
        Rectangle card = area(x + i * (width + 10) / 3, cards_y, (width - 20) / 3, 98);
        if (ui_button(u, card, "", 1, a->difficulty == i, NULL)) {
            a->difficulty = i;
            u->focus = 3;
        }
        ui_text(u, card.x + 12, card.y + 10, a->difficulty == i ? UI_GOLD : UI_TEXT, "%s",
                fmt_difficulty_name(i));
        ui_wrap(u, area(card.x + 12, card.y + 38, card.width - 24, 53), UI_TEXT, descriptions[i]);
        if (CheckCollisionPointRec(GetMousePosition(), card) &&
            core_get_difficulty(i, &difficulty) == CORE_OK)
            (void)snprintf(
                u->tooltip, sizeof(u->tooltip),
                u8"시작 주민 %d명 · 식량 %d · 원목 %d\n습격 세기 %d%% · 자연 사건 %d%% · 승급 공물 %d%%",
                difficulty.start_villagers, difficulty.start_resources[CORE_RES_FOOD],
                difficulty.start_resources[CORE_RES_LOGS], difficulty.raid_strength_pct,
                difficulty.nature_pct, difficulty.tribute_pct);
    }
    u->font_size = old_size;
    int seed = 0;
    int valid_name = core_check_castle_name(a->castle_name) == CORE_OK;
    int valid_seed = read_seed(a->seed_text, &seed);
    int current = m->preview_ready == 1 && m->preview_seed == seed && m->preview_delay <= 0;
    ui_small(u, x, b.y + b.height - 82, valid_name && valid_seed ? UI_MUTED : UI_BAD,
             !valid_name   ? u8"이름은 올바른 한글·영문 등 1~47바이트여야 합니다."
             : !valid_seed ? u8"지도 시드는 0~2147483647의 정수여야 합니다."
                           : u8"Tab 입력칸 이동 · 좌우 난이도 선택 · Enter 시작");
    int start = ui_button(u, area(x, b.y + b.height - 54, width, 36), u8"이 영지에서 시작하기",
                          valid_name && valid_seed && current, 1, NULL);
    if (valid_name && valid_seed && current && !u->consumed && IsKeyPressed(KEY_ENTER)) {
        game_sound(&a->assets, SND_CLICK, u->volume);
        start = 1;
    }
    if (start && !m->pending_action)
        request_action(a, 8);
}
