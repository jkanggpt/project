/* v123_ui.c — 방문 상인, 가구 구성원, 급수·연구 예보 검사창 */
#include "app.h"
#include "format.h"
#include <stdio.h>
static Rectangle box(float x, float y, float w, float h) { return (Rectangle){x, y, w, h}; }
int game_v123_merchant_details(GameApp *a, float x, float *yy, float width) {
    if (a->selection_kind != 5)
        return 0;
    GameUi *u = &a->ui;
    CoreWanderingMerchantView m;
    core_expansion_merchant_view(a->session.world, &m);
    float y = *yy;
    ui_text(u, x, y, UI_GOLD, u8"떠돌이 상인");
    y += 36;
    DrawTexturePro(a->assets.merchant_portrait, (Rectangle){0, 0, 96, 58}, box(x, y, 76, 48), (Vector2){0, 0},
                   0, WHITE);
    ui_small(u, x + 85, y + 8, UI_TEXT, "%s",
             m.state == CORE_MERCHANT_TRADING    ? u8"교환 가능"
             : m.state == CORE_MERCHANT_ENTERING ? u8"영주관으로 이동"
             : m.state == CORE_MERCHANT_LEAVING  ? u8"퇴장 중"
                                                 : u8"다음 방문 대기");
    y += 60;
    if (m.state != CORE_MERCHANT_TRADING) {
        ui_wrap(u, box(x, y, width, 90), UI_MUTED,
                u8"영주관에 도착한 뒤 3일간 거래합니다. 다음 방문까지 24~36일입니다.");
        *yy = y + 100;
        return 1;
    }
    ui_small(u, x, y, UI_TEXT, u8"남은 시간 %.1f시간 · 방문 #%d",
             (m.expires_tick - a->session.summary.tick) / 6.0, m.visit_id);
    y += 29;
    for (int i = 0; i < m.offer_count; i++) {
        CoreBarterRecipe r;
        core_expansion_barter_recipe(m.recipe_ids[i], &r);
        CoreSummaryView *s = &a->session.summary;
        char title[256];
        int icon = !m.remaining[i] ? 2 : m.last_offer == r.id ? 1 : 0;
        DrawTexturePro(a->assets.soil_markers, (Rectangle){icon * 128.0f, 128, 128, 128},
                       box(x + width - 25, y, 22, 22), (Vector2){0, 0}, 0, WHITE);
        ui_small(u, x, y, UI_GOLD, u8"%s · 상대혜택 +%.1f%%", r.code, r.benefit_bp / 100.0);
        y += 24;
        int resources[2] = {r.pay_resource, r.get_resource}, amounts[2] = {r.pay_count, r.get_count};
        for (int k = 0; k < 2; k++) {
            Texture2D t = a->assets.resources[resources[k]];
            DrawTexturePro(t, box(0, 0, (float)t.width, (float)t.height), box(x, y, 23, 23), (Vector2){0, 0},
                           0, WHITE);
            ui_small(u, x + 30, y + 2, UI_TEXT, u8"%s %s %d", k ? u8"받기" : u8"내기",
                     fmt_resource_name(resources[k]), amounts[k]);
            y += 28;
        }
        int reason = m.remaining[i] < 1                           ? 1
                     : s->resources[r.pay_resource] < r.pay_count ? 2
                     : s->resource_capacity[r.get_resource] > 0 &&
                             s->resources[r.get_resource] + r.get_count > s->resource_capacity[r.get_resource]
                         ? 3
                         : 0;
        snprintf(title, sizeof(title),
                 reason == 1   ? u8"교환 완료 (0회)"
                 : reason == 2 ? u8"지불 자원 부족 (%d회)"
                 : reason == 3 ? u8"창고 공간 부족 (%d회)"
                               : u8"교환 (%d회 남음)",
                 m.remaining[i]);
        if (ui_button(u, box(x, y, width, 33), title, !reason, 0, NULL)) {
            int32_t deny = 0;
            int rc =
                core_expansion_merchant_trade(a->session.world, m.visit_id, r.id, m.last_nonce + 1, &deny);
            game_app_result(a, rc, deny, 0);
            game_session_refresh(&a->session);
            core_expansion_merchant_view(a->session.world, &m);
        }
        y += 47;
    }
    *yy = y;
    return 1;
}
float game_v123_building_details(GameApp *a, const CoreBuildingView *b, float x, float y, float width) {
    GameUi *u = &a->ui;
    y=game_v124_building_details(a,b,x,y,width);
    if (b->type == CORE_BLD_WELL || b->type == CORE_BLD_WATER_TOWER) {
        int32_t radius, capacity, served;
        core_expansion_water_provider(a->session.world, b->id, &radius, &capacity, &served);
        ui_small(u, x, y, UI_TEXT, u8"급수 %d/%d명 · 반경 %d", served, capacity, radius);
        y += 28;
        if (b->type == CORE_BLD_WATER_TOWER) {
            ui_wrap(u, box(x, y, width, 70), UI_GOOD,
                    u8"급수받는 주민의 위생 +6/일 (자연 감소 포함 +1). 우물과 "
                    u8"중첩되지 않습니다.");
            y += 76;
        }
    }
    if (b->type == CORE_BLD_OBSERVATORY) {
        CoreForecastView f;
        core_expansion_forecast(a->session.world, b->id, &f);
        ui_small(u, x, y, UI_GOLD, u8"사건 예보 · 최대 %d일", f.horizon);
        y += 28;
        if (f.reason) {
            ui_wrap(u, box(x, y, width, 76), UI_BAD,
                    u8"지능 5 이상 직원이 실제 근무해야 예보합니다. 근무 외 시간에는 "
                    u8"예보가 중지됩니다.");
            y += 82;
        }
        {
            ui_small(u, x, y, UI_MUTED, u8"%d일 갱신%s · 습격 제외", f.last_day,
                     f.stale ? u8" (과거 기록)" : "");
            y += 26;
            for (int i = 0; i < f.count; i++) {
                CoreForecastEntry *e = &f.entries[i];
                ui_wrap(u, box(x, y, width, 60), UI_TEXT,
                        TextFormat(u8"%s · 예상 %d~%d일 · 탐지율 %d%%", fmt_nature_name(e->type), e->start_day,
                                   e->end_day, e->confidence));
                y += 52;
                ui_small(u,x,y,e->status==2||e->status==3?UI_BAD:UI_MUTED,u8"%s%s",e->status==1?u8"발생":e->status==2?u8"기간 내 미발생(오보)":e->status==3?u8"다른 사건 발생":u8"불확실한 예보",e->revised?u8" · 수정됨":"");
                y += 26;
            }
            if (!f.count) {
                ui_small(u, x, y, UI_MUTED, u8"관측된 예정 사건 없음");
                y += 28;
            }
        }
    }
    if (b->resident_slots > 0) {
        ui_text(u, x, y, UI_GOLD, u8"실제 거주자 %d명", b->residents);
        y += 34;
        for (int i = 0; i < a->session.villager_count; i++) {
            const CoreVillagerView *v = &a->session.villagers[i];
            if (v->home_id != b->id)
                continue;
            char label[192], job[80];
            game_building_label(&a->session, v->workplace_id, job, sizeof(job));
            snprintf(label, sizeof(label), u8"#%d %s · %s", v->personal_id, v->name, job);
            game_portrait(a, v->appearance, box(x, y, 38, 38));
            if (ui_button(u, box(x + 44, y, width - 44, 35), label, 1, 0, u8"주민 프로필 열기")) {
                a->profile_home = b->id;
                game_select(a, 2, v->id);
                return y + 44;
            }
            y += 41;
            ui_wrap(u, box(x, y, width, 88), UI_MUTED,
                    TextFormat(u8"%s · 건강 %d · 행복 %d\n기분 %+d · 위생 %d · 식수 %d",
                               fmt_villager_state(v->state), v->health, v->happiness, v->mood, v->hygiene,
                               v->water));
            y += 92;
        }
    }
    return y;
}
