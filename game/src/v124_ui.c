/* v124_ui.c — 실제 의료·수확·어업 조회와 건물 툴팁 */
#include "app.h"
#include "format.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

float game_v124_building_details(GameApp *a, const CoreBuildingView *b, float x, float y, float width) {
    CoreMedicalView med;
    if (core_expansion_medical(a->session.world, b->id, &med) == CORE_OK) {
        ui_small(&a->ui, x, y, med.qualified ? UI_GOOD : UI_BAD, u8"의료진 %d/%d명 · %s", med.workers,
                 b->worker_slots, med.qualified ? u8"자격 충족" : u8"자격 미달");
        DrawTexturePro(a->assets.clinic_icon,(Rectangle){0,0,64,64},(Rectangle){x+width-24,y,22,22},(Vector2){0,0},0,WHITE);
        y += 25;
        ui_wrap(&a->ui, (Rectangle){x, y, width, 72}, UI_TEXT,
                TextFormat(u8"지능 최고 %d / 필요%d · 합계 %d / 필요%d\n최소 실제 근무 %d명 · 기본 %d명/일",
                           med.highest, med.required_highest, med.intellect_sum, med.required_sum,
                           med.required_workers, med.base_patients));
        y += 76;
        ui_small(&a->ui, x, y, UI_TEXT, u8"약초 %d/환자 · 중증+1 · 역병+1", med.herbs);
        y += 25;
        ui_small(&a->ui, x, y, UI_MUTED, u8"오늘 치료 %d명 · 대기 %d명", med.treated, med.waiting);
        y += 28;
    }
    CoreHarvestView harvest;
    if (core_expansion_harvest(a->session.world, b->id, &harvest) == CORE_OK) {
        ui_wrap(&a->ui, (Rectangle){x, y, width, 78}, UI_TEXT,
                TextFormat(u8"작기 노동 %.1f인시\n%d년 수확 %.3f / 분배 %.3f\n미수령 %.3f · 창고가 비면 수령",
                           b->crop_labor_milli / 60000.0, harvest.year, harvest.total_milli / 1000.0,
                           harvest.released_milli / 1000.0, harvest.pending_milli / 1000.0));
        y += 84;
    }
    if (b->type == CORE_BLD_LIGHTHOUSE) {
        int water = core_expansion_adjacent_water(a->session.world, b->type, b->x, b->y, b->rotation);
        ui_wrap(&a->ui, (Rectangle){x, y, width, 94}, b->operating ? UI_GOOD : UI_BAD,
                TextFormat(
                    u8"인접 호수 #%d · 시야12칸\n같은 호수 중심10칸 어업 +15%%\n중첩 없음 · 유지비2금/일\n%s",
                    water, b->operating ? u8"정상 가동" : u8"유지비 부족/가동 중지"));
        y += 100;
    }
    if (b->type == CORE_BLD_FISHING_SPOT || b->type == CORE_BLD_PORT) {
        CoreBoatView boats[CORE_EXP_MAX_BOATS];
        int32_t count = 0;
        int owned = 0, crew = 0, working = 0, returning = 0;
        if (core_expansion_copy_boats(a->session.world, boats, CORE_EXP_MAX_BOATS, &count) == CORE_OK)
            for (int i = 0; i < count; i++)
                if (boats[i].port_id == b->id) {
                    owned++;
                    crew += boats[i].crew_count;
                    working += boats[i].fishing_phase > 0;
                    returning += boats[i].returning != 0;
                }
        ui_wrap(
            &a->ui, (Rectangle){x, y, width, 86}, UI_TEXT,
            TextFormat(u8"%s %d척 · 승무원%d명\n척당 최대2명 · 그물 조업%d / 귀항%d\n유효한 호숫가에서 출항합니다.",
                       b->type == CORE_BLD_PORT ? u8"고급 어선" : u8"소형 어선", owned, crew, working,
                       returning));
        y += 91;
        if (!owned && b->working_now) {
            ui_small(&a->ui, x, y, UI_MUTED, u8"출항 대기 / 선박 표시 한도 · 생산 유지");
            y += 25;
        }
    }
    return y;
}
void game_v124_hover(GameApp *a) {
    if (a->ui.tooltip[0] || a->panel != PANEL_NONE || !a->in_game)
        return;
    Vector2 mouse = GetMousePosition(), world = GetScreenToWorld2D(mouse, a->camera);
    if (mouse.y < 132 || mouse.y >= GetScreenHeight() - 248 || mouse.x >= GetScreenWidth() - 350)
        return;
    for (int i = a->session.villager_count - 1; i >= 0; i--) {
        CoreVillagerView *v = &a->session.villagers[i];
        GameActor *actor = game_actor(&a->session, v->id);
        if (!actor || actor->indoor >= .5f || actor->boat_port)
            continue;
        Rectangle body = {actor->position.x * 16 - 12, actor->position.y * 16 - 24, 24, 24};
        if (CheckCollisionPointRec(world, body)) {
            int p = (int)actor->position.y * a->session.summary.map_width + (int)actor->position.x;
            if (p < 0 || p >= a->session.cells ||
                (a->session.visibility[p] < 2 && !v->incoming && v->state != CORE_VS_LEAVING))
                continue;
            snprintf(a->ui.tooltip, sizeof(a->ui.tooltip), u8"%s #%d\n건강 %d / 100 · %s", v->name,
                     v->personal_id, v->health, fmt_villager_state(v->state));
            return;
        }
    }
    for (int i = a->session.enemy_count - 1; i >= 0; i--) {
        GameEnemy *e = &a->session.enemies[i];
        if (e->hp <= 0 || e->pending_spawn)
            continue;
        int p = (int)e->actor.position.y * a->session.summary.map_width + (int)e->actor.position.x;
        if (p < 0 || p >= a->session.cells || a->session.visibility[p] < 2)
            continue;
        float w = e->info.width_milli * .016f, h = e->info.height_milli * .016f;
        if (CheckCollisionPointRec(
                world, (Rectangle){e->actor.position.x * 16 - w / 2, e->actor.position.y * 16 - h, w, h})) {
            snprintf(a->ui.tooltip, sizeof(a->ui.tooltip), u8"%s\nHP %d/%d · 공격%d · 사거리%d", e->info.name,
                     e->hp, e->def.hp, e->def.damage, e->def.range);
            return;
        }
    }
    for (int i = a->session.animal_count - 1; i >= 0; i--) {
        CoreAnimalView *animal = &a->session.animals[i];
        if (!animal->alive || animal->hp <= 0 || animal->owner)
            continue;
        GameAnimalActor *actor = &a->session.animal_actors[i];
        int p = (int)actor->position.y * a->session.summary.map_width + (int)actor->position.x;
        if (p < 0 || p >= a->session.cells || a->session.visibility[p] < 2)
            continue;
        CoreAnimalInfo info;
        core_expansion_animal_info(animal->kind, &info);
        float w = info.width_milli * .016f, h = info.height_milli * .016f;
        if (CheckCollisionPointRec(
                world, (Rectangle){actor->position.x * 16 - w / 2, actor->position.y * 16 - h, w, h})) {
            snprintf(a->ui.tooltip, sizeof(a->ui.tooltip), u8"%s\nHP %d/%d · 공격%d\n식량%d · 가죽%d",
                     info.name, animal->hp, info.hp, info.attack, info.food, info.leather);
            return;
        }
    }
    CoreWanderingMerchantView *merchant = &a->session.wandering;
    if (merchant->state != CORE_MERCHANT_DORMANT) {
        int p = merchant->y_milli / 1000 * a->session.summary.map_width + merchant->x_milli / 1000;
        if (p >= 0 && p < a->session.cells && a->session.visibility[p] >= 2 &&
            CheckCollisionPointRec(
                world, (Rectangle){merchant->x_milli * .016f - 12, merchant->y_milli * .016f - 24, 24, 24})) {
            snprintf(a->ui.tooltip, sizeof(a->ui.tooltip), u8"떠돌이 상인\n%s · 제안%d종",
                     merchant->state == CORE_MERCHANT_TRADING ? u8"교환 가능" : u8"이동 중",
                     merchant->offer_count);
            return;
        }
    }
    int id = game_hover_building(a, mouse);
    CoreBuildingView *b = game_building(&a->session, id);
    if (!b)
        return;
    CoreExpansionBuildingInfo info;
    core_expansion_building_info(b->type, &info);
    const char *state = b->state == CORE_BSTATE_CONSTRUCTING ? u8"공사 중"
                        : b->state == CORE_BSTATE_RUINED     ? u8"파괴됨"
                        : b->operating                       ? u8"운영 중"
                                                             : u8"운영 중지";
    CoreMedicalView med;
    if (core_expansion_medical(a->session.world, id, &med) == CORE_OK)
        snprintf(a->ui.tooltip, sizeof(a->ui.tooltip),
                 u8"%s · %s\n의료진 %d/%d · %s\n오늘 진료%d · 기본%d/일 · 약초%d/인\nHP %d/%d · %s",
                 info.name, core_expansion_rank_name(info.rank), med.workers, info.workers,
                 med.qualified ? u8"자격 충족" : u8"자격 미달", med.treated, med.base_patients, med.herbs,
                 b->hp, b->max_hp, state);
    else
        snprintf(a->ui.tooltip, sizeof(a->ui.tooltip),
                 u8"%s · %s\n직원 %d/%d · %s\nHP %d/%d · 유지비%d금/일\n%s", info.name,
                 core_expansion_rank_name(info.rank), b->working_now, info.workers, state, b->hp, b->max_hp,
                 info.upkeep, info.description);
}
