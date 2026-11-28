/* main.c — 화면 계층와 .NET 없이 실행하는 C 게임의 진입점과 한 판의 흐름 */
#include "app.h"
#include "expansion.h"
#include "format.h"
#include "platform.h"
#include "sim.h"
#include "render/gpu_bridge.h"
#include "review.h"
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct Options {
    const char *data, *assets, *save_dir, *screenshot, *screen;
    int seed, difficulty, days, smoke, stress, probe, frames, no_audio, width, height, autoplay, wildlife;
    int gpu_transitions;
    int review, review_phase, review_view, review_azimuth, review_elevation, crowd, review_zoom, review_clean;
} Options;
static int number(const char *text, int min, int max, int *out) {
    char *end;
    long n;
    errno = 0;
    n = strtol(text, &end, 10);
    if (!*text || *end || errno || n < min || n > max)
        return 0;
    *out = (int)n;
    return 1;
}
static void help(void) {
    puts("Magic Castle - C / raylib\ncastle_game [--data dir] [--assets dir] [--save-dir dir]\n"
         "  --smoke-test [--days 20] [--seed 42]  C runtime test without a window\n"
         "  --stress | --stress-probe              150 villagers + 40 enemies\n"
         "  --lighting-review [--crowd n] [--review-phase 0-3] [--review-view 0-7] [--review-sun az el]\n"
         "                    [--review-zoom 1-6] [--review-clean]\n"
         "                                         조명 검수 장면 (저장하지 않음)\n"
         "  --screenshot file.png [--screen "
         "title|tutorial|tutorial_build|game|villagers|uniforms|farms|population|rotations|connections|"
         "research|rank|market|load]\n"
         "  --frames N --autoplay --no-audio --width 1440 --height 900\n"
         u8"  --gpu-transitions N                  GPU 화면·자원 반복 검사\n"
         "  --difficulty peaceful|normal|harsh (or 0|1|2)");
}
static int capture_scene_needs_town(const char *name) {
    static const char *const scenes[] = {"clinic",   "lighthouse",        "smallboat",
                                         "windmill", "irrigated",         "largeboat",
                                         "roads",    "rank_requirements", "alerts"};
    for (size_t i = 0; i < sizeof(scenes) / sizeof(scenes[0]); i++)
        if (!strcmp(name, scenes[i]))
            return 1;
    return 0;
}
static int capture_scene_known(const char *name) {
    static const char *const scenes[] = {"merchant",          "clinic",    "lighthouse", "smallboat",
                                         "windmill",          "irrigated", "fishing",    "largeboat",
                                         "household",         "forecast",  "roads",      "facilities",
                                         "rank_requirements", "alerts",    "cards_high", "gates"};
    for (size_t i = 0; i < sizeof(scenes) / sizeof(scenes[0]); i++)
        if (!strcmp(name, scenes[i]))
            return 1;
    return 0;
}
static int options(int argc, char **argv, Options *o) {
    int i;
    memset(o, 0, sizeof(*o));
    o->seed = 42;
    o->difficulty = 1;
    o->days = 20;
    o->width = 1440;
    o->height = 900;
    o->screen = "title";
    o->wildlife = 360;
    o->crowd = 240;
    o->review_azimuth = 200;
    o->review_elevation = 42;
    for (i = 1; i < argc; i++) {
        const char *arg = argv[i];
        if (!strcmp(arg, "--smoke-test") || !strcmp(arg, "--headless")) {
            o->smoke = 1;
            continue;
        }
        if (!strcmp(arg, "--stress")) {
            o->stress = 1;
            continue;
        }
        if (!strcmp(arg, "--stress-probe")) {
            o->stress = 1;
            o->probe = 1;
            o->frames = 720;
            continue;
        }
        if (!strcmp(arg, "--no-audio")) {
            o->no_audio = 1;
            continue;
        }
        if (!strcmp(arg, "--autoplay")) {
            o->autoplay = 1;
            continue;
        }
        if (!strcmp(arg, "--lighting-review")) {
            o->review = 1;
            continue;
        }
        if (!strcmp(arg, "--review-clean")) {
            o->review_clean = 1;
            continue;
        }
        if (!strcmp(arg, "--review-sun")) {
            if (i + 2 >= argc || !number(argv[i + 1], 0, 359, &o->review_azimuth) ||
                !number(argv[i + 2], 8, 85, &o->review_elevation))
                return 0;
            i += 2;
            continue;
        }
        if (i + 1 >= argc)
            return 0;
        if (!strcmp(arg, "--data"))
            o->data = argv[++i];
        else if (!strcmp(arg, "--assets"))
            o->assets = argv[++i];
        else if (!strcmp(arg, "--save-dir"))
            o->save_dir = argv[++i];
        else if (!strcmp(arg, "--screenshot"))
            o->screenshot = argv[++i];
        else if (!strcmp(arg, "--screen"))
            o->screen = argv[++i];
        else if (!strcmp(arg, "--seed")) {
            if (!number(argv[++i], 0, INT_MAX, &o->seed))
                return 0;
        } else if (!strcmp(arg, "--days")) {
            if (!number(argv[++i], 1, 10000, &o->days))
                return 0;
        } else if (!strcmp(arg, "--frames")) {
            if (!number(argv[++i], 1, 1000000, &o->frames))
                return 0;
        } else if (!strcmp(arg, "--gpu-transitions")) {
            if (!number(argv[++i], 1, 1000, &o->gpu_transitions))
                return 0;
        } else if (!strcmp(arg, "--crowd")) {
            if (!number(argv[++i], 0, 300, &o->crowd))
                return 0;
        } else if (!strcmp(arg, "--review-zoom")) {
            if (!number(argv[++i], 1, 6, &o->review_zoom))
                return 0;
        } else if (!strcmp(arg, "--review-phase")) {
            if (!number(argv[++i], 0, 3, &o->review_phase))
                return 0;
        } else if (!strcmp(arg, "--review-view")) {
            if (!number(argv[++i], 0, 7, &o->review_view))
                return 0;
        } else if (!strcmp(arg, "--stress-animals")) {
            if (!number(argv[++i], 360, 420, &o->wildlife) || (o->wildlife != 360 && o->wildlife != 420))
                return 0;
        } else if (!strcmp(arg, "--width")) {
            if (!number(argv[++i], 800, 3840, &o->width))
                return 0;
        } else if (!strcmp(arg, "--height")) {
            if (!number(argv[++i], 576, 2160, &o->height))
                return 0;
        } else if (!strcmp(arg, "--difficulty")) {
            const char *d = argv[++i];
            if (!strcmp(d, "peaceful"))
                o->difficulty = 0;
            else if (!strcmp(d, "normal"))
                o->difficulty = 1;
            else if (!strcmp(d, "harsh"))
                o->difficulty = 2;
            else if (!number(d, 0, 2, &o->difficulty))
                return 0;
        } else
            return 0;
    }
    if (o->gpu_transitions) {
        if (o->smoke || o->stress || o->screenshot || o->frames)
            return 0;
        o->frames = o->gpu_transitions * 24;
    }
    if (!capture_scene_known(o->screen) && strcmp(o->screen, "title") && strcmp(o->screen, "tutorial") &&
        strcmp(o->screen, "tutorial_build") && strcmp(o->screen, "game") && strcmp(o->screen, "villagers") &&
        strcmp(o->screen, "population") && strcmp(o->screen, "rotations") &&
        strcmp(o->screen, "connections") && strcmp(o->screen, "research") && strcmp(o->screen, "rank") &&
        strcmp(o->screen, "market") && strcmp(o->screen, "load") && strcmp(o->screen, "uniforms") &&
        strcmp(o->screen, "farms") && strcmp(o->screen, "raid") && strcmp(o->screen, "alerts") &&
        strcmp(o->screen, "tower") && strcmp(o->screen, "assign") && strcmp(o->screen, "effects") &&
        strcmp(o->screen, "selection") && strcmp(o->screen, "demolition") && strcmp(o->screen, "workforce") &&
        strcmp(o->screen, "trade") && strcmp(o->screen, "skills"))
        return 0;
    return 1;
}
void game_app_notice(GameApp *a, const char *text) {
    GameNotice *n;
    size_t length = strlen(text);
    if (a->notice_count == 8) {
        memmove(a->notices, a->notices + 1, 7 * sizeof(*a->notices));
        a->notice_count--;
    }
    n = &a->notices[a->notice_count++];
    memset(n, 0, sizeof(*n));
    if (length >= sizeof(n->text)) {
        length = sizeof(n->text) - 1;
        while (length > 0 && ((unsigned char)text[length] & 0xC0) == 0x80)
            length--;
    }
    memcpy(n->text, text, length);
}
void game_app_result(GameApp *a, int result, int reason, int placement) {
    const char *message = result == CORE_ERR_REJECTED
                              ? (placement ? fmt_place_result(reason) : fmt_deny_reason(reason))
                              : fmt_result_message(result);
    (void)snprintf(a->message, sizeof(a->message), "%s", message);
    if (result < 0)
        game_sound(&a->assets, SND_DENIED, a->ui.volume);
}
void game_app_open(GameApp *a, GamePanel panel) {
    if (a->panel == PANEL_NONE)
        a->saved_speed = a->speed;
    if (a->panel_depth < 8)
        a->panel_stack[a->panel_depth++] = a->panel;
    a->return_panel = a->panel;
    a->panel = panel;
    a->speed = 0;
    a->scroll = 0;
    a->ui.focus = 0;
    a->dragging = 0;
}
void game_app_close(GameApp *a) {
    if (a->panel == PANEL_TITLE)
        return;
    if (a->panel == PANEL_SETTINGS && !a->automated && !game_settings_save(&a->settings, a->settings_path))
        game_app_notice(a, u8"설정 파일을 저장하지 못했습니다.");
    a->panel =
        a->panel_depth > 0 ? a->panel_stack[--a->panel_depth] : (a->in_game ? PANEL_NONE : PANEL_TITLE);
    a->return_panel = a->in_game ? PANEL_NONE : PANEL_TITLE;
    if (a->panel == PANEL_NONE)
        a->speed = a->saved_speed;
    a->scroll = 0;
    a->ui.focus = 0;
}
static void enter_game(GameApp *a) {
    game_gpu_new_world();
    game_main_menu_enter(a);
    a->in_game = 1;
    a->panel = PANEL_NONE;
    a->return_panel = PANEL_NONE;
    a->panel_depth = 0;
    a->speed = 1;
    game_select(a, 0, 0);
    a->raid_alert_id = 0;
    a->build_type = -1;
    a->demolish = 0;
    a->dragging = 0;
    a->inspector_scroll = 0;
    a->shown_outcome = a->session.summary.outcome == CORE_OUTCOME_WON ? CORE_OUTCOME_WON : CORE_OUTCOME_NONE;
    a->camera.target = (Vector2){a->session.summary.keep_x * 16.0f, a->session.summary.keep_y * 16.0f};
    a->camera.zoom = 3;
    a->accumulator = 0;
    a->autosave_day = a->session.summary.day;
    a->autosave_pending = 0;
    a->notice_count = 0;
}
/* 창과 같은 진입점을 반복해 통과하되 검사 저장은 별도 폴더에만 기록합니다. */
static int gpu_transition(GameApp *a, int stage) {
    int okay = 1;
    switch (stage) {
    case 0:
        okay = game_app_start(a);
        if (okay)
            okay = game_app_save(a, 0) == CORE_OK && game_app_save(a, 1) == CORE_OK;
        break;
    case 2:
        okay = game_tutorial_start(a);
        break;
    case 4: {
        int slot = game_app_latest(a);
        okay = slot >= 0 && game_app_load(a, slot, 0);
        break;
    }
    case 6:
        okay = game_app_load(a, 1, 0);
        break;
    default:
        a->in_game = 0;
        a->panel = PANEL_TITLE;
        a->speed = 0;
        break;
    }
    a->speed = 0;
    return okay;
}
int game_app_start(GameApp *a) {
    int seed = (int)((unsigned long)time(NULL) & 0x7FFFFFFFul), rc;
    if (a->seed_text[0] && !number(a->seed_text, 0, INT_MAX, &seed)) {
        game_app_notice(a, u8"지도 시드는 0~2147483647의 정수여야 합니다.");
        return 0;
    }
    rc = game_session_new(&a->session, a->castle_name, seed, a->difficulty);
    if (rc != CORE_OK) {
        game_app_result(a, rc, 0, 0);
        return 0;
    }
    enter_game(a);
    game_session_events(&a->session);
    if (!a->automated && !a->stress)
        (void)game_app_save(a, 0);
    return 1;
}
void game_app_slot_path(GameApp *a, int slot, int backup, char path[GAME_PATH_BYTES]) {
    char name[48];
    if (slot == 0)
        (void)snprintf(name, sizeof(name), "autosave.sav%s", backup ? ".bak" : "");
    else
        (void)snprintf(name, sizeof(name), "slot%d.sav%s", slot, backup ? ".bak" : "");
    if (!game_join_path(path, GAME_PATH_BYTES, a->save_dir, name))
        path[0] = '\0';
}
int game_app_save(GameApp *a, int slot) {
    /* 조명 검수 장면은 사용자 저장 슬롯에 기록하지 않습니다. */
    if (a->review)
        return CORE_ERR_REJECTED;
    char path[GAME_PATH_BYTES];
    int rc;
    if (!a->in_game || a->stress || a->session.summary.outcome == CORE_OUTCOME_LOST)
        return CORE_IGNORED;
    if (!game_make_dirs(a->save_dir)) {
        game_app_result(a, CORE_ERR_FILE_IO, 0, 0);
        return CORE_ERR_FILE_IO;
    }
    game_app_slot_path(a, slot, 0, path);
    rc = core_save(a->session.world, path);
    if (slot == 0 && rc == CORE_ERR_REJECTED)
        a->autosave_pending = 1;
    if (rc == CORE_OK) {
        if (slot == 0)
            a->autosave_pending = 0;
        a->save_count++;
        (void)snprintf(a->message, sizeof(a->message), u8"%s 저장했습니다.",
                       slot ? u8"슬롯에" : u8"자동으로");
    } else
        game_app_result(a, rc, 0, 0);
    return rc;
}

