/* expansion_people.c — 다섯 능력치, 실제 근무 숙련, 생활 시간표와 계약 주민 */
#include "difficulty.h"
#include "expansion.h"
#include "population.h"
#include "villager_ai.h"
#include <stdlib.h>
#include <string.h>

static const int WEIGHTS[12][5] = {{35, 0, 30, 20, 15}, {40, 20, 30, 10, 0}, {40, 0, 35, 15, 10},
                                   {0, 15, 30, 30, 25}, {0, 30, 10, 40, 20}, {10, 40, 20, 30, 0},
                                   {10, 0, 20, 40, 30}, {35, 30, 25, 10, 0}, {0, 0, 15, 35, 50},
                                   {0, 25, 0, 25, 50},  {0, 20, 0, 30, 50},  {0, 0, 15, 15, 70}};
static const int COEFF[10] = {500, 600, 750, 950, 1200, 1450, 1800, 2200, 2700, 3400};
static int stat(const Villager *v, int i) {
    switch (i) {
    case 0:
        return v->stats.strength;
    case 1:
        return v->stats.agility;
    case 2:
        return v->stats.vitality;
    case 3:
        return v->stats.dexterity;
    default:
        return v->stats.intellect;
    }
}
static void set_stat(Villager *v, int i, int value) {
    switch (i) {
    case 0:
        v->stats.strength = value;
        break;
    case 1:
        v->stats.agility = value;
        break;
    case 2:
        v->stats.vitality = value;
        break;
    case 3:
        v->stats.dexterity = value;
        break;
    default:
        v->stats.intellect = value;
        break;
    }
}
static int draw_stat(World *w) {
    static const int ends[9] = {400, 1300, 3200, 6600, 8900, 9700, 9940, 9990, 10000};
    int roll = rng_range(&w->rng, 0, 9999), i;
    for (i = 0; i < 9; i++)
        if (roll < ends[i])
            return i + 1;
    return 9;
}
void expansion_initialize_villager(World *w, Villager *v) {
    int i;
    for (i = 0; i < 5; i++) {
        v->initial_stats[i] = draw_stat(w);
        set_stat(v, i, v->initial_stats[i]);
    }
    v->personal_id = w->expansion->next_person_id++;
    v->water = 80;
    v->hygiene = 80;
    v->meal_day = 0;
    v->visit_day = 0;
    v->current_x_milli = v->loc_x * 1000 + 500;
    v->current_y_milli = v->loc_y * 1000 + 500;
    v->mood = w->difficulty == 0 ? 50 : w->difficulty == 1 ? 42 : 34;
}
static int skill_bonus(int minutes) {
    static const int ends[4] = {0, 8 * 540, 30 * 540, 75 * 540}, bonus[4] = {0, 20, 50, 100};
    int i;
    for (i = 1; i < 4; i++)
        if (minutes < ends[i])
            return bonus[i - 1] +
                   (bonus[i] - bonus[i - 1]) * (minutes - ends[i - 1]) / (ends[i] - ends[i - 1]);
    return 100;
}
int expansion_work_percent(const World *w, const Villager *v, int type) {
    const ExpansionBuildingSpec *s = expansion_spec(type);
    int field = s->field, weighted = 0, i, traits = 100, result;
    if (field < 0 || field >= 12)
        return 100;
    for (i = 0; i < 5; i++) {
        int weight = WEIGHTS[field][i];
        if (s->category == CORE_CAT_TOWERS && field == 7)
            weight = (int[]){0, 40, 20, 40, 0}[i];
        weighted += COEFF[exp_clamp(stat(v, i), 1, 10) - 1] * weight;
    }
    for (i = 0; i < 2; i++)
        if (v->traits[i] >= 0 && v->traits[i] < w->defs.trait_count)
            traits += w->defs.traits[v->traits[i]].work_pct;
    result = (int)((int64_t)weighted * (100 + skill_bonus(v->skill_minutes[field])) * (50 + v->mood) *
                   exp_clamp(traits, 20, 200) * exp_clamp(v->health, 25, 100) / 100000000000LL);
    if ((field == 1 || field == 2) && expansion_has_research(w, 1))
        result = result * 110 / 100;
    return exp_clamp(result, 25, 400);
}
int expansion_qualified(const World *w, const Building *b) {
    int i, sum = 0, highest = 0;
    (void)w;
    if (b->type == CORE_BLD_HEALER)
        return b->worker_count >= 1 && b->workers[0]->stats.intellect >= 4;
    if (b->type != CORE_BLD_HOSPITAL)
        return b->worker_count > 0 || !expansion_spec(b->type)->workers;
    for (i = 0; i < b->worker_count; i++) {
        int n = b->workers[i]->stats.intellect;
        sum += n;
        if (n > highest)
            highest = n;
    }
    return b->worker_count >= 2 && sum >= 10 && highest >= 6;
}
void expansion_assign(World *w) {
    int i, j, pass;
    for (i = 0; i < w->villager_count; i++) {
        Villager *v = w->villagers[i];
        if (v->state == CORE_VS_LEAVING)
            continue;
        if (v->home && v->home->state != CORE_BSTATE_ACTIVE)
            entity_unlink_resident(v);
        if (v->workplace && v->workplace->state != CORE_BSTATE_ACTIVE)
            entity_unlink_worker(v);
        {
            Building *best = NULL;
            int score = v->home ? expansion_spec(v->home->type)->housing_mood : -100;
            for (j = 0; j < w->building_count; j++) {
                Building *b = w->buildings[j];
                int slots = w->defs.buildings[b->type].resident_slots;
                if (b->state == CORE_BSTATE_ACTIVE && b->resident_count < slots &&
                    expansion_spec(b->type)->housing_mood > score) {
                    best = b;
                    score = expansion_spec(b->type)->housing_mood;
                }
            }
            if (best &&
                entity_link_resident(best, v, w->defs.buildings[best->type].resident_slots) != CORE_OK)
                w->expansion->error = CORE_ERR_OUT_OF_MEMORY;
        }
    }
    /* 첫 일꾼을 먼저 분산한 뒤 남는 자리를 채워 무인 시설을 줄입니다. */
    for (pass = 0; pass < 8; pass++)
        for (i = 0; i < w->building_count; i++) {
            Building *b = w->buildings[i];
            int slots = w->defs.buildings[b->type].worker_slots;
            Villager *best = NULL;
            int score = -1;
            if (!b->auto_assign || b->state != CORE_BSTATE_ACTIVE || b->worker_count >= slots ||
                b->worker_count > pass)
                continue;
            for (j = 0; j < w->villager_count; j++) {
                Villager *v = w->villagers[j];
                int candidate;
                if (v->workplace || v->state == CORE_VS_LEAVING || (v->flags & CORE_VF_MANUAL))
                    continue;
                if (b->type == CORE_BLD_HEALER && v->stats.intellect < 4)
                    continue;
                candidate = expansion_work_percent(w, v, b->type);
                if (b->type == CORE_BLD_HOSPITAL)
                    candidate += v->stats.intellect * 50;
                if (candidate > score) {
                    score = candidate;
                    best = v;
                }
            }
            if (best && entity_link_worker(b, best, slots) != CORE_OK)
                w->expansion->error = CORE_ERR_OUT_OF_MEMORY;
        }
    expansion_water_supply(w);
}
void expansion_water_supply(World *w) {
    int i, j;
    w->expansion->water_served = 0;
    for (i = 0; i < w->building_count; i++)
        w->buildings[i]->water_served = 0;
    for (i = 0; i < w->villager_count; i++) {
        Villager *v = w->villagers[i];
        Building *best = NULL;
        const Building *home = v->home ? v->home : world_keep(w);
        int distance = 9;
        v->water_building = 0;
        if (!home || v->state == CORE_VS_LEAVING)
            continue;
        for (j = 0; j < w->building_count; j++) {
            Building *b = w->buildings[j];
            int d = exp_dist(home->x, home->y, b->x, b->y);
            if (b->type == CORE_BLD_WELL && b->state == CORE_BSTATE_ACTIVE && b->water_served < 18 &&
                d < distance) {
                distance = d;
                best = b;
            }
        }
        if (best) {
            best->water_served++;
            v->water_building = best->id;
            if (!v->contract_end_tick)
                w->expansion->water_served++;
        }
    }
}
static int well_near(const World *w, const Villager *v) {
    (void)w;
    return v->water_building != 0;
}
int expansion_mood(const World *w, const Villager *v) {
    int value = w->difficulty == 0 ? 31 : w->difficulty == 1 ? 23 : 15, i, commute = 0;
    value += exp_clamp((v->needs.hunger - 50) * 30 / 100, -15, 12) +
             exp_clamp((v->needs.rest - 50) * 23 / 100, -12, 10) +
             exp_clamp((v->needs.safety - 50) / 5, -10, 10);
    value += v->home ? expansion_spec(v->home->type)->housing_mood : -18;
    value += well_near(w, v) ? 6 : -12;
    value += v->flags & CORE_VF_UNPAID ? -15 : v->workplace ? 5 : -3;
    value += (int[]){0, 5, 0, -6, -13}[w->expansion->tax];
    if (v->culture_until >= exp_day(w))
        value += v->culture_bonus;
    value += v->hygiene >= 60   ? (v->hygiene - 60) * 6 / 40
             : v->hygiene >= 35 ? 0
                                : -5 - (35 - v->hygiene) / 5;
    if (v->home && v->workplace)
        commute = exp_dist(v->home->door_x, v->home->door_y, v->workplace->door_x, v->workplace->door_y);
    value -= commute >= 50 ? 8 : commute >= 35 ? 5 : commute >= 20 ? 2 : 0;
    if (v->sick_ticks || (v->home && (v->home->flags & CORE_BF_COLD)))
        value -= 12;
    for (i = 0; i < 2; i++)
        if (v->traits[i] >= 0 && v->traits[i] < w->defs.trait_count)
            value += exp_clamp(w->defs.traits[v->traits[i]].mood, -12, 12);
    return exp_clamp(value, 0, 100);
}
static void intent(Villager *v, int state, Building *b, int x, int y) {
    if (v->target != b || v->target_x != x || v->target_y != y)
        v->inside_building = 0;
    vil_set_intent(v, state, b, x, y, 0);
    if (state == CORE_VS_GUARD_POST)
        v->inside_building = 0;
    else if (v->at_target && b && !(expansion_spec(b->type)->flags & CORE_EXP_OUTDOOR))
        v->inside_building = b->id;
}
static void to_building(const World *w, Villager *v, int state, Building *b, const Building *keep) {
    if (!b) {
        int attempt;
        for (attempt = 0; attempt < 15; attempt++) {
            int n = (v->id + attempt) % 15, x = keep->door_x + n % 5 - 2, y = keep->door_y + n / 5;
            if (grid_in_bounds(&w->grid, x, y) && w->grid.cost[grid_index(&w->grid, x, y)]) {
                intent(v, state, NULL, x, y);
                return;
            }
        }
        intent(v, state, NULL, keep->door_x, keep->door_y);
        return;
    }
    if (expansion_spec(b->type)->flags & CORE_EXP_PLOT) {
        intent(v, state, b, b->x + (v->id % b->w), b->y + ((v->id / b->w) % b->h));
    } else
        intent(v, state, b, b->door_x, b->door_y);
}
void expansion_add_xp(Villager *v, int index, int amount) {
    static const int thresholds[9] = {12, 24, 40, 66, 90, 124, 172, 236, 300};
    int level = stat(v, index);
    if (level >= 10 || level >= v->initial_stats[index] + 3)
        return;
    v->stat_xp[index] += amount;
    while (level < 10 && level < v->initial_stats[index] + 3 &&
           v->stat_xp[index] >= thresholds[level - 1] * 1000) {
        v->stat_xp[index] -= thresholds[level - 1] * 1000;
        set_stat(v, index, ++level);
    }
    if (level == 10 || level == v->initial_stats[index] + 3)
        v->stat_xp[index] = 0;
}
static void gain_xp(Villager *v, int field, int type) {
    int i, main = 0, max = 0;
    static const int ranged[5] = {0, 40, 20, 40, 0};
    const int *weights = expansion_spec(type)->category == CORE_CAT_TOWERS ? ranged : WEIGHTS[field];
    v->skill_minutes[field] += 10;
    for (i = 0; i < 5; i++) {
        int weight = weights[i];
        if (weight > max) {
            max = weight;
            main = i;
        }
    }
    for (i = 0; i < 5; i++) {
        int minutes = v->skill_minutes[field], weight = i == main ? max : weights[i];
        int amount = max ? (int)(((int64_t)minutes * 1000 * weight) / (360 * max) -
                                 ((int64_t)(minutes - 10) * 1000 * weight) / (360 * max))
                         : 0;
        expansion_add_xp(v, i, amount);
    }
}
static int patient_limit(const World *w, const Building *b) {
    int base = b->type == CORE_BLD_HEALER ? 2 : b->worker_count == 2 ? 5 : 8;
    if (!expansion_has_research(w, 18))
        return base;
    return base * 125 / 100 + ((exp_day(w) % 4) < (base * 125 % 100) / 25 ? 1 : 0);
}
static int service_limit(const World *w, const Building *b) {
    int cap = b->type == CORE_BLD_TAVERN      ? 8
              : b->type == CORE_BLD_THEATRE   ? 15
              : b->type == CORE_BLD_CHURCH    ? 12
              : b->type == CORE_BLD_SCHOOL    ? 8
              : b->type == CORE_BLD_LIBRARY   ? 6
              : b->type == CORE_BLD_ACADEMY   ? 12
              : b->type == CORE_BLD_BATHHOUSE ? 12
                                              : 0;
    int workers = w->defs.buildings[b->type].worker_slots;
    if (!workers || !expansion_qualified(w, b))
        return 0;
    return cap * b->worker_count / workers;
}
void expansion_people_tick(World *w) {
    int i, j, minute = exp_minute(w);
    Building *keep = world_keep(w);
    if (!keep)
        return;
    for (i = w->villager_count - 1; i >= 0; i--) {
        Villager *v = w->villagers[i];
        Building *job = v->workplace;
        if (v->state == CORE_VS_LEAVING)
            continue;
        if (v->contract_end_tick && w->clock.tick >= v->contract_end_tick) {
            if (v->state != CORE_VS_LEAVING) {
                entity_unlink_worker(v);
                entity_unlink_resident(v);
                v->inside_building = 0;
                vil_set_intent(v, CORE_VS_LEAVING, NULL, 0, w->keep_y, 0);
            }
            continue;
        }
        if (v->sick_ticks > 0) {
            v->sick_ticks--;
            if (!v->sick_ticks)
                v->health = 100;
        }
        if (minute % 60 == 0) {
            int target = expansion_mood(w, v);
            v->mood += exp_clamp(target - v->mood, -2, 2);
            v->needs.hunger = exp_clamp(v->needs.hunger - 3, 0, 100);
            v->needs.rest = exp_clamp(v->needs.rest - 2, 0, 100);
        }
        if (w->expansion->alarm) {
            if (job && job->type == CORE_BLD_BARRACKS) {
                v->inside_building = 0;
                to_building(w, v, CORE_VS_GUARD_POST, job, keep);
            } else
                to_building(w, v, CORE_VS_FLEE, keep, keep);
            if (v->at_target && v->meal_day < exp_day(w) && expansion_bank_have(w, CORE_RES_FOOD) >= 1200) {
                (void)expansion_bank_add(w, CORE_RES_FOOD, -1200, CORE_LEDGER_CONSUMPTION);
                v->meal_day = exp_day(w);
                v->needs.hunger = 100;
            }
            v->needs.safety = exp_clamp(v->needs.safety + (v->inside_building ? 2 : -1), 0, 100);
            continue;
        }
        v->needs.safety = exp_clamp(v->needs.safety + 1, 0, 100);
        if (minute < 360 || minute >= 1260) {
            to_building(w, v, CORE_VS_SLEEP, v->home, keep);
            if (v->at_target)
                v->needs.rest = exp_clamp(v->needs.rest + 2, 0, 100);
            if (v->at_target && minute % 60 == 0 && !v->sick_ticks && v->meal_day >= exp_day(w) - 1)
                v->health = exp_clamp(v->health + 1, 1, 100);
            continue;
        }
        if (v->sick_ticks || v->health < 85) {
            Building *clinic = NULL;
            for (j = 0; j < w->building_count; j++) {
                Building *b = w->buildings[j];
                if ((b->type == CORE_BLD_HEALER || b->type == CORE_BLD_HOSPITAL) &&
                    b->state == CORE_BSTATE_ACTIVE && expansion_qualified(w, b)) {
                    clinic = b;
                    break;
                }
            }
            if (clinic) {
                to_building(w, v, CORE_VS_SEEK_HEALING, clinic, keep);
                if (v->at_target && minute % 60 == 0 && clinic->visits_today < patient_limit(w, clinic) &&
                    expansion_bank_have(w, CORE_RES_HERBS) >= 1000) {
                    (void)expansion_bank_add(w, CORE_RES_HERBS, -1000, CORE_LEDGER_HEALING);
                    clinic->visits_today++;
                    v->sick_ticks = 0;
                    v->health = 100;
                }
                continue;
            }
        }
        if (v->meal_day < exp_day(w) || v->needs.hunger < 25) {
            to_building(w, v, CORE_VS_EAT, keep, keep);
            if (v->at_target && expansion_bank_have(w, CORE_RES_FOOD) >= 1200) {
                (void)expansion_bank_add(w, CORE_RES_FOOD, -1200, CORE_LEDGER_CONSUMPTION);
                v->needs.hunger = 100;
                v->meal_day = exp_day(w);
                if (well_near(w, v))
                    v->water = 100;
            }
            if (v->meal_day < exp_day(w) && minute < 480)
                continue;
        }
        if (minute < 480 && v->water_building) {
            Building *well = entity_find_building(w, v->water_building);
            if (well) {
                to_building(w, v, CORE_VS_IDLE, well, keep);
                v->inside_building = 0;
                if (v->at_target) {
                    v->water = 100;
                    if (v->hygiene < 65)
                        v->hygiene = 65;
                }
                continue;
            }
        }
        if (minute >= 480 && minute < 1020 && job && job->state == CORE_BSTATE_ACTIVE) {
            to_building(w, v, v->at_target && v->target == job ? CORE_VS_WORKING : CORE_VS_GO_WORK, job,
                        keep);
            if (v->at_target) {
                int effective = expansion_work_percent(w, v, job->type) * 100;
                v->state = CORE_VS_WORKING;
                if (expansion_qualified(w, job)) {
                    job->daily_work_milli += effective;
                    if (exp_season(w) < 2 && (job->type == CORE_BLD_CROPLAND || job->type == CORE_BLD_FARM ||
                                              job->type == CORE_BLD_IRRIGATED_FARM))
                        job->crop_labor_milli += effective;
                    gain_xp(v, expansion_spec(job->type)->field, job->type);
                }
            }
            continue;
        }
        if (minute >= 1020 && minute < 1260 && v->visit_day < exp_day(w)) {
            Building *service = NULL;
            int best = INT32_MAX;
            for (j = 0; j < w->building_count; j++) {
                Building *b = w->buildings[j];
                if (b->state == CORE_BSTATE_ACTIVE && b->operating && b->visits_today < service_limit(w, b)) {
                    int score = exp_dist(v->loc_x, v->loc_y, b->door_x, b->door_y) +
                                (v->id * 7 + b->id * 3 + exp_day(w)) % 17;
                    if (v->target == b)
                        score -= 50;
                    if (b->type == CORE_BLD_BATHHOUSE && v->hygiene < 50)
                        score -= 100;
                    if (score < best) {
                        best = score;
                        service = b;
                    }
                }
            }
            if (service) {
                to_building(w, v, CORE_VS_IDLE, service, keep);
                if (v->at_target) {
                    v->visit_day = exp_day(w);
                    service->visits_today++;
                    if (service->type == CORE_BLD_BATHHOUSE)
                        v->hygiene = 100;
                    else if (service->type == CORE_BLD_SCHOOL || service->type == CORE_BLD_LIBRARY ||
                             service->type == CORE_BLD_ACADEMY)
                        expansion_add_xp(v, 4,
                                         (service->type == CORE_BLD_SCHOOL    ? 2
                                          : service->type == CORE_BLD_LIBRARY ? 1
                                                                              : 3) *
                                             1000);
                    else {
                        v->culture_bonus = service->type == CORE_BLD_TAVERN   ? 6
                                           : service->type == CORE_BLD_CHURCH ? 7
                                                                              : 10;
                        v->culture_until = exp_day(w) + 2;
                    }
                }
                continue;
            }
        }
        to_building(w, v, CORE_VS_IDLE, NULL, keep);
    }
}
void expansion_people_daily(World *w) {
    ExpansionState *e = w->expansion;
    int i, mood = 0, pop = 0, free_slots = 0, day = exp_day(w), water = 0;
    for (i = 0; i < w->building_count; i++) {
        Building *b = w->buildings[i];
        b->visits_today = 0;
        if (b->state == CORE_BSTATE_ACTIVE)
            free_slots += w->defs.buildings[b->type].resident_slots - b->resident_count;
    }
    for (i = w->villager_count - 1; i >= 0; i--) {
        Villager *v = w->villagers[i];
        v->hygiene = exp_clamp(v->hygiene - 5, 0, 100);
        v->water = exp_clamp(v->water - 20, 0, 100);
        if (!v->contract_end_tick) {
            int wage = v->workplace ? w->defs.buildings[v->workplace->type].wage : 0;
            pop++;
            mood += v->mood;
            water += well_near(w, v);
            if (v->home)
                (void)expansion_bank_add(w, CORE_RES_GOLD, e->tax * 1000, CORE_LEDGER_TAXES);
            if (wage && w->bank.amount[CORE_RES_GOLD] >= wage) {
                (void)expansion_bank_add(w, CORE_RES_GOLD, -wage * 1000, CORE_LEDGER_WAGES);
                v->flags &= ~CORE_VF_UNPAID;
            } else if (wage)
                v->flags |= CORE_VF_UNPAID;
        }
        if (v->needs.hunger == 0)
            v->health -= 12;
        if (v->health <= 0) {
            population_kill(w, v, CORE_DEATH_STARVATION);
            continue;
        }
        if (v->mood < 15)
            v->low_mood_ticks += 144;
        else
            v->low_mood_ticks = 0;
        if (v->low_mood_ticks >= 5 * 144 && !v->contract_end_tick) {
            entity_unlink_worker(v);
            entity_unlink_resident(v);
            v->inside_building = 0;
            vil_set_intent(v, CORE_VS_LEAVING, NULL, 0, w->keep_y, 0);
        }
    }
    e->water_served = water;
    expansion_assign(w);
    if (pop)
        mood /= pop;
    if (day < e->immigration_day || day < e->disaster_day + 5 || free_slots <= 0 || water < pop)
        return;
    {
        static const int moods[6] = {35, 48, 52, 55, 57, 60}, fooddays[6] = {5, 8, 10, 12, 12, 12},
                         interval[6] = {5, 7, 6, 5, 5, 4};
        int count = w->rank == 0 ? 2
                                 : rng_range(&w->rng,
                                             w->rank == 1  ? 2
                                             : w->rank < 4 ? 3
                                                           : 4,
                                             w->rank == 1   ? 3
                                             : w->rank == 2 ? 4
                                             : w->rank == 3 ? 5
                                             : w->rank == 4 ? 6
                                                            : 7);
        if (w->rank == 0 && (e->starter_waves >= 2 || (e->starter_waves == 0 && day < 5) ||
                             (e->starter_waves == 1 && day < 10)))
            return;
        if (mood < moods[w->rank] || expansion_bank_have(w, CORE_RES_FOOD) < pop * 1200 * fooddays[w->rank])
            return;
        if (w->rank == 0 && free_slots < 2)
            return;
        if (exp_season(w) == 3)
            count = (count + 1) / 2;
        if (count > free_slots)
            count = free_slots;
        for (i = 0; i < count; i++) {
            Villager *v = population_create(w, w->keep_x, 0);
            if (!v) {
                e->error = CORE_ERR_OUT_OF_MEMORY;
                break;
            }
            v->intent_flags |= CORE_INTENT_SPAWN_EDGE;
            v->at_target = 0;
            v->target_x = w->keep_x;
            v->target_y = w->keep_y + 2;
            events_push_villager(&w->events, CORE_EVT_IMMIGRANT, v->id, 0, 0, v->name);
        }
        if (w->rank == 0)
            e->starter_waves++;
        e->immigration_day = day + interval[w->rank];
        expansion_assign(w);
        e->dirty = 1;
    }
}
