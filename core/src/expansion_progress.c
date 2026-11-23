/* expansion_progress.c — 연구 24종, 여섯 등급, 고정 시장과 12일 계약 */
#include "expansion.h"
#include "mem.h"
#include "population.h"
#include <stdlib.h>
#include <string.h>

static const char *const RESEARCH_KEYS[24] = {
    "rotation",      "tools",         "water_survey", "logistics",     "seeds",       "smelting",
    "storage",       "royal_trade",   "scouting",     "longbows",      "gatecraft",   "armor",
    "gunpowder",     "fortification", "tactics",      "royal_defense", "mana_survey", "abundance",
    "healing_light", "embers",        "mana_control", "ward",          "alchemy",     "grand_magic"};
static const int GOLD[24] = {70,  90,  100, 130, 180, 190, 250, 320, 70,  90,  100, 150,
                             190, 200, 260, 340, 100, 140, 160, 180, 220, 260, 300, 400};
static const int MANA[24] = {0,  0,  5,  10, 15, 18, 25, 30, 0,  0,  5,  10,
                             15, 20, 25, 30, 10, 15, 20, 25, 30, 35, 45, 60};
static const int POINTS[24] = {80,  100, 120, 150, 190, 210, 260, 300, 80,  100, 120, 160,
                               200, 220, 260, 310, 120, 150, 170, 190, 240, 280, 320, 400};
