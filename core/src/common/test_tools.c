/* test_tools.c — 실행 중 세계 검사. 모든 우회는 명시적으로 켠 세계에만 적용합니다. */
#include "test_tools.h"
#include "expansion.h"
#include "population.h"
#include "threat.h"
#include <string.h>

static int allowed(const World *w) {
    if (!w)
        return CORE_ERR_NULL_ARG;
    return (w->test_flags & CORE_TEST_ENABLED) ? CORE_OK : CORE_ERR_REJECTED;
}

static void restore_people(World *w) {
    for (int i = 0; i < w->villager_count; i++) {
        Villager *v = w->villagers[i];
        nature_cure(w, v, 0);
        v->health = 100;
        v->needs = (Needs){100, 100, 100, 100};
        v->water = v->hygiene = 100;
        v->hunger_ticks = v->hunger_acc = v->rest_acc = v->low_mood_ticks = 0;
        v->critical = v->thirst_stage = v->hunger_stage = 0;
    }
}

void test_tools_maintain(World *w) {
    if (!(w->test_flags & CORE_TEST_ENABLED))
        return;
    if (w->test_weather >= 0)
        w->nature.weather = w->test_weather;
    if (w->test_flags & CORE_TEST_RESOURCES)
        for (int r = 0; r < CORE_RES_COUNT; r++) {
            int limit = eco_bank_limit(&w->bank, r);
            /* 화폐도 과도한 장부 변동 없이 충분한 시험 잔고만 유지합니다. */
            int target = limit > 100000 ? 100000 : limit;
            if (w->bank.amount[r] < target)
                (void)core_test_resource(w, r, target);
        }
    if (w->test_flags & CORE_TEST_SUSTAIN)
        restore_people(w);
}

CORE_API int32_t core_test_configure(CoreWorld *w, int32_t flags, int32_t weather) {
    if (!w)
        return CORE_ERR_NULL_ARG;
    if (!w->expansion || flags < 0 || (flags & ~CORE_TEST_ALL_FLAGS) ||
        (!(flags & CORE_TEST_ENABLED) && flags) || weather < -1 || weather >= CORE_WEATHER_COUNT ||
        (!flags && weather != -1))
        return CORE_ERR_INVALID_ARG;
    w->test_flags = flags;
    w->test_weather = weather;
    test_tools_maintain(w);
    expansion_visibility(w);
    return CORE_OK;
}

CORE_API int32_t core_test_read(const CoreWorld *w, int32_t *flags, int32_t *weather) {
    if (!w || !flags || !weather)
        return CORE_ERR_NULL_ARG;
    *flags = w->test_flags;
    *weather = (w->test_flags & CORE_TEST_ENABLED) ? w->test_weather : -1;
    return CORE_OK;
}

CORE_API int32_t core_test_resource(CoreWorld *w, int32_t r, int32_t amount) {
    int rc = allowed(w);
    if (rc != CORE_OK)
        return rc;
    if (r < 0 || r >= CORE_RES_COUNT || amount < 0 || amount > eco_bank_limit(&w->bank, r))
        return CORE_ERR_INVALID_ARG;
    int delta = amount - w->bank.amount[r];
    int64_t total = (int64_t)w->ledger.today[r][CORE_LEDGER_OTHER] + delta;
    if (total < INT32_MIN || total > INT32_MAX)
        return CORE_ERR_INVALID_ARG;
    w->bank.amount[r] = amount;
    w->ledger.today[r][CORE_LEDGER_OTHER] = (int32_t)total;
    w->expansion->fractional[r] = 0;
    if (r == CORE_RES_LOGS && w->expansion->magic_logs_milli > expansion_bank_have(w, r))
        w->expansion->magic_logs_milli = expansion_bank_have(w, r);
    return CORE_OK;
}

CORE_API int32_t core_test_clock(CoreWorld *w, int32_t day, int32_t minute) {
    int rc = allowed(w);
    if (rc != CORE_OK)
        return rc;
    if (day < 1 || day > CLOCK_MAX_DAYS || minute < 0 || minute >= 1440 || minute % 10)
        return CORE_ERR_INVALID_ARG;
    int tick = (day - 1) * CLOCK_TICKS_PER_DAY + minute / CLOCK_MINUTES_PER_TICK;
    if (tick < w->clock.tick)
        return CORE_ERR_REJECTED;
    w->clock.tick = tick;
    memset(w->expansion->frontend_clock, 0, sizeof(w->expansion->frontend_clock));
    expansion_visibility(w);
    return CORE_OK;
}

