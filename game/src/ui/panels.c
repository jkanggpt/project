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
    for (int display = 0; display < CORE_RES_COUNT; display++) {
        i = core_expansion_display_resource(display);
        if (d->cost[i] > 0 && used < size) {
            int n = snprintf(text + used, size - used, "%s%s %d", used ? " / " : "", fmt_resource_name(i),
                             d->cost[i]);
            if (n < 0 || (size_t)n >= size - used)
                break;
            used += (size_t)n;
        }
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
    if (width < 1100)
        DrawTextEx(a->assets.font,
                   TextFormat(u8"%d년 %s %d일  %02d:%02d", s->year, fmt_season_name(s->season),
                              s->day_of_season, s->minute_of_day / 60, s->minute_of_day % 60),
                   (Vector2){100, 45}, 14, 0, UI_MUTED);
    else
        ui_small(u, 100, width < 1100 ? 44.0f : 48.0f, UI_MUTED, u8"%d년 %s %d일  %02d:%02d  (시드 %d)",
                 s->year, fmt_season_name(s->season), s->day_of_season, s->minute_of_day / 60,
                 s->minute_of_day % 60, s->seed);
    if (ui_button(u, rect(width < 1100 ? width - 200 : 540, 4, 42, 30), a->speed ? "||" : ">", 1, !a->speed,
                  u8"스페이스: 일시정지 / 계속"))
        a->speed = a->speed ? 0 : 1;
    for (i = 1; i <= 3; i++) {
        (void)snprintf(text, sizeof(text), "%dx", i);
        if (ui_button(u, rect((width < 1100 ? width - 152 : 590) + (i - 1) * 47.0f, 4, 42, 30), text, 1,
                      a->speed == i, u8"숫자 1~3: 배속"))
            a->speed = i;
    }
    {
        static const char *const labels[] = {u8"영지", u8"연구", u8"거래", u8"고용", u8"주민", u8"장부"};
        static const GamePanel panels[] = {PANEL_RANK,     PANEL_RESEARCH,  PANEL_MARKET,
                                           PANEL_MERCHANT, PANEL_VILLAGERS, PANEL_LEDGER};
        float menu_x = width < 1100 ? width - 365 : 740;
        float button_w = (width - menu_x - 8) / 6;
        if (button_w > 90)
            button_w = 90;
        for (i = 0; i < 6; i++)
            if (ui_button(u, rect(menu_x + i * button_w, width < 1100 ? 43.0f : 9.0f, button_w - 5, 36),
                          labels[i], 1, 0, NULL))
                game_app_open(a, panels[i]);
    }
    float hud_font = u->font_size;
    if (width < 1100)
        u->font_size = 14;
    int32_t daily[CORE_RES_COUNT] = {0};
    core_expansion_daily_requirements(a->session.world, daily);
    for (int display = 0; display < CORE_RES_COUNT; display++) {
        i = core_expansion_display_resource(display);
        Texture2D icon = a->assets.resources[i];
        float x = 10 + display % 8 * (width - 20) / 8, ry = 75 + display / 8 * 27.0f;
        if (icon.id)
            DrawTexturePro(icon, (Rectangle){0, 0, (float)icon.width, (float)icon.height},
                           rect(x, ry, 20, 20), (Vector2){0, 0}, 0, WHITE);
        const char *name = fmt_resource_name(i);
        float label_width = MeasureTextEx(a->assets.font, name, u->font_size - 3, 0).x;
        ui_small(u, x + 24, ry + 1, UI_TEXT, "%s", name);
        Color amount_color = daily[i] > 0 && (int64_t)s->resources[i] * 1000 <= daily[i] ? UI_BAD : UI_TEXT;
        ui_small(u, x + 30 + label_width, ry + 1, amount_color, "%d", s->resources[i]);
        (void)snprintf(hint, sizeof(hint),
                       u8"%s\n어제 변화 %+d / 저장 한도 %d (0은 제한 없음)\n하루 필수 소비 %.2f 이하이면 "
                       u8"재고 숫자가 빨강입니다.\n주민 식사·가축 사료·급여·유지비·겨울 난방 기준입니다.",
                       fmt_resource_name(i), s->resource_delta[i], s->resource_capacity[i],
                       daily[i] / 1000.0);
        if (CheckCollisionPointRec(GetMousePosition(), rect(x, ry, (width - 20) / 8, 25)))
            (void)snprintf(u->tooltip, sizeof(u->tooltip), "%s", hint);
    }
    ui_small(u, 10 + 7 * (width - 20) / 8, 96, UI_TEXT, u8"인구%d · 행복%d", s->population,
             s->average_happiness);
    {
        CoreImmigrationView status;
        core_expansion_immigration(a->session.world, &status);
        static const char *const labels[] = {u8"재이주 대기",    u8"이주 조건 부족", u8"다음 06시 검사",
                                             u8"이주단 이동 중", u8"특별 이주 대기", u8"개척지 보충"};
        char next[96];
        int remaining = status.next_day > s->day ? status.next_day - s->day : 0;
        if (status.status == 0)
            snprintf(next, sizeof(next), u8"이주 대기 %d일", remaining);
        else if (status.status == 4)
            snprintf(next, sizeof(next), u8"특별이주 D-%d", remaining);
        else if (status.status == 5 && status.permanent >= 8)
            snprintf(next, sizeof(next), u8"보충 인원 충족");
        else if (status.status == 5)
            snprintf(next, sizeof(next), u8"보충이주 D-%d", remaining);
        else
            snprintf(next, sizeof(next), "%s", labels[status.status]);
        ui_small(u, 10 + 7 * (width - 20) / 8, 115, UI_MUTED, "%s", next);
        if (CheckCollisionPointRec(GetMousePosition(),
                                   rect(10 + 7 * (width - 20) / 8, 96, (width - 20) / 8, 36))) {
            snprintf(u->tooltip, sizeof(u->tooltip),
                     u8"%s · 최근 도착 %d일 / 다음 검사 %d일 06시\n행복도 %d/%d · 빈집 %d · 우물 %d/%d · "
                     u8"\n식량 %.1f/%.1f · 특별 이주 %d/2 · 보충 예정 %d일\n부족: %s%s%s%s%s%s",
                     labels[status.status], status.last_day, status.next_day, status.happiness,
                     status.required_happiness, status.vacant, status.water_served, status.permanent,
                     status.food_available / 1000.0, status.food_needed / 1000.0, status.special_waves,
                     status.refill_day, status.reasons & 1 ? u8"대기 기간 " : "",
                     status.reasons & 2 ? u8"주거 " : "", status.reasons & 4 ? u8"우물 " : "",
                     status.reasons & 16 ? u8"식량 " : "", status.reasons & 32 ? u8"역병·재난 " : "",
                     status.reasons & 64 ? u8"행복도" : "");
        }
    }
    u->font_size = hud_font;
    char lines[6][180];
    Color colors[6];
    int count = 0;
    CoreEventLayers layers = {0};
    core_expansion_events(a->session.world, &layers);
    if (s->nature_event) {
        snprintf(lines[count], 180, u8"%s · %d일 남음", fmt_nature_name(s->nature_event),
                 s->nature_days_left);
        colors[count++] = UI_GOLD;
    }
    if (layers.plague_days) {
        snprintf(lines[count], 180, u8"역병 · %d일 남음", layers.plague_days);
        colors[count++] = UI_BAD;
    }
    if (layers.economic_days) {
        snprintf(lines[count], 180, u8"%s · %d일 남음", fmt_nature_name(layers.economic_event),
                 layers.economic_days);
        colors[count++] = UI_GOLD;
    }
    if (s->raid_id) {
        if (a->session.raid.phase == CORE_RAID_RETREAT)
            snprintf(lines[count], 180, u8"%s 철수 중 · 시야 밖으로 모두 사라지면 종료",
                     game_faction_name(a->session.raid.type - 1));
        else
            snprintf(lines[count], 180, u8"%s 습격 %d명 · 남은 공격 %.1f시간",
                     game_faction_name(a->session.raid.type - 1), s->raid_enemies_left,
                     fmaxf(0, (a->session.raid.assault_end_tick - s->tick) / 6.0f));
        colors[count++] = UI_BAD;
    } else if (s->threat_type) {
        snprintf(lines[count], 180, u8"위협: %s · %d일 %d시간 후", fmt_threat_name(s->threat_type),
                 s->threat_ticks_left / 144, s->threat_ticks_left % 144 / 6);
        colors[count++] = UI_GOLD;
    }
    if (s->season == CORE_SEASON_AUTUMN || s->season == CORE_SEASON_WINTER) {
        int days = s->season == CORE_SEASON_WINTER ? 13 - s->day_of_season : 12, need = s->population * days;
        snprintf(lines[count], 180, u8"겨울 %d일분 식량 %d/%d", days, s->resources[CORE_RES_FOOD], need);
        colors[count++] = s->resources[CORE_RES_FOOD] < need ? UI_BAD : UI_GOOD;
    }
    float status_y = 141;
    for (int row = 0; row < count; row++) {
        float height = ui_wrap_height(u, 370, lines[row]) + 14;
        ui_box(rect(12, status_y, 390, height), UI_PANEL);
        ui_wrap(u, rect(22, status_y + 7, 370, height - 10), colors[row], lines[row]);
        status_y += height + 5;
    }
    a->status_height = (int)(status_y - 141);
}
static void build_bar(GameApp *a) {
    static const int groups[] = {1, 7, 9, 10, 11, 15, 8, 12, 3, 5, 13, 14, 17, 16, 4, 6};
    static const char *const names[] = {u8"특수",      u8"주거",   u8"자원", u8"저장·거래", u8"성벽·성문",
                                        u8"의료·위생", u8"도로",   u8"농업", u8"축산",      u8"임업",
                                        u8"광업",      u8"어업",   u8"가공", u8"문화·교육", u8"마법",
                                        u8"수렵",      u8"방어탑", u8"군사"};
    static const Color colors[6] = {{133, 127, 121, 255}, {117, 164, 95, 255}, {63, 157, 175, 255},
                                    {89, 116, 212, 255},  {161, 75, 199, 255}, {229, 184, 66, 255}};
    GameUi *u = &a->ui;
    float y = GetScreenHeight() - 248.0f, width = (float)GetScreenWidth(), tab = (width - 126) / 8;
    ui_box(rect(0, y, width, 248), UI_BG);
    for (int i = 0; i < 16; i++) {
        Rectangle r = rect(8 + i % 8 * tab, y + 5 + i / 8 * 29, tab - 4, 26);
        if (ui_button(u, r, names[groups[i]], 1, a->category == groups[i], NULL)) {
            a->category = groups[i];
            a->build_scroll = 0;
        }
    }
    if (ui_button(u, rect(width - 112, y + 5, 104, 26), u8"철거 X", 1, a->demolish,
                  u8"도로 연결을 보존하는 철거")) {
        a->demolish = !a->demolish;
        a->build_type = -1;
    }
    CoreBuildingView *selected = game_building(&a->session, a->selected_building);
    int rotate = a->build_type >= 0 ? a->build_type : selected ? selected->type : -1;
    CoreExpansionBuildingInfo info = {0};
    if (rotate >= 0)
        core_expansion_building_info(rotate, &info);
    int can_rotate =
        rotate >= 0 && rotate != CORE_BLD_KEEP && !(info.flags & (CORE_EXP_WALL | CORE_EXP_ROAD));
    if (ui_button(u, rect(width - 112, y + 34, 104, 26), u8"회전 R", can_rotate, 0, NULL)) {
        if (a->build_type >= 0)
            a->rotation = (a->rotation + 1) % 4;
        else if (selected) {
            int32_t reason = 0;
            game_app_result(
                a,
                core_rotate_building(a->session.world, selected->id, (selected->rotation + 1) % 4, &reason),
                reason, 1);
        }
    }
    int list[CORE_BLD_COUNT], count = 0;
    for (int order = 0; order < CORE_BLD_COUNT - 1; order++) {
        int type = core_expansion_display_building(order);
        if (type > 0 && a->session.building_defs[type].category == a->category)
            list[count++] = type;
    }
    int columns = (int)((width - 16 + 8) / 168);
    if (columns < 1)
        columns = 1;
    float card = fminf(192, (width - 16 - (columns - 1) * 8) / columns);
    int max = count - columns;
    if (max < 0)
        max = 0;
    if (a->build_scroll > max)
        a->build_scroll = max;
    float old_font = u->font_size;
    u->font_size = 15;
    for (int column = 0; column < columns && a->build_scroll + column < count; column++) {
        int type = list[a->build_scroll + column];
        CoreBuildingDefView *d = &a->session.building_defs[type];
        core_expansion_building_info(type, &info);
        int group = d->required_rank, missing = 0;
        char cost[700], tip[1800];
        cost_text(d, cost, sizeof(cost));
        for (int r = 0; r < CORE_RES_COUNT; r++)
            missing |= a->session.summary.resources[r] < d->cost[r];
        int unlocked = a->session.summary.rank >= d->required_rank,
            locked = core_expansion_build_locked(a->session.world);
        snprintf(tip, sizeof(tip),
                 u8"%s 해금%s\n%s · %d×%d칸\n%s\n직원 %d · 공사 %d시간 · 유지 %d금/일\n비용: %s\n%s",
                 core_expansion_rank_name(d->required_rank), unlocked ? u8" · 충족" : u8" · 등급 부족",
                 info.name, d->w, d->h, info.description, d->worker_slots, info.build_hours, d->upkeep, cost,
                 locked    ? u8"습격 중 건설 불가"
                 : missing ? u8"건설 재료 부족"
                           : u8"배치 시 지형·도로·시야를 다시 검사합니다.");
        Rectangle r = rect(8 + column * (card + 8), y + 66, card, 148);
        Color fill = colors[group < 0 ? 0 : group];
        DrawRectangleRounded(r, .08f, 4, Fade(fill, .18f));

        Rectangle source = {0};
        Texture2D texture = game_building_preview(a, type, &source);
        float scale = fminf((r.width - 22) / source.width, 67 / source.height);
        DrawTexturePro(texture, source,
                       rect(r.x + (r.width - source.width * scale) / 2, r.y + 5, source.width * scale,
                            source.height * scale),
                       (Vector2){0, 0}, 0, WHITE);
        ui_wrap(u, rect(r.x + 7, r.y + 74, r.width - 14, 23), UI_TEXT, info.name);
        float cx = r.x + 8, cy = r.y + 92;
        for (int order = 0; order < CORE_RES_COUNT; order++) {
            int resource = core_expansion_display_resource(order);
            if (!d->cost[resource])
                continue;
            char amount[24];
            snprintf(amount, sizeof(amount), "%d", d->cost[resource]);
            float need = 20 + MeasureTextEx(a->assets.font, amount, 12, 0).x + 8;
            if (cx + need > r.x + r.width - 5) {
                cx = r.x + 8;
                cy += 18;
            }
            Texture2D icon = a->assets.resources[resource];
            DrawTexturePro(icon, rect(0, 0, (float)icon.width, (float)icon.height), rect(cx, cy, 16, 16),
                           (Vector2){0, 0}, 0, WHITE);
            DrawTextEx(a->assets.font, amount, (Vector2){cx + 19, cy + 2}, 12, 0,
                       a->session.summary.resources[resource] < d->cost[resource] ? UI_BAD : UI_TEXT);
            cx += need;
        }
        if (!unlocked)
            DrawRectangleRounded(r, .08f, 4, (Color){19, 25, 35, 115});
        DrawRectangleLinesEx(r, a->build_type == type ? 3.0f : 1.5f, fill);

        if (CheckCollisionPointRec(GetMousePosition(), r)) {
            snprintf(u->tooltip, sizeof(u->tooltip), "%s", tip);
            u->tooltip_bad = !unlocked;
            if (!u->input_blocked && !u->consumed && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
                u->consumed = 1;
                if (unlocked && !missing && !locked) {
                    a->build_type = type;
                    a->demolish = 0;
                } else
                    game_app_notice(a, locked    ? u8"습격 중 건설 불가"
                                       : missing ? u8"건설 자원 부족"
                                                 : u8"해금 등급 부족");
            }
        }
    }
    u->font_size = old_font;
    if (ui_button(u, rect(width - 84, y + 218, 34, 25), "<", a->build_scroll > 0, 0, NULL))
        a->build_scroll--;
    if (ui_button(u, rect(width - 44, y + 218, 34, 25), ">", a->build_scroll < max, 0, NULL))
        a->build_scroll++;
    if (a->build_type >= 0)
        ui_small(u, 12, y + 224, UI_GOLD, u8"%s · R 회전 · 좌클릭 배치 · 우클릭 취소",
                 core_expansion_building_name(a->build_type));
    else
        ui_small(u, 12, y + 224, UI_MUTED, u8"WASD 이동 · 휠 확대 · V 주민 · F1 도움말");
}
static void inspector(GameApp *a) {
    float x = GetScreenWidth() - 340.0f, y = 145, width = 326;
    GameUi *u = &a->ui;
    GameSession *s = &a->session;
    CoreBuildingView *b = game_building(s, a->selected_building);
    CoreVillagerView *v = game_villager(s, a->selected_villager);
    int blocked = u->input_blocked;
    Rectangle viewport = rect(x - 6, 135, 346, GetScreenHeight() - 388.0f);
    ui_box(rect(x - 6, 135, 346, GetScreenHeight() - 388.0f), UI_PANEL);
    if (CheckCollisionPointRec(GetMousePosition(), viewport)) {
        a->inspector_scroll -= (int)(GetMouseWheelMove() * 50);
        if (a->inspector_scroll < 0)
            a->inspector_scroll = 0;

    } else
        u->input_blocked = 1;
    y -= a->inspector_scroll;
    ui_clip_begin((int)viewport.x, (int)viewport.y, (int)viewport.width, (int)viewport.height);
    if (a->build_type >= 0 && !a->demolish) {
        CoreExpansionBuildingInfo info;
        core_expansion_building_info(a->build_type, &info);
        CoreBuildingView candidate = {0};
        candidate.type = a->build_type;
        candidate.x = a->build_preview_x;
        candidate.y = a->build_preview_y;
        candidate.rotation = a->rotation;
        candidate.w = (a->rotation & 1) ? info.h : info.w;
        candidate.h = (a->rotation & 1) ? info.w : info.h;
        candidate.state = CORE_BSTATE_ACTIVE;
        candidate.worker_slots = info.workers;
        candidate.job_field = info.job_field;
        ui_text(u, x + 8, y, UI_GOLD, u8"%s · 건설 후보", info.name);
        y += 40;
        y = game_facility_details(a, &candidate, x + 8, y, width - 24);
    } else if (game_ui_merchant_details(a, x + 8, &y, width - 24)) {
    } else if (game_unit_details(a, x + 8, &y, width - 24)) {
    } else if (b) {
        ui_text(u, x + 8, y, UI_GOLD, "%s #%d", core_expansion_building_name(b->type), b->type_serial);
        y += 40;
        ui_small(u, x + 8, y, UI_TEXT, u8"체력 %d/%d · 거주 %d/%d", b->hp, b->max_hp, b->residents,
                 b->resident_slots);
        y += 30;
        y = game_ui_services_details(a, b, x + 8, y, width - 24);
        y = game_facility_details(a, b, x + 8, y, width - 24);
        {
            CoreExpansionBuildingInfo info;
            (void)core_expansion_building_info(b->type, &info);
            ui_wrap(u, rect(x + 8, y, width - 24, 115), UI_MUTED, info.description);
            y += 120;
            if (b->type == CORE_BLD_KEEP)
                ui_small(u, x + 8, y, UI_TEXT, u8"정면 고정 · 유지비 %d금/일", b->upkeep);
            else if (b->kind_flags & CORE_EXP_WALL)
                ui_small(u, x + 8, y, UI_TEXT, u8"주변 벽 자동 연결 · 유지비 %d금/일", b->upkeep);
            else
                ui_small(u, x + 8, y, UI_TEXT, u8"회전 %d° · 유지비 %d금/일", b->rotation * 90, b->upkeep);
            y += 32;
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
                static const int kinds[7] = {7, 9, 8, 10, 32, 33, 34};
                int counts[7] = {0}, total = 0;
                for (int n = 0; n < s->animal_count; n++)
                    if (s->animals[n].alive && s->animals[n].owner == b->id)
                        for (int k = 0; k < 7; k++)
                            if (s->animals[n].kind == kinds[k]) {
                                counts[k]++;
                                total++;
                            }
                ui_small(u, x + 8, y, UI_TEXT, u8"실제 가축 %d/%d", total,
                         b->type == CORE_BLD_RANCH ? 6 : 16);
                y += 28;
                for (int k = 0; k < 7; k++) {
                    CoreLivestockInfo data;
                    CoreAnimalInfo species;
                    core_expansion_livestock_info(kinds[k], b->type, &data);
                    core_expansion_animal_info(kinds[k], &species);
                    if (counts[k]) {
                        int young = 0, age = 0;
                        for (int n = 0; n < s->animal_count; n++)
                            if (s->animals[n].alive && s->animals[n].owner == b->id &&
                                s->animals[n].kind == kinds[k]) {
                                young += s->animals[n].age < data.mature_days;
                                if (s->animals[n].age > age)
                                    age = s->animals[n].age;
                            }
                        ui_small(u, x + 8, y, UI_MUTED, u8"%s 새끼%d/성체%d · %d/%d일", species.name, young,
                                 counts[k] - young, age, data.life_days);
                        y += 23;
                        ui_small(u, x + 8, y, UI_MUTED, u8"번식 %.2f/%d일 · 다음%d마리%s",
                                 b->breeding_progress[data.slot] / 1000.0, data.period_days, data.babies,
                                 data.allowed ? "" : u8" (기존 보유분만 유지)");
                        y += 23;
                    }
                }
                ui_small(u, x + 8, y, UI_MUTED, u8"지급 사료 %.3f · 실제 효율 %.2f", b->feed_milli / 1000.0,
                         b->labor_milli / 1000.0);
                y += 28;
                int button = 0;
                for (int k = 0; k < 7; k++) {
                    CoreLivestockInfo data;
                    CoreAnimalInfo species;
                    core_expansion_livestock_info(kinds[k], b->type, &data);
                    if (!data.allowed)
                        continue;
                    core_expansion_animal_info(kinds[k], &species);
                    char label[80];
                    snprintf(label, sizeof(label), u8"%s %d금", species.name, data.price);
                    if (ui_button(
                            u, rect(x + 8 + button % 2 * 150, y + button / 2 * 40, 145, 34), label,
                            s->summary.resources[CORE_RES_GOLD] >= data.price &&
                                total < (b->type == CORE_BLD_RANCH ? 6 : 16),
                            0,
                            u8"새끼를 구매합니다. 사료를 먼저 지불하며 종별 성숙/번식/노화 규칙을 따릅니다."))
                        command(a, core_expansion_buy_animal(s->world, b->id, kinds[k]), 0);
                    button++;
                }
                y += ((button + 1) / 2) * 40 + 8;
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
        if (a->profile_home) {
            if (ui_button(u, rect(x + 8, y, width - 24, 32), u8"거주 건물로 돌아가기", 1, 0, NULL)) {
                int home = a->profile_home;
                a->profile_home = 0;
                game_select(a, 1, home);
            }
            y += 42;
        }
        y = game_person_details(a, v, x + 8, y, width - 24);
        if (ui_button(u, rect(x + 8, y, width - 24, 36), u8"일터 직접 선택", 1, 0, NULL))
            game_app_open(a, PANEL_ASSIGN);
        y += 44;
        if (ui_button(u, rect(x + 8, y, width - 24, 36), u8"일터에서 빼기", v->workplace_id != 0, 0, NULL))
            command(a, core_unassign_worker(s->world, v->id), 0);
        y += 44;
        if (ui_button(u, rect(x + 8, y, width - 24, 36), u8"자동 배치에 맡기기", 1, 0, NULL))
            command(a, core_release_worker(s->world, v->id), 0);
        y += 44;
        CoreExileView check;
        core_expansion_can_expel(s->world, v->id, &check);
        const char *why = check.reason == CORE_EXILE_FLOOR8
                              ? u8"영구 주민은 떠날 예정인 인원을 제외해도 8명 이상이어야 합니다."
                          : check.reason == CORE_EXILE_NO_PATH ? u8"지도 경계까지 걸어갈 경로가 없습니다."
                          : check.reason ? u8"이미 떠나는 주민이거나 추방할 수 없습니다."
                                         : u8"일터와 주거를 해제하고 실제로 영지를 떠납니다.";
        if (CheckCollisionPointRec(GetMousePosition(), rect(x + 8, y, width - 24, 36)))
            snprintf(u->tooltip, sizeof(u->tooltip), "%s", why);
        if (ui_color_button(u, rect(x + 8, y, width - 24, 36), u8"추방", !check.reason, 0, UI_BAD)) {
            a->confirm_action = 5;
            a->confirm_entity = v->id;
            snprintf(
                a->confirm_text, sizeof(a->confirm_text),
                u8"%s 주민을 추방하시겠습니까? 승인 후 일터와 주거가 해제되며 영지 경계까지 걸어서 떠납니다.",
                v->name);
            game_app_open(a, PANEL_CONFIRM);
        }
        DrawRectangleLinesEx(rect(x + 8, y, width - 24, 36), 1, UI_BAD);
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
    int max_scroll = (int)fmaxf(0, y + a->inspector_scroll - viewport.y - viewport.height + 24);
    if (a->inspector_scroll > max_scroll)
        a->inspector_scroll = max_scroll;
    ui_clip_end();
    u->input_blocked = blocked;
}
static void notices(GameApp *a) {
    float bottom = fminf(GetScreenHeight() - 104.0f, GetScreenHeight() - 248.0f - 12);
    float y = bottom;
    int shown = 0;
    a->notice_top = a->notice_bottom = (int)bottom;
    for (int i = a->notice_count - 1; i >= 0 && shown < (a->message[0] ? 2 : 3); i--) {
        GameNotice *n = &a->notices[i];
        if (n->age > 16)
            continue;
        float height = 44;
        y -= height;
        if (y < 136 + a->status_height)
            break;
        Rectangle area = rect(12, y, 390, height);
        if (a->toast_area_count < 4)
            a->toast_areas[a->toast_area_count++] = area;
        ui_box(area, Fade(UI_PANEL, n->age > 13 ? (16 - n->age) / 3 : .94f));
        ui_wrap(&a->ui, rect(22, y + 5, 370, 34), UI_TEXT, n->text);
        if (CheckCollisionPointRec(GetMousePosition(), area)) {
            snprintf(a->ui.tooltip, sizeof(a->ui.tooltip), "%s", n->text);
            if (a->panel == PANEL_NONE && !a->raid_alert_id && n->has_position &&
                IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
                a->camera.target = (Vector2){(n->x + .5f) * 16, (n->y + .5f) * 16};
        }
        y -= 6;
        shown++;
        a->notice_top = (int)y;
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
    if (row_button(a, b, &y, u8"튜토리얼", 1))
        (void)game_tutorial_start(a); /* 메인 화면 ↔ 튜토리얼의 약속 함수 (ui/tutorial.h) */
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
    float y = b.y + 102;
    int slot;
    for (slot = saving ? 1 : 0; slot <= 3; slot++) {
        char path[GAME_PATH_BYTES], summary[512], label[600];
        CoreSaveInfoView info;
        int ok, backup = 0;
        game_app_slot_path(a, slot, 0, path);
        ok = core_peek_save(path, a->session.defs, &info) == CORE_OK;
        if (ok)
            (void)snprintf(summary, sizeof(summary), u8"%s · %d년 %s · %d일 %02d:%02d · %s · %d명",
                           info.castle_name, info.year, fmt_season_name(info.season), info.day,
                           info.minute_of_day / 60, info.minute_of_day % 60,
                           core_expansion_rank_name(info.rank), info.population);
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
                (void)game_app_load(a, slot, !ok && backup);
        }
        y += 96;
    }
    ui_small(&a->ui, b.x + 28, b.y + b.height - 50, UI_MUTED,
             u8"전투 중 상태도 저장합니다. 현재 저장 스키마 %d", CORE_SAVE_VERSION);
}
static void settings(GameApp *a) {
    Rectangle b = modal(a, u8"설정", 760, 610);
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
    (void)snprintf(label, sizeof(label), u8"화면 효과: %s", a->settings.low_effects ? u8"낮음" : u8"기본");
    if (row_button(a, b, &y, label, 1))
        a->settings.low_effects = !a->settings.low_effects;
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
    DrawTexturePro(a->assets.territory_icon, (Rectangle){0, 0, 64, 64},
                   rect(b.x + b.width - 80, y - 6, 38, 38), (Vector2){0, 0}, 0, WHITE);
    ui_text(u, x, y, UI_GOLD, u8"현재 %s · 다음 %s", core_expansion_rank_name(s->rank), r.name);
    y += 45;
    if (s->promotion_state) {
        ui_small(u, x, y, UI_GOLD, u8"영주관 승급 공사 %d%% · 남은 %.1f시간",
                 (144 - s->promotion_remaining) * 100 / 144, s->promotion_remaining / 6.0);
        y += 32;
    }
    if (s->rank < 5) {
        CoreRankRequirementView rows[18];
        int count = core_expansion_rank_requirements(a->session.world, rows, 18);
        float row_width = (b.width - 76) / 2;
        for (int k = 0; k < count; k++) {
            CoreRankRequirementView *row = &rows[k];
            float xx = x + (k % 2) * (row_width + 12), yy = y + (k / 2) * 30;
            Color color = row->satisfied ? UI_GOOD : UI_BAD;
            ui_small(u, xx, yy, color, u8"%s %d / %d", row->label, row->current, row->required);
            Rectangle hit = rect(xx, yy - 3, row_width, 28);
            if (row->type_count && CheckCollisionPointRec(GetMousePosition(), hit)) {
                snprintf(u->tooltip, sizeof(u->tooltip), u8"%s\n%s", row->label,
                         row->building_id ? u8"완공 시설로 이동" : u8"해당 건설 카드 열기");
                if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
                    if (row->building_id) {
                        CoreBuildingView *target = game_building(&a->session, row->building_id);
                        if (target) {
                            game_select(a, 1, target->id);
                            a->camera.target = (Vector2){(target->x + target->w * .5f) * 16,
                                                         (target->y + target->h * .5f) * 16};
                        }
                    } else {
                        a->build_type = row->types[0];
                        a->category = a->session.building_defs[a->build_type].category;
                        a->build_scroll = 0;
                    }
                    game_app_close(a);
                    return;
                }
            }
        }
        y += ((count + 1) / 2) * 30 + 12;
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
    Rectangle b = modal(a, contracts ? u8"계약 주민 · 12일 선불 고용" : u8"거래 · 개별 자원 거래", 1080, 760);
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
            game_portrait(a, c->appearance, rect(b.x + 30, y, 64, 64));
            ui_text(&a->ui, b.x + 108, y, UI_GOLD, u8"%s · %s · %d세", c->name, fields[c->field], c->age);
            game_stat_numbers(a, c->stats, b.x + 108, y + 32, 48);
            ui_small(&a->ui, b.x + 108, y + 60, UI_MUTED, u8"주 분야 근무 %.1f일 · 개인 번호 %d",
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
        if (!m.open) {
            ui_wrap(&a->ui, rect(b.x + 30, b.y + 85, b.width - 60, 100), UI_BAD,
                    u8"가판대·시장·교역소를 짓고 일꾼을 배치하면 거래할 수 있습니다.");
            return;
        }
        CoreFacilityView trade = {0};
        for (int n = 0; n < s->building_count; n++)
            if (s->buildings[n].type == CORE_BLD_STALL || s->buildings[n].type == CORE_BLD_MARKET ||
                s->buildings[n].type == CORE_BLD_TRADING_POST) {
                CoreBuildingView *shop = &s->buildings[n];
                core_expansion_facility(s->world, shop->id, shop->type, shop->x, shop->y, shop->rotation,
                                        &trade);
                break;
            }
        ui_small(&a->ui, b.x + 30, b.y + 74, UI_TEXT, u8"거래 %d / %d개 · 기준 %d · 직원 E %.2f · 보정 %+d개",
                 trade.trade_used, trade.trade_limit, trade.trade_base, trade.trade_efficiency / 100.0,
                 trade.trade_limit - trade.trade_base);
        ui_small(&a->ui, b.x + 30, b.y + 101, trade.present ? UI_MUTED : UI_BAD,
                 u8"현장 %d명 · 다음 재입고 %d일 · %s", trade.present, trade.trade_next_day,
                 trade.present ? u8"직원 변경은 한도에 즉시 반영 · 사용량 유지"
                               : u8"현재 근무 직원 없음 · 08~17시 근무");
        int goods = 0;
        for (int resource = 1; resource < 15; resource++)
            goods += core_expansion_tradable(s->world, resource);
        y = scroll_start(a, b, goods, 64) + 72;
        for (int display = 1; display < 15; display++) {
            i = core_expansion_display_resource(display);
            if (!core_expansion_tradable(s->world, i))
                continue;
            float row = y;
            y += 64;
            CoreTradeQuote q1, q10, qs;
            core_expansion_quote(s->world, i, 1, &q1);
            core_expansion_quote(s->world, i, 10, &q10);
            core_expansion_quote(s->world, i, -10, &qs);
            int32_t reason = 0, gold = 0;
            char label[96];
            int enabled = m.open && q1.reason != CORE_DENY_NOT_TRADABLE,
                stock = core_expansion_market_stock(s->world, i);
            if (row < b.y + 148 || row + 54 > b.y + b.height - 45)
                continue;
            ui_small(&a->ui, b.x + 30, row + 8, UI_TEXT, u8"%s · 보유 %d", fmt_resource_name(i),
                     s->summary.resources[i]);
            ui_small(&a->ui, b.x + 210, row + 8, stock == 0 ? UI_BAD : UI_TEXT, u8"재고 %d", stock);
            ui_small(&a->ui, b.x + 30, row + 31, UI_MUTED, u8"매수 %.2f금/개 · %+.2f%%",
                     q1.unit_cents / 100.0, q1.rate_bp / 100.0);
            char quote_tip[512];
            snprintf(quote_tip, sizeof(quote_tip),
                     u8"매수 %.2f금/개 · 시세 %+.2f%%\n계절 %+.2f%% / 사건 %+.2f%% / 거래 압력 %+.2f%%\n매도 "
                     u8"%.2f금/개 · 일일 잔여 %d개\n주문별 실제 총액은 반올림 잔액을 포함합니다.",
                     q1.unit_cents / 100.0, q1.rate_bp / 100.0, q1.season_bp / 100.0, q1.event_bp / 100.0,
                     q1.pressure_bp / 100.0, qs.unit_cents / 100.0, q1.remaining);
            if (CheckCollisionPointRec(GetMousePosition(), rect(b.x + 25, row, b.width - 50, 58)))
                snprintf(a->ui.tooltip, sizeof(a->ui.tooltip), "%s", quote_tip);
            (void)snprintf(label, sizeof(label), u8"1개 구매 %d금", q1.total_gold);
            if (ui_button(&a->ui, rect(b.x + b.width - 610, row, 180, 38), label, enabled && !q1.reason, 0,
                          quote_tip)) {
                int rc = core_trade(s->world, i, 1, &gold, &reason);
                command(a, rc, reason);
            }
            (void)snprintf(label, sizeof(label), u8"10개 구매 %d금", q10.total_gold);
            if (ui_button(&a->ui, rect(b.x + b.width - 420, row, 180, 38), label, enabled && !q10.reason, 0,
                          quote_tip)) {
                int rc = core_trade(s->world, i, 10, &gold, &reason);
                command(a, rc, reason);
            }
            (void)snprintf(label, sizeof(label), u8"10개 판매 %d금", qs.total_gold);
            if (ui_button(&a->ui, rect(b.x + b.width - 230, row, 190, 38), label, enabled && !qs.reason, 0,
                          quote_tip)) {
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
        result = (a->personal_id > b->personal_id) - (a->personal_id < b->personal_id);
    else if (sort_column == 1)
        result = strcmp(a->name, b->name);
    else if (sort_column == 2)
        result = (a->job > b->job) - (a->job < b->job);
    else
        result = (a->happiness > b->happiness) - (a->happiness < b->happiness);
    if (!result)
        return (a->id > b->id) - (a->id < b->id);
    return sort_reverse ? -result : result;
}

static void villagers(GameApp *a) {
    Rectangle b = modal(a, u8"주민 목록 · 제목을 눌러 정렬", 1120, 760);
    GameSession *s = &a->session;
    float y;
    int i;
    CoreImmigrationView imm;
    core_expansion_immigration(s->world, &imm);
    const char *status = imm.status == CORE_IMM_ARRIVING  ? u8"이주단 이동 중"
                         : imm.status == CORE_IMM_WAIT    ? u8"재이주 대기"
                         : imm.status == CORE_IMM_BLOCKED ? u8"재심사 가능 · 조건 부족"
                         : imm.status == CORE_IMM_SPECIAL ? u8"특별 이주 예정"
                         : imm.status == CORE_IMM_REFILL  ? u8"개척지 보충"
                                                          : u8"조건 충족 · 다음 06시 심사";
    static const char *const causes[] = {u8"대기 기간", u8"집 부족",   u8"물 부족",    u8"도로 미연결",
                                         u8"식량 부족", u8"역병/재난", u8"행복도 부족"};
    const char *cause = u8"충족";
    char details[512];
    snprintf(details, sizeof(details),
             u8"전체 거절 사유: %s%s%s%s%s%s%s\n보충 이주는 일반 조건과 무관합니다.",
             imm.reasons & 1 ? u8"대기 기간 / " : "", imm.reasons & 2 ? u8"집 부족 / " : "",
             imm.reasons & 4 ? u8"물 부족 / " : "", imm.reasons & 8 ? u8"도로 미연결 / " : "",
             imm.reasons & 16 ? u8"식량 부족 / " : "", imm.reasons & 32 ? u8"역병·재난 / " : "",
             imm.reasons & 64 ? u8"행복도 부족" : "");
    for (i = 0; i < 7; i++)
        if (imm.reasons & (1 << i)) {
            cause = causes[i];
            break;
        }
    Rectangle summary = rect(b.x + 28, b.y + 63, b.width - 56, 106);
    ui_box(summary, UI_BG);
    ui_small(&a->ui, b.x + 38, b.y + 71, UI_GOLD, u8"영구 이주 · %s · 영구 주민 %d명", status, imm.permanent);
    int check_day = s->summary.day + (s->summary.minute_of_day >= 360);
    if (check_day < imm.next_day)
        check_day = imm.next_day;
    if (!s->summary.rank) {
        ui_small(&a->ui, b.x + 38, b.y + 95, UI_TEXT,
                 u8"특별 이주 %d/2회 도착 · 인구 %d/8 · 다음 예정 %d일 · 마지막 합류 %d일", imm.special_waves,
                 imm.permanent, imm.next_day, imm.last_day);
        ui_small(&a->ui, b.x + 38, b.y + 120, UI_MUTED,
                 u8"특별 이주는 2명씩 · 두 번째 합류 뒤 3일마다 1명 보충 · 일반 조건 제한 없음");
    } else {
        ui_small(&a->ui, b.x + 38, b.y + 95, UI_TEXT,
                 u8"최근 합류 %d일 · 대기 해제 %d일 · 다음 심사 %d일 06:00 · 우선 사유: %s", imm.last_day,
                 imm.next_day, check_day, cause);
        ui_small(&a->ui, b.x + 38, b.y + 120, UI_TEXT,
                 u8"행복 %d/%d · 빈집 %d · 우물 %d/%d · 식량 %.0f/%.0f · 예상 %d~%d명", imm.happiness,
                 imm.required_happiness, imm.vacant, imm.water_served, imm.permanent,
                 imm.food_available / 1000.0, imm.food_needed / 1000.0, imm.minimum, imm.maximum);
    }
    ui_small(&a->ui, b.x + 38, b.y + 145, UI_MUTED,
             u8"조건·규모는 심사 때 다시 계산합니다. 전체 사유는 마우스를 올려 확인하세요.");
    if (CheckCollisionPointRec(GetMousePosition(), summary))
        snprintf(a->ui.tooltip, sizeof(a->ui.tooltip), "%s", details);
    static const char *const labels[] = {u8"번호", u8"이름", u8"일터", u8"행복도"};
    for (i = 0; i < 4; i++)
        if (ui_button(&a->ui, rect(b.x + 28 + i * (b.width - 56) / 4, b.y + 179, (b.width - 64) / 4, 36),
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
    Rectangle list = rect(b.x + 28, b.y + 230, b.width - 56, b.height - 263);
    int maximum = (int)fmaxf(0, s->villager_count * 72.0f - list.height);
    if (CheckCollisionPointRec(GetMousePosition(), list))
        a->scroll -= (int)(GetMouseWheelMove() * 72);
    if (a->scroll < 0)
        a->scroll = 0;
    if (a->scroll > maximum)
        a->scroll = maximum;
    y = list.y - a->scroll;
    ui_clip_begin((int)list.x, (int)list.y, (int)list.width, (int)list.height);
    for (i = 0; i < s->villager_count; i++, y += 72) {
        CoreVillagerView *v = &s->villagers[i];
        char label[300], workplace[120];
        game_building_label(s, v->workplace_id, workplace, sizeof(workplace));
        if (y + 66 < list.y || y > list.y + list.height)
            continue;
        (void)snprintf(label, sizeof(label), u8"#%d %s | %s | 행복 %d · 기분 %+d | 체력 %d | %s",
                       v->personal_id, v->name, workplace, v->happiness, v->mood, v->health,
                       fmt_villager_state(v->state));
        if (ui_button(&a->ui, rect(list.x, y, list.width, 66), "", 1, a->selected_villager == v->id, label)) {
            GameActor *actor = game_actor(s, v->id);
            game_select(a, 2, v->id);
            a->inspector_scroll = 0;
            if (actor)
                a->camera.target = (Vector2){actor->position.x * 16, actor->position.y * 16};
            game_app_close(a);
        }
        game_portrait(a, v->appearance, rect(list.x + 6, y + 5, 56, 56));
        ui_small(&a->ui, list.x + 74, y + 7, UI_TEXT, u8"#%d %s · %s", v->personal_id, v->name, workplace);
        ui_small(&a->ui, list.x + 74, y + 34, UI_MUTED, u8"행복%d · 기분%+d · %s", v->happiness, v->mood,
                 fmt_villager_state(v->state));
        int values[5] = {v->strength, v->agility, v->vitality, v->dexterity, v->intellect};
        game_stat_numbers(a, values, list.x + list.width - 196, y + 22, 37);
    }
    ui_clip_end();
    if (maximum > 0)
        ui_bar(rect(b.x + b.width - 18, list.y, 4, list.height), a->scroll / (float)maximum, UI_GOLD);
}
static int shift_occupied(GameApp *a, int building, int shift, int resident) {
    for (int i = 0; i < a->session.villager_count; i++) {
        CoreVillagerView *v = &a->session.villagers[i];
        if (v->workplace_id != building || v->id == resident)
            continue;
        CoreWorkSchedule schedule;
        core_expansion_schedule(a->session.world, v->id, &schedule);
        if (schedule.shift == shift)
            return 1;
    }
    return 0;
}
static void assign(GameApp *a) {
    Rectangle b = modal(a, u8"일꾼 직접 배치", 1080, 760);
    GameSession *s = &a->session;
    int building = a->selected_building, source_count = building ? s->villager_count : s->building_count,
        valid = 0;
    CoreBuildingView *fixed = game_building(s, building);
    CoreVillagerView *selected = game_villager(s, a->selected_villager);
    int tower = fixed && s->building_defs[fixed->type].category == CORE_CAT_TOWERS;
    if (a->assign_shift < 0 || a->assign_shift > 1)
        a->assign_shift = 0;
    if (tower)
        for (int shift = 0; shift < 2; shift++) {
            if (ui_button(&a->ui, rect(b.x + 28 + shift * 240, b.y + 66, 230, 34),
                          shift ? u8"야간 18:00~06:00" : u8"주간 06:00~18:00", 1, a->assign_shift == shift,
                          u8"빈 근무조를 선택한 뒤 주민을 누르세요."))
                a->assign_shift = shift;
        }
    if (!building && selected) {
        game_portrait(a, selected->appearance, rect(b.x + 28, b.y + 62, 45, 45));
        ui_small(&a->ui, b.x + 86, b.y + 72, UI_GOLD, u8"%s · 방어탑은 주간/야간을 선택하세요",
                 selected->name);
    }
    for (int i = 0; i < source_count; i++) {
        if (building) {
            if (!s->villagers[i].incoming && s->villagers[i].state != CORE_VS_LEAVING)
                valid++;
        } else if (s->buildings[i].worker_slots && s->buildings[i].state == CORE_BSTATE_ACTIVE)
            valid++;
    }
    Rectangle list = rect(b.x + 28, b.y + 119, b.width - 56, b.height - 151);
    int maximum = (int)fmaxf(0, valid * 80.0f - list.height);
    if (CheckCollisionPointRec(GetMousePosition(), list))
        a->scroll -= (int)(GetMouseWheelMove() * 80);
    if (a->scroll < 0)
        a->scroll = 0;
    if (a->scroll > maximum)
        a->scroll = maximum;
    float y = list.y - a->scroll;
    ui_clip_begin((int)list.x, (int)list.y, (int)list.width, (int)list.height);
    for (int i = 0; i < source_count; i++) {
        CoreVillagerView *v = building ? &s->villagers[i] : selected;
        CoreBuildingView *job = building ? fixed : &s->buildings[i];
        if (!v || !job || (!building && (!job->worker_slots || job->state != CORE_BSTATE_ACTIVE)) ||
            (building && (v->incoming || v->state == CORE_VS_LEAVING)))
            continue;
        float row = y;
        y += 80;
        if (row + 72 < list.y || row > list.y + list.height)
            continue;
        char work[120], tip[400];
        game_building_label(s, building ? v->workplace_id : job->id, work, sizeof(work));
        int32_t efficiency = 100;
        core_expansion_efficiency(s->world, v->id, job->type, &efficiency);
        int field = job->job_field, minutes = field >= 0 ? v->skill_minutes[field] : 0;
        snprintf(tip, sizeof(tip), u8"%s · 현재 일터 %s · 배치 대상 %s · E %.2f · 숙련 %.1f일 · %s", v->name,
                 work, core_expansion_building_name(job->type), efficiency / 100.0, minutes / 540.0,
                 fmt_villager_state(v->state));
        int is_tower = s->building_defs[job->type].category == CORE_CAT_TOWERS;
        if (is_tower && !building) {
            ui_box(rect(list.x, row, list.width, 72), UI_BG);
            for (int shift = 0; shift < 2; shift++)
                if (ui_button(&a->ui, rect(list.x + list.width - 166 + shift * 80, row + 18, 74, 36),
                              shift ? u8"야간" : u8"주간", !shift_occupied(a, job->id, shift, v->id), 0,
                              NULL)) {
                    int rc = core_expansion_assign_shift(s->world, v->id, job->id, shift);
                    command(a, rc, 0);
                    if (rc == CORE_OK)
                        game_app_close(a);
                }
        } else if (ui_button(&a->ui, rect(list.x, row, list.width, 72), "",
                             !is_tower || !shift_occupied(a, job->id, a->assign_shift, v->id), 0, tip)) {
            int rc = is_tower ? core_expansion_assign_shift(s->world, v->id, job->id, a->assign_shift)
                              : core_assign_worker(s->world, v->id, job->id);
            command(a, rc, 0);
            if (rc == CORE_OK)
                game_app_close(a);
        }
        game_portrait(a, v->appearance, rect(list.x + 5, row + 5, 62, 62));
        ui_small(&a->ui, list.x + 79, row + 7, UI_TEXT, u8"%s · %s", v->name, work);
        int values[5] = {v->strength, v->agility, v->vitality, v->dexterity, v->intellect};
        game_stat_numbers(a, values, list.x + 79, row + 40, 40);
        ui_small(&a->ui, list.x + 310, row + 40, UI_MUTED, u8"E %.2f · 숙련 %.1f일 · %s", efficiency / 100.0,
                 minutes / 540.0, fmt_villager_state(v->state));
    }
    ui_clip_end();
    if (maximum > 0)
        ui_bar(rect(b.x + b.width - 18, list.y, 4, list.height), a->scroll / (float)maximum, UI_GOLD);
}

static void ledger(GameApp *a) {
    Rectangle b = modal(a, u8"자원 장부", 920, 710);
    CoreLedgerView l;
    int i, r = a->category % CORE_RES_COUNT;
    float y = b.y + 84;
    GameUi *u = &a->ui;
    (void)core_get_ledger(a->session.world, &l);
    for (i = 0; i < CORE_RES_COUNT; i++) {
        int resource = core_expansion_display_resource(i);
        if (ui_button(u, rect(b.x + 24 + i % 5 * (b.width - 48) / 5, y + i / 5 * 35, (b.width - 60) / 5, 31),
                      fmt_resource_name(resource), 1, r == resource, NULL))
            a->category = resource;
    }
    y += 120;
    ui_text(u, b.x + 32, y, UI_GOLD, u8"항목                         오늘          어제");
    y += 42;
    for (i = 0; i < CORE_LEDGER_COUNT; i++, y += 28) {
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
        } else if (action == 5) {
            int rc = core_expansion_expel(a->session.world, a->confirm_entity);
            game_app_result(a, rc, 0, 0);
            game_app_close(a);
            game_session_refresh(&a->session);
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
    a->toast_area_count = 0;
    if (a->in_game) {
        a->ui.input_blocked = a->panel != PANEL_NONE || a->raid_alert_id;
        hud(a);
        a->ui.input_blocked = a->panel != PANEL_NONE || a->raid_alert_id;
        build_bar(a);
        inspector(a);
        notices(a);
        game_tutorial_draw(a); /* 튜토리얼 안내: HUD 위, 창 아래 */
        a->ui.input_blocked = a->raid_alert_id != 0;
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
                u8"공급합니다. 5~8일·10~13일에 각각2명이 조건 없이 이주합니다.\n"
                u8"경작지·농장·관개농장은 봄·여름의 실제 노동을 모아 25일 08시에 한 번 수확합니다. 텃밭은 "
                u8"매일 생산하며 겨울에는 절반입니다. 연료는 석탄→원목→목재 순입니다.\n"
                u8"촌락 이후 건물 입구를 성채 도로망에 연결하세요. 도로·성문을 지워 연결이 끊기면 철거가 "
                u8"거절됩니다. 무인 건물은 빨간 느낌표, 작업·주민 상태는 아이콘으로 표시합니다.\n"
                u8"적을 실제 발견한 순간 경보가 울리고 병영이 자동 출동합니다. 연구 24종과 여섯 등급을 "
                u8"진행합니다. 가까운 보호구역으로 피신하며 내부에서는 안전 욕구가 회복됩니다.\n"
                u8"고용 계약은 이동시간 포함 정확히12일 선불이며 승급 인구·세금에는 제외합니다. 매일 자동 "
                u8"저장하며 한글 이름은 Ctrl+V로 붙여넣을 수 있습니다.");
        break;
    }
    case PANEL_NONE:
        break;
    }
    if (a->message[0]) {
        float toast_y = fminf(GetScreenHeight() - 104.0f, GetScreenHeight() - 248.0f - 12) - 44;
        if (a->notice_top < a->notice_bottom)
            toast_y = (float)a->notice_top - 44;
        ui_box(rect(12, toast_y, fminf(GetScreenWidth() - 370.0f, 1000), 38), (Color){23, 20, 30, 245});
        ui_small(&a->ui, 22, toast_y + 8, UI_GOLD, "%s", a->message);
        Rectangle feedback = rect(12, toast_y, fminf(GetScreenWidth() - 370.0f, 1000), 38);
        if (a->toast_area_count < 4)
            a->toast_areas[a->toast_area_count++] = feedback;
        if (CheckCollisionPointRec(GetMousePosition(), feedback))
            snprintf(a->ui.tooltip, sizeof(a->ui.tooltip), "%s", a->message);
    }
    if (a->stress)
        ui_small(&a->ui, GetScreenWidth() - 330.0f, GetScreenHeight() - 34.0f, UI_GOLD,
                 u8"FPS %d · 주민 %d · 적 %d", GetFPS(), a->session.villager_count, a->session.enemy_count);
    game_ui_building_hover(a);
    if (a->in_game && a->panel == PANEL_NONE && !a->ui.tooltip[0]) {
        Vector2 screen = GetMousePosition(), point = GetScreenToWorld2D(screen, a->camera);
        int x = (int)floorf(point.x / 16), y = (int)floorf(point.y / 16);
        GameSession *s = &a->session;
        static const char *const biomes[] = {u8"초원", u8"숲",   u8"타이가", u8"산악", u8"습지",
                                             u8"설원", u8"사막", u8"호수",   u8"늪",   u8"사바나"};
        if (screen.y > 132 && screen.y < GetScreenHeight() - 248 && screen.x < GetScreenWidth() - 350 &&
            x >= 0 && y >= 0 && x < s->summary.map_width && y < s->summary.map_height && s->exp_tiles) {
            CoreExpansionTileView *tile = &s->exp_tiles[y * s->summary.map_width + x];
            if (tile->explored) {
                if (tile->water_id) {
                    CoreWaterView water;
                    if (core_expansion_water(s->world, tile->water_id, &water) == CORE_OK)
                        snprintf(a->ui.tooltip, sizeof(a->ui.tooltip),
                                 u8"%s 수역 #%d · 호수 판정\n면적 %d칸 · 공동 어획 한도 %.2f/일\n물가는 인접 "
                                 u8"생물군계에 따라 연결됩니다.",
                                 water.cold ? u8"차가운" : u8"따뜻한", water.id, water.area,
                                 water.capacity_milli / 1000.0);
                } else if (tile->tree_kind && tile->tree_logs_milli) {
                    static const char *const names[] = {u8"자작나무", u8"소나무", u8"참나무", u8"마력수"};
                    int mature = core_expansion_tree_mature_age(tile->tree_kind);
                    snprintf(a->ui.tooltip, sizeof(a->ui.tooltip),
                             u8"%s · %s\n성장 %.1f/%d일 · 원목 잔량 %.1f\n생물군계: %s",
                             names[tile->tree_kind - 1],
                             tile->tree_age < mature * 3 / 10 ? u8"묘목"
                             : tile->tree_age < mature        ? u8"유목"
                                                              : u8"성목",
                             tile->tree_age / 1000.0, mature / 1000, tile->tree_logs_milli / 1000.0,
                             biomes[tile->biome]);
                } else
                    snprintf(a->ui.tooltip, sizeof(a->ui.tooltip), "%s (%d,%d)", biomes[tile->biome], x, y);
                if (!tile->water_id) {
                    size_t used = strlen(a->ui.tooltip);
                    snprintf(a->ui.tooltip + used, sizeof(a->ui.tooltip) - used,
                             u8"\n토양 점토 %.1f / 최초 %.1f · 해안 보정 %d%%\n채토장에서만 채굴하며 자연 "
                             u8"재생하지 않습니다.",
                             tile->clay_milli / 1000.0, tile->clay_initial_milli / 1000.0,
                             tile->clay_shore_percent);
                }
            }
        }
    }
    game_raid_alert(a);
    ui_tooltip(&a->ui);
}
