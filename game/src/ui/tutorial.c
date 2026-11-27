/* tutorial.c — 튜토리얼 안내의 화면 (설명은 tutorial.h)
 *
 * 코어가 정한 지금 단계(core_guide_view)를 받아 왼쪽 아래 안내 창을 그리고, 할 일이 있는 단추·건설 카드·지도
 * 칸을 깜빡이는 테두리로 강조합니다. 정보 단계를 읽는 동안에는 시간을 멈추고, 행동 단계로 넘어가면 원래
 * 배속으로 돌려 놓습니다. 화면 이동 단계는 플레이어가 화면을 움직이면 코어에 "다음"을 요청합니다.
 */
#include "app.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#define TUTORIAL_CASTLE_NAME u8"튜토리얼 영지"
#define TUTORIAL_SEED "42"            /* tests/test_guide.c 가 이 지도로 끝까지 진행해 봅니다 */
#define TUTORIAL_ADVISOR_PORTRAIT 8   /* 은발의 늙은 조언자 (ui/portraits.png 의 8번 얼굴) */
#define TUTORIAL_BUILD_BAR_HEIGHT 248 /* ui/panels.c 의 건설 막대 높이 */
#define TUTORIAL_INSPECTOR_WIDTH 350

static Rectangle rect(float x, float y, float w, float h) { return (Rectangle){x, y, w, h}; }

int game_tutorial_start(GameApp *a) {
    char name[sizeof(a->castle_name)], seed[sizeof(a->seed_text)];
    int difficulty = a->difficulty, started;
    /* 새 게임 창에 적어 둔 값은 튜토리얼이 끝난 뒤에도 그대로 남도록 잠깐만 바꿉니다. */
    memcpy(name, a->castle_name, sizeof(name));
    memcpy(seed, a->seed_text, sizeof(seed));
    (void)snprintf(a->castle_name, sizeof(a->castle_name), "%s", TUTORIAL_CASTLE_NAME);
    (void)snprintf(a->seed_text, sizeof(a->seed_text), "%s", TUTORIAL_SEED);
    a->difficulty = CORE_DIFF_PEACEFUL;
    started = game_app_start(a);
    memcpy(a->castle_name, name, sizeof(name));
    memcpy(a->seed_text, seed, sizeof(seed));
    a->difficulty = difficulty;
    memset(&a->tutorial, 0, sizeof(a->tutorial));
    a->tutorial.last_step = -1;
    if (!started)
        return 0;
    if (core_guide_start(a->session.world) != CORE_OK) {
        game_app_notice(a, u8"튜토리얼 안내를 시작하지 못했습니다. 일반 게임으로 계속합니다.");
        return 1;
    }
    return 1;
}

void game_tutorial_anchor(GameApp *a, int anchor, Rectangle area) {
    if (anchor < 0 || anchor >= GAME_ANCHOR_COUNT)
        return;
    a->tutorial.anchors[anchor] = area;
    a->tutorial.anchor_set[anchor] = 1;
}

int game_tutorial_covers(const GameApp *a, Vector2 point) {
    return a->tutorial.active && a->tutorial.panel.width > 0 &&
           CheckCollisionPointRec(point, a->tutorial.panel);
}

/* 정보 단계를 읽는 동안 시간을 멈추고, 행동 단계로 넘어가면 돌려 놓습니다 */
static void apply_pause(GameApp *a, const CoreGuideView *v) {
    GameTutorialState *t = &a->tutorial;
    if (v->active && !v->finished && v->pause) {
        if (a->speed && !t->paused_by_guide) {
            t->resume_speed = a->speed;
            t->paused_by_guide = 1;
        }
        a->speed = 0;
    } else if (t->paused_by_guide) {
        if (!a->speed)
            a->speed = t->resume_speed ? t->resume_speed : 1;
        t->paused_by_guide = 0;
    }
}