CORE_API const char *core_expansion_research_name(int32_t i) {
    static const char *const names[24] = {
        u8"윤작",      u8"작업 도구",   u8"수로 측량", u8"물류 배치",   u8"우수 종자", u8"정밀 제련",
        u8"창고 정리", u8"왕립 교역",   u8"정찰 훈련", u8"장궁 제작",   u8"성문 기술", u8"중장갑 훈련",
        u8"화약 공성", u8"성벽 보강",   u8"전술 교본", u8"왕립 방어망", u8"마력 탐지", u8"풍요의 룬",
        u8"치유의 빛", u8"따뜻한 불씨", u8"마력 제어", u8"방어 결계",   u8"연금술",    u8"대마법"};
    return i >= 0 && i < 24 ? names[i] : u8"알 수 없음";
}
CORE_API const char *core_expansion_goal_name(int32_t i) {
    static const char *const goals[5] = {u8"텃밭으로 식량 마련", u8"막사로 주거 확보", u8"일꾼 직접 배치",
                                         u8"연결 도로 10칸", u8"영구 주민 8명으로 촌락 승급"};
    return i >= 0 && i < 5 ? goals[i] : "";
}
CORE_API const char *core_expansion_research_effect(int32_t i) {
    static const char *const effects[24] = {u8"가을 수확 +12%",         u8"임업·광업 효율 +10%",
                                            u8"관개농장 가뭄 근로 적립65→75%", u8"장거리 물류 가용률 +15%p",
                                            u8"경작지·농장 수확 +15%",  u8"철괴 생산 4→5/일",
                                            u8"자원 저장 한도 +20%",    u8"시장 판매가 +10%",
                                            u8"초소·망루 시야 +2칸",    u8"망루·감시탑 공격 +15%",
                                            u8"모든 성문 체력 +20%",    u8"경비병 받는 피해 -10%",
                                            u8"포탑 광역 반경 +1칸",    u8"성벽 체력 +25%",
                                            u8"경비병 공격 +15%",       u8"자동 출동 지연 -20%",
                                            u8"마정석 광상 탐지 +3칸",  u8"완료 이후 작기 농업 근로 적립+8%",
                                            u8"치료 처리량 +25%",       u8"겨울 난방 연료 -20%",
                                            u8"마탑 연구점수 +20%",     u8"마정석으로 보호구역 피해 -15%",
                                            u8"마법 자원 소모 -10%",    u8"대마탑 연구점수 +20%"};
    return i >= 0 && i < 24 ? effects[i] : "";
}
static int setup_defs(Defs *defs) {
    ResearchDef *list = (ResearchDef *)mem_calloc(24, sizeof(*list));
    int i;
    if (!list)
        return CORE_ERR_OUT_OF_MEMORY;
    for (i = 0; i < 24; i++) {
        ResearchDef *r = &list[i];
        strcpy(r->key, RESEARCH_KEYS[i]);
        r->branch = i / 8;
        r->requires = i % 8 ? i - 1 : -1;
        r->required_rank = i >= 16 ? (i >= 21 ? 5 : 4) : i % 8 < 3 ? 3 : i % 8 < 6 ? 4 : 5;
        r->cost_gold = GOLD[i];
        r->cost_mana = MANA[i];
        r->points = POINTS[i];
        r->effect = 0;
        r->value = 0;
    }
    mem_free(defs->research);
    defs->research = list;
    defs->research_count = defs->research_capacity = 24;
    defs->expanded = 1;
    return CORE_OK;
}
CORE_API int32_t core_defs_expand(CoreDefs *handle) {
    Defs next;
    int rc;
    if (!handle)
        return CORE_ERR_NULL_ARG;
    if (handle->defs.expanded)
        return CORE_OK;
    rc = defs_copy(&next, &handle->defs);
    if (rc != CORE_OK)
        return rc;
    expansion_fill_definitions(&next, 1);
    rc = setup_defs(&next);
    if (rc != CORE_OK) {
        defs_free(&next);
        return rc;
    }
    defs_free(&handle->defs);
    handle->defs = next;
    return CORE_OK;
}
int expansion_setup_research(World *w) { return w->defs.expanded ? CORE_OK : setup_defs(&w->defs); }
int expansion_has_research(const World *w, int i) {
    return i >= 0 && i < 24 && (w->research.done & (1u << i)) != 0;
}
void expansion_apply_research(World *w) {
    int i;
    memcpy(w->defs.buildings, w->base_buildings, sizeof(w->base_buildings));
    for (i = 0; i < CORE_BLD_COUNT; i++) {
        BuildingDef *d = &w->defs.buildings[i];
        const ExpansionBuildingSpec *s = expansion_spec(i);
        if ((i == CORE_BLD_WATCHTOWER || i == CORE_BLD_TOWER) && expansion_has_research(w, 9))
            d->attack = d->attack * 115 / 100;
        if (i == CORE_BLD_BARRACKS && expansion_has_research(w, 14))
            d->attack = d->attack * 115 / 100;
        if ((s->flags & CORE_EXP_GATE) && expansion_has_research(w, 10))
            d->max_hp = d->max_hp * 120 / 100;
        if ((s->flags & CORE_EXP_WALL) && expansion_has_research(w, 13))
            d->max_hp = d->max_hp * 125 / 100;
    }
    w->expansion->dirty = 1;
    expansion_capacity(w);
}
int expansion_start_research(World *w, int index, int *reason) {
    ResearchDef *r;
    int has = 0, i;
    *reason = CORE_DENY_RESEARCH_LOCKED;
    if (index < 0 || index >= 24)
        return CORE_ERR_NOT_FOUND;
    r = &w->defs.research[index];
    if (w->research.active >= 0) {
        *reason = CORE_DENY_RESEARCH_BUSY;
        return CORE_ERR_REJECTED;
    }
    if (expansion_has_research(w, index)) {
        *reason = CORE_DENY_RESEARCH_DONE;
        return CORE_ERR_REJECTED;
    }
    for (i = 0; i < w->building_count; i++) {
        Building *b = w->buildings[i];
        if (b->state == CORE_BSTATE_ACTIVE && b->worker_count &&
            (index < 16 ? (b->type == CORE_BLD_LIBRARY || b->type == CORE_BLD_MAGIC_WORKSHOP ||
                           b->type == CORE_BLD_MAGE_TOWER || b->type == CORE_BLD_GRAND_MAGE_TOWER)
                        : (b->type == CORE_BLD_MAGE_TOWER || b->type == CORE_BLD_GRAND_MAGE_TOWER)))
            has = 1;
    }
    if (!has || w->rank < r->required_rank || (r->requires >= 0 && !expansion_has_research(w, r->requires)))
        return CORE_ERR_REJECTED;
    if (w->bank.amount[CORE_RES_GOLD] < r->cost_gold) {
        *reason = CORE_DENY_NO_GOLD;
        return CORE_ERR_REJECTED;
    }
    if (w->bank.amount[CORE_RES_MANA] < r->cost_mana) {
        *reason = CORE_DENY_NO_MANA;
        return CORE_ERR_REJECTED;
    }
    (void)expansion_bank_add(w, CORE_RES_GOLD, -r->cost_gold * 1000, CORE_LEDGER_RESEARCH);
    (void)expansion_bank_add(w, CORE_RES_MANA, -r->cost_mana * 1000, CORE_LEDGER_RESEARCH);
    w->research.active = index;
    w->research.progress_milli = 0;
    *reason = 0;
    events_push_progress(&w->events, CORE_EVT_RESEARCH_STARTED, index, 0, 0, NULL);
    return CORE_OK;
}
static int facility(const World *w, int type) {
    int i;
    for (i = 0; i < w->building_count; i++)
        if (w->buildings[i]->type == type && w->buildings[i]->state == CORE_BSTATE_ACTIVE)
            return 1;
    return 0;
}
CORE_API int32_t core_expansion_rank(const CoreWorld *w, CoreExpansionRankView *out) {
    static const int pop[6] = {0, 8, 18, 40, 70, 100}, moods[6] = {0, 40, 48, 56, 64, 72},
                     roads[6] = {0, 10, 35, 90, 170, 280}, areas[6] = {0, 0, 25, 120, 300, 600},
                     protected_pct[6] = {0, 0, 15, 35, 55, 75}, gold[6] = {0, 20, 70, 240, 520, 950},
                     mana[6] = {0, 0, 0, 0, 20, 110}, research[6] = {0, 0, 0, 0, 2, 6},
                     fooddays[6] = {0, 8, 0, 0, 0, 0};
    int target, i, permanent = 0, mood = 0, facilities = 1;
    if (!w || !out)
        return CORE_ERR_NULL_ARG;
    if (!w->expansion)
        return CORE_ERR_INVALID_ARG;
    memset(out, 0, sizeof(*out));
    target = w->rank + 1;
    out->target = target;
    if (target >= 6) {
        out->reason = CORE_DENY_MAX_RANK;
        strcpy(out->name, core_expansion_rank_name(5));
        return CORE_OK;
    }
    out->population = pop[target];
    out->mood = moods[target];
    out->roads = roads[target];
    out->area = areas[target];
    out->protected_pct = protected_pct[target];
    out->gold = gold[target];
    out->mana = mana[target];
    out->gold_bars = target == 5 ? 10 : 0;
    out->research = research[target];
    strcpy(out->name, core_expansion_rank_name(target));
    for (i = 0; i < w->villager_count; i++)
        if (!w->villagers[i]->contract_end_tick) {
            permanent++;
            mood += w->villagers[i]->happiness;
        }
    if (permanent)
        mood /= permanent;
    if (target == 1)
        facilities = facility(w, CORE_BLD_WELL);
    if (target == 2)
        facilities = facility(w, CORE_BLD_GRANARY) && facility(w, CORE_BLD_CROPLAND) &&
                     (facility(w, CORE_BLD_OUTPOST) || facility(w, CORE_BLD_WATCHTOWER) ||
                      facility(w, CORE_BLD_TOWER) || facility(w, CORE_BLD_BARRACKS));
    if (target == 3)
        facilities = facility(w, CORE_BLD_SAWMILL) &&
                     (facility(w, CORE_BLD_KILN) || facility(w, CORE_BLD_SMELTER)) &&
                     facility(w, CORE_BLD_MARKET);
    if (target == 4)
        facilities = facility(w, CORE_BLD_MINE) || facility(w, CORE_BLD_MANA_MINE);
    if (target == 5)
        facilities = facility(w, CORE_BLD_BARRACKS) &&
                     (facility(w, CORE_BLD_CROSSBOW_TOWER) || facility(w, CORE_BLD_CANNON_TOWER) ||
                      facility(w, CORE_BLD_ARCANE_TOWER));
    out->reason = CORE_DENY_REQUIREMENTS;
    out->ready = permanent >= pop[target] && mood >= moods[target] &&
                 w->expansion->effective_roads >= roads[target] &&
                 w->expansion->protected_area >= areas[target] &&
                 w->expansion->protected_population * 100 >= permanent * protected_pct[target] &&
                 w->research.done_count >= research[target] &&
                 expansion_bank_have(w, CORE_RES_FOOD) >= permanent * 1000 * fooddays[target] && facilities;
    if (out->ready) {
        out->reason = w->bank.amount[CORE_RES_GOLD] < gold[target]         ? CORE_DENY_NO_GOLD
                      : w->bank.amount[CORE_RES_MANA] < mana[target]       ? CORE_DENY_NO_MANA
                      : w->bank.amount[CORE_RES_GOLD_BAR] < out->gold_bars ? CORE_DENY_NO_RESOURCES
                                                                           : 0;
        out->ready = out->reason == 0 && !w->expansion->promotion_state;
    }
    for (i = 1; i < CORE_BLD_COUNT; i++)
        if (expansion_spec(i)->rank == target)
            out->unlocks[i / 64] |= UINT64_C(1) << (i % 64);
    strcpy(out->requirements, target == 1   ? u8"정상 우물 · 식량 8일분"
                              : target == 2 ? u8"저장고 · 경작지 · 경비 시설"
                              : target == 3 ? u8"제재소 · 가마 또는 제련소 · 시장"
                              : target == 4 ? u8"광산 · 기초 연구 2개"
                                            : u8"병영 · 상급 방어탑 · 연구 6개");
    return CORE_OK;
}
int expansion_rank_up(World *w, int *reason) {
    CoreExpansionRankView v;
    int rc = core_expansion_rank(w, &v);
    if (rc != CORE_OK)
        return rc;
    if(core_expansion_build_locked(w)){*reason=CORE_DENY_RAID;return CORE_ERR_REJECTED;}
    *reason = v.reason;
    if (!v.ready)
        return CORE_ERR_REJECTED;
    (void)expansion_bank_add(w, CORE_RES_GOLD, -v.gold * 1000, CORE_LEDGER_TRIBUTE);
    (void)expansion_bank_add(w, CORE_RES_MANA, -v.mana * 1000, CORE_LEDGER_TRIBUTE);
    (void)expansion_bank_add(w, CORE_RES_GOLD_BAR, -v.gold_bars * 1000, CORE_LEDGER_TRIBUTE);
    w->expansion->promotion_state = 1;
    w->expansion->promotion_target = w->rank + 1;
    w->expansion->promotion_remaining = 144;
    return CORE_OK;
}
void expansion_promotion_tick(World *w) {
    ExpansionState *e = w->expansion;
    Building *keep = world_keep(w);
    if (!e->promotion_state)
        return;
    if (!keep || keep->state == CORE_BSTATE_RUINED) {
        e->promotion_state = e->promotion_remaining = 0;
        return;
    }
    if (--e->promotion_remaining > 0)
        return;
    w->rank = e->promotion_target;
    e->promotion_state = 0;
    events_push_progress(&w->events, CORE_EVT_RANK_UP, w->rank, 0, 0, NULL);
    if (w->rank == 5) {
        w->outcome = CORE_OUTCOME_WON;
        w->stats.victory_day = exp_day(w);
        events_push_progress(&w->events, CORE_EVT_VICTORY, exp_day(w), 0, 0, NULL);
    }
}
int expansion_market_level(const World *w) {
    int i, level = 0;
    for (i = 0; i < w->building_count; i++) {
        Building *b = w->buildings[i];
        if (b->state != CORE_BSTATE_ACTIVE || !b->worker_count)
            continue;
        if (b->type == CORE_BLD_TRADING_POST)
            level = 3;
        else if (b->type == CORE_BLD_MARKET && level < 2)
            level = 2;
        else if (b->type == CORE_BLD_STALL && !level)
            level = 1;
    }
    return level;
}
CORE_API int32_t core_expansion_market_stock(const CoreWorld *w, int32_t resource) {
    if (!w)
        return CORE_ERR_NULL_ARG;
    if (!w->expansion || resource < 1 || resource >= 15)
        return CORE_ERR_INVALID_ARG;
    return w->expansion->trade_stock[resource];
}
void expansion_candidate_price(const World *w, CoreCandidateView *c) {
    Villager v = {0};
    int type = -1, j, eff, quality;
    v.stats.strength = c->stats[0];
    v.stats.agility = c->stats[1];
    v.stats.vitality = c->stats[2];
    v.stats.dexterity = c->stats[3];
    v.stats.intellect = c->stats[4];
    v.health = 100;
    v.happiness = 50;
    v.traits[0] = v.traits[1] = -1;
    memcpy(v.skill_minutes, c->skills, sizeof(c->skills));
    for (j = 1; j < CORE_BLD_COUNT; j++)
        if (expansion_spec(j)->field == c->field && expansion_spec(j)->workers &&
            (type < 0 || w->defs.buildings[j].wage < w->defs.buildings[type].wage))
            type = j;
    eff = type < 0 ? 100 : expansion_work_percent(w, &v, type);
    quality = exp_clamp(1000 + 3 * (eff - 100) +
                            (c->skills[c->field] >= 64800   ? 600
                             : c->skills[c->field] >= 25920 ? 400
                             : c->skills[c->field] >= 6912  ? 200
                                                               : 0),
                        800, 2600);
    c->fee = (18 * (type < 0 ? 1 : w->defs.buildings[type].wage) * quality + 999) / 1000 + 5;
}
void expansion_candidates(World *w) {
    ExpansionState *e = w->expansion;
    int period = (exp_day(w) - 1) / 3, level = expansion_market_level(w), count = 15, i;
    if (exp_day(w) >= e->market_next_restock_day) {
        int efficiency=e->market_staff_efficiency>0 && e->market_staff_day>=exp_day(w)-1?e->market_staff_efficiency:100;
        int base=0,mult=0,quantity=0;
        core_expansion_restock(level?level:1,efficiency,&base,&mult,&quantity);
        for (i=1;i<15;i++) e->trade_stock[i]=quantity;
        e->market_next_restock_day=exp_day(w)+(efficiency>=300?2:efficiency>=200?3:4);
    }
    if (e->candidate_day == period && e->candidate_count == count)
        return;
    e->candidate_day = period;
    e->candidate_count = count;
    for (i = 0; i < count; i++) {
        CoreCandidateView *c = &e->candidates[i];
        Villager temporary;
        int j;
        memset(&temporary, 0, sizeof(temporary));
        temporary.loc_x = w->keep_x;
        temporary.loc_y = w->keep_y;
        temporary.health = 100;
        temporary.mood = 60;
        temporary.traits[0] = temporary.traits[1] = -1;
        expansion_initialize_villager(w, &temporary);
        memset(c, 0, sizeof(*c));
        c->person_id = temporary.personal_id;
        c->age = rng_range(&w->rng, 18, 50);
        c->field = rng_range(&w->rng, 0, 11);
        for (j = 0; j < 5; j++)
            c->stats[j] = c->initial[j] = temporary.initial_stats[j];
        c->skills[c->field] = rng_range(&w->rng, 0, 64800);
        c->traits[0] = c->traits[1] = -1;
        expansion_choose_identity(w, &temporary);
        strcpy(c->name, temporary.name);
        c->sex = temporary.sex;
        c->appearance = temporary.appearance;
        expansion_candidate_price(w, c);
    }
    /* 떠난 계약 주민은 같은 개인 기록으로 다시 고용할 수 있습니다. */
    for (i = 0; i < e->archive_count && i < count; i++) {
        int j;
        e->candidates[i] = e->archive[e->archive_count - 1 - i];
        expansion_candidate_price(w, &e->candidates[i]);
        for (j = 0; j < w->villager_count; j++)
            if (w->villagers[j]->personal_id == e->candidates[i].person_id)
                e->candidates[i].hired = 1;
    }
}
CORE_API int32_t core_expansion_copy_candidates(const CoreWorld *w, CoreCandidateView *out, int32_t capacity,
                                                int32_t *count) {
    if (!w || !count)
        return CORE_ERR_NULL_ARG;
    if (!w->expansion)
        return CORE_ERR_INVALID_ARG;
    *count = (int[]){0, 3, 8, 15}[expansion_market_level(w)];
    if (*count > w->expansion->candidate_count)
        *count = w->expansion->candidate_count;
    if (capacity < *count)
        return CORE_ERR_BUFFER_TOO_SMALL;
    if (*count && !out)
        return CORE_ERR_NULL_ARG;
    if (*count)
        memcpy(out, w->expansion->candidates, (size_t)*count * sizeof(*out));
    return CORE_OK;
}
CORE_API int32_t core_expansion_hire(CoreWorld *w, int32_t person_id, int32_t *villager_id) {
    ExpansionState *e;
    CoreCandidateView *c = NULL;
    Villager *v;
    int i, count = 0, level, x = 0, y = 0, free_housing = 0;
    if (!w)
        return CORE_ERR_NULL_ARG;
    if (!w->expansion)
        return CORE_ERR_INVALID_ARG;
    e = w->expansion;
    level = expansion_market_level(w);
    for (i = 0; i < w->villager_count; i++)
        if (w->villagers[i]->personal_id == person_id)
            return CORE_ERR_REJECTED;
        else if (w->villagers[i]->contract_end_tick)
            count++;
    if (!level || count >= (int[]){0, 1, 3, 8}[level])
        return CORE_ERR_REJECTED;
    for (i = 0; i < (int[]){0, 3, 8, 15}[level]; i++)
        if (e->candidates[i].person_id == person_id)
            c = &e->candidates[i];
    if (!c)
        return CORE_ERR_NOT_FOUND;
    if (c->hired || w->bank.amount[CORE_RES_GOLD] < c->fee)
        return CORE_ERR_REJECTED;
    free_housing = expansion_available_housing(w);
    if (free_housing < 1 || expansion_bank_have(w, CORE_RES_FOOD) < 1000 ||
        !expansion_edge(w, w->keep_x, 0, &x, &y))
        return CORE_ERR_REJECTED;
    v = population_create(w, x, y);
    if (!v)
        return CORE_ERR_OUT_OF_MEMORY;
    (void)expansion_bank_add(w, CORE_RES_GOLD, -c->fee * 1000, CORE_LEDGER_WAGES);
    v->personal_id = c->person_id;
    v->stats.strength = c->stats[0];
    v->stats.agility = c->stats[1];
    v->stats.vitality = c->stats[2];
    v->stats.dexterity = c->stats[3];
    v->stats.intellect = c->stats[4];
    memcpy(v->initial_stats, c->initial, sizeof(c->initial));
    memcpy(v->stat_xp, c->xp, sizeof(c->xp));
    memcpy(v->skill_minutes, c->skills, sizeof(c->skills));
    memcpy(v->traits, c->traits, sizeof(c->traits));
    strcpy(v->name, c->name);
    v->sex = c->sex;
    v->appearance = c->appearance;
    v->age = c->age;
    v->contract_end_tick = w->clock.tick + 12 * 144;
    v->incoming = 1;
    v->at_target = 0;
    v->intent_flags = CORE_INTENT_SPAWN_EDGE;
    c->hired = 1;
    if (villager_id)
        *villager_id = v->id;
    expansion_assign(w);
    return CORE_OK;
}
CORE_API int32_t core_expansion_set_tax(CoreWorld *w, int32_t tax) {
    if (!w)
        return CORE_ERR_NULL_ARG;
    if (!w->expansion || tax < 1 || tax > 4)
        return CORE_ERR_INVALID_ARG;
    if (w->expansion->tax == tax)
        return CORE_OK;
    if (w->expansion->tax_day == exp_day(w))
        return CORE_ERR_REJECTED;
    w->expansion->tax = tax;
    w->expansion->tax_day = exp_day(w);
    return CORE_OK;
}
CORE_API int32_t core_expansion_set_smelter(CoreWorld *w, int32_t id, int32_t mode) {
    Building *b;
    if (!w)
        return CORE_ERR_NULL_ARG;
    if (!w->expansion || mode < 0 || mode > 1)
        return CORE_ERR_INVALID_ARG;
    b = entity_find_building(w, id);
    if (!b)
        return CORE_ERR_NOT_FOUND;
    if (b->type != CORE_BLD_SMELTER || b->mode_day == exp_day(w))
        return CORE_ERR_REJECTED;
    b->mode = mode;
    b->mode_day = exp_day(w);
    return CORE_OK;
}
void expansion_read_summary(const World *w, CoreSummaryView *out) {
    int i, level = expansion_market_level(w);
    CoreExpansionRankView rank;
    out->ruleset = 1;
    for (i = 0; i < w->villager_count; i++)
        if (!w->villagers[i]->contract_end_tick)
            out->average_happiness += w->villagers[i]->happiness;
    if (expansion_permanent(w))
        out->average_happiness /= expansion_permanent(w);
    out->promotion_state = w->expansion->promotion_state;
    out->promotion_target = w->expansion->promotion_target;
    out->promotion_remaining = w->expansion->promotion_remaining;
    out->warehouse_hub_visits = w->expansion->hub_visits;
    out->manor_hub_visits = w->expansion->manor_visits;
    out->saved_travel_ticks = w->expansion->saved_travel_ticks;
    out->habitat_regions = w->expansion->region_count;
    for (i = 0; i < w->expansion->region_count; i++) {
        out->wild_count += w->expansion->regions[i].count;
        out->wild_biomass += w->expansion->regions[i].biomass;
        out->habitat_overcapacity += w->expansion->regions[i].biomass > w->expansion->regions[i].capacity;
    }
    out->day = exp_day(w);
    out->minute_of_day = exp_minute(w);
    out->season = exp_season(w);
    out->day_of_season = (exp_day(w) - 1) % 12 + 1;
    out->year = exp_year(w);
    out->is_night = exp_minute(w) < 360 || exp_minute(w) >= 1260;
    for (i = 0; i < w->villager_count; i++)
        if (w->villagers[i]->contract_end_tick)
            out->contractors++;
        else
            out->permanent_population++;
    out->tax = w->expansion->tax;
    out->valid_roads = w->expansion->effective_roads;
    out->protected_area = w->expansion->protected_area;
    out->protected_population = w->expansion->protected_population;
    out->water_served = w->expansion->water_served;
    out->market_limit = expansion_trade_limit(w);
    out->market_used = w->expansion->trade_used;
    out->market_open = level > 0;
    out->candidate_count = (int[]){0, 3, 8, 15}[level];
    for (i = 0; i < w->expansion->animal_count; i++)
        out->animal_count += w->expansion->animals[i].alive;
    out->threat_ticks_left = out->threat_type = out->threat_strength = 0;
    out->raid_id = w->expansion->alarm ? w->threat.raid.id : 0;
    out->winter_food_need = w->villager_count * 12;
    for (i = 0; i < 15; i++)
        out->resource_capacity[i] = eco_bank_limit(&w->bank, i);
    (void)core_expansion_rank(w, &rank);
    out->rank_ready = rank.ready;
}
void expansion_read_building(const World *w, const Building *b, CoreBuildingView *out) {
    const ExpansionBuildingSpec *s = expansion_spec(b->type);
    (void)w;
    out->rotation = b->rotation;
    out->kind_flags = s->flags;
    out->job_field = s->field;
    out->completed_day = b->completed_day;
    out->mode = b->mode;
    out->upkeep = s->upkeep;
    out->operating = b->operating;
    out->blocked_reason = b->blocked_reason;
    out->crop_labor_milli = b->crop_labor_milli;
    out->pending_harvest_milli = b->pending_harvest_milli;
    out->labor_milli = expansion_labor_milli(w,b);
    out->service_credit_milli = b->service_credit_milli;
    memcpy(out->breeding_progress,b->breeding_progress,sizeof(out->breeding_progress));
    out->feed_milli = b->feed_milli;
    out->actual_work_milli = b->actual_work_milli;
    out->type_serial=b->type_serial;out->combat_guard_id=b->combat_guard_id;
}
void expansion_read_villager(const World *w, const Villager *v, CoreVillagerView *out) {
    out->agility = v->stats.agility;
    memcpy(out->initial_stats, v->initial_stats, sizeof(out->initial_stats));
    memcpy(out->stat_xp, v->stat_xp, sizeof(out->stat_xp));
    memcpy(out->skill_minutes, v->skill_minutes, sizeof(out->skill_minutes));
    out->contract_end_tick = v->contract_end_tick;
    out->personal_id = v->personal_id;
    out->water = v->water;
    out->hygiene = v->hygiene;
    out->inside_building = v->inside_building;
    out->mood_target = expansion_mood(w, v);
    out->happiness = v->happiness;
    out->long_hunger = v->long_needs[0] / 1000;
    out->long_rest = v->long_needs[1] / 1000;
    out->long_safety = v->long_needs[2] / 1000;
    out->mood_cause = v->mood_cause;
    out->incoming = v->incoming;
    out->current_x_milli = v->current_x_milli;
    out->current_y_milli = v->current_y_milli;
    out->mission=v->mission;out->mission_target=v->mission_target;out->shift_kind=v->shift_kind;
    out->duty_minutes=v->duty_minutes;out->mood_event_tick=v->mood_event_tick;out->mood_event_amount=v->mood_event_amount;
    out->mood_event_source=v->mood_event_source;out->mood_source_kind=v->mood_source_kind;out->mood_source_type=v->mood_source_type;
    out->leisure_elapsed=v->leisure_elapsed;out->leisure_duration=v->leisure_duration;
    if(v->workplace&&expansion_military(v->workplace->type))out->combat_max_hp=out->combat_max_hp*(v->workplace->type==CORE_BLD_GARRISON?150:125)/100;
}