CORE_API int32_t core_test_rank(CoreWorld *w, int32_t rank) {
    int rc = allowed(w);
    if (rc != CORE_OK)
        return rc;
    if (rank < 0 || rank >= CORE_RANK_COUNT)
        return CORE_ERR_INVALID_ARG;
    w->rank = rank;
    w->rank_ready_announced = -1;
    w->expansion->promotion_state = w->expansion->promotion_target = 0;
    w->expansion->promotion_remaining = 0;
    expansion_capacity(w);
    expansion_visibility(w);
    return CORE_OK;
}

CORE_API int32_t core_test_research(CoreWorld *w, int32_t index) {
    int rc = allowed(w);
    if (rc != CORE_OK)
        return rc;
    if (index < -1 || index >= w->defs.research_count)
        return CORE_ERR_INVALID_ARG;
    if (index < 0)
        w->research.done = (UINT32_C(1) << w->defs.research_count) - 1;
    else {
        /* 선행 연구도 함께 완료해 정상 연구 화면과 저장 검사를 지킵니다. */
        for (int i = index; i >= 0; i = w->defs.research[i].requires)
            w->research.done |= UINT32_C(1) << i;
    }
    w->research.done_count = 0;
    for (int i = 0; i < w->defs.research_count; i++)
        w->research.done_count += (w->research.done >> i) & 1u;
    if (w->research.active >= 0 && (w->research.done & (UINT32_C(1) << w->research.active))) {
        w->research.active = -1;
        w->research.progress_milli = 0;
    }
    expansion_apply_research(w);
    expansion_capacity(w);
    expansion_rebuild(w);
    expansion_visibility(w);
    return CORE_OK;
}

CORE_API int32_t core_test_finish_buildings(CoreWorld *w) {
    int rc = allowed(w);
    if (rc != CORE_OK)
        return rc;
    for (int i = 0; i < w->building_count; i++) {
        Building *b = w->buildings[i];
        if (b->state != CORE_BSTATE_CONSTRUCTING)
            continue;
        b->state = CORE_BSTATE_ACTIVE;
        b->build_progress = world_building_def(w, b->type)->build_ticks;
        b->completed_day = exp_day(w);
        events_push_building(&w->events, CORE_EVT_BUILDING_COMPLETED, b->id, b->type, b->x, b->y);
    }
    expansion_capacity(w);
    expansion_rebuild(w);
    expansion_assign(w);
    expansion_visibility(w);
    return CORE_OK;
}

CORE_API int32_t core_test_restore(CoreWorld *w) {
    int rc = allowed(w);
    if (rc != CORE_OK)
        return rc;
    restore_people(w);
    for (int i = 0; i < w->building_count; i++) {
        Building *b = w->buildings[i];
        b->hp = world_building_def(w, b->type)->max_hp;
        if (b->state == CORE_BSTATE_RUINED)
            b->state = CORE_BSTATE_ACTIVE;
    }
    w->outcome = CORE_OUTCOME_NONE;
    w->lose_reason = CORE_LOSE_NONE;
    w->expansion->plague_days = 0;
    expansion_rebuild(w);
    expansion_capacity(w);
    expansion_visibility(w);
    return CORE_OK;
}

CORE_API int32_t core_test_population(CoreWorld *w, int32_t count) {
    int rc = allowed(w);
    if (rc != CORE_OK)
        return rc;
    if (count < 1 || count > 300)
        return CORE_ERR_INVALID_ARG;
    while (w->villager_count < count)
        if (!population_create(w, w->keep_x, w->keep_y + 3))
            return CORE_ERR_OUT_OF_MEMORY;
    while (w->villager_count > count)
        entity_remove_villager(w, w->villagers[w->villager_count - 1]);
    if (w->stats.peak_population < count)
        w->stats.peak_population = count;
    expansion_rebuild(w);
    expansion_assign(w);
    expansion_visibility(w);
    return CORE_OK;
}

CORE_API int32_t core_test_raid(CoreWorld *w, int32_t type, int32_t direction, int32_t count) {
    int rc = allowed(w);
    if (rc != CORE_OK)
        return rc;
    if (count < 0 || count > 64 || type <= CORE_THREAT_NONE || type >= CORE_THREAT_COUNT || direction < 0 ||
        direction >= CORE_DIR_COUNT)
        return CORE_ERR_INVALID_ARG;
    if (!count)
        return w->threat.raid.id ? core_report_raid_ended(w, w->threat.raid.id) : CORE_OK;
    return threat_debug_raid(w, type, direction, count);
}
