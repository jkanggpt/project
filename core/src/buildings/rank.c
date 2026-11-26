/* rank.c — 현재 승급 조건의 공개 뷰와 패배 처리 */
#include "rank.h"
#include "expansion.h"
#include "threat.h"
#include <string.h>

int32_t rank_defense(const World *world) {
    int32_t total = 0;
    int32_t i;
    for (i = 0; i < world->building_count; i++) {
        const Building *building = world->buildings[i];
        if (building->state == CORE_BSTATE_ACTIVE) {
            total += world_building_def(world, building->type)->defense;
        }
    }
    for (i = 0; i < world->villager_count; i++) {
        total += threat_is_guard(world, world->villagers[i]);
    }
    return total;
}
void rank_lose(World *world, int32_t reason) {
    if (world->outcome == CORE_OUTCOME_LOST) {
        return;
    }
    world->outcome = CORE_OUTCOME_LOST;
    world->lose_reason = reason;
    threat_abort_raid(world);
    events_push_progress(&world->events, CORE_EVT_DEFEAT, reason, clock_day(&world->clock), 0, NULL);
}
int32_t rank_storage_bonus(const World *world) {
    (void)world;
    return 0;
}
int rank_is_ready(const World *world) {
    CoreExpansionRankView v;
    return core_expansion_rank(world, &v) == CORE_OK && v.ready;
}
void rank_fill_view(const World *world, CoreRankView *out) {
    CoreExpansionRankView v;
    memset(out, 0, sizeof(*out));
    if (core_expansion_rank(world, &v) != CORE_OK)
        return;
    int total = 0, count = 0;
    for (int i = 0; i < world->villager_count; i++) {
        const Villager *p = world->villagers[i];
        if (!p->incoming && !p->contract_end_tick && p->state != CORE_VS_LEAVING) {
            total += p->happiness;
            count++;
        }
    }
    out->rank = world->rank;
    out->next_rank = world->rank < 5 ? v.target : -1;
    out->ready = v.ready;
    out->have_population = count;
    out->have_mood = count ? total / count : 0;
    out->have_defense = rank_defense(world);
    out->have_research = world->research.done_count;
    out->need_population = v.population;
    out->need_mood = v.mood;
    out->need_research = v.research;
    out->tribute_gold = v.gold;
    out->tribute_mana = v.mana;
    memcpy(out->unlocks, v.unlocks, sizeof(out->unlocks));
    for (int i = 0; i < world->defs.research_count; i++)
        out->research_unlocks += world->defs.research[i].required_rank == v.target;
}
