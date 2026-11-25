/* expansion_v124_rank.c — 승급 명령과 UI가 같은 조건 행을 판정합니다. */
#include "expansion.h"
#include <string.h>

static const int POP[6] = {0, 8, 18, 40, 70, 100}, MOOD[6] = {0, 40, 48, 56, 64, 72};
static const int ROADS[6] = {0, 10, 35, 90, 170, 280}, AREA[6] = {0, 0, 25, 120, 300, 600};
static const int PROTECTED[6] = {0, 0, 15, 35, 55, 75}, GOLD[6] = {0, 20, 70, 240, 520, 950};
static const int MANA[6] = {0, 0, 0, 0, 20, 110}, RESEARCH[6] = {0, 0, 0, 0, 2, 6};
static void add_row(CoreRankRequirementView *rows, int *count, const char *label, int have, int need,
                    int resource) {
    CoreRankRequirementView *r = &rows[(*count)++];
    *r = (CoreRankRequirementView){0};
    r->kind = resource >= 0 ? 1 : 0;
    r->resource = resource;
    r->current = have;
    r->required = need;
    r->satisfied = have >= need;
    snprintf(r->label, sizeof(r->label), "%s", label);
}
static void add_group(const World *w, CoreRankRequirementView *rows, int *count, int a, int b, int c) {
    CoreRankRequirementView *r = &rows[(*count)++];
    *r = (CoreRankRequirementView){0};
    r->kind = 2;
    r->resource = -1;
    r->required = 1;
    int choices[3] = {a, b, c};
    for (int k = 0; k < 3; k++)
        if (choices[k] >= 0 && expansion_spec(choices[k])->rank <= w->rank) {
            int type = choices[k];
            r->types[r->type_count++] = type;
            size_t used = strlen(r->label);
            snprintf(r->label + used, sizeof(r->label) - used, "%s%s", used ? u8" / " : "",
                     core_expansion_building_name(type));
            for (int i = 0; i < w->building_count; i++) {
                const Building *building = w->buildings[i];
                if (building->type == type && building->state == CORE_BSTATE_ACTIVE && building->hp > 0) {
                    r->satisfied = 1;
                    if (!r->building_id || building->id < r->building_id)
                        r->building_id = building->id;
                }
            }
        }
    r->current = r->satisfied;
}
static int requirements(const World *w, CoreRankRequirementView *rows) {
    int t = w->rank + 1, n = 0, permanent = 0, happiness = 0;
    if (t > 5)
        return 0;
    for (int i = 0; i < w->villager_count; i++) {
        const Villager *v = w->villagers[i];
        if (!v->contract_end_tick && !v->incoming && v->state != CORE_VS_LEAVING) {
            permanent++;
            happiness += v->happiness;
        }
    }
    add_row(rows, &n, u8"영구 주민", permanent, POP[t], -1);
    add_row(rows, &n, u8"평균 행복", permanent ? happiness / permanent : 0, MOOD[t], -1);
    add_row(rows, &n, u8"연결 도로", w->expansion->effective_roads, ROADS[t], -1);
    add_row(rows, &n, u8"보호구역 면적", w->expansion->protected_area, AREA[t], -1);
    add_row(rows, &n, u8"보호 주민 %", permanent ? w->expansion->protected_population * 100 / permanent : 0,
            PROTECTED[t], -1);
    if (t == 1)
        add_row(rows, &n, u8"식량 8일분", w->bank.amount[CORE_RES_FOOD], permanent * 8, CORE_RES_FOOD);
    add_row(rows, &n, u8"금화", w->bank.amount[CORE_RES_GOLD], GOLD[t], CORE_RES_GOLD);
    if (MANA[t])
        add_row(rows, &n, u8"마정석", w->bank.amount[CORE_RES_MANA], MANA[t], CORE_RES_MANA);
    if (t == 5)
        add_row(rows, &n, u8"금괴", w->bank.amount[CORE_RES_GOLD_BAR], 10, CORE_RES_GOLD_BAR);
    if (RESEARCH[t])
        add_row(rows, &n, u8"완료 연구", w->research.done_count, RESEARCH[t], -1);
    switch (t) {
    case 1:
        add_group(w, rows, &n, CORE_BLD_WELL, -1, -1);
        break;
    case 2:
        add_group(w, rows, &n, CORE_BLD_GRANARY, -1, -1);
        add_group(w, rows, &n, CORE_BLD_CROPLAND, -1, -1);
        add_group(w, rows, &n, CORE_BLD_WATCHTOWER, -1, -1);
        break;
    case 3:
        add_group(w, rows, &n, CORE_BLD_MARKET, -1, -1);
        add_group(w, rows, &n, CORE_BLD_MINE, -1, -1);
        add_group(w, rows, &n, CORE_BLD_RANCH, CORE_BLD_HUNTING_LODGE, CORE_BLD_CLINIC);
        break;
    case 4:
        add_group(w, rows, &n, CORE_BLD_LIBRARY, CORE_BLD_MAGIC_WORKSHOP, -1);
        add_group(w, rows, &n, CORE_BLD_CROSSBOW_TOWER, CORE_BLD_GREENHOUSE, CORE_BLD_LOGISTICS);
        break;
    case 5:
        add_group(w, rows, &n, CORE_BLD_GARRISON, -1, -1);
        add_group(w, rows, &n, CORE_BLD_CANNON_TOWER, CORE_BLD_MAGE_TOWER, -1);
        break;
    }
    return n;
}
CORE_API int32_t core_expansion_rank_requirements(const CoreWorld *w, CoreRankRequirementView *out,
                                                  int32_t capacity) {
    if (!w || (!out && capacity))
        return CORE_ERR_NULL_ARG;
    if (!w->expansion || capacity < 0)
        return CORE_ERR_INVALID_ARG;
    CoreRankRequirementView rows[18];
    int count = requirements(w, rows);
    if (capacity < count)
        return CORE_ERR_BUFFER_TOO_SMALL;
    if (count)
        memcpy(out, rows, (size_t)count * sizeof(*out));
    return count;
}
CORE_API int32_t core_expansion_rank(const CoreWorld *w, CoreExpansionRankView *out) {
    if (!w || !out)
        return CORE_ERR_NULL_ARG;
    if (!w->expansion)
        return CORE_ERR_INVALID_ARG;
    *out = (CoreExpansionRankView){0};
    int t = w->rank + 1;
    out->target = t;
    snprintf(out->name, sizeof(out->name), "%s", core_expansion_rank_name(t < 6 ? t : 5));
    if (t >= 6) {
        out->reason = CORE_DENY_MAX_RANK;
        return CORE_OK;
    }
    out->population = POP[t];
    out->mood = MOOD[t];
    out->roads = ROADS[t];
    out->area = AREA[t];
    out->protected_pct = PROTECTED[t];
    out->gold = GOLD[t];
    out->mana = MANA[t];
    out->research = RESEARCH[t];
    out->gold_bars = t == 5 ? 10 : 0;
    CoreRankRequirementView rows[18];
    int n = requirements(w, rows);
    out->ready = 1;
    for (int i = 0; i < n; i++)
        if (!rows[i].satisfied) {
            out->ready = 0;
            if (!out->reason)
                out->reason = rows[i].resource == CORE_RES_GOLD   ? CORE_DENY_NO_GOLD
                              : rows[i].resource == CORE_RES_MANA ? CORE_DENY_NO_MANA
                                                                  : CORE_DENY_REQUIREMENTS;
        }
    if (w->expansion->promotion_state) {
        out->ready = 0;
        out->reason = CORE_DENY_REQUIREMENTS;
    }
    if (core_expansion_build_locked(w)) {
        out->ready = 0;
        out->reason = CORE_DENY_RAID;
    }
    for (int i = 1; i < CORE_BLD_COUNT; i++)
        if (expansion_spec(i)->rank == t)
            out->unlocks[i / 64] |= UINT64_C(1) << (i % 64);
    snprintf(out->requirements, sizeof(out->requirements), "%s",
             t == 1   ? u8"우물 · 식량8일분"
             : t == 2 ? u8"저장고 · 경작지 · 망루"
             : t == 3 ? u8"시장 · 광산 · 축사/수렵기지/의원"
             : t == 4 ? u8"도서관/마법 공방 · 쇠뇌탑/온실/물류창고"
                      : u8"군영 · 포탑/마탑");
    return CORE_OK;
}
