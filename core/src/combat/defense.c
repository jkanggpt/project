/* defense — 현재 기능의 단일 구현 */
#include "expansion.h"
#include "placement.h"
#include "rank.h"
#include "villager_ai.h"
#include <stdlib.h>
#include <string.h>

int expansion_military(int type) { return type == CORE_BLD_BARRACKS || type == CORE_BLD_GARRISON; }

void expansion_initialize_shift(World *w, Building *b, Villager *v) {
    v->mission = CORE_MISSION_NONE;
    v->leisure_building = 0;
    v->leisure_elapsed = 0;
    if (!expansion_tower(b->type))
        return;
    Villager *other = NULL;
    for (int i = 0; i < b->worker_count; i++)
        if (b->workers[i] != v) {
            other = b->workers[i];
            break;
        }
    v->shift_kind = other ? 1 - other->shift_kind : 0;
    v->shift_changed_day = v->shift_kind ? exp_day(w) : 0;
    v->shift_adapt_days = 0;
    v->shift_sleep_day = 0;
}

int expansion_defend_damage(const Villager *v, int damage) {
    if (damage <= 0 || !v->workplace || !expansion_military(v->workplace->type))
        return damage;
    int armor = exp_clamp(2 + v->stats.vitality / 4, 2, 4) + (v->workplace->type == CORE_BLD_GARRISON);
    int maximum = exp_clamp(100 + 8 * (v->stats.vitality - 4), 76, 148) *
                  (v->workplace->type == CORE_BLD_GARRISON ? 150 : 125) / 100;
    int reduced = exp_clamp(damage - armor, 1, 10000);
    return exp_clamp((reduced * 100 + maximum / 2) / maximum, 1, 10000);
}

CORE_API int32_t core_expansion_building_hit(CoreWorld *w, int32_t id, int32_t damage, int32_t kind,
                                             int32_t source) {
    if (!w)
        return CORE_ERR_NULL_ARG;
    Building *b = entity_find_building(w, id);
    if (!w->expansion || !b)
        return CORE_ERR_NOT_FOUND;
    if (damage < 1 || damage > 10000 || kind < 0 || kind > 3)
        return CORE_ERR_INVALID_ARG;
    if (b->state == CORE_BSTATE_RUINED || (kind == 1 && b->state == CORE_BSTATE_CONSTRUCTING))
        return CORE_IGNORED;
    b->damage_kind = kind;
    b->damage_source = source;
    b->damage_tick = w->clock.tick;
    if (kind == 1)
        return core_report_building_damage(w, w->threat.raid.id, id, damage);
    placement_damage(w, b, damage);
    if (b->type == CORE_BLD_KEEP && b->state == CORE_BSTATE_RUINED)
        rank_lose(w, CORE_LOSE_KEEP);
    return CORE_OK;
}

CORE_API int32_t core_expansion_person_hit(CoreWorld *w, int32_t id, int32_t damage, int32_t kind,
                                           int32_t source) {
    if (!w)
        return CORE_ERR_NULL_ARG;
    Villager *v = entity_find_villager(w, id);
    if (!w->expansion || !v)
        return CORE_ERR_NOT_FOUND;
    v->damage_kind = kind;
    v->damage_source = source;
    return core_report_villager_damage(w, w->threat.raid.id, id, damage);
}

int expansion_tower(int type) { return expansion_spec(type)->category == CORE_CAT_TOWERS; }