void game_tutorial_update(GameApp *a, float delta) {
    GameTutorialState *t = &a->tutorial;
    CoreGuideView *v = &t->view;
    memset(t->anchor_set, 0, sizeof(t->anchor_set));
    t->pulse += delta;
    if (!a->session.world || core_guide_view(a->session.world, v) != CORE_OK || !v->active) {
        if (t->active)
            apply_pause(a, v); /* 건너뛰었으면 멈춰 둔 시간을 돌려 놓음 */
        t->active = 0;
        t->panel = rect(0, 0, 0, 0);
        return;
    }
    if (!t->active) { /* 튜토리얼 게임을 시작했거나 튜토리얼 세이브를 불러옴 */
        t->active = 1;
        t->last_step = -1;
    }
    if (v->step != t->last_step) {
        if (t->last_step >= 0 && v->step > t->last_step) {
            game_sound(&a->assets, SND_DONE, a->settings.sfx_volume * 0.6f);
            if (v->finished && !t->done_notice) {
                t->done_notice = 1;
                game_app_notice(a, u8"튜토리얼을 마쳤습니다. 이제 자유롭게 영지를 키우세요.");
            }
        }
        t->last_step = v->step;
        t->confirm_skip = 0;
        t->camera_start = a->camera.target;
        t->zoom_start = a->camera.zoom;
    }
    apply_pause(a, v);
    /* 화면 이동 단계: 실제로 화면을 움직이거나 확대하면 넘어갑니다 */
    if (!v->finished && v->target == CORE_GUIDE_TARGET_UI && v->target_id == CORE_GUIDE_UI_MAP &&
        a->panel == PANEL_NONE) {
        float dx = a->camera.target.x - t->camera_start.x, dy = a->camera.target.y - t->camera_start.y;
        if (dx * dx + dy * dy > 48.0f * 48.0f || fabsf(a->camera.zoom - t->zoom_start) > 0.4f)
            (void)core_guide_next(a->session.world);
    }
}

/* ---- 그리기 -------------------------------------------------------------- */

static float glow(const GameTutorialState *t) { return 0.55f + 0.45f * sinf(t->pulse * 5.0f); }

static void outline(const GameTutorialState *t, Rectangle r) {
    Color gold = Fade(UI_GOLD, glow(t));
    DrawRectangleLinesEx(rect(r.x - 4, r.y - 4, r.width + 8, r.height + 8), 3, gold);
}

static int outline_anchor(const GameTutorialState *t, int anchor) {
    if (anchor < 0 || anchor >= GAME_ANCHOR_COUNT || !t->anchor_set[anchor])
        return 0;
    outline(t, t->anchors[anchor]);
    return 1;
}

/* 지도 칸 (x, y)부터 w×h 칸을 화면에 표시합니다 (지도 영역 밖은 그리지 않음) */
static void map_mark(GameApp *a, int x, int y, int w, int h, Color color) {
    const GameTutorialState *t = &a->tutorial;
    int width = GetScreenWidth() - TUTORIAL_INSPECTOR_WIDTH, height = GetScreenHeight();
    Vector2 p = GetWorldToScreen2D((Vector2){x * GAME_TILE_SIZE, y * GAME_TILE_SIZE}, a->camera);
    float size = GAME_TILE_SIZE * a->camera.zoom;
    Rectangle r = rect(p.x, p.y, w * size, h * size);
    ui_clip_begin(0, 132, width, height - 132 - TUTORIAL_BUILD_BAR_HEIGHT);
    DrawRectangleRec(r, Fade(color, 0.22f * glow(t)));
    DrawRectangleLinesEx(r, 2, Fade(color, glow(t)));
    ui_clip_end();
}

/* 지을 건물: 분류 탭 → 카드 → (도구를 들었으면) 지도 자리 순서로 강조 */
static void highlight_build(GameApp *a, int type) {
    const GameTutorialState *t = &a->tutorial;
    int category;
    if (type < 0 || type >= CORE_BLD_COUNT)
        return;
    category = a->session.building_defs[type].category;
    if (a->build_type == type)
        return;
    if (a->category != category)
        (void)outline_anchor(t, GAME_ANCHOR_TAB + category);
    else
        (void)outline_anchor(t, GAME_ANCHOR_CARD + type);
}

static void highlight_target(GameApp *a) {
    const CoreGuideView *v = &a->tutorial.view;
    switch (v->target) {
    case CORE_GUIDE_TARGET_UI:
        if (v->target_id != CORE_GUIDE_UI_MAP)
            (void)outline_anchor(&a->tutorial, GAME_ANCHOR_UI + v->target_id);
        break;
    case CORE_GUIDE_TARGET_BUILD:
        highlight_build(a, v->target_id);
        if (v->hint_x >= 0)
            map_mark(a, v->hint_x, v->hint_y, v->hint_w, v->hint_h, UI_GOOD);
        break;
    case CORE_GUIDE_TARGET_BUILDING:
        if (v->hint_x >= 0)
            map_mark(a, v->hint_x, v->hint_y, v->hint_w, v->hint_h, UI_GOLD);
        break;
    case CORE_GUIDE_TARGET_LINE:
        highlight_build(a, v->target_id);
        if (v->hint_x >= 0)
            map_mark(a, v->hint_x, v->hint_y, 1, 1, UI_GOOD);
        if (v->goal_x >= 0)
            map_mark(a, v->goal_x, v->goal_y, 1, 1, UI_GOLD);
        break;
    default:
        break;
    }
}

