/*
 * population.c — 첫 주민, 이주, 떠남, 죽음 (설명은 population.h)
 */
#include "population.h"
#include "expansion.h"

#include <string.h>

#include "difficulty.h"
#include "jobs.h"
#include "villager_ai.h"

/* 난이도별 첫 주민 수 (평화, 보통, 혹독) */

Villager *population_create(World *world, int32_t x, int32_t y) {
    Villager *villager = entity_add_villager(world);
    if (villager == NULL) {
        return NULL;
    }
    expansion_choose_identity(world, villager);
    villager->age = rng_range(&world->rng, 16, 55);
    villager->stats.strength = rng_range(&world->rng, 2, 9);
    villager->stats.intellect = rng_range(&world->rng, 2, 9);
    villager->stats.dexterity = rng_range(&world->rng, 2, 9);
    villager->stats.vitality = rng_range(&world->rng, 2, 9);
    if (world->defs.trait_count > 0) {
        villager->traits[0] = rng_range(&world->rng, 0, world->defs.trait_count - 1);
        if (world->defs.trait_count > 1) {
            /* 두 번째 특성은 첫 번째와 다르게 */
            villager->traits[1] =
                (villager->traits[0] + rng_range(&world->rng, 1, world->defs.trait_count - 1)) %
                world->defs.trait_count;
        }
    }
    villager->needs.hunger = 80;
    villager->needs.rest = 80;
    villager->needs.safety = 100;
    villager->needs.housing = 25;
    villager->mood = 60;
    villager->health = 100;
    villager->state = CORE_VS_IDLE;
    villager->loc_x = x;
    villager->loc_y = y;
    villager->target_x = x;
    villager->target_y = y;
    villager->at_target = 1;
    villager->intent_rev = 1;
    if (world->expansion)
        expansion_initialize_villager(world, villager);
    return villager;
}

int population_spawn_initial(World *world) {
    const Building *keep = world_keep(world);
    int32_t count = ECO_START_POPULATION;
    int32_t i;
    if (keep == NULL) {
        return CORE_ERR_INVALID_ARG;
    }
    for (i = 0; i < count; i++) {
        if (population_create(world, keep->door_x, keep->door_y) == NULL) {
            return CORE_ERR_OUT_OF_MEMORY;
        }
    }
    return CORE_OK;
}

/* 다 지은 집의 빈자리 수 */

/* 이주민이 오려면 있어야 할 식량: 보통은 주민 × 5, 가을에는 한 명 더 늘어도 겨울을 날 만큼 */

void population_begin_leaving(World *world, Villager *villager) {
    int32_t ex;
    int32_t ey;
    if (world->expansion && !expansion_departure_allowed(world, villager, 1))
        return;
    entity_unlink_worker(villager);
    entity_unlink_resident(villager);
    if (world->expansion) {
        villager->departure_reason = CORE_DEPART_VOLUNTARY;
        villager->departure_tick = world->clock.tick;
    }
    villager->hold_ticks = 0;
    vil_edge_tile(world, villager->loc_x, villager->loc_y, &ex, &ey);
    vil_set_intent(villager, CORE_VS_LEAVING, NULL, ex, ey, 0);
    events_push_villager(&world->events, CORE_EVT_VILLAGER_LEAVING, villager->id, 0, 0, villager->name);
    jobs_on_buildings_changed(world); /* 빈 일자리를 다른 주민으로 채움 */
}

void population_remove_departed(World *world, Villager *villager) {
    if (world->expansion && villager->contract_end_tick) {
        ExpansionState *e = world->expansion;
        CoreCandidateView *c;
        int j;
        c = NULL;
        for (j = 0; j < e->archive_count; j++)
            if (e->archive[j].person_id == villager->personal_id)
                c = &e->archive[j];
        if (!c) {
            if (e->archive_count == 64) {
                memmove(e->archive, e->archive + 1, 63 * sizeof(*e->archive));
                e->archive_count--;
            }
            c = &e->archive[e->archive_count++];
        }
        memset(c, 0, sizeof(*c));
        c->person_id = villager->personal_id;
        c->age = villager->age;
        c->field = 0;
        strcpy(c->name, villager->name);
        c->stats[0] = villager->stats.strength;
        c->stats[1] = villager->stats.agility;
        c->stats[2] = villager->stats.vitality;
        c->stats[3] = villager->stats.dexterity;
        c->stats[4] = villager->stats.intellect;
        c->sex = villager->sex;
        c->appearance = villager->appearance;
        memcpy(c->initial, villager->initial_stats, sizeof(c->initial));
        memcpy(c->xp, villager->stat_xp, sizeof(c->xp));
        memcpy(c->skills, villager->skill_minutes, sizeof(c->skills));
        memcpy(c->traits, villager->traits, sizeof(c->traits));
        for (j = 1; j < 12; j++)
            if (c->skills[j] > c->skills[c->field])
                c->field = j;
        expansion_candidate_price(world, c);
    }
    if (world->expansion && !villager->contract_end_tick) {
        if (!villager->departure_approved && !expansion_departure_allowed(world, villager, 0)) {
            vil_set_intent(villager, CORE_VS_IDLE, NULL, world->keep_x, world->keep_y + 3, 0);
            villager->low_mood_ticks = 0;
            world->expansion->dirty = 1;
            return;
        }
        world->expansion->departures_today++;
    }
    events_push_villager(&world->events, CORE_EVT_VILLAGER_LEFT, villager->id, 0, 0, villager->name);
    world->stats.villagers_left++;
    entity_remove_villager(world, villager);
}

void population_kill(World *world, Villager *villager, int32_t cause) {
    Event death = {0};
    death.type = CORE_EVT_VILLAGER_DIED;
    death.as.villager.id = villager->id;
    death.as.villager.detail = cause;
    memcpy(death.text, villager->name, sizeof(death.text));
    death.event_tick = world->clock.tick;
    if (cause == CORE_DEATH_RAID || cause == CORE_DEATH_WILDLIFE) {
        death.cause_kind = villager->damage_kind;
        death.source_id = villager->damage_source;
        if (death.cause_kind == 1 && death.source_id >= 1 &&
            death.source_id <= world->threat.raid.enemy_count)
            death.as.villager.value = world->threat.raid.enemies[death.source_id - 1].type + 1;
        if (death.cause_kind == 2 && world->expansion)
            for (int i = 0; i < world->expansion->animal_count; i++)
                if (world->expansion->animals[i].id == death.source_id)
                    death.as.villager.value = world->expansion->animals[i].kind + 1;
    }
    events_push(&world->events, &death);
    world->stats.villagers_died++;
    world->mourning += POP_MOURNING_PER_DEATH;
    if (world->mourning > POP_MOURNING_MAX) {
        world->mourning = POP_MOURNING_MAX;
    }
    if (world->expansion && !villager->contract_end_tick) {
        int i;
        for (i = 0; i < world->villager_count; i++) {
            Villager *other = world->villagers[i];
            if (other == villager || other->contract_end_tick)
                continue;
            int witness = !other->inside_building &&
                          exp_dist(other->current_x_milli / 1000, other->current_y_milli / 1000,
                                   villager->current_x_milli / 1000, villager->current_y_milli / 1000) <= 4;
            /* 같은 사망에 목격 충격과 공동체 충격을 함께 더하지 않습니다. */
            expansion_mood_event(world, other, witness ? 19 : 27, witness ? -8 : -12, villager->id,
                                 1000000000, 5);
        }
    }
    entity_remove_villager(world, villager);
    jobs_on_buildings_changed(world);
}
