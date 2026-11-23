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
    v->mood = 0;
    v->long_needs[0] = v->needs.hunger * 1000;
    v->long_needs[1] = v->needs.rest * 1000;
    v->long_needs[2] = v->needs.safety * 1000;
    v->happiness = expansion_mood(w, v);
}
CORE_API int32_t core_expansion_skill_threshold(int32_t level) {
    return level>=0 && level<=3 ? (int[]){0,6912,25920,64800}[level] : -1;
}
CORE_API int32_t core_expansion_weights(int32_t type,int32_t out[5]) {
    const ExpansionBuildingSpec *s=expansion_spec(type);
    if(!out)return CORE_ERR_NULL_ARG;
    if(!s || s->field<0 || s->field>=12)return CORE_ERR_INVALID_ARG;
    for(int i=0;i<5;i++)out[i]=s->category==CORE_CAT_TOWERS && s->field==7 ? (int[]){0,40,20,40,0}[i] : WEIGHTS[s->field][i];
    return CORE_OK;
}
static int skill_bonus(int minutes) {
    static const int ends[4] = {0, 6912, 25920, 64800}, bonus[4] = {0, 20, 50, 100};
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
    result = (int)((int64_t)weighted * (100 + skill_bonus(v->skill_minutes[field])) * (50 + v->happiness) *
                   exp_clamp(traits, 20, 200) * exp_clamp(v->health, 25, 100) / 100000000000LL);
    if ((field == 1 || field == 2) && expansion_has_research(w, 1))
        result = result * 110 / 100;
    return exp_clamp(result, 25, 750);
}
int expansion_qualified(const World *w, const Building *b) {
    int sum = 0, highest = 0;
    (void)w;
    for (int i = 0; i < b->worker_count; i++) {
        int n = b->workers[i]->stats.intellect;
        sum += n;
        if (n > highest) highest = n;
    }
    switch (b->type) {
    case CORE_BLD_HEALER: return b->worker_count >= 1 && highest >= 4;
    case CORE_BLD_HOSPITAL: return b->worker_count >= 2 && highest >= 6 && sum >= 10;
    case CORE_BLD_SCHOOL: return highest >= 5 && sum >= 7;
    case CORE_BLD_ACADEMY: return highest >= 7 && sum >= 15;
    default: return b->worker_count > 0 || !expansion_spec(b->type)->workers;
    }
}
int expansion_qualified_present(const World *w,const Building *b) {
    int sum=0,highest=0,count=0;
    for(int i=0;i<b->worker_count;i++)if(expansion_working_present(w,b,b->workers[i])) {
        int n=b->workers[i]->stats.intellect;sum+=n;count++;if(n>highest)highest=n;
    }
    switch(b->type) {
    case CORE_BLD_HEALER:return count>=1 && highest>=4;
    case CORE_BLD_HOSPITAL:return count>=2 && highest>=6 && sum>=10;
    case CORE_BLD_SCHOOL:return highest>=5 && sum>=7;
    case CORE_BLD_ACADEMY:return highest>=7 && sum>=15;
    default:return count>0 || !expansion_spec(b->type)->workers;
    }
}
void expansion_assign(World *w) {
    int i, j, pass;
    for (i = 0; i < w->villager_count; i++) {
        Villager *v = w->villagers[i];
        if (v->state == CORE_VS_LEAVING) {
            continue;
        }
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
            if(best) {
                if(entity_link_worker(b,best,slots)!=CORE_OK)w->expansion->error=CORE_ERR_OUT_OF_MEMORY;
                else expansion_initialize_shift(w,b,best);
            }
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
            int d = expansion_centers_distance(home, b);
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
int expansion_available_housing(World *w) {
    int i, j, total = 0;
    int *remaining = w->expansion->queue;
    for (j = 0; j < w->building_count; j++)
        remaining[j] = w->buildings[j]->type == CORE_BLD_WELL && w->buildings[j]->state == CORE_BSTATE_ACTIVE
                           ? 18 - w->buildings[j]->water_served
                           : 0;
    for (i = 0; i < w->building_count; i++) {
        Building *home = w->buildings[i];
        int slots = w->defs.buildings[home->type].resident_slots - home->resident_count;
        if (home->state != CORE_BSTATE_ACTIVE || slots <= 0)
            continue;
        for (j = 0; j < w->building_count && slots > 0; j++) {
            Building *well = w->buildings[j];
            if (remaining[j] > 0 && expansion_centers_distance(home, well) <= 8) {
                int supplied = slots < remaining[j] ? slots : remaining[j];
                total += supplied;
                slots -= supplied;
                remaining[j] -= supplied;
            }
        }
    }
    return total;
}
int expansion_mood(const World *w, const Villager *v) {
    int value = w->difficulty == 0 ? 31 : w->difficulty == 1 ? 23 : 15, i, commute = 0, traits = 0;
    int h = v->long_needs[0], r = v->long_needs[1], s = v->long_needs[2];
    int mood = exp_clamp(v->mood, -200, 200), absolute = abs(mood);
    int fraction =
        exp_clamp(10000 - 15 * (100000 - h) / 100 - (h < 40000 ? (40000 - h) / 2 : 0), -25000, 10000);
    fraction +=
        exp_clamp(10000 - 12 * (100000 - r) / 100 - (r < 30000 ? 35 * (30000 - r) / 100 : 0), -14000, 10000);
    fraction += exp_clamp(10000 - 14 * (100000 - s) / 100, -4000, 10000);
    fraction += (mood < 0 ? -1 : 1) * (30 * absolute + 3 * absolute * absolute / 4);
    value += (fraction + (fraction >= 0 ? 500 : -500)) / 1000;
    value += v->home ? expansion_spec(v->home->type)->housing_mood : -18;
    value += well_near(w, v) ? 6 : -12;
    value += v->flags & CORE_VF_UNPAID ? -15 : v->workplace ? 5 : -3;
    value += (int[]){0, 5, 0, -6, -13}[w->expansion->tax];
    if (v->home && w->expansion->protected_cells[grid_index(&w->grid, v->home->x, v->home->y)])
        value += 4;
    value += v->hygiene >= 60   ? (v->hygiene - 60) * 6 / 40
             : v->hygiene >= 35 ? 0
                                : -5 - (35 - v->hygiene) / 5;
    if (v->home && v->workplace)
        commute = exp_dist(v->home->door_x, v->home->door_y, v->workplace->door_x, v->workplace->door_y);
    value -= commute >= 50 ? 8 : commute >= 35 ? 5 : commute >= 20 ? 2 : 0;
    if (v->sick_ticks || (v->home && (v->home->flags & CORE_BF_COLD)))
        value -= 12;

    if (w->expansion->disaster_day > 0 && exp_day(w) < w->expansion->disaster_day + 5)
        value -= 3;
    for (i = 0; i < 2; i++)
        if (v->traits[i] >= 0 && v->traits[i] < w->defs.trait_count)
            traits += w->defs.traits[v->traits[i]].mood;
    value += exp_clamp(traits, -12, 12);
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
    if(state!=CORE_VS_GUARD_POST && state!=CORE_VS_WORKING && v->mission!=CORE_MISSION_WALK)v->mission=CORE_MISSION_NONE;
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
static int outfit_type(int type) {
    return type==CORE_BLD_HUNTER_CAMP?1:type==CORE_BLD_HUNTING_LODGE?2:type==CORE_BLD_BARRACKS?3:type==CORE_BLD_GARRISON?4:0;
}
CORE_API int32_t core_expansion_outfit(const CoreWorld *w,int32_t id) {
    if(!w||!w->expansion)return 0;
    const Villager *v=entity_find_villager(w,id);if(!v || v->mission==CORE_MISSION_EQUIP)return 0;
    const Building *b=entity_find_building(w,v->mission_target);
    return b && b->state==CORE_BSTATE_ACTIVE ? outfit_type(b->type) : 0;
}
static int uniform_tick(World *w,Villager *v,Building *job,int on_duty) {
    Building *wardrobe=entity_find_building(w,v->mission_target);
    int wanted=job && on_duty && outfit_type(job->type) && !(w->expansion->alarm && !expansion_military(job->type));
    if(v->mission_target && (!wardrobe || wardrobe->state!=CORE_BSTATE_ACTIVE)) {
        v->mission_target=0;v->mission=CORE_MISSION_NONE;v->inside_building=0;
    }
    if(v->mission_target && (!wanted || job!=wardrobe) && v->mission!=CORE_MISSION_UNEQUIP) {
        v->mission=CORE_MISSION_UNEQUIP;v->mission_next_tick=0;
    }
    if(wanted && !v->mission_target) {
        v->mission_target=job->id;wardrobe=job;v->mission=CORE_MISSION_EQUIP;v->mission_next_tick=0;
    }
    if(v->mission!=CORE_MISSION_EQUIP && v->mission!=CORE_MISSION_UNEQUIP)return 0;
    if(!wardrobe)return 0;
    intent(v,CORE_VS_GO_WORK,wardrobe,wardrobe->door_x,wardrobe->door_y);
    if(!v->at_target)return 1;
    v->inside_building=wardrobe->id;
    if(!v->mission_next_tick){v->mission_next_tick=w->clock.tick+2;return 1;}
    if(w->clock.tick<v->mission_next_tick)return 1;
    int equipped=v->mission==CORE_MISSION_EQUIP;
    if(!equipped)v->mission_target=0;
    v->mission=CORE_MISSION_NONE;v->inside_building=0;v->mission_next_tick=0;
    return 0;
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
void expansion_gain_xp(Villager *v, int field, int type, int minutes) {
    static const int ranged[5] = {0, 40, 20, 40, 0};
    if (field < 0 || field >= 12 || !minutes) return;
    const int *weights = expansion_spec(type)->category == CORE_CAT_TOWERS ? ranged : WEIGHTS[field];
    v->skill_minutes[field] += minutes;
    while(minutes>0) {
        int chunk=360-v->xp_work_minutes[field];if(chunk>minutes)chunk=minutes;
        for(int i=0;i<5;i++)v->xp_pending[field][i]+=weights[i]*(i==4?30:60)*chunk;
        v->xp_work_minutes[field]+=chunk;minutes-=chunk;
        if(v->xp_work_minutes[field]==360) {
            v->xp_work_minutes[field]=0;
            for(int i=0;i<5;i++) {
                expansion_add_xp(v,i,v->xp_pending[field][i]/360);
                v->xp_pending[field][i]%=360;
            }
        }
    }
}
void expansion_migrate_xp(Villager *v) {
    static const int ranged[5]={0,40,20,40,0};
    for(int f=0;f<12;f++)for(int i=0;i<5;i++) {
        const int *weights=f==7 && v->workplace && expansion_tower(v->workplace->type)?ranged:WEIGHTS[f];
        v->xp_pending[f][i]=v->xp_work_minutes[f]*weights[i]*(i==4?30:60);
    }
}
static int patient_limit(const World *w, const Building *b) {
    int highest = 0, sum = 0, count = 0;
    for (int i = 0; i < b->worker_count; i++)
        if (expansion_working_present(w, b, b->workers[i])) {
            int n = b->workers[i]->stats.intellect;
            count++; sum += n;
            if (n > highest) highest = n;
        }
    if (b->type == CORE_BLD_HEALER ? highest < 4 : count < 2 || highest < 6 || sum < 10) return 0;
    return b->visits_today + b->service_credit_milli / 1000;
}
static int service_efficiency(const World *w,const Building *b) {
    int staff=expansion_working_count(w,b);
    return staff ? exp_clamp(b->daily_work_milli / (staff * 540), 0, 7500) : 0;
}
void expansion_leisure_reward(World *w,Villager *v, Building *b, int day) {
    int x = service_efficiency(w,b), type = b->type;
    b->service_credit_milli -= 1000;
    b->visits_today++;
    if (type == CORE_BLD_SCHOOL || type == CORE_BLD_LIBRARY || type == CORE_BLD_ACADEMY) {
        int base = type == CORE_BLD_SCHOOL ? 2000 : type == CORE_BLD_LIBRARY ? 1000 : 3000;
        expansion_add_xp(v, 4, base + x * (type == CORE_BLD_LIBRARY ? 250 : 500) / 1000);
        return;
    }
    int index = type == CORE_BLD_TAVERN ? 0 : type == CORE_BLD_BATHHOUSE ? 1 : type == CORE_BLD_CHURCH ? 3 : 2;
    if (type == CORE_BLD_BATHHOUSE) v->hygiene = 100;
    if (day >= v->leisure_days[index]) {
        int base = (int[]){12, 10, 20, 14}[index], slope = (int[]){800,600,1000,800}[index];
        v->mood_fraction_milli += base * 1000 + x * slope / 1000;
        v->mood = exp_clamp(v->mood + v->mood_fraction_milli / 1000, -200, 200);
        v->mood_fraction_milli %= 1000;
        v->leisure_days[index] = day + 2;
        v->mood_cause = 6 + index;
    }
}
void expansion_people_tick(World *w) {
    int i, j, minute = exp_minute(w);
    Building *keep = world_keep(w);
    if (!keep)
        return;
    for (i = w->villager_count - 1; i >= 0; i--) {
        Villager *v = w->villagers[i];
        Building *job = v->workplace;
        if(job && job->combat_until_tick<=w->clock.tick)job->combat_guard_id=0;
        CoreWorkSchedule schedule;expansion_schedule(w,v,&schedule);
        if (v->health >= 30)
            v->critical = 0;
        if (v->state == CORE_VS_LEAVING) {
            if (!v->contract_end_tick && !expansion_departure_allowed(w, v, 1)) {
                v->low_mood_ticks = 0;
                to_building(w, v, CORE_VS_IDLE, v->home, keep);
                w->expansion->dirty = 1;
            } else {
                if (v->at_target && (!v->contract_end_tick || w->clock.tick >= v->contract_end_tick))
                    population_remove_departed(w, v);
                continue;
            }
        }
        if (v->incoming) {
            to_building(w, v, CORE_VS_IDLE, keep, keep);
            if (!v->at_target)
                continue;
            expansion_arrival_join(w, v);
        }
        expansion_v122_life(w,v);
        expansion_wellbeing_sample(w, v);
        if (!v->at_target)
            w->expansion->travel_minutes += 10;
        if (v->contract_end_tick &&
            (w->clock.tick >= v->contract_end_tick ||
             (v->contract_end_tick - w->clock.tick) * vil_walk_speed(w, v) <=
                 exp_dist(v->current_x_milli / 1000, v->current_y_milli / 1000,
                          v->current_x_milli / 1000 < w->grid.width / 2 ? 0 : w->grid.width - 1,
                          v->current_y_milli / 1000) *
                     1000)) {
            if (v->state != CORE_VS_LEAVING) {
                int x = 0, y = w->keep_y;
                if (!expansion_edge(w, v->current_x_milli / 1000, v->current_y_milli / 1000, &x, &y))
                    continue;
                entity_unlink_worker(v);
                entity_unlink_resident(v);
                v->inside_building = 0;
                vil_set_intent(v, CORE_VS_LEAVING, NULL, x, y, 0);
            }
            continue;
        }
        if (v->sick_ticks > 0) {
            v->sick_ticks--;
            if (!v->sick_ticks) {
                v->health = 100;
                v->flags &= ~CORE_VF_SICK;
            }
        }
        if (minute % 60 == 0) {
            int safe = expansion_person_protected(w, v);
            int desired = safe ? 100 : w->expansion->alarm ? 30 : v->inside_building ? 75 : 60;
            v->needs.safety += exp_clamp(desired - v->needs.safety, -4, safe ? 8 : 3);
            v->needs.hunger = exp_clamp(v->needs.hunger - 3, 0, 100);
            v->needs.rest = exp_clamp(v->needs.rest - 2, 0, 100);
        }
        if (uniform_tick(w,v,job,schedule.on_duty))continue;
        if (w->expansion->alarm || v->hold_ticks > 0) {
            if (v->hold_ticks > 0)
                v->hold_ticks--;
            if (job && (expansion_military(job->type) || (expansion_tower(job->type) && schedule.on_duty))) {
                v->inside_building = 0;
                (void)expansion_duty(w,v,job);
                to_building(w, v, expansion_military(job->type) ? CORE_VS_GUARD_POST : CORE_VS_WORKING, job, keep);
                if (expansion_working_present(w, job, v)) expansion_gain_xp(v, 7, job->type, expansion_begin_work(w, v, job));
            } else if (v->state != CORE_VS_FLEE ||
                       (v->refuge_revision != w->grid.cost_rev &&
                        !(grid_in_bounds(&w->grid, v->target_x, v->target_y) &&
                          w->expansion->protected_cells[grid_index(&w->grid, v->target_x, v->target_y)]))) {
                int x, y;
                Building *refuge = expansion_refuge(w, v, &x, &y);
                if (refuge)
                    to_building(w, v, CORE_VS_FLEE, refuge, keep);
                else
                    intent(v, CORE_VS_FLEE, NULL, x, y);
                v->refuge_revision = w->grid.cost_rev;
            }
            if (v->at_target && v->meal_day < exp_day(w) && expansion_bank_have(w, CORE_RES_FOOD) >= 1000) {
                (void)expansion_bank_add(w, CORE_RES_FOOD, -1000, CORE_LEDGER_CONSUMPTION);
                v->meal_day = exp_day(w);
                v->needs.hunger = 100;
                expansion_mood_event(w,v,0,2,0,1,3);
            }
            continue;
        }
        int sleeping=schedule.sleep_start<schedule.sleep_end ? minute>=schedule.sleep_start&&minute<schedule.sleep_end : minute>=schedule.sleep_start||minute<schedule.sleep_end;
        if (sleeping && !schedule.on_duty) {
            to_building(w, v, CORE_VS_SLEEP, v->home, keep);
            if (v->at_target)
                v->needs.rest = exp_clamp(v->needs.rest + 2, 0, 100);
            if(v->at_target && job && expansion_tower(job->type) && schedule.shift && v->shift_adapt_days<3 && v->shift_sleep_day!=exp_day(w)) {
                expansion_mood_event(w,v,14,(int[]){-8,-5,-2}[v->shift_adapt_days],job->id,144,1);
                v->shift_adapt_days++;v->shift_sleep_day=exp_day(w);
            }
            if (v->at_target && minute % 60 == 0 && !v->sick_ticks && v->meal_day >= exp_day(w) - 1)
                v->health = exp_clamp(v->health + 1, 1, 100);
            continue;
        }
        if (v->sick_ticks || v->health < 85) {
            Building *clinic = NULL;
            for (j = 0; j < w->building_count; j++) {
                Building *b = w->buildings[j];
                if ((b->type == CORE_BLD_HEALER || b->type == CORE_BLD_HOSPITAL) && b != job &&
                    b->state == CORE_BSTATE_ACTIVE && expansion_qualified_present(w, b) &&
                    patient_limit(w, b) > b->visits_today) {
                    clinic = b;
                    break;
                }
            }
            if (clinic) {
                to_building(w, v, CORE_VS_SEEK_HEALING, clinic, keep);
                if (v->at_target && expansion_bank_have(w, CORE_RES_HERBS) >= 1000) {
                    int staff = expansion_working_count(w, clinic), efficiency = 0;
                    for (int k = 0; k < clinic->worker_count; k++)
                        if (expansion_working_present(w, clinic, clinic->workers[k]))
                            efficiency += expansion_work_percent(w, clinic->workers[k], clinic->type) * 10;
                    if (staff) efficiency /= staff;
                    v->healing_progress_milli += efficiency * 10;
                    int duration = clinic->type == CORE_BLD_HEALER ? 270000 : staff >= 3 ? 67500 : 108000;
                    if(v->healing_progress_milli>duration)v->healing_progress_milli=duration;
                    if (v->healing_progress_milli >= duration && clinic->service_credit_milli >= 1000) {
                        (void)expansion_bank_add(w, CORE_RES_HERBS, -1000, CORE_LEDGER_HEALING);
                        clinic->service_credit_milli -= 1000;
                        clinic->visits_today++;
                        v->healing_progress_milli = 0;
                        v->sick_ticks = 0; v->health = 100; v->flags &= ~CORE_VF_SICK;
                    }
                }
                continue;
            }
        }
        int work_hungry=job && schedule.on_duty && (expansion_spec(job->type)->field==0 || expansion_spec(job->type)->field==9) && expansion_bank_have(w,CORE_RES_FOOD)<1000;
        if (v->meal_day < exp_day(w) && !work_hungry) {
            Building *hub = expansion_hub(w, v, 1, NULL);
            to_building(w, v, CORE_VS_EAT, hub ? hub : keep, keep);
            if (v->at_target && expansion_bank_have(w, CORE_RES_FOOD) >= 1000) {
                (void)expansion_bank_add(w, CORE_RES_FOOD, -1000, CORE_LEDGER_CONSUMPTION);
                v->needs.hunger = 100;
                expansion_mood_event(w,v,0,2,0,1,3);
                v->meal_day = exp_day(w);
                if (hub && hub->type != CORE_BLD_KEEP)
                    w->expansion->hub_visits++;
                else
                    w->expansion->manor_visits++;
                v->hub_revision = -1;
                if (well_near(w, v)) {
                    v->water = 100;
                    expansion_mood_event(w,v,1,1,0,12,3);
                }
            }
            if (v->meal_day < exp_day(w) && (minute < 480 || expansion_bank_have(w, CORE_RES_FOOD) >= 1000))
                continue;
        }
        if (minute < 480 && v->water_building && v->water<80) {
            Building *well = entity_find_building(w, v->water_building);
            if (well) {
                to_building(w, v, CORE_VS_IDLE, well, keep);
                v->inside_building = 0;
                if (v->at_target) {
                    v->water = 100;
                    expansion_mood_event(w,v,1,1,0,12,3);
                    if (v->hygiene < 65)
                        v->hygiene = 65;
                }
                continue;
            }
        }
        if (job && job->state == CORE_BSTATE_ACTIVE &&
            schedule.on_duty) {
            v->leisure_building=0;v->leisure_elapsed=0;
            if(expansion_duty(w,v,job)) {
                expansion_gain_xp(v,expansion_spec(job->type)->field,job->type,expansion_begin_work(w,v,job));continue;
            }
            int input = 0;
            for (j = 0; j < CORE_RES_COUNT; j++)
                input |= expansion_spec(job->type)->input[j] > 0;
            if (input && v->hub_day != exp_day(w)) {
                Building *hub = expansion_hub(w, v, 0, job);
                if (hub) {
                    to_building(w, v, CORE_VS_GO_WORK, hub, keep);
                    if (!v->at_target)
                        continue;
                    v->hub_day = exp_day(w);
                    v->hub_revision = -1;
                    if (hub->type == CORE_BLD_KEEP)
                        w->expansion->manor_visits++;
                    else
                        w->expansion->hub_visits++;
                }
            }
            if (expansion_military(job->type)) {
                to_building(w, v, CORE_VS_GUARD_POST, job, keep);
                if (v->at_target) {
                    expansion_gain_xp(v, 7, job->type, expansion_begin_work(w, v, job));
                }
                continue;
            }
            to_building(w, v, v->at_target && v->target == job ? CORE_VS_WORKING : CORE_VS_GO_WORK, job,
                        keep);
            if (v->at_target) {
                v->state = CORE_VS_WORKING;
                if (expansion_qualified(w, job))
                    expansion_gain_xp(v, expansion_spec(job->type)->field, job->type, expansion_begin_work(w, v, job));
            }
            continue;
        }
        if(v->mission!=CORE_MISSION_WALK)v->mission=CORE_MISSION_NONE;
        if(expansion_leisure(w,v))continue;
        v->mission=CORE_MISSION_NONE;
        to_building(w, v, CORE_VS_IDLE, v->home ? v->home : expansion_hub(w, v, 0, NULL), keep);
    }
}
void expansion_arrival_join(World *w, Villager *v) {
    ExpansionState *e = w->expansion;
    if (!v->incoming)
        return;
    v->incoming = 0;
    events_push_villager(&w->events, CORE_EVT_IMMIGRANT, v->id, 0, 0, v->name);
    if (!v->contract_end_tick && e->pending_arrivals > 0 && --e->pending_arrivals == 0) {
        e->last_arrival_day = exp_day(w);
        e->immigration_day = e->last_arrival_day + rng_range(&w->rng, 2, 3);
        e->arrived_today = e->pending_total;
        e->arrival_type = e->pending_type;
        if (e->pending_type == 1 && e->starter_waves == 2)
            e->refill_day = e->last_arrival_day + 3;
    }
}
static int immigrate(World *w, int count, int local) {
    Villager *created[8] = {0};
    int x = 0, y = 0, i;
    int edge=rng_range(&w->rng,0,3);
    if(edge==w->expansion->last_immigration_edge && rng_range(&w->rng,0,1))edge=(edge+rng_range(&w->rng,1,3))%4;
    int edge_x=edge==1?w->grid.width-1:edge==3?0:rng_range(&w->rng,0,w->grid.width-1);
    int edge_y=edge==0?0:edge==2?w->grid.height-1:rng_range(&w->rng,0,w->grid.height-1);
    if (local || !expansion_edge(w, edge_x, edge_y, &x, &y)) {
        Building *keep = world_keep(w);
        x = keep->door_x;
        y = keep->door_y;
    }
    if(!local)w->expansion->last_immigration_edge=edge;
    for (i = 0; i < count; i++) {
        created[i] = population_create(w, x, y);
        if (!created[i]) {
            while (i > 0)
                entity_remove_villager(w, created[--i]);
            w->expansion->error = CORE_ERR_OUT_OF_MEMORY;
            return 0;
        }
    }
    for (i = 0; i < count; i++) {
        Villager *v = created[i];
        Building *keep = world_keep(w);
        v->incoming = !local;
        v->intent_flags |= CORE_INTENT_SPAWN_EDGE;
        v->at_target = 0;
        v->target_x = keep->door_x;
        v->target_y = keep->door_y;
        if (local)
            events_push_villager(&w->events, CORE_EVT_IMMIGRANT, v->id, 0, 0, v->name);
    }
    if (!local) {
        w->expansion->pending_arrivals = w->expansion->pending_total = count;
        w->expansion->pending_type = w->rank ? 3 : 1;
    }
    expansion_assign(w);
    w->expansion->dirty = 1;
    return 1;
}
void expansion_people_daily(World *w) {
    ExpansionState *e = w->expansion;
    int i, pop = expansion_permanent(w), free_slots = 0, day = exp_day(w), water = 0;
    e->trade_used = 0;
    e->trade_capacity_milli %= 1000;
    e->last_travel_minutes = e->travel_minutes;
    e->travel_minutes = 0;
    e->last_farm_minutes = e->farm_minutes;
    e->farm_minutes = 0;
    e->report_arrivals = e->arrived_today;
    e->report_arrival_type = e->arrival_type;
    e->report_departures = e->departures_today;
    e->report_floor8 = e->blocked_floor8;
    e->report_deaths = w->stats.villagers_died - e->death_mark;
    e->death_mark = w->stats.villagers_died;
    e->arrived_today = e->arrival_type = e->departures_today = e->blocked_floor8 = 0;
    if (w->mourning > 0)
        w->mourning--;
    for (i = 0; i < w->building_count; i++) {
        Building *b = w->buildings[i];
        b->visits_today = 0;
        b->service_credit_milli %= 1000;
        if (b->state == CORE_BSTATE_ACTIVE)
            free_slots += w->defs.buildings[b->type].resident_slots - b->resident_count;
    }
    for (i = w->villager_count - 1; i >= 0; i--) {
        Villager *v = w->villagers[i];
        v->hygiene = exp_clamp(v->hygiene - 5, 0, 100);
        v->water = exp_clamp(v->water - 20, 0, 100);
        if (!v->contract_end_tick) {
            int wage = v->workplace ? w->defs.buildings[v->workplace->type].wage : 0;
            water += well_near(w, v);
            if (v->home)
                (void)expansion_bank_add(w, CORE_RES_GOLD, e->tax * 1000, CORE_LEDGER_TAXES);
            if (wage && w->bank.amount[CORE_RES_GOLD] >= wage) {
                (void)expansion_bank_add(w, CORE_RES_GOLD, -wage * 1000, CORE_LEDGER_WAGES);
                v->flags &= ~CORE_VF_UNPAID;
            } else if (wage)
                v->flags |= CORE_VF_UNPAID;
            else
                v->flags &= ~CORE_VF_UNPAID;
        }
        expansion_wellbeing_daily(w, v);
        if (v->needs.hunger == 0)
            v->health -= 12;
        if (v->health <= 0) {
            population_kill(w, v, CORE_DEATH_STARVATION);
            continue;
        }
        if (v->happiness < 15)
            v->low_mood_ticks += 144;
        else
            v->low_mood_ticks = 0;
        if (v->low_mood_ticks >= 5 * 144 && !v->contract_end_tick && v->state != CORE_VS_LEAVING &&
            expansion_departure_allowed(w, v, 1)) {
            int x = 0, y = w->keep_y;
            if (!expansion_edge(w, v->current_x_milli / 1000, v->current_y_milli / 1000, &x, &y))
                continue;
            entity_unlink_worker(v);
            entity_unlink_resident(v);
            v->inside_building = 0;
            vil_set_intent(v, CORE_VS_LEAVING, NULL, x, y, 0);
        }
    }
    e->water_served = water;
    expansion_assign(w);
    free_slots = expansion_available_housing(w);
    (void)free_slots;
    if (e->pending_arrivals)
        return;
    if (w->rank == 0) {
        CoreImmigrationView status;
        core_expansion_immigration(w, &status);
        if (e->starter_waves < 2 && day >= status.next_day && immigrate(w, 2, 0)) {
            e->starter_waves++;
        } else if (e->starter_waves == 2 && day >= e->refill_day) {
            if (pop < 8 && immigrate(w, 1, 1)) {
                e->last_arrival_day = day;
                e->arrived_today = 1;
                e->arrival_type = 2;
            }
            e->refill_day = day + 3;
        }
        return;
    }
    {
        CoreImmigrationView status;
        core_expansion_immigration(w, &status);
        if (status.reasons || status.maximum <= 0)
            return;
        int minimum=(int[]){0,2,3,3,3,4}[w->rank], maximum=(int[]){0,4,4,5,6,6}[w->rank];
        if(maximum>status.vacant)maximum=status.vacant;
        if(minimum>maximum)minimum=maximum;
        int count = rng_range(&w->rng, minimum, maximum);
        if(exp_season(w)==CORE_SEASON_WINTER)count=(count+1)/2;
        if (count > status.vacant) count = status.vacant;
        if (immigrate(w, count, 0)) {
            e->pending_type = 3;
        }
    }
}