static void guide_panel(GameApp *a) {
    GameTutorialState *t = &a->tutorial;
    const CoreGuideView *v = &t->view;
    GameUi *u = &a->ui;
    float old_font = u->font_size, width = fminf(480, GetScreenWidth() * 0.42f), x = 12;
    float text_x = x + 86, text_w = width - 98, body_h, height, y;
    char progress[96] = "";
    u->font_size = old_font > 20 ? 20 : 17;
    body_h = ui_wrap_height(u, text_w, v->body);
    if (v->progress_need > 0)
        (void)snprintf(progress, sizeof(progress), u8"%s %d / %d", v->progress_label, (int)v->progress,
                       (int)v->progress_need);
    height = 12 + 34 + body_h + 8 + (progress[0] ? 26.0f : 0) + 48;
    if (height < 140)
        height = 140;
    y = GetScreenHeight() - TUTORIAL_BUILD_BAR_HEIGHT - height - 10;
    if (y < 140)
        y = 140;
    t->panel = rect(x, y, width, height);
    ui_box(t->panel, UI_PANEL);
    DrawRectangleLinesEx(t->panel, 2, Fade(UI_GOLD, 0.8f));
    if (a->assets.portraits.id) {
        int face = TUTORIAL_ADVISOR_PORTRAIT;
        DrawTexturePro(a->assets.portraits, rect(face % 8 * 64.0f, face / 8 * 64.0f, 64, 64),
                       rect(x + 12, y + 14, 64, 64), (Vector2){0, 0}, 0, WHITE);
        DrawRectangleLinesEx(rect(x + 12, y + 14, 64, 64), 1, UI_EDGE);
    }
    ui_small(u, x + 12, y + 84, UI_MUTED, u8"튜토리얼");
    ui_small(u, x + 12, y + 104, UI_MUTED, "%d / %d", (int)v->step + 1, (int)v->count);
    ui_text(u, text_x, y + 12, UI_GOLD, "%s", v->title);
    ui_wrap(u, rect(text_x, y + 46, text_w, body_h + 4), UI_TEXT, v->body);
    float row = y + 46 + body_h + 8;
    if (progress[0]) {
        ui_small(u, text_x, row, v->progress >= v->progress_need ? UI_GOOD : UI_GOLD, "%s", progress);
        row += 26;
    }
    float buttons = y + height - 44;
    if (t->confirm_skip) {
        ui_small(u, text_x, buttons + 8, UI_TEXT, u8"안내를 끄고 자유롭게 할까요?");
        if (ui_button(u, rect(x + width - 196, buttons, 88, 34), u8"끄기", 1, 0, NULL))
            (void)core_guide_skip(a->session.world);
        if (ui_button(u, rect(x + width - 100, buttons, 88, 34), u8"계속", 1, 0, NULL))
            t->confirm_skip = 0;
    } else {
        if (ui_button(u, rect(text_x, buttons, 104, 34), u8"건너뛰기", 1, 0, u8"튜토리얼 안내를 끕니다"))
            t->confirm_skip = 1;
        if (v->kind == CORE_GUIDE_INFO) {
            if (ui_button(u, rect(x + width - 112, buttons, 100, 34), u8"다음", 1, 1, NULL))
                (void)core_guide_next(a->session.world);
        } else {
            ui_small(u, text_x + 116, buttons + 8, UI_MUTED, u8"해내면 저절로 넘어갑니다");
        }
    }
    u->font_size = old_font;
}

void game_tutorial_draw(GameApp *a) {
    GameTutorialState *t = &a->tutorial;
    if (!t->active || t->view.finished) {
        t->panel = rect(0, 0, 0, 0);
        return;
    }
    if (a->panel == PANEL_NONE)
        highlight_target(a);
    guide_panel(a);
}
