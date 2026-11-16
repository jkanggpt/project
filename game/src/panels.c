/* panels.c — 제목·설정·저장·연구·거래·주민 관리 화면을 C로 구성합니다. */
#include "app.h"
#include "format.h"
#include "platform.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static Rectangle rect(float x, float y, float w, float h) { return (Rectangle){x, y, w, h}; }
static void command(GameApp *a, int rc, int reason) { game_app_result(a, rc, reason, 0); }
static void cost_text(const CoreBuildingDefView *d, char *text, size_t size) {
    size_t used = 0;
    int i;
    text[0] = '\0';
    for (i = 0; i < CORE_RES_COUNT; i++)
        if (d->cost[i] > 0 && used < size) {
            int n = snprintf(text + used, size - used, "%s%s %d", used ? " / " : "", fmt_resource_name(i),
                             d->cost[i]);
            if (n < 0 || (size_t)n >= size - used)
                break;
            used += (size_t)n;
        }
}
static void hud(GameApp *a) {
    GameUi *u = &a->ui;
    CoreSummaryView *s = &a->session.summary;
    int i;
    char text[160], hint[512];
    float width = (float)GetScreenWidth();
    ui_box(rect(0, 0, width, 132), UI_BG);
    if (ui_button(u, rect(10, 9, 76, 36), u8"메뉴", 1, 0, u8"Esc: 일시정지 메뉴"))
        game_app_open(a, PANEL_PAUSE);
    ui_text(u, 100, 16, UI_GOLD, u8"%s · %s", s->castle_name, core_expansion_rank_name(s->rank));
    ui_small(u, 100, 48, UI_MUTED, u8"%d년 %s %d일  %02d:%02d  (시드 %d)", s->year,
             fmt_season_name(s->season), s->day_of_season, s->minute_of_day / 60, s->minute_of_day % 60,
             s->seed);
    if (ui_button(u, rect(540, 9, 46, 36), a->speed ? "||" : ">", 1, !a->speed,
                  u8"스페이스: 일시정지 / 계속"))
        a->speed = a->speed ? 0 : 1;
    for (i = 1; i <= 3; i++) {
        (void)snprintf(text, sizeof(text), "%dx", i);
        if (ui_button(u, rect(590 + (i - 1) * 47.0f, 9, 42, 36), text, 1, a->speed == i, u8"숫자 1~3: 배속"))
            a->speed = i;
    }
    {
        static const char *const labels[] = {u8"승급", u8"연구", u8"시장", u8"고용", u8"주민", u8"장부"};
        static const GamePanel panels[] = {PANEL_RANK,     PANEL_RESEARCH,  PANEL_MARKET,
                                           PANEL_MERCHANT, PANEL_VILLAGERS, PANEL_LEDGER};
        float button_w = (width - 750) / 6;
        if (button_w > 90)
            button_w = 90;
        for (i = 0; i < 6; i++)
            if (ui_button(u, rect(740 + i * button_w, 9, button_w - 5, 36), labels[i], 1, 0, NULL))
                game_app_open(a, panels[i]);
    }
    for (i = 0; i < CORE_RES_COUNT; i++) {
        Texture2D icon = a->assets.resources[i];
        float x = 10 + i % 8 * (width - 360) / 8, ry = 75 + i / 8 * 27.0f;
        if (icon.id)
            DrawTexturePro(icon, (Rectangle){0, 0, (float)icon.width, (float)icon.height},
                           rect(x, ry, 20, 20), (Vector2){0, 0}, 0, WHITE);
        ui_small(u, x + 24, ry + 1, UI_TEXT, "%s %d", fmt_resource_name(i), s->resources[i]);
        (void)snprintf(
            hint, sizeof(hint),
            u8"%s\n어제 변화 %+d / 저장 한도 %d (0은 제한 없음)\n장부에서 항목별 수입·지출을 확인합니다.",
            fmt_resource_name(i), s->resource_delta[i], s->resource_capacity[i]);
        if (CheckCollisionPointRec(GetMousePosition(), rect(x, ry, (width - 360) / 8, 25)))
            (void)snprintf(u->tooltip, sizeof(u->tooltip), "%s", hint);
    }
    ui_small(u, width - 350, 80, UI_TEXT, u8"주민 %d · 기분 %d · %s", s->population, s->average_mood,
             fmt_weather_name(s->weather));
    if (s->raid_id)
        ui_small(u, 540, 49, UI_BAD, u8"습격: %s %d명 · 저장/수리 불가", fmt_threat_name(s->raid_type),
                 s->raid_enemies_left);
    else if (s->threat_type)
        ui_small(u, 540, 49, UI_GOLD, u8"위협: %s %s · %d일 %d시간 후", fmt_threat_name(s->threat_type),
                 fmt_direction_name(s->threat_direction), s->threat_ticks_left / 144,
                 (s->threat_ticks_left % 144) / 6);
    if (s->season == CORE_SEASON_AUTUMN || s->season == CORE_SEASON_WINTER)
        ui_small(u, width - 460, 49, s->resources[CORE_RES_FOOD] < s->winter_food_need ? UI_BAD : UI_GOOD,
                 u8"겨울 식량 %d/%d · 목재 %d/%d", s->resources[CORE_RES_FOOD], s->winter_food_need,
                 s->resources[CORE_RES_WOOD], s->winter_wood_need);
}
static void build_bar(GameApp *a) {
    static const int groups[] = {1, 7, 8, 9, 10, 11, 12, 3, 4, 16, 17, 5, 13, 14, 15, 6};
    static const char *const names[] = {u8"특수",      u8"주거",   u8"자원", u8"저장·거래", u8"성벽·성문",
                                        u8"의료·위생", u8"도로",   u8"농업", u8"축산",      u8"임업",
                                        u8"광업",      u8"어업",   u8"가공", u8"문화·교육", u8"마법",
                                        u8"수렵",      u8"방어탑", u8"군사"};
    GameUi *u = &a->ui;
    float y = GetScreenHeight() - 190.0f, width = (float)GetScreenWidth(), cw = (width - 130) / 8,
          bw = (width - 20) / 8;
    int i, count = 0;
    char cost[300], tip[600];
    ui_box(rect(0, y, width, 190), UI_BG);
    for (i = 0; i < 16; i++)
        if (ui_button(u, rect(10 + i % 8 * cw, y + 6 + i / 8 * 34, cw - 4, 30), names[groups[i]], 1,
                      a->category == groups[i], NULL))
            a->category = groups[i];
    if (ui_button(u, rect(width - 114, y + 8, 104, 32), u8"철거 X", 1, a->demolish,
                  u8"도로 연결이 끊기는 철거는 거절합니다.")) {
        a->demolish = !a->demolish;
        a->build_type = -1;
    }
    if (ui_button(u, rect(width - 114, y + 44, 104, 28), u8"회전 R", 1, 0,
                  u8"선택한 건물 또는 건설 미리보기를 회전합니다.")) {
        if (a->build_type >= 0)
            a->rotation = (a->rotation + 1) % 4;
        else if (a->selected_building) {
            CoreBuildingView *b = game_building(&a->session, a->selected_building);
            int32_t reason = 0;
            if (b) {
                int rc = core_rotate_building(a->session.world, b->id, (b->rotation + 1) % 4, &reason);
                game_app_result(a, rc, reason, 1);
            }
        }
    }
    for (i = 1; i < CORE_BLD_COUNT; i++) {
        CoreBuildingDefView *d = &a->session.building_defs[i];
        CoreExpansionBuildingInfo info;
        if (d->category != a->category)
            continue;
        (void)core_expansion_building_info(i, &info);
        cost_text(d, cost, sizeof(cost));
        (void)snprintf(tip, sizeof(tip), u8"%s · %d×%d칸\n%s\n일꾼 %d · 유지비 %d금/일 · %s 해금\n%s",
                       info.name, d->w, d->h, cost, d->worker_slots, d->upkeep,
                       core_expansion_rank_name(d->required_rank), info.description);
        if (ui_button(u, rect(10 + count % 8 * bw, y + 80 + count / 8 * 38, bw - 5, 34), info.name,
                      d->available, a->build_type == i, tip)) {
            a->build_type = i;
            a->demolish = 0;
        }
        count++;
    }
    if (a->build_type >= 0) {
        CoreBuildingDefView *d = &a->session.building_defs[a->build_type];
        Vector2 p = GetScreenToWorld2D(GetMousePosition(), a->camera);
        CorePlacementView preview;
        int bworld = (a->rotation & 1) ? d->h : d->w, bhworld = (a->rotation & 1) ? d->w : d->h;
        cost_text(d, cost, sizeof(cost));
        (void)core_can_place_rotated(a->session.world, a->build_type,
                                     (int)floorf(p.x / 16) - (bworld - 1) / 2,
                                     (int)floorf(p.y / 16) - (bhworld - 1) / 2, a->rotation, &preview);
        ui_small(u, 12, y + 153, preview.result == CORE_PLACE_OK ? UI_GOOD : UI_BAD,
                 u8"%s · %d×%d · 회전 %d° · %s", core_expansion_building_name(a->build_type), bworld, bhworld,
                 a->rotation * 90, fmt_place_result(preview.result));
    } else
        ui_small(u, 12, y + 153, UI_MUTED,
                 u8"WASD 이동 · 휠 확대 · R 회전 · 스페이스 일시정지 · V 주민 · F1 도움말");
}
static void inspector(GameApp *a) {
    float x = GetScreenWidth() - 340.0f, y = 145, width = 326;
    GameUi *u = &a->ui;
    GameSession *s = &a->session;
    CoreBuildingView *b = game_building(s, a->selected_building);
    CoreVillagerView *v = game_villager(s, a->selected_villager);
    char text[1000];
    int blocked = u->input_blocked;
    Rectangle viewport = rect(x - 6, 135, 346, GetScreenHeight() - 330.0f);
    ui_box(rect(x - 6, 135, 346, GetScreenHeight() - 330.0f), UI_PANEL);
    if (CheckCollisionPointRec(GetMousePosition(), viewport)) {
        a->inspector_scroll -= (int)(GetMouseWheelMove() * 50);
        if (a->inspector_scroll < 0)
            a->inspector_scroll = 0;
        if (a->inspector_scroll > 650)
            a->inspector_scroll = 650;
    } else
        u->input_blocked = 1;
    y -= a->inspector_scroll;
    BeginScissorMode((int)viewport.x, (int)viewport.y, (int)viewport.width, (int)viewport.height);
    if (b) {
        ui_text(u, x + 8, y, UI_GOLD, "%s #%d", core_expansion_building_name(b->type), b->id);
        y += 40;
        (void)snprintf(
            text, sizeof(text),
            u8"체력 %d / %d\n일꾼 %d / %d (작업 중 %d)\n거주민 %d / %d\n생산성 %d%%\n문 앞 (%d, %d)", b->hp,
            b->max_hp, b->workers, b->worker_slots, b->working_now, b->residents, b->resident_slots,
            b->productivity, b->door_x, b->door_y);
        ui_wrap(u, rect(x + 8, y, width - 24, 205), UI_TEXT, text);
        y += 215;
        {
            CoreExpansionBuildingInfo info;
            (void)core_expansion_building_info(b->type, &info);
            ui_wrap(u, rect(x + 8, y, width - 24, 115), UI_MUTED, info.description);
            y += 120;
            ui_small(u, x + 8, y, UI_TEXT, u8"회전 %d° · 유지비 %d금/일", b->rotation * 90, b->upkeep);
            y += 32;
            if (b->type == CORE_BLD_HEALER || b->type == CORE_BLD_HOSPITAL) {
                ui_wrap(u, rect(x + 8, y, width - 24, 82), b->operating ? UI_GOOD : UI_BAD,
                        b->type == CORE_BLD_HEALER
                            ? u8"자격: 지능 4 이상 치료사 1명"
                            : u8"자격: 일꾼 2명 이상, 지능 6 이상 1명, 지능 합 10 이상");
                y += 85;
            }
            if (info.annual_yield) {
                ui_small(u, x + 8, y, UI_TEXT, u8"작기 노동 %.1f인시 · 수확 대기 %.1f",
                         b->crop_labor_milli / 60000.0, b->pending_harvest_milli / 1000.0);
                y += 32;
            }
            if (b->type == CORE_BLD_SMELTER) {
                int mode;
                for (mode = 0; mode < 2; mode++)
                    if (ui_button(u, rect(x + 8 + mode * 150, y, 145, 34),
                                  mode ? u8"금괴 모드" : u8"철괴 모드", 1, b->mode == mode,
                                  u8"하루 한 번 변경합니다."))
                        command(a, core_expansion_set_smelter(s->world, b->id, mode), 0);
                y += 42;
            }
            if (b->type == CORE_BLD_RANCH || b->type == CORE_BLD_PASTURE) {
                static const int kinds[4] = {7, 9, 8, 10};
                static const char *const labels[] = {u8"소 35금", u8"돼지 14금", u8"양 18금", u8"닭 5금"};
                int k;
                for (k = 0; k < 4; k++)
                    if (ui_button(u, rect(x + 8 + k % 2 * 150, y + k / 2 * 40, 145, 34), labels[k], 1, 0,
                                  u8"가축은 일꾼이 없어도 사료를 먹습니다."))
                        command(a, core_expansion_buy_animal(s->world, b->id, kinds[k]), 0);
                y += 88;
            }
        }
        if (b->state == CORE_BSTATE_CONSTRUCTING) {
            ui_bar(rect(x + 8, y, width - 24, 12), b->build_progress / (float)b->build_total, UI_GOLD);
            y += 30;
        }
        if (b->worker_slots) {
            if (ui_button(u, rect(x + 8, y, width - 24, 36),
                          b->auto_assign ? u8"자동 배치: 켜짐" : u8"자동 배치: 꺼짐", 1, 0, NULL))
                command(a, core_set_auto_assign(s->world, b->id, !b->auto_assign), 0);
            y += 44;
            if (ui_button(u, rect(x + 8, y, width - 24, 36), u8"일꾼 직접 배치", 1, 0, NULL))
                game_app_open(a, PANEL_ASSIGN);
            y += 44;
        }
        if (b->hp < b->max_hp) {
            int32_t reason = 0;
            if (ui_button(u, rect(x + 8, y, width - 24, 36), u8"수리", !s->summary.raid_id, 0,
                          u8"수리 비용은 필요한 자재에 비례합니다.")) {
                int rc = core_repair(s->world, b->id, &reason);
                command(a, rc, reason);
            }
            y += 44;
        }
        if (b->type != CORE_BLD_KEEP &&
            ui_button(u, rect(x + 8, y, width - 24, 36), u8"철거 (비용 절반 환불)", 1, 0, NULL))
            command(a, core_demolish(s->world, b->id), 0);
    } else if (v) {
        ui_text(u, x + 8, y, UI_GOLD, "%s #%d", v->name, v->id);
        y += 40;
        (void)snprintf(text, sizeof(text),
                       u8"%d세 · %s%s\n기분 %d → %d / 건강 %d\n배부름 %d / 휴식 %d\n물 %d / 위생 %d\n힘 %d · "
                       u8"민첩 %d · 체력 %d\n손재주 %d · 지능 %d\n집 #%d / 일터 #%d",
                       v->age, fmt_villager_state(v->state), v->inside_building ? u8" · 실내" : "", v->mood,
                       v->mood_target, v->health, v->hunger, v->rest, v->water, v->hygiene, v->strength,
                       v->agility, v->vitality, v->dexterity, v->intellect, v->home_id, v->workplace_id);
        ui_wrap(u, rect(x + 8, y, width - 24, 280), UI_TEXT, text);
        y += 285;
        if (v->contract_end_tick) {
            ui_small(u, x + 8, y, UI_GOLD, u8"계약 남은 기간 %.1f일",
                     (v->contract_end_tick - s->summary.tick) / 144.0);
            y += 32;
        }
        {
            static const char *const stats[] = {u8"힘", u8"민첩", u8"체력", u8"손재주", u8"지능"};
            int k;
            for (k = 0; k < 5; k++) {
                ui_small(u, x + 8, y, UI_MUTED, u8"%s 초기 %d / 성장 XP %.2f", stats[k], v->initial_stats[k],
                         v->stat_xp[k] / 1000.0);
                y += 25;
            }
        }
        {
            static const char *const fields[] = {u8"농업", u8"임업", u8"광업", u8"축산", u8"어업", u8"수렵",
                                                 u8"가공", u8"경비", u8"의료", u8"교역", u8"학술", u8"마법"};
            int k;
            for (k = 0; k < 12; k++)
                if (v->skill_minutes[k]) {
                    ui_small(u, x + 8, y, UI_TEXT, u8"%s 숙련 %.1f일", fields[k],
                             v->skill_minutes[k] / 540.0);
                    y += 25;
                }
        }
        if (ui_button(u, rect(x + 8, y, width - 24, 36), u8"일터 직접 선택", 1, 0, NULL))
            game_app_open(a, PANEL_ASSIGN);
        y += 44;
        if (ui_button(u, rect(x + 8, y, width - 24, 36), u8"일터에서 빼기", v->workplace_id != 0, 0, NULL))
            command(a, core_unassign_worker(s->world, v->id), 0);
        y += 44;
        if (ui_button(u, rect(x + 8, y, width - 24, 36), u8"자동 배치에 맡기기", 1, 0, NULL))
            command(a, core_release_worker(s->world, v->id), 0);
    } else {
        ui_text(u, x + 8, y, UI_GOLD, u8"성채 안내");
        y += 40;
        ui_wrap(u, rect(x + 8, y, width - 24, 220), UI_TEXT,
                u8"벌목지·채집장·텃밭으로 시작하고 막사와 우물을 마련하세요. 원목과 가공 목재는 "
                u8"다릅니다.\n\n촌락부터 제재소·경작지·저장고를 도로로 연결해 확장하세요. 주민이 일터에 "
                u8"도착해야 생산합니다.");
        y += 230;
        ui_text(u, x + 8, y, UI_GOLD, u8"처음 목표");
        y += 38;
        {
            int i;
            for (i = 0; i < CORE_TUTORIAL_COUNT; i++) {
                ui_small(u, x + 8, y, s->summary.tutorial_done & (1 << i) ? UI_GOOD : UI_MUTED, "[%s] %s",
                         s->summary.tutorial_done & (1 << i) ? u8"완료" : " ", core_expansion_goal_name(i));
                y += 32;
            }
        }
    }
    EndScissorMode();
    u->input_blocked = blocked;
}
static void notices(GameApp *a) {
    int i;
    float y = 143;
    GameUi *u = &a->ui;
    for (i = a->notice_count - 1; i >= 0 && y < GetScreenHeight() - 185; i--) {
        GameNotice *n = &a->notices[i];
        Rectangle area = rect(12, y, 390, 64);
        if (n->age > 16)
            continue;
        ui_box(area, Fade(UI_PANEL, n->age > 13 ? (16 - n->age) / 3 : 0.94f));
        ui_wrap(u, rect(area.x + 10, area.y + 7, area.width - 20, area.height - 10), UI_TEXT, n->text);
        if (a->panel == PANEL_NONE && n->has_position && IsMouseButtonReleased(MOUSE_BUTTON_LEFT) &&
            CheckCollisionPointRec(GetMousePosition(), area))
            a->camera.target = (Vector2){(n->x + 0.5f) * 16, (n->y + 0.5f) * 16};
        y += 70;
    }
}
static int row_button(GameApp *a, Rectangle box, float *y, const char *label, int enabled) {
    int pressed = ui_button(&a->ui, rect(box.x + 32, *y, box.width - 64, 42), label, enabled, 0, NULL);
    *y += 52;
    return pressed;
}
static Rectangle modal(GameApp *a, const char *title, float wanted_width, float wanted_height) {
    float w = fminf(wanted_width, GetScreenWidth() - 40.0f),
          h = fminf(wanted_height, GetScreenHeight() - 40.0f);
    Rectangle box = rect((GetScreenWidth() - w) / 2, (GetScreenHeight() - h) / 2, w, h);
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), (Color){10, 9, 16, 185});
    ui_box(box, UI_PANEL);
    ui_text(&a->ui, box.x + 28, box.y + 20, UI_GOLD, "%s", title);
    if (a->panel != PANEL_TITLE &&
        ui_button(&a->ui, rect(box.x + box.width - 68, box.y + 12, 48, 34), u8"닫기", 1, 0, NULL))
        game_app_close(a);
    return box;
}
static void title(GameApp *a) {
    Rectangle b = modal(a, u8"마법성채 키우기", 650, 600);
    float y = b.y + 110;
    ui_text(&a->ui, b.x + 32, b.y + 65, UI_MUTED, u8"C로 다스리는 중세 판타지 성채");
    if (row_button(a, b, &y, u8"이어하기", game_app_latest(a) >= 0)) {
        int slot = game_app_latest(a);
        if (slot >= 0)
            (void)game_app_load(a, slot, 0);
    }
    if (row_button(a, b, &y, u8"새 게임", 1))
        game_app_open(a, PANEL_NEW);
    if (row_button(a, b, &y, u8"불러오기", 1))
        game_app_open(a, PANEL_LOAD);
    if (row_button(a, b, &y, u8"설정", 1))
        game_app_open(a, PANEL_SETTINGS);
    if (row_button(a, b, &y, u8"도움말", 1))
        game_app_open(a, PANEL_HELP);
    if (row_button(a, b, &y, u8"종료", 1))
        a->quit = 1;
    ui_small(&a->ui, b.x + 32, b.y + b.height - 55, UI_MUTED,
             u8"농장 하나에서 왕성까지, 주민과 함께 성장하세요.");
}
static void new_game(GameApp *a) {
    Rectangle b = modal(a, u8"새 게임", 740, 620);
    GameUi *u = &a->ui;
    CoreDifficultyView d;
    float y = b.y + 78;
    int i;
    ui_text(u, b.x + 32, y, UI_TEXT, u8"성채 이름 (한글은 붙여넣기도 가능)");
    y += 36;
    ui_textbox(u, rect(b.x + 32, y, b.width - 64, 46), 1, a->castle_name, sizeof(a->castle_name), 0);
    y += 64;
    ui_text(u, b.x + 32, y, UI_TEXT, u8"지도 시드 (비우면 무작위)");
    y += 36;
    ui_textbox(u, rect(b.x + 32, y, b.width - 64, 46), 2, a->seed_text, sizeof(a->seed_text), 1);
    y += 64;
    for (i = 0; i < CORE_DIFF_COUNT; i++)
        if (ui_button(u, rect(b.x + 32 + i * (b.width - 64) / 3, y, (b.width - 76) / 3, 40),
                      fmt_difficulty_name(i), 1, a->difficulty == i, NULL))
            a->difficulty = i;
    y += 60;
    if (core_get_difficulty(a->difficulty, &d) == CORE_OK)
        ui_text(u, b.x + 32, y, UI_TEXT, u8"시작 주민 4명 · 192×192 지도 · 한 계절 12일 · 하루 120초");
    y += 48;
    if (core_check_castle_name(a->castle_name) != CORE_OK)
        ui_small(u, b.x + 32, y, UI_BAD, u8"이름은 올바른 UTF-8로 1~47바이트여야 합니다.");
    if (ui_button(u, rect(b.x + 32, b.y + b.height - 76, b.width - 64, 44), u8"성채 다스리기",
                  core_check_castle_name(a->castle_name) == CORE_OK, 0, NULL))
        (void)game_app_start(a);
}
static void leave(GameApp *a, int action) {
    int rc = game_app_save(a, 0);
    if (rc < 0 && a->in_game && a->session.summary.outcome != CORE_OUTCOME_LOST && !a->automated) {
        a->confirm_action = action;
        (void)snprintf(a->confirm_text, sizeof(a->confirm_text),
                       u8"자동 저장을 할 수 없습니다. %s\n저장되지 않은 진행을 남기고 %s?",
                       fmt_result_message(rc), action == 1 ? u8"제목으로 돌아갈까요" : u8"종료할까요");
        game_app_open(a, PANEL_CONFIRM);
        return;
    }
    if (action == 1) {
        a->in_game = 0;
        a->panel = PANEL_TITLE;
        a->speed = 0;
    } else
        a->quit = 1;
}
static void pause_menu(GameApp *a) {
    Rectangle b = modal(a, u8"일시정지", 620, 620);
    float y = b.y + 82;
    if (row_button(a, b, &y, u8"계속하기", 1))
        game_app_close(a);
    if (row_button(a, b, &y, u8"저장하기", !a->session.summary.raid_id))
        game_app_open(a, PANEL_SAVE);
    if (row_button(a, b, &y, u8"불러오기", 1))
        game_app_open(a, PANEL_LOAD);
    if (row_button(a, b, &y, u8"설정", 1))
        game_app_open(a, PANEL_SETTINGS);
    if (row_button(a, b, &y, u8"도움말", 1))
        game_app_open(a, PANEL_HELP);
    if (row_button(a, b, &y, u8"제목 화면", 1))
        leave(a, 1);
    if (row_button(a, b, &y, u8"게임 종료", 1))
        leave(a, 2);
}
static void slots(GameApp *a, int saving) {
    Rectangle b = modal(a, saving ? u8"저장하기" : u8"불러오기", 1000, 610);
    float y = b.y + 78;
    int slot;
    for (slot = saving ? 1 : 0; slot <= 3; slot++) {
        char path[GAME_PATH_BYTES], summary[512], label[600];
        CoreSaveInfoView info;
        int ok, backup = 0;
        game_app_slot_path(a, slot, 0, path);
        ok = core_peek_save(path, a->session.defs, &info) == CORE_OK;
        if (ok)
            fmt_save_info(&info, summary, sizeof(summary));
        else {
            (void)snprintf(summary, sizeof(summary), "%s",
                           info.result == CORE_ERR_FILE_OPEN ? u8"빈 슬롯" : fmt_result_message(info.result));
            game_app_slot_path(a, slot, 1, path);
            backup = core_peek_save(path, a->session.defs, &info) == CORE_OK;
        }
        (void)snprintf(label, sizeof(label), u8"%s%d · %s%s", slot ? u8"슬롯 " : u8"자동 저장 ", slot,
                       summary, backup ? u8" (예비 파일 있음)" : "");
        if (ui_button(&a->ui, rect(b.x + 28, y, b.width - 56, 80), label, saving || ok || backup, 0, NULL)) {
            if (saving) {
                game_app_slot_path(a, slot, 0, path);
                if (platform_file_exists(path)) {
                    a->confirm_action = 4;
                    a->backup_slot = slot;
                    (void)snprintf(a->confirm_text, sizeof(a->confirm_text),
                                   u8"슬롯 %d의 이전 저장을 덮어쓸까요? 이전 파일은 .bak으로 남습니다.",
                                   slot);
                    game_app_open(a, PANEL_CONFIRM);
                } else
                    (void)game_app_save(a, slot);
            } else
                (void)game_app_load(a, slot, 0);
        }
        y += 96;
    }
    ui_small(&a->ui, b.x + 28, b.y + b.height - 50, UI_MUTED,
             u8"습격 중 저장은 습격 종료 후로 미뤄집니다. 저장 형식 v%d", CORE_SAVE_VERSION);
}
static void settings(GameApp *a) {
    Rectangle b = modal(a, u8"설정", 760, 530);
    GameUi *u = &a->ui;
    float x = b.x + 32, y = b.y + 86;
    char label[120];
    ui_text(u, x, y, UI_TEXT, u8"전체 음량 %d%%", (int)(a->settings.master_volume * 100));
    y += 36;
    ui_slider(u, rect(x, y, b.width - 64, 34), &a->settings.master_volume);
    y += 58;
    ui_text(u, x, y, UI_TEXT, u8"효과음 %d%%", (int)(a->settings.sfx_volume * 100));
    y += 36;
    ui_slider(u, rect(x, y, b.width - 64, 34), &a->settings.sfx_volume);
    y += 56;
    (void)snprintf(label, sizeof(label), u8"전체 화면: %s", a->settings.fullscreen ? u8"켜짐" : u8"꺼짐");
    if (row_button(a, b, &y, label, 1)) {
        ToggleFullscreen();
        a->settings.fullscreen = IsWindowFullscreen();
    }
    (void)snprintf(label, sizeof(label), u8"가장자리 스크롤: %s",
                   a->settings.edge_scroll ? u8"켜짐" : u8"꺼짐");
    if (row_button(a, b, &y, label, 1))
        a->settings.edge_scroll = !a->settings.edge_scroll;
    (void)snprintf(label, sizeof(label), u8"글자 크기: %s", a->settings.ui_scale == 2 ? u8"크게" : u8"기본");
    if (row_button(a, b, &y, label, 1))
        a->settings.ui_scale = a->settings.ui_scale == 2 ? 1 : 2;
    if (a->assets.audio_ready)
        SetMasterVolume(a->settings.master_volume);
}
static float scroll_start(GameApp *a, Rectangle b, int rows, float height) {
    int max = (int)fmaxf(0, rows * height - (b.height - 150));
    a->scroll -= (int)(GetMouseWheelMove() * height);
    if (a->scroll < 0)
        a->scroll = 0;
    if (a->scroll > max)
        a->scroll = max;
    if (max > 0)
        ui_bar(rect(b.x + b.width - 16, b.y + 70, 5, b.height - 100), a->scroll / (float)max, UI_GOLD);
    return b.y + 76 - a->scroll;
}
static void research(GameApp *a) {
    Rectangle b = modal(a, u8"연구 · 경제 / 방어 / 마법", 1050, 760);
    GameSession *s = &a->session;
    CoreResearchView r;
    int count = 0, i;
    float y;
    while (count < CORE_MAX_RESEARCH && core_get_research(s->world, count, &r) == CORE_OK)
        count++;
    y = scroll_start(a, b, count, 100);
    for (i = 0; i < count; i++, y += 100) {
        int32_t reason = 0;
        char effect[200], label[180];
        if (y < b.y + 70 || y + 90 > b.y + b.height - 48)
            continue;
        if (core_get_research(s->world, i, &r) != CORE_OK)
            continue;
        (void)snprintf(effect, sizeof(effect), "%s", core_expansion_research_effect(i));
        ui_text(&a->ui, b.x + 30, y, UI_GOLD, u8"%s · %s", fmt_branch_name(r.branch),
                core_expansion_research_name(i));
        ui_small(&a->ui, b.x + 30, y + 32, UI_TEXT, u8"%s / 금화 %d · 마정석 %d / %s", effect, r.cost_gold,
                 r.cost_mana, fmt_research_state(r.state));
        if (r.state == CORE_RSTATE_ACTIVE)
            ui_bar(rect(b.x + 30, y + 63, b.width - 240, 10), r.progress / (float)r.points, UI_GOOD);
        if (r.state == CORE_RSTATE_ACTIVE)
            (void)snprintf(label, sizeof(label), "%d / %d", r.progress, r.points);
        else
            (void)snprintf(label, sizeof(label), "%s",
                           r.state == CORE_RSTATE_DONE ? u8"완료" : u8"연구 시작");
        if (ui_button(&a->ui, rect(b.x + b.width - 190, y + 8, 155, 44), label,
                      r.state == CORE_RSTATE_AVAILABLE, 0, NULL)) {
            int rc = core_start_research(s->world, i, &reason);
            command(a, rc, reason);
        }
    }
}
static void rank_panel(GameApp *a) {
    Rectangle b = modal(a, u8"영지 승급 · 세금", 940, 740);
    CoreExpansionRankView r;
    CoreSummaryView *s = &a->session.summary;
    GameUi *u = &a->ui;
    float x = b.x + 32, y = b.y + 80;
    int32_t reason = 0;
    int i;
    if (core_expansion_rank(a->session.world, &r) != CORE_OK)
        return;
    ui_text(u, x, y, UI_GOLD, u8"현재 %s · 다음 %s", core_expansion_rank_name(s->rank), r.name);
    y += 45;
    if (s->rank < 5) {
        ui_text(u, x, y, s->permanent_population >= r.population ? UI_GOOD : UI_BAD,
                u8"영구 주민 %d / %d · 계약직 %d명 제외", s->permanent_population, r.population,
                s->contractors);
        y += 36;
        ui_text(u, x, y, s->average_mood >= r.mood ? UI_GOOD : UI_BAD, u8"평균 기분 %d / %d", s->average_mood,
                r.mood);
        y += 36;
        ui_text(u, x, y, s->valid_roads >= r.roads ? UI_GOOD : UI_BAD, u8"시설에 쓰이는 연결 도로 %d / %d칸",
                s->valid_roads, r.roads);
        y += 36;
        ui_text(u, x, y, s->protected_area >= r.area ? UI_GOOD : UI_BAD,
                u8"폐쇄 보호구역 %d / %d칸 · 보호 주민 %d / %d%%", s->protected_area, r.area,
                s->permanent_population ? s->protected_population * 100 / s->permanent_population : 0,
                r.protected_pct);
        y += 36;
        ui_text(u, x, y, s->research_done >= r.research ? UI_GOOD : UI_BAD, u8"완료 연구 %d / %d개",
                s->research_done, r.research);
        y += 36;
        ui_wrap(u, rect(x, y, b.width - 64, 70), UI_TEXT, r.requirements);
        y += 80;
        ui_text(u, x, y, UI_GOLD, u8"공물: 금화 %d · 마정석 %d · 금괴 %d", r.gold, r.mana, r.gold_bars);
        y += 48;
        if (row_button(a, b, &y, u8"승급하기", r.ready)) {
            int rc = core_rank_up(a->session.world, &reason);
            command(a, rc, reason);
        }
    } else {
        ui_text(u, x, y, UI_GOOD, u8"왕성입니다. 모든 건물을 사용하며 계속 다스릴 수 있습니다.");
        y += 80;
    }
    ui_small(u, x, y, UI_MUTED, u8"세금: 거주 중인 영구 주민만 납부 · 하루 한 번 변경 · 다음 정산에 반영");
    y += 30;
    for (i = 1; i <= 4; i++) {
        char label[48];
        (void)snprintf(label, sizeof(label), u8"%d금 / 주민 / 일", i);
        if (ui_button(u, rect(x + (i - 1) * (b.width - 64) / 4, y, (b.width - 80) / 4, 36), label, 1,
                      s->tax == i, NULL))
            command(a, core_expansion_set_tax(a->session.world, i), 0);
    }
}
static void market(GameApp *a, int contracts) {
    Rectangle b = modal(a, contracts ? u8"계약 주민 · 12일 선불 고용" : u8"시장 · 개별 자원 거래", 1080, 760);
    GameSession *s = &a->session;
    float y;
    int i;
    if (contracts) {
        static const char *const fields[] = {u8"농업", u8"임업", u8"광업", u8"축산", u8"어업", u8"수렵",
                                             u8"가공", u8"경비", u8"의료", u8"교역", u8"학술", u8"마법"};
        CoreCandidateView candidates[15];
        int32_t count = 0;
        (void)core_expansion_copy_candidates(s->world, candidates, 15, &count);
        ui_small(&a->ui, b.x + 30, b.y + 74, UI_MUTED,
                 u8"이동 시간 포함 12일 · 세금/추가 임금 없음 · 후보는 1·4·7…일 갱신");
        if (!count) {
            ui_text(&a->ui, b.x + 30, b.y + 130, UI_BAD, u8"일꾼이 배치된 가판대·시장·교역소가 필요합니다.");
            return;
        }
        y = scroll_start(a, b, count, 112) + 45;
        for (i = 0; i < count; i++, y += 112) {
            CoreCandidateView *c = &candidates[i];
            char label[100];
            if (y < b.y + 120 || y + 100 > b.y + b.height - 45)
                continue;
            ui_text(&a->ui, b.x + 30, y, UI_GOLD, u8"%s · %s · %d세", c->name, fields[c->field], c->age);
            ui_small(&a->ui, b.x + 30, y + 32, UI_TEXT, u8"힘 %d / 민첩 %d / 체력 %d / 손재주 %d / 지능 %d",
                     c->stats[0], c->stats[1], c->stats[2], c->stats[3], c->stats[4]);
            ui_small(&a->ui, b.x + 30, y + 60, UI_MUTED, u8"주 분야 근무 %.1f일 · 개인 번호 %d",
                     c->skills[c->field] / 540.0, c->person_id);
            (void)snprintf(label, sizeof(label), c->hired ? u8"고용 완료" : u8"고용 %d금", c->fee);
            if (ui_button(&a->ui, rect(b.x + b.width - 195, y + 10, 160, 44), label,
                          !c->hired && s->summary.resources[CORE_RES_GOLD] >= c->fee, 0,
                          u8"가판대 1명 / 시장 3명 / 교역소 8명 상한, 합산하지 않습니다.")) {
                int32_t id = 0;
                command(a, core_expansion_hire(s->world, c->person_id, &id), 0);
            }
        }
    } else {
        CoreMarketView m;
        (void)core_get_market(s->world, &m);
        ui_small(&a->ui, b.x + 30, b.y + 74, m.open ? UI_GOOD : UI_BAD,
                 u8"하루 거래 %d / %d개 · 6일마다 재고 보충 · 아래 가격은 개당", s->summary.market_used,
                 s->summary.market_limit);
        y = scroll_start(a, b, 14, 64) + 45;
        for (i = 1; i < 15; i++, y += 64) {
            int32_t reason = 0, gold = 0;
            char label[96];
            int enabled = m.open && m.buy_price[i] > 0, stock = core_expansion_market_stock(s->world, i);
            if (y < b.y + 120 || y + 54 > b.y + b.height - 45)
                continue;
            ui_small(&a->ui, b.x + 30, y + 8, UI_TEXT, u8"%s · 보유 %d / 재고 %d", fmt_resource_name(i),
                     s->summary.resources[i], stock);
            (void)snprintf(label, sizeof(label), u8"1개 구매 %d금", m.buy_price[i]);
            if (ui_button(&a->ui, rect(b.x + b.width - 610, y, 180, 38), label, enabled, 0, NULL)) {
                int rc = core_trade(s->world, i, 1, &gold, &reason);
                command(a, rc, reason);
            }
            (void)snprintf(label, sizeof(label), u8"10개 구매 %d금", m.buy_price[i] * 10);
            if (ui_button(&a->ui, rect(b.x + b.width - 420, y, 180, 38), label, enabled, 0, NULL)) {
                int rc = core_trade(s->world, i, 10, &gold, &reason);
                command(a, rc, reason);
            }
            (void)snprintf(label, sizeof(label), u8"10개 판매 %d금", m.sell_price[i] * 10);
            if (ui_button(&a->ui, rect(b.x + b.width - 230, y, 190, 38), label, enabled, 0, NULL)) {
                int rc = core_trade(s->world, i, -10, &gold, &reason);
                command(a, rc, reason);
            }
        }
    }
}
static int sort_column = 0, sort_reverse = 0;
static int villager_compare(const void *left, const void *right) {
    const CoreVillagerView *a = (const CoreVillagerView *)left, *b = (const CoreVillagerView *)right;
    int result;
    if (sort_column == 0)
        result = strcmp(a->name, b->name);
    else if (sort_column == 1)
        result = (a->job > b->job) - (a->job < b->job);
    else if (sort_column == 2)
        result = (a->mood > b->mood) - (a->mood < b->mood);
    else
        result = (a->health > b->health) - (a->health < b->health);
    return sort_reverse ? -result : result;
}
static void villagers(GameApp *a) {
    Rectangle b = modal(a, u8"주민 목록 · 제목을 눌러 정렬", 1120, 760);
    GameSession *s = &a->session;
    float y;
    int i;
    static const char *const labels[] = {u8"이름", u8"일터", u8"기분", u8"체력"};
    for (i = 0; i < 4; i++)
        if (ui_button(&a->ui, rect(b.x + 28 + i * (b.width - 56) / 4, b.y + 67, (b.width - 64) / 4, 36),
                      labels[i], 1, a->sort_column == i, NULL)) {
            if (a->sort_column == i)
                a->sort_reverse = !a->sort_reverse;
            else {
                a->sort_column = i;
                a->sort_reverse = 0;
            }
        }
    sort_column = a->sort_column;
    sort_reverse = a->sort_reverse;
    qsort(s->villagers, (size_t)s->villager_count, sizeof(*s->villagers), villager_compare);
    y = scroll_start(a, b, s->villager_count + 1, 48) + 48;
    for (i = 0; i < s->villager_count; i++, y += 48) {
        CoreVillagerView *v = &s->villagers[i];
        char label[300];
        if (y < b.y + 110 || y + 42 > b.y + b.height - 42)
            continue;
        (void)snprintf(label, sizeof(label), u8"%s #%d | %s | 기분 %d | 체력 %d | %s", v->name, v->id,
                       v->job < 0 ? u8"일터 없음" : core_expansion_building_name(v->job), v->mood, v->health,
                       fmt_villager_state(v->state));
        if (ui_button(&a->ui, rect(b.x + 28, y, b.width - 56, 40), label, 1, a->selected_villager == v->id,
                      NULL)) {
            GameActor *actor = game_actor(s, v->id);
            a->selected_villager = v->id;
            a->selected_building = 0;
            if (actor)
                a->camera.target = (Vector2){actor->position.x * 16, actor->position.y * 16};
            game_app_close(a);
        }
    }
}
static void assign(GameApp *a) {
    Rectangle b = modal(a, u8"일꾼 직접 배치", 1000, 730);
    GameSession *s = &a->session;
    int building = a->selected_building, count = building ? s->villager_count : s->building_count, i;
    float y = scroll_start(a, b, count, 55);
    for (i = 0; i < count; i++, y += 55) {
        char label[260];
        int target;
        if (y < b.y + 70 || y + 45 > b.y + b.height - 42)
            continue;
        if (building) {
            CoreVillagerView *v = &s->villagers[i];
            target = v->id;
            (void)snprintf(label, sizeof(label), u8"%s · %s · 힘 %d / 지능 %d / 손재주 %d", v->name,
                           v->job < 0 ? u8"일터 없음" : core_expansion_building_name(v->job), v->strength,
                           v->intellect, v->dexterity);
        } else {
            CoreBuildingView *d = &s->buildings[i];
            target = d->id;
            if (!d->worker_slots)
                continue;
            (void)snprintf(label, sizeof(label), u8"%s #%d · 일꾼 %d/%d",
                           core_expansion_building_name(d->type), d->id, d->workers, d->worker_slots);
        }
        if (ui_button(&a->ui, rect(b.x + 28, y, b.width - 56, 42), label, 1, 0, NULL)) {
            int rc = core_assign_worker(s->world, building ? target : a->selected_villager,
                                        building ? building : target);
            command(a, rc, 0);
            if (rc == CORE_OK)
                game_app_close(a);
        }
    }
}
static void ledger(GameApp *a) {
    Rectangle b = modal(a, u8"자원 장부", 920, 710);
    CoreLedgerView l;
    int i, r = a->category % CORE_RES_COUNT;
    float y = b.y + 84;
    GameUi *u = &a->ui;
    (void)core_get_ledger(a->session.world, &l);
    for (i = 0; i < CORE_RES_COUNT; i++)
        if (ui_button(u, rect(b.x + 24 + i * (b.width - 48) / 6, y, (b.width - 60) / 6, 36),
                      fmt_resource_name(i), 1, r == i, NULL))
            a->category = i;
    y += 62;
    ui_text(u, b.x + 32, y, UI_GOLD, u8"항목                         오늘          어제");
    y += 42;
    for (i = 0; i < CORE_LEDGER_COUNT; i++, y += 32) {
        if (y + 30 > b.y + b.height - 35)
            break;
        ui_text(u, b.x + 32, y, UI_TEXT, "%s", fmt_ledger_name(i));
        ui_text(u, b.x + b.width - 290, y, l.today[r][i] >= 0 ? UI_GOOD : UI_BAD, "%+d", l.today[r][i]);
        ui_text(u, b.x + b.width - 140, y, UI_MUTED, "%+d", l.yesterday[r][i]);
    }
}
static void outcome(GameApp *a) {
    Rectangle b = modal(
        a, a->session.summary.outcome == CORE_OUTCOME_WON ? u8"왕성 달성!" : u8"성채의 마지막 날", 790, 650);
    CoreRunStatsView r;
    float y = b.y + 82;
    GameUi *u = &a->ui;
    (void)core_get_run_stats(a->session.world, &r);
    if (r.outcome == CORE_OUTCOME_LOST) {
        ui_text(u, b.x + 32, y, UI_BAD, "%s", fmt_lose_reason(r.lose_reason));
        y += 50;
    }
    ui_text(u, b.x + 32, y, UI_TEXT, u8"다스린 날 %d일 / 최다 주민 %d명", r.days, r.peak_population);
    y += 42;
    ui_text(u, b.x + 32, y, UI_TEXT, u8"견딘 습격 %d회 / 쓰러뜨린 적 %d명", r.raids_survived,
            r.enemies_killed);
    y += 42;
    ui_text(u, b.x + 32, y, UI_TEXT, u8"지은 건물 %d채 / 끝낸 연구 %d개", r.buildings_built, r.research_done);
    y += 70;
    if (r.outcome == CORE_OUTCOME_WON && row_button(a, b, &y, u8"계속 다스리기", 1)) {
        a->panel = PANEL_NONE;
        a->speed = 1;
    }
    if (row_button(a, b, &y, u8"새 게임", 1))
        game_app_open(a, PANEL_NEW);
    if (row_button(a, b, &y, u8"제목 화면", 1)) {
        a->in_game = 0;
        a->panel = PANEL_TITLE;
    }
}
static void confirm(GameApp *a) {
    Rectangle b = modal(a, u8"확인", 760, 410);
    float y = b.y + 260;
    ui_wrap(&a->ui, rect(b.x + 32, b.y + 86, b.width - 64, 150), UI_TEXT, a->confirm_text);
    if (ui_button(&a->ui, rect(b.x + 32, y, (b.width - 80) / 2, 44), u8"예", 1, 0, NULL)) {
        int action = a->confirm_action;
        if (action == 1) {
            a->in_game = 0;
            a->panel = PANEL_TITLE;
        } else if (action == 2)
            a->quit = 1;
        else if (action == 3)
            (void)game_app_load(a, a->backup_slot, 1);
        else if (action == 4) {
            (void)game_app_save(a, a->backup_slot);
            game_app_close(a);
        }
    }
    if (ui_button(&a->ui, rect(b.x + b.width / 2 + 8, y, (b.width - 80) / 2, 44), u8"아니오", 1, 0, NULL))
        game_app_close(a);
}
void game_render_interface(GameApp *a) {
    if (a->in_game) {
        a->ui.input_blocked = a->panel != PANEL_NONE;
        hud(a);
        a->ui.input_blocked = a->panel != PANEL_NONE;
        build_bar(a);
        inspector(a);
        notices(a);
        a->ui.input_blocked = 0;
    }
    switch (a->panel) {
    case PANEL_TITLE:
        title(a);
        break;
    case PANEL_NEW:
        new_game(a);
        break;
    case PANEL_PAUSE:
        pause_menu(a);
        break;
    case PANEL_SAVE:
        slots(a, 1);
        break;
    case PANEL_LOAD:
        slots(a, 0);
        break;
    case PANEL_SETTINGS:
        settings(a);
        break;
    case PANEL_RESEARCH:
        research(a);
        break;
    case PANEL_RANK:
        rank_panel(a);
        break;
    case PANEL_MARKET:
        market(a, 0);
        break;
    case PANEL_MERCHANT:
        market(a, 1);
        break;
    case PANEL_VILLAGERS:
        villagers(a);
        break;
    case PANEL_ASSIGN:
        assign(a);
        break;
    case PANEL_LEDGER:
        ledger(a);
        break;
    case PANEL_OUTCOME:
        outcome(a);
        break;
    case PANEL_CONFIRM:
        confirm(a);
        break;
    case PANEL_HELP: {
        Rectangle b = modal(a, u8"도움말", 1000, 730);
        ui_wrap(&a->ui, rect(b.x + 32, b.y + 80, b.width - 64, b.height - 120), UI_TEXT,
                u8"WASD / 화살표: 이동 · 가운데 버튼 끌기: 카메라 이동 · 휠: 확대\n스페이스: 일시정지 · "
                u8"1/2/3: 배속 · V: 주민 · X: 철거 · R: 건물/미리보기 회전\n왼쪽: 건설/선택 · 도로·성벽: "
                u8"드래그 · 오른쪽: 취소 · Esc: 닫기/메뉴\n\n"
                u8"개척지에서는 벌목지·채집장·텃밭·막사와 우물로 시작합니다. 우물은 반경8칸, 최대18명을 "
                u8"공급합니다. 촌락에서 제재소로 원목을 목재로 가공하세요.\n"
                u8"경작지·농장·관개농장은 봄·여름의 실제 노동을 모아 25일 08시에 한 번 수확합니다. 텃밭은 "
                u8"매일 생산하며 겨울에는 절반입니다. 연료는 석탄→원목→목재 순입니다.\n"
                u8"촌락 이후 건물 입구를 성채 도로망에 연결하세요. 도로를 지워 연결이 끊기면 철거가 "
                u8"거절됩니다. 무인 건물은 빨간 느낌표, 작업·주민 상태는 아이콘으로 표시합니다.\n"
                u8"적을 실제 발견한 순간 경보가 울리고 병영이 자동 출동합니다. 연구 24종과 여섯 등급을 "
                u8"진행하며 왕성 이후에도 계속할 수 있습니다.\n"
                u8"고용 계약은 이동시간 포함 정확히12일 선불이며 승급 인구·세금에는 제외합니다. 매일 자동 "
                u8"저장하며 한글 이름은 Ctrl+V로 붙여넣을 수 있습니다.");
        break;
    }
    case PANEL_NONE:
        break;
    }
    if (a->message[0]) {
        ui_box(rect(12, GetScreenHeight() - 235.0f, fminf(GetScreenWidth() - 24.0f, 1000), 38),
               (Color){23, 20, 30, 245});
        ui_small(&a->ui, 22, GetScreenHeight() - 227.0f, UI_GOLD, "%s", a->message);
    }
    if (a->stress)
        ui_small(&a->ui, GetScreenWidth() - 330.0f, GetScreenHeight() - 34.0f, UI_GOLD,
                 u8"FPS %d · 주민 %d · 적 %d", GetFPS(), a->session.villager_count, a->session.enemy_count);
    ui_tooltip(&a->ui);
}