int game_app_load(GameApp *a, int slot, int backup) {
    char path[GAME_PATH_BYTES];
    int rc, line = 0;
    game_app_slot_path(a, slot, backup, path);
    rc = game_session_load(&a->session, path, &line);
    if (rc == CORE_OK) {
        enter_game(a);
        game_app_notice(a, u8"저장된 성채를 불러왔습니다.");
        return 1;
    }
    if (!backup && (rc == CORE_ERR_FORMAT || rc == CORE_ERR_VERSION || rc == CORE_ERR_FILE_OPEN)) {
        CoreSaveInfoView info;
        game_app_slot_path(a, slot, 1, path);
        if (core_peek_save(path, a->session.defs, &info) == CORE_OK) {
            a->backup_slot = slot;
            a->confirm_action = 3;
            (void)snprintf(
                a->confirm_text, sizeof(a->confirm_text),
                u8"저장 파일을 읽지 못했습니다 (%s, %d번째 줄).\n이전 저장인 예비 파일(.bak)로 불러올까요?",
                fmt_result_message(rc), line);
            game_app_open(a, PANEL_CONFIRM);
            return 0;
        }
    }
    (void)snprintf(a->message, sizeof(a->message), u8"불러오기 실패: %s (%d번째 줄)", fmt_result_message(rc),
                   line);
    return 0;
}
int game_app_latest(GameApp *a) {
    int slot, best = -1, best_tick = -1;
    unsigned long long newest = 0;
    for (slot = 0; slot <= 3; slot++) {
        char path[GAME_PATH_BYTES];
        CoreSaveInfoView info;
        unsigned long long modified;
        game_app_slot_path(a, slot, 0, path);
        modified = game_file_modified(path);
        if (core_peek_save(path, a->session.defs, &info) != CORE_OK) {
            game_app_slot_path(a, slot, 1, path);
            if (core_peek_save(path, a->session.defs, &info) != CORE_OK)
                continue;
            if (!modified)
                modified = game_file_modified(path);
        }
        if (best < 0 || modified > newest || (modified == newest && info.tick > best_tick)) {
            best = slot;
            newest = modified;
            best_tick = info.tick;
        }
    }
    return best;
}
static void event(void *context, const CoreEventView *e) {
    GameApp *a = (GameApp *)context;
    char text[512];
    int sound = -1;
    if (!a->in_game)
        return;
    switch (e->type) {
    case CORE_EVT_PRODUCED:
    case CORE_EVT_DAY_STARTED:
    case CORE_EVT_NIGHT_FELL:
    case CORE_EVT_DAWN:
        return;
    case CORE_EVT_THREAT_NEAR:
    case CORE_EVT_RAID_STARTED:
        sound = SND_ALARM;
        break;
    case CORE_EVT_BUILDING_COMPLETED:
        sound = SND_DONE;
        break;
    case CORE_EVT_RESEARCH_DONE:
    case CORE_EVT_RANK_UP:
    case CORE_EVT_VICTORY:
    case CORE_EVT_TUTORIAL_DONE:
        sound = SND_FANFARE;
        break;
    default:
        break;
    }
    if ((e->type == CORE_EVT_RANK_UP || e->type == CORE_EVT_RANK_READY ||
         e->type == CORE_EVT_RESEARCH_STARTED || e->type == CORE_EVT_RESEARCH_DONE ||
         e->type == CORE_EVT_TUTORIAL_STEP || e->type == CORE_EVT_TUTORIAL_DONE))
        text[0] = '\0';
    else
        fmt_event(e, text, sizeof(text));
    {
        if (e->type == CORE_EVT_RAID_STARTED) {
            if (!a->automated && !a->raid_alert_id) {
                a->raid_alert_id = e->a;
                a->raid_alert_speed = a->speed;
                a->raid_scroll = 0;
                a->speed = 0;
            }
            static const char *const factions[] = {u8"도적", u8"마물", u8"언데드",
                                                   u8"숲",   u8"북방", u8"광신도"};
            (void)snprintf(text, sizeof(text),
                           u8"경보! %s %d명이 발견되었습니다. 가까운 보호구역으로 대피합니다.",
                           e->b >= 1 && e->b <= 6 ? factions[e->b - 1] : u8"습격군", e->c);
        }
        if (e->type == CORE_EVT_RANK_UP)
            (void)snprintf(text, sizeof(text), u8"영지가 %s(으)로 승급했습니다.",
                           core_expansion_rank_name(e->a));
        else if (e->type == CORE_EVT_RANK_READY)
            (void)snprintf(text, sizeof(text), u8"%s 승급 조건을 확인하세요.",
                           core_expansion_rank_name(e->a));
        else if (e->type == CORE_EVT_RESEARCH_STARTED || e->type == CORE_EVT_RESEARCH_DONE)
            (void)snprintf(text, sizeof(text), u8"%s 연구를 %s.", core_expansion_research_name(e->a),
                           e->type == CORE_EVT_RESEARCH_DONE ? u8"완료했습니다" : u8"시작했습니다");
        else if (e->type == CORE_EVT_TUTORIAL_STEP)
            (void)snprintf(text, sizeof(text), u8"기초 목표 완료: %s", core_expansion_goal_name(e->a));
        else if (e->type == CORE_EVT_TUTORIAL_DONE)
            (void)snprintf(text, sizeof(text), u8"기초 목표를 모두 마쳤습니다. 다음 등급에 도전하세요.");
    }
    if (text[0]) {
        game_app_notice(a, text);
        if (e->type == CORE_EVT_BUILDING_COMPLETED || e->type == CORE_EVT_BUILDING_PLACED ||
            e->type == CORE_EVT_BUILDING_RUINED) {
            GameNotice *n = &a->notices[a->notice_count - 1];
            n->x = e->c;
            n->y = e->d;
            n->has_position = 1;
        }
    }
    if (sound >= 0)
        game_sound(&a->assets, sound, a->settings.sfx_volume);
}
static int smoke(GameApp *a, const Options *o) {
    int day = 0, rc = CORE_OK, target = o->days * 144, i;
    uint64_t before = 0, after = 0;
    char path[GAME_PATH_BYTES];
    unsigned long arrivals, shots, kills;
    uint8_t cost_before[256 * 256], cost_after[256 * 256];
    a->in_game = 1;
    rc = game_session_new(&a->session, u8"자동 검사 성채", o->seed, o->difficulty);
    if (rc != CORE_OK)
        return 1;
    for (i = 0; i < target; i++) {
        if (day != a->session.summary.day) {
            day = a->session.summary.day;
            (void)sim_bot_daily(a->session.world, "reasonable");
        }
        if (a->session.summary.minute_of_day == 720)
            sim_expansion_bot_trade(a->session.world);
        rc = game_session_step(&a->session, 10);
        if (rc != CORE_OK || core_validate(a->session.world) != 0 ||
            a->session.summary.outcome == CORE_OUTCOME_LOST) {
            fprintf(stderr, "SMOKE FAILED: day=%d result=%d population=%d\n", a->session.summary.day, rc,
                    a->session.summary.population);
            return 1;
        }
        if (a->autosave_day != a->session.summary.day) {
            a->autosave_day = a->session.summary.day;
            (void)game_app_save(a, 0);
        }
        if (a->autosave_pending && !a->session.summary.raid_id)
            (void)game_app_save(a, 0);
    }
    for (i = 0; a->session.summary.raid_id && i < 150; i++)
        if (game_session_step(&a->session, 10) != CORE_OK)
            return 1;
    if (!game_make_dirs(a->save_dir) || !game_join_path(path, sizeof(path), a->save_dir, "roundtrip.sav"))
        return 1;
    arrivals = a->session.arrivals;
    shots = a->session.shots;
    kills = a->session.kills;
    (void)core_copy_cost_grid(a->session.world, cost_before, 256 * 256, NULL);
    rc = core_state_hash(a->session.world, &before);
    if (rc == CORE_OK)
        rc = core_save(a->session.world, path);
    if (rc != CORE_OK) {
        fprintf(stderr, "SMOKE SAVE FAILED: result=%d validate=%d\n", rc, core_validate(a->session.world));
        return 1;
    }
    int load_line = 0;
    rc = game_session_load(&a->session, path, &load_line);
    if (rc != CORE_OK) {
        fprintf(stderr, "SMOKE LOAD FAILED: result=%d line=%d\n", rc, load_line);
        return 1;
    }
    rc = core_state_hash(a->session.world, &after);
    if (rc != CORE_OK || before != after || core_validate(a->session.world) != 0) {
        (void)core_copy_cost_grid(a->session.world, cost_after, 256 * 256, NULL);
        int mismatches = 0;
        for (int p = 0; p < a->session.cells; p++)
            if (cost_before[p] != cost_after[p]) {
                if (mismatches < 8)
                    fprintf(stderr, "cost mismatch %d,%d: %d/%d\n", p % a->session.summary.map_width,
                            p / a->session.summary.map_width, cost_before[p], cost_after[p]);
                mismatches++;
            }
        fprintf(stderr, "grid mismatches=%d\n", mismatches);
        fprintf(stderr, "SMOKE ROUNDTRIP FAILED: result=%d before=%llu after=%llu validate=%d\n", rc,
                (unsigned long long)before, (unsigned long long)after, core_validate(a->session.world));
        if (game_join_path(path, sizeof(path), a->save_dir, "roundtrip_loaded.sav"))
            (void)core_save(a->session.world, path);
        return 1;
    }
    printf("SMOKE PASS: days=%d population=%d rank=%d research=%d arrivals=%lu shots=%lu kills=%lu saves=%d "
           "hash=%llu\n",
           o->days, a->session.summary.population, a->session.summary.rank, a->session.summary.research_done,
           arrivals, shots, kills, a->save_count, (unsigned long long)after);
    return 0;
}
static Building *screenshot_facility(World *w, int type, int x, int y) {
    const ExpansionBuildingSpec *spec = expansion_spec(type);
    Building *b = entity_add_building(w, type);
    if (!b)
        return NULL;
    b->x = x;
    b->y = y;
    b->w = spec->w;
    b->h = spec->h;
    b->hp = spec->hp;
    b->state = CORE_BSTATE_ACTIVE;
    b->door_x = x + spec->w / 2;
    b->door_y = y + spec->h;
    b->auto_assign = 0;
    for (int yy = y - 1; yy <= y + b->h; yy++)
        for (int xx = x - 1; xx <= x + b->w; xx++) {
            int p = grid_index(&w->grid, xx, yy);
            w->grid.terrain[p] = CORE_TERRAIN_GRASS;
            w->expansion->tiles[p].tree_kind = 0;
            w->expansion->tiles[p].tree_logs_milli = 0;
            w->expansion->tiles[p].rock_variant = 0;
            w->expansion->tiles[p].explored = 1;
        }
    return b;
}
static void game_capture_scenario(GameApp *a, const Options *o) {
    World *w = a->session.world;
    w->clock.tick = 54;
    w->rank = 5;
    for (int r = 0; r < CORE_RES_COUNT; r++)
        w->bank.amount[r] = r == CORE_RES_GOLD ? 2000 : r == CORE_RES_MANA ? 40 : 80;
    if (!strcmp(o->screen, "merchant")) {
        w->rank = 2;
        w->expansion->wandering.next_target_tick = w->clock.tick + 1;
        for (int tick = 0; tick < 400; tick++) {
            expansion_merchant_tick(w);
            if (w->expansion->wandering.state == CORE_MERCHANT_TRADING)
                break;
            w->clock.tick++;
        }
        game_session_refresh(&a->session);
        game_select(a, 5, w->expansion->wandering.visit_id);
    } else {
        int type = !strcmp(o->screen, "clinic")       ? CORE_BLD_CLINIC
                   : !strcmp(o->screen, "lighthouse") ? CORE_BLD_LIGHTHOUSE
                   : !strcmp(o->screen, "smallboat")  ? CORE_BLD_FISHING_SPOT
                   : !strcmp(o->screen, "windmill")   ? CORE_BLD_WINDMILL
                   : !strcmp(o->screen, "irrigated")  ? CORE_BLD_IRRIGATED_FARM
                   : (!strcmp(o->screen, "fishing") || !strcmp(o->screen, "largeboat")) ? CORE_BLD_PORT
                   : !strcmp(o->screen, "household")                                    ? CORE_BLD_HOUSE
                   : !strcmp(o->screen, "forecast")                                     ? CORE_BLD_OBSERVATORY
                                                                                        : CORE_BLD_CLAY_PIT;
        Building *b = screenshot_facility(w, type, w->keep_x - 5, w->keep_y + 5);
        if (!b)
            return;
        b->operating = 1;
        if (type == CORE_BLD_PORT || type == CORE_BLD_FISHING_SPOT || type == CORE_BLD_LIGHTHOUSE) {
            for (int y = b->y - 2; y < b->y + 12; y++)
                for (int x = b->x + b->w; x < b->x + b->w + 14; x++) {
                    int p = grid_index(&w->grid, x, y);
                    w->grid.terrain[p] = CORE_TERRAIN_WATER;
                    w->expansion->tiles[p].biome = CORE_BIOME_LAKE;
                    w->expansion->tiles[p].tree_kind = 0;
                    w->expansion->tiles[p].tree_logs_milli = 0;
                    w->expansion->tiles[p].rock_variant = 0;
                    w->expansion->tiles[p].explored = 1;
                }
            expansion_water_bodies(w);
        }
        expansion_rebuild(w);
        for (int i = 0; i < 4; i++) {
            Villager *v = w->villagers[i];
            v->needs.hunger = v->needs.rest = 100;
            v->health = 100;
            v->happiness = 95;
            v->meal_day = exp_day(w);
            v->current_x_milli = b->door_x * 1000 + 500;
            v->current_y_milli = b->door_y * 1000 + 500;
            if (type == CORE_BLD_HOUSE) {
                entity_unlink_resident(v);
                entity_link_resident(b, v, 4);
            } else if (i < expansion_spec(type)->workers) {
                entity_unlink_worker(v);
                entity_link_worker(b, v, 4);
                v->target = b;
                v->at_target = 1;
                v->state = CORE_VS_WORKING;
                v->inside_building = b->id;
                if (type == CORE_BLD_OBSERVATORY || type == CORE_BLD_CLINIC)
                    v->stats.intellect = 8;
            }
        }
        if (type == CORE_BLD_PORT || type == CORE_BLD_FISHING_SPOT) {
            expansion_boats_tick(w);
            for (int t = 0; t < 8; t++) {
                w->clock.tick++;
                expansion_boats_tick(w);
            }
        }
        if (capture_scene_needs_town(o->screen)) {
            if (type == CORE_BLD_LIGHTHOUSE)
                w->clock.tick = 120;
            for (int boat = 0; boat < w->expansion->boat_count; boat++) {
                w->expansion->boats[boat].fishing_phase = 3;
                w->expansion->boats[boat].phase_milli = 1400;
            }
            a->session.visual_time = .2f;
            if (!strcmp(o->screen, "roads")) {
                for (int kind = 0; kind < 3; kind++)
                    for (int dy = -2; dy <= 2; dy++)
                        for (int dx = -2; dx <= 2; dx++) {
                            int x = w->keep_x + kind * 7 - 7 + dx, y = w->keep_y + 7 + dy,
                                p = grid_index(&w->grid, x, y);
                            w->grid.terrain[p] = CORE_TERRAIN_GRASS;
                            w->expansion->tiles[p].tree_kind = 0;
                            w->expansion->tiles[p].tree_logs_milli = 0;
                            w->expansion->tiles[p].rock_variant = 0;
                            w->expansion->tiles[p].explored = 1;
                            w->grid.road[p] = (dx == 0 || dy == 0 || (dx >= 0 && dy >= 0))
                                                  ? (uint8_t)(kind == 2 ? 4 : kind + 1)
                                                  : 0;
                        }
                expansion_rebuild(w);
            }
        }
        if (type == CORE_BLD_OBSERVATORY)
            expansion_forecast_refresh(w, b);
        expansion_ensure_building_serials(w);
        expansion_visibility(w);
        game_session_refresh(&a->session);
        game_select(a, 1, b->id);
        if (!strcmp(o->screen, "facilities")) {
            const int types[3] = {CORE_BLD_WATER_TOWER, CORE_BLD_OBSERVATORY, CORE_BLD_ART_MUSEUM};
            for (int i = 0; i < 3; i++)
                screenshot_facility(w, types[i], w->keep_x + (i % 2) * 7 - 6, w->keep_y - (i / 2) * 7 - 8);
            expansion_rebuild(w);
            game_session_refresh(&a->session);
            a->category = CORE_CAT_MINING;
        }
        a->camera.target = (Vector2){(b->x + b->w / 2.0f) * 16, (b->y + b->h / 2.0f) * 16};
    }
    if (!strcmp(o->screen, "facilities")) {
        a->camera.zoom = 1.25f;
        a->camera.target = (Vector2){w->keep_x * 16.0f, (w->keep_y - 2) * 16.0f};
    }
    if (capture_scene_needs_town(o->screen)) {
        a->camera.zoom = 3;
        if (!strcmp(o->screen, "rank_requirements")) {
            w->rank = 1;
            game_session_refresh(&a->session);
            a->panel = PANEL_RANK;
        }
        if (!strcmp(o->screen, "roads")) {
            a->selected_building = 0;
            a->selection_kind = 0;
            a->category = expansion_spec(CORE_BLD_PAVED_ROAD)->category;
            a->camera.target = (Vector2){w->keep_x * 16.0f, (w->keep_y + 6) * 16.0f};
        }
    }
    if (!strcmp(o->screen, "alerts")) {
        game_app_notice(a, u8"겨울 대비 · 식량8일분을 비축하세요.");
        game_app_notice(a, u8"역병 발생 · 의원의 의료진과 약초를 확인하세요.");
        game_app_notice(a, u8"상인 도착 · 영주관 근처에서 거래할 수 있습니다.");
    }
    a->speed = 0;
    if (!strcmp(o->screen, "cards_high"))
        a->build_scroll = 3;
    if (!strcmp(o->screen, "gates")) {
        a->category = CORE_CAT_DEFENSE;
        a->build_scroll = 4;
    }
    a->session.wandering_previous = a->session.wandering;
    a->session.boat_previous_count = 0;
}
static void screenshot_screen(GameApp *a, const Options *o) {
    if (strcmp(o->screen, "demolition"))
        SetMousePosition(GetScreenWidth() - 3, GetScreenHeight() - 3);
    if (!strcmp(o->screen, "title")) {
        a->panel = PANEL_TITLE;
        a->in_game = 0;
        return;
    }
    if (!strcmp(o->screen, "tutorial") || !strcmp(o->screen, "tutorial_build")) {
        /* 메인 화면의 "튜토리얼" 단추와 같은 길. tutorial_build 는 첫 건설 단계(텃밭)까지 넘김 */
        (void)game_tutorial_start(a);
        for (int step = 0; !strcmp(o->screen, "tutorial_build") && step < 3; step++)
            (void)core_guide_next(a->session.world);
        return;
    }
    (void)game_session_new(&a->session, u8"바람언덕", o->seed, o->difficulty);
    enter_game(a);
    if (capture_scene_known(o->screen)) {
        game_capture_scenario(a, o);
        return;
    }
    /* 명시적인 촬영 모드의 재현 자료이며 정상 자동플레이 평가에는 사용하지 않습니다. */
    if (!strcmp(o->screen, "effects")) {
        a->speed = 0;
        return;
    }
    if (!strcmp(o->screen, "selection") || !strcmp(o->screen, "demolition") ||
        !strcmp(o->screen, "workforce") || !strcmp(o->screen, "trade") || !strcmp(o->screen, "skills")) {
        World *w = a->session.world;
        int trade = !strcmp(o->screen, "trade") || !strcmp(o->screen, "skills");
        int type = trade ? CORE_BLD_MARKET : CORE_BLD_FARM;
        Building *b = entity_add_building(w, type);
        if (b) {
            const ExpansionBuildingSpec *spec = expansion_spec(type);
            b->x = w->keep_x - 6;
            b->y = w->keep_y + 2;
            b->w = spec->w;
            b->h = spec->h;
            b->door_x = b->x + b->w / 2;
            b->door_y = b->y + b->h;
            b->state = CORE_BSTATE_ACTIVE;
            b->hp = w->defs.buildings[type].max_hp;
            expansion_ensure_building_serials(w);
            expansion_rebuild(w);
            Villager *v = w->villagers[0];
            (void)core_assign_worker(w, v->id, b->id);
            v->at_target = 1;
            v->target = b;
            v->target_x = b->door_x;
            v->target_y = b->door_y;
            v->state = CORE_VS_WORKING;
            v->meal_day = 1;
            v->current_x_milli = b->door_x * 1000 + 500;
            v->current_y_milli = b->door_y * 1000 + 500;
            v->skill_minutes[spec->field] = 40 * 540;
            w->clock.tick = 60;
            w->bank.amount[CORE_RES_FOOD] = 1;
            w->bank.amount[CORE_RES_GOLD] = 0;
            w->expansion->trade_stock[CORE_RES_FOOD] = 0;
            expansion_visibility(w);
            (void)game_session_refresh(&a->session);
            game_select(a, 1, b->id);
            if (!strcmp(o->screen, "workforce"))
                a->panel = PANEL_ASSIGN;
            if (trade)
                a->panel = PANEL_MARKET;
            if (!strcmp(o->screen, "skills")) {
                a->panel = PANEL_NONE;
                game_select(a, 2, v->id);
                a->skill_tab = 9;
                a->inspector_scroll = 350;
            }
            if (!strcmp(o->screen, "demolition")) {
                a->demolish = 1;
                SetMousePosition(260, 340);
            }
        }
        a->speed = 0;
        return;
    }
    if (!strcmp(o->screen, "raid")) {
        World *w = a->session.world;
        (void)threat_debug_raid(w, CORE_THREAT_BANDITS, CORE_DIR_EAST, 5);
        Raid *r = &w->threat.raid;
        r->type = 6;
        memset(r->initial_types, 0, sizeof(r->initial_types));
        for (int n = 0; n < 5; n++) {
            r->enemies[n].type = 23 + n;
            r->initial_types[23 + n] = 1;
        }
        expansion_raid_discover(w);
        (void)game_session_refresh(&a->session);
        if (a->session.enemy_count)
            a->session.enemies[0].actor.position = (GamePoint){w->keep_x + 1.5f, w->keep_y + 4.5f};
        a->raid_alert_id = r->id;
        a->raid_alert_speed = 0;
        a->speed = 0;
        return;
    }
    if (!strcmp(o->screen, "alerts")) {
        World *w = a->session.world;
        w->clock.tick = 36 * 144 + 66;
        w->nature.event = CORE_NATURE_COLD_SNAP;
        w->nature.event_days = 3;
        w->expansion->plague_days = 4;
        w->expansion->economic_event = CORE_NATURE_TRADE_BOOM;
        w->expansion->economic_days = 2;
        (void)game_session_refresh(&a->session);
        game_app_notice(a, u8"식량 비축량이 부족합니다. 겨울을 대비해 거래와 식량 생산을 확인하세요.");
        game_app_notice(a, u8"주민이 새로운 일터에 배치되었습니다.");
        a->speed = 0;
        return;
    }
    if (!strcmp(o->screen, "tower") || !strcmp(o->screen, "assign")) {
        World *w = a->session.world;
        Building *b = entity_add_building(w, CORE_BLD_OUTPOST);
        if (b) {
            b->x = w->keep_x + 6;
            b->y = w->keep_y + 2;
            b->w = b->h = 1;
            b->door_x = b->x;
            b->door_y = b->y + 1;
            b->hp = 110;
            b->state = CORE_BSTATE_ACTIVE;
            expansion_ensure_building_serials(w);
            expansion_rebuild(w);
            (void)core_expansion_assign_shift(w, w->villagers[0]->id, b->id, 1);
            if (!strcmp(o->screen, "tower"))
                (void)core_expansion_assign_shift(w, w->villagers[1]->id, b->id, 0);
            (void)game_session_refresh(&a->session);
            game_select(a, 1, b->id);
            a->assign_shift = 0;
            if (!strcmp(o->screen, "assign"))
                a->panel = PANEL_ASSIGN;
        }
        a->speed = 0;
        return;
    }
    if (!strcmp(o->screen, "uniforms") || !strcmp(o->screen, "farms") || !strcmp(o->screen, "villagers") ||
        !strcmp(o->screen, "rotations") || !strcmp(o->screen, "connections")) {
        a->speed = 0;
        return;
    }
    {
        int day;
        int preview_days = !strcmp(o->screen, "market") ? 40 : 16;
        for (day = 0; day < preview_days; day++) {
            (void)sim_expansion_bot_daily(a->session.world);
            (void)game_session_step(&a->session, 720);
            sim_expansion_bot_trade(a->session.world);
            (void)game_session_step(&a->session, 720);
        }
        {
            int x, y, placed = 0;
            int32_t id = 0, reason = 0;
            for (y = a->session.summary.keep_y - 7; y <= a->session.summary.keep_y + 7 && !placed; y++)
                for (x = a->session.summary.keep_x - 7; x <= a->session.summary.keep_x + 7 && !placed; x++) {
                    CorePlacementView p;
                    (void)core_can_place_rotated(a->session.world, CORE_BLD_GARDEN, x, y, 0, &p);
                    if (p.result == CORE_PLACE_OK && core_place_rotated(a->session.world, CORE_BLD_GARDEN, x,
                                                                        y, 0, &id, &reason) == CORE_OK) {
                        (void)core_set_auto_assign(a->session.world, id, 0);
                        placed = 1;
                    }
                }
            (void)game_session_step(&a->session, 360);
            (void)game_session_step(&a->session,
                                    (float)((660 - a->session.summary.minute_of_day + 1440) % 1440));
        }
        (void)game_session_refresh(&a->session);
    }
    game_session_events(&a->session);
    a->notice_count = 0;
    a->message[0] = '\0';
    if (!strcmp(o->screen, "game")) {
        int b, v;
        for (b = 0; b < a->session.building_count; b++)
            if (a->session.buildings[b].type == CORE_BLD_GARDEN) {
                int id = a->session.buildings[b].id;
                (void)core_set_auto_assign(a->session.world, id, 0);
                for (v = 0; v < a->session.villager_count; v++)
                    if (a->session.villagers[v].workplace_id == id)
                        (void)core_unassign_worker(a->session.world, a->session.villagers[v].id);
                break;
            }
        (void)game_session_refresh(&a->session);
    }
    a->speed = 0;
    if (!strcmp(o->screen, "research"))
        a->panel = PANEL_RESEARCH;
    else if (!strcmp(o->screen, "population"))
        a->panel = PANEL_VILLAGERS;
    else if (!strcmp(o->screen, "rank"))
        a->panel = PANEL_RANK;
    else if (!strcmp(o->screen, "market"))
        a->panel = PANEL_MARKET;
    else if (!strcmp(o->screen, "load"))
        a->panel = PANEL_LOAD;
}
static int compare_double(const void *left, const void *right) {
    double a = *(const double *)left, b = *(const double *)right;
    return (a > b) - (a < b);
}
static int run(int argc, char **argv) {
    Options o;
    GameApp *a;
    char exe[GAME_PATH_BYTES], data[GAME_PATH_BYTES], assets[GAME_PATH_BYTES];
    int status = 0, frame = 0, last_bot_day = -1, last_trade_day = -1;
    double frame_total = 0, frame_max = 0;
    double samples[600], sim_total = 0, render_total = 0;
    int measured = 0, min_villagers = 100000, min_enemies = 100000, min_wild = 100000;
    int min_livestock = 100000, max_enemies = 0;
    if (argc > 1 && (!strcmp(argv[1], "--help") || !strcmp(argv[1], "-h"))) {
        help();
        return 0;
    }
    if (!options(argc, argv, &o)) {
        help();
        return 2;
    }
    if (!game_executable_directory(exe, sizeof(exe), argv[0]) ||
        !game_join_path(data, sizeof(data), exe, "data") ||
        !game_join_path(assets, sizeof(assets), exe, "assets"))
        return 1;
    a = (GameApp *)calloc(1, sizeof(*a));
    if (!a)
        return 1;
    a->automated = o.smoke || o.screenshot || o.frames || o.probe;
    a->stress = o.stress;
    a->review = o.review;
    a->build_type = -1;
    a->category = CORE_CAT_AGRICULTURE;
    a->difficulty = o.difficulty;
    strcpy(a->castle_name, u8"바람언덕");
    if (a->automated || o.stress)
        (void)snprintf(a->seed_text, sizeof(a->seed_text), "%d", o.seed);
    if (!game_user_directory(a->user_dir, sizeof(a->user_dir))) {
        free(a);
        return 1;
    }
    if (o.save_dir) {
        if (strlen(o.save_dir) >= sizeof(a->save_dir)) {
            free(a);
            return 2;
        }
        strcpy(a->save_dir, o.save_dir);
    } else if (!game_join_path(a->save_dir, sizeof(a->save_dir), a->automated ? exe : a->user_dir,
                               o.review            ? "lighting_review_saves"
                               : o.gpu_transitions ? "gpu_transition_saves"
                               : a->automated    ? "saves_automated"
                                                 : "saves")) {
        free(a);
        return 1;
    }
    if (!game_join_path(a->settings_path, sizeof(a->settings_path), a->user_dir, "settings.cfg")) {
        free(a);
        return 1;
    }
    status = game_session_init(&a->session, o.data ? o.data : data);
    a->session.ruleset = CORE_RULES_EXPANSION;
    if (status != CORE_OK) {
        fprintf(stderr, "C core initialization failed: %d\n", status);
        free(a);
        return 1;
    }
    a->session.on_event = event;
    a->session.event_context = a;
    game_settings_default(&a->settings);
    if (o.smoke) {
        status = smoke(a, &o);
        game_session_free(&a->session);
        free(a);
        return status;
    }
    if (!a->automated)
        (void)game_settings_load(&a->settings, a->settings_path);
    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | (o.probe ? 0 : FLAG_VSYNC_HINT));
    InitWindow(o.width, o.height, u8"마법성채 키우기 — C");
    SetWindowMinSize(800, 576);
    SetExitKey(KEY_NULL);
    SetTargetFPS(o.probe ? 0 : 60);
    int window_ready = IsWindowReady();
    int assets_ready =
        window_ready && game_assets_load(&a->assets, o.assets ? o.assets : assets, !o.no_audio);
    int graphics_ready = assets_ready && game_gpu_initialize(a, o.assets ? o.assets : assets);
    if (!graphics_ready) {
        if (!window_ready)
            fprintf(stderr, u8"게임 창을 열지 못했습니다. OpenGL 4.3 이상 그래픽 드라이버를 확인하세요.\n");
        else if (!assets_ready)
            fprintf(stderr, u8"게임 리소스를 읽지 못했습니다. --assets로 assets 폴더를 지정하세요.\n");
        else
            fprintf(
                stderr,
                u8"GPU 풍경을 초기화하지 못했습니다. OpenGL 4.3 이상 드라이버와 조명 자료를 확인하세요.\n");
        game_gpu_shutdown();
        game_assets_free(&a->assets);
        if (IsWindowReady())
            CloseWindow();
        game_session_free(&a->session);
        free(a);
        return 1;
    }
    if (a->settings.fullscreen)
        ToggleFullscreen();
    if (a->assets.audio_ready)
        SetMasterVolume(a->settings.master_volume);
    a->camera = (Camera2D){{o.width * 0.5f - 160, o.height * 0.5f}, {48 * 16, 48 * 16}, 0, 3};
    a->panel = PANEL_TITLE;
    game_main_menu_update(a, 0);
    if (o.stress || (o.frames && !o.screenshot && !o.gpu_transitions)) {
        (void)game_app_start(a);
        if (o.stress) {
            (void)core_debug_stress(a->session.world, 150, 40);
            if (o.wildlife == 420)
                (void)core_expansion_debug_wildlife(a->session.world, 420);
            (void)game_session_refresh(&a->session);
            a->camera.zoom = 2;
            for (int i = 0; i < a->session.enemy_count; i++) {
                int x = a->session.summary.keep_x - 14 + (i % 20),
                    y = a->session.summary.keep_y - 12 + (i / 20) * 2;
                while (x < a->session.summary.map_width - 1 &&
                       !a->session.cost[y * a->session.summary.map_width + x])
                    x++;
                a->session.enemies[i].actor.position = (GamePoint){x + .5f, y + .5f};
            }
        }
    }
    if (o.review) {
        if (!game_review_start(a, o.seed, o.crowd)) {
            fprintf(stderr, u8"조명 검수 장면을 만들지 못했습니다.\n");
            status = 1;
            a->quit = 1;
        }
        a->in_game = 1;
        a->panel = PANEL_NONE;
        game_gpu_review_set(o.review_phase, o.review_view, (float)o.review_azimuth, (float)o.review_elevation);
        game_review_set_clean(o.review_clean);
        if (o.review_zoom)
            a->camera.zoom = (float)o.review_zoom;
        if (o.screenshot)
            SetMousePosition(GetScreenWidth() - 3, GetScreenHeight() - 3);
    } else if (o.screenshot)
        screenshot_screen(a, &o);
    while (!a->quit) {
        double frame_started = GetTime(), sim_ms = 0, render_started = 0, render_ms = 0;
        float delta = o.probe ? 1.0f / 60 : fminf(GetFrameTime(), 0.1f);
        int i;
        unsigned long shots = a->session.shots;
        if (WindowShouldClose()) {
            if (!a->automated && !a->review)
                (void)game_app_save(a, 0);
            break;
        }
        frame++;
        if (o.gpu_transitions && (frame - 1) % 3 == 0 && !gpu_transition(a, ((frame - 1) / 3) % 8)) {
            status = 1;
            break;
        }
        a->camera.offset = (Vector2){(GetScreenWidth() - 350) * 0.5f, GetScreenHeight() * 0.5f};
        for (i = 0; i < a->notice_count; i++)
            a->notices[i].age += delta;
        if (a->panel != PANEL_NONE && a->panel != PANEL_TITLE && IsKeyPressed(KEY_ESCAPE))
            game_app_close(a);
        else
            game_world_input(a, delta);
        if (o.autoplay && a->in_game && a->session.summary.day != last_bot_day) {
            last_bot_day = a->session.summary.day;
            (void)sim_bot_daily(a->session.world, "reasonable");
        }
        if (o.autoplay && a->in_game && a->session.summary.minute_of_day >= 720 &&
            last_trade_day != a->session.summary.day) {
            last_trade_day = a->session.summary.day;
            sim_expansion_bot_trade(a->session.world);
        }
        if (o.autoplay && a->in_game && a->session.summary.rank_ready) {
            int32_t why = 0;
            (void)core_rank_up(a->session.world, &why);
        }
        if (a->review)
            game_review_update(a);
        /* 기능별 매 프레임 연결점: 입력 뒤, 시간 진행 전 */
        if (a->in_game)
            game_tutorial_update(a, delta);
        else
            game_main_menu_update(a, delta);
        if (a->in_game && a->panel == PANEL_NONE && a->speed > 0 &&
            a->session.summary.outcome != CORE_OUTCOME_LOST) {
            float minutes = delta * a->speed * 12.0f;
            double sim_started = GetTime();
            status = game_session_step(&a->session, minutes);
            sim_ms = (GetTime() - sim_started) * 1000;
            if (status != CORE_OK && status != CORE_ERR_REJECTED) {
                game_app_result(a, status, 0, 0);
                a->speed = 0;
            }
            if (!a->automated && !a->stress && !a->review && a->session.summary.day != a->autosave_day) {
                a->autosave_day = a->session.summary.day;
                (void)game_app_save(a, 0);
            }
            if (a->autosave_pending && !a->review && !a->session.summary.raid_id)
                (void)game_app_save(a, 0);
        }
        if (a->session.shots != shots)
            game_sound(&a->assets, SND_ARROW, a->settings.sfx_volume * 0.25f);
        if (a->in_game && a->session.summary.outcome && a->session.summary.outcome != a->shown_outcome &&
            !a->automated) {
            a->shown_outcome = a->session.summary.outcome;
            game_app_open(a, PANEL_OUTCOME);
        }
        ui_begin(&a->ui, &a->assets, a->settings.ui_scale == 2 ? 24.0f : 20.0f, a->settings.sfx_volume);
        render_started = GetTime();
        BeginDrawing();
        ClearBackground(UI_BG);
        if (o.screenshot && !strcmp(o.screen, "villagers"))
            game_render_villager_preview(a);
        else if (o.screenshot && !strcmp(o.screen, "uniforms"))
            game_render_uniform_preview(a);
        else if (o.screenshot && !strcmp(o.screen, "farms"))
            game_render_farm_preview(a);
        else if (o.screenshot && !strcmp(o.screen, "effects"))
            game_render_effect_preview(a);
        else if (o.screenshot && !strcmp(o.screen, "rotations"))
            game_render_rotation_preview(a);
        else if (o.screenshot && !strcmp(o.screen, "connections"))
            game_render_connection_preview(a);
        else {
            game_render_world(a);
            if (!a->review || game_review_interface_visible())
                game_render_interface(a);
            if (a->review)
                game_review_draw_help(a);
        }
        EndDrawing();
        if (game_gpu_failed()) {
            fprintf(stderr, u8"GPU 풍경 그리기에 실패하여 게임을 종료합니다.\n");
            status = 1;
            break;
        }
        render_ms = (GetTime() - render_started) * 1000;
        /* 일시정지 중의 명령도 다음 프레임 전에 스냅숏에 반영합니다. */
        if (game_session_refresh(&a->session) != CORE_OK) {
            status = 1;
            break;
        }
        game_session_events(&a->session);
        if (o.probe && frame > 120 && measured < 600) {
            double ms = (GetTime() - frame_started) * 1000;
            samples[measured++] = ms;
            frame_total += ms;
            sim_total += sim_ms;
            render_total += render_ms;
            if (ms > frame_max)
                frame_max = ms;
            if (a->session.villager_count < min_villagers)
                min_villagers = a->session.villager_count;
            int active_enemies = 0;
            for (int n = 0; n < a->session.enemy_count; n++)
                active_enemies += a->session.enemies[n].hp > 0;
            if (active_enemies < min_enemies)
                min_enemies = active_enemies;
            if (active_enemies > max_enemies)
                max_enemies = active_enemies;
            if (a->session.summary.wild_count < min_wild)
                min_wild = a->session.summary.wild_count;
            int livestock = a->session.summary.animal_count - a->session.summary.wild_count;
            if (livestock < min_livestock)
                min_livestock = livestock;
        }
        if (o.screenshot && frame == (o.review && o.frames ? o.frames : 4)) {
            status = game_screenshot(o.screenshot) ? 0 : 1;
            break;
        }
        if (o.gpu_transitions && frame % 24 == 0 && game_gpu_transition_checkpoint(frame == 24)) {
            status = 1;
            break;
        }
        if (o.frames && frame >= o.frames)
            break;
    }
    if (o.probe) {
        qsort(samples, (size_t)measured, sizeof(double), compare_double);
        double avg = measured ? frame_total / measured : 0,
               p95 = measured ? samples[(measured - 1) * 95 / 100] : 0,
               p99 = measured ? samples[(measured - 1) * 99 / 100] : 0;
        int conditions = measured == 600 && min_villagers >= 150 && min_enemies >= 40 &&
                         min_wild >= o.wildlife && min_livestock >= 60;
        int passed = conditions && (o.wildlife == 420 || (avg <= 16.67 && p95 <= 20 && p99 <= 33.3));
        printf("STRESS: warmup=120 frames=%d avg_ms=%.3f p95_ms=%.3f p99_ms=%.3f max_ms=%.3f sim_ms=%.3f "
               "render_ms=%.3f min_villagers=%d min_enemies=%d min_wild=%d min_livestock=%d max_enemies=%d "
               "mode=%s "
               "conditions=%d passed=%d\n",
               measured, avg, p95, p99, frame_max, measured ? sim_total / measured : 0,
               measured ? render_total / measured : 0, min_villagers, min_enemies, min_wild, min_livestock,
               max_enemies, o.wildlife == 420 ? "debug420" : "normal360", conditions, passed);
        if (!passed)
            status = 1;
    }
    if (!a->automated) {
        (void)game_make_dirs(a->user_dir);
        (void)game_settings_save(&a->settings, a->settings_path);
    }
    if (o.gpu_transitions)
        status |= game_gpu_validate();
    game_render_free(a);
    if (o.gpu_transitions) {
        unsigned int remaining = game_gpu_live_resources();
        printf("GPU TRANSITIONS: cycles=%d frames=%d remaining=%u errors=%d\n", o.gpu_transitions, frame,
               remaining, status != 0);
        if (remaining)
            status = 1;
    }
    game_assets_free(&a->assets);
    CloseWindow();
    game_session_free(&a->session);
    free(a);
    return status < 0 ? 1 : status;
}

int main(int argc, char **argv) {
    GameArguments args;
    int result;
    if (!game_arguments(&args, argc, argv))
        return 1;
    result = run(args.count, args.values);
    game_arguments_free(&args);
    return result;
}
