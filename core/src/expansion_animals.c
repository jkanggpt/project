/* expansion_animals.c — 실제 야생 개체와 먹이·번식·수용량을 갖는 가축 */
#include "expansion.h"
#include "population.h"
#include <string.h>

static const int MATURE[4] = {24, 12, 16, 8}, PERIOD[4] = {24, 12, 16, 8}, BABIES[4] = {1, 2, 1, 2};
static const int KINDS[4] = {7, 9, 8, 10}, FEED[4] = {600, 700, 350, 150}, OUTPUT[4] = {1000, 0, 450, 350},
                 MEAT[4] = {30, 18, 12, 2}, LEATHER[4] = {6, 3, 3, 0};
static int livestock(int kind) {
    int i;
    for (i = 0; i < 4; i++)
        if (KINDS[i] == kind)
            return i;
    return -1;
}
static CoreAnimalView *new_animal(World *w, int kind, int owner, int age) {
    ExpansionState *e = w->expansion;
    CoreAnimalView *a = NULL;
    int i;
    for (i = 0; i < e->animal_count; i++)
        if (!e->animals[i].alive) {
            a = &e->animals[i];
            break;
        }
    if (!a) {
        if (e->animal_count >= CORE_EXP_MAX_ANIMALS)
            return NULL;
        a = &e->animals[e->animal_count++];
    }
    memset(a, 0, sizeof(*a));
    a->id = e->next_animal_id++;
    a->kind = kind;
    a->owner = owner;
    a->age = age;
    a->alive = 1;
    a->hp = EXP_ANIMALS[kind].hp;
    a->breed_day = exp_day(w) + (livestock(kind) >= 0 ? PERIOD[livestock(kind)] : 24);
    return a;
}
static int occupancy(const ExpansionState *e, int owner) {
    int i, count = 0;
    for (i = 0; i < e->animal_count; i++)
        if (e->animals[i].alive && e->animals[i].owner == owner)
            count++;
    return count;
}
static void slaughter(World *w, CoreAnimalView *a) {
    int k = livestock(a->kind);
    if (k < 0)
        return;
    a->alive = 0;
    (void)expansion_bank_add(w, CORE_RES_FOOD, MEAT[k] * 1000, CORE_LEDGER_PRODUCTION);
    (void)expansion_bank_add(w, CORE_RES_LEATHER, LEATHER[k] * 1000, CORE_LEDGER_PRODUCTION);
}
void expansion_animal_daily(World *w) {
    ExpansionState *e = w->expansion;
    int i, j, initial = e->animal_count;
    uint16_t fed[CORE_EXP_MAX_ANIMALS] = {0};
    for (i = 0; i < initial; i++) {
        CoreAnimalView *a = &e->animals[i];
        Building *owner;
        int k, feed, paid, grass = 0, x, y;
        if (!a->alive)
            continue;
        a->age++;
        if (!a->owner)
            continue;
        owner = entity_find_building(w, a->owner);
        if (!owner || owner->state != CORE_BSTATE_ACTIVE) {
            a->alive = 0;
            continue;
        }
        k = livestock(a->kind);
        if (k < 0)
            continue;
        feed = FEED[k];
        if (a->age < MATURE[k])
            feed /= 2;
        if (owner->type == CORE_BLD_PASTURE && (k == 0 || k == 2) && exp_season(w) != 3) {
            for (y = owner->y - 3; y < owner->y + owner->h + 3; y++)
                for (x = owner->x - 3; x < owner->x + owner->w + 3; x++)
                    if (grid_in_bounds(&w->grid, x, y) &&
                        w->expansion->tiles[grid_index(&w->grid, x, y)].biome == 0)
                        grass++;
            if (grass >= 12)
                feed /= 2;
        }
        paid = expansion_bank_have(w, CORE_RES_FOOD);
        if (paid > feed)
            paid = feed;
        if (paid < feed) {
            if (++a->starve_days >= 2) {
                a->hp -= 5;
                if (a->hp <= 0 || rng_range(&w->rng, 0, 99) < 25) {
                    a->hp = 0;
                    a->alive = 0;
                }
            }
        } else
            a->starve_days = 0;
        (void)expansion_bank_add(w, CORE_RES_FOOD, -paid, CORE_LEDGER_CONSUMPTION);
        fed[i] = (uint16_t)(paid * 1000 / feed);
    }
    /* 모든 사료를 먼저 지불한 뒤 산출·도축·번식을 처리합니다. */
    for (i = 0; i < initial; i++) {
        CoreAnimalView *a = &e->animals[i];
        Building *owner;
        int k;
        if (!fed[i] || !a->alive)
            continue;
        owner = entity_find_building(w, a->owner);
        k = livestock(a->kind);
        if (!owner || k < 0)
            continue;
        if (owner->worker_count && a->age >= MATURE[k]) {
            int factor = (int)((int64_t)owner->daily_work_milli * 1000 /
                               (w->defs.buildings[owner->type].worker_slots * 540000));
            factor = factor * fed[i] / 1000;
            (void)expansion_bank_add(w, CORE_RES_FOOD, OUTPUT[k] * factor / 1000, CORE_LEDGER_PRODUCTION);
            if (fed[i] == 1000 && exp_day(w) >= a->breed_day) {
                int adults = 0, cap = owner->type == CORE_BLD_PASTURE ? 16 : 6, needed = BABIES[k],
                    can = cap - occupancy(e, owner->id);
                for (j = 0; j < initial; j++)
                    if (e->animals[j].alive && e->animals[j].owner == owner->id &&
                        e->animals[j].kind == a->kind && e->animals[j].age >= MATURE[k])
                        adults++;
                if (adults < 2) {
                    a->breed_day = exp_day(w) + 1;
                    continue;
                }
                /* 어미 둘을 남길 수 있는 성체만 도축하고, 자리를 못 만들면 번식을 미룹니다. */
                if (can < needed && adults > 2)
                    for (j = initial - 1; j >= 0 && can < needed && adults > 2; j--) {
                        CoreAnimalView *other = &e->animals[j];
                        if (other != a && other->alive && other->owner == owner->id &&
                            other->kind == a->kind && other->age >= MATURE[k]) {
                            slaughter(w, other);
                            adults--;
                            can++;
                        }
                    }
                if (can >= needed) {
                    for (j = 0; j < needed; j++) {
                        CoreAnimalView *baby = new_animal(w, a->kind, owner->id, 0);
                        if (!baby)
                            break;
                        baby->x_milli = (owner->x + owner->w / 2) * 1000 + 500;
                        baby->y_milli = (owner->y + owner->h / 2) * 1000 + 500;
                    }
                    for (j = 0; j < initial; j++)
                        if (e->animals[j].alive && e->animals[j].owner == owner->id &&
                            e->animals[j].kind == a->kind && e->animals[j].age >= MATURE[k])
                            e->animals[j].breed_day = exp_day(w) + PERIOD[k];
                }
            }
        }
    }
}
CORE_API int32_t core_expansion_damage_animal(CoreWorld *w, int32_t animal_id, int32_t damage) {
    int i;
    if (!w)
        return CORE_ERR_NULL_ARG;
    if (!w->expansion || damage < 1 || damage > 10000)
        return CORE_ERR_INVALID_ARG;
    for (i = 0; i < w->expansion->animal_count; i++) {
        CoreAnimalView *a = &w->expansion->animals[i];
        if (a->id != animal_id || !a->alive)
            continue;
        a->hp = exp_clamp(a->hp - damage, 0, EXP_ANIMALS[a->kind].hp);
        if (EXP_ANIMALS[a->kind].behavior == 1 && EXP_ANIMALS[a->kind].attack) {
            int best = INT32_MAX;
            for (int j = 0; j < w->villager_count; j++) {
                Villager *v = w->villagers[j];
                if (v->inside_building || v->incoming || v->state == CORE_VS_LEAVING)
                    continue;
                int d = exp_dist(a->x_milli / 1000, a->y_milli / 1000, v->current_x_milli / 1000,
                                 v->current_y_milli / 1000);
                if (d < best) {
                    best = d;
                    a->retaliate_id = v->id;
                }
            }
        }
        if (!a->hp) {
            a->alive = 0;
            a->attack_remaining_milli = 0;
            if (!a->owner) {
                (void)expansion_bank_add(w, CORE_RES_FOOD, EXP_ANIMALS[a->kind].food * 1000,
                                         CORE_LEDGER_PRODUCTION);
                (void)expansion_bank_add(w, CORE_RES_LEATHER, EXP_ANIMALS[a->kind].leather * 1000,
                                         CORE_LEDGER_PRODUCTION);
                if (a->region >= 0 && a->region < 30)
                    w->expansion->region_hunt_days[a->region][exp_day(w) % 12]++;
            }
            expansion_ecology_recount(w);
        }
        return CORE_OK;
    }
    return CORE_ERR_NOT_FOUND;
}
static void animal_impact(World *w, CoreAnimalView *a) {
    a->attack_remaining_milli = 0;
    Villager *v = entity_find_villager(w, a->target_id);
    if (!a->alive || !v || v->inside_building || v->incoming || v->state == CORE_VS_LEAVING ||
        exp_dist(a->x_milli / 1000, a->y_milli / 1000, v->current_x_milli / 1000, v->current_y_milli / 1000) >
            1)
        return;
    int damage = EXP_ANIMALS[a->kind].attack;
    v->health -= damage;
    expansion_hurt(w, v, damage, 0);
    v->needs.safety = exp_clamp(v->needs.safety - 10, 0, 100);
    if (v->health <= 0)
        population_kill(w, v, CORE_DEATH_WILDLIFE);
}
CORE_API int32_t core_expansion_advance_animal_attacks(CoreWorld *w, int32_t elapsed_milli) {
    if (!w)
        return CORE_ERR_NULL_ARG;
    if (!w->expansion || elapsed_milli < 0 || elapsed_milli > 10000)
        return CORE_ERR_INVALID_ARG;
    for (int i = 0; i < w->expansion->animal_count; i++) {
        CoreAnimalView *a = &w->expansion->animals[i];
        if (a->attack_remaining_milli <= 0)
            continue;
        a->attack_remaining_milli -= elapsed_milli;
        if (a->attack_remaining_milli <= 0)
            animal_impact(w, a);
    }
    return CORE_OK;
}
void expansion_animals_tick(World *w) {
    ExpansionState *e = w->expansion;
    for (int i = 0; i < e->animal_count; i++) {
        CoreAnimalView *a = &e->animals[i];
        const CoreAnimalInfo *info = &EXP_ANIMALS[a->kind];
        int x = a->x_milli / 1000, y = a->y_milli / 1000, nearest = -1, distance = 13;
        int active = 0, aggressive = info->behavior == 2 || a->retaliate_id;
        if (!a->alive || a->owner)
            continue;
        /* 순수 코어 호출자는 세부 시간을 보고하지 않으므로 다음 틱에서 미처리 타격을 마칩니다. */
        if (a->attack_remaining_milli > 0 && a->attack_tick < w->clock.tick)
            animal_impact(w, a);
        for (int j = 0; j < w->villager_count; j++) {
            Villager *v = w->villagers[j];
            int d = exp_dist(x, y, v->current_x_milli / 1000, v->current_y_milli / 1000);
            active |= d <= 12;
            if (v->inside_building || v->incoming || v->state == CORE_VS_LEAVING)
                continue;
            if (a->retaliate_id && v->id != a->retaliate_id)
                continue;
            if (d < distance) {
                distance = d;
                nearest = j;
            }
        }
        if (!active && w->clock.tick % 144)
            continue;
        int p = grid_index(&w->grid, x, y), water = e->tiles[p].water_id && !w->grid.road[p];
        int cost = w->grid.cost[p] == CORE_COST_GATE ? 67 : w->grid.cost[p];
        int speed = info->speed_milli;
        if (water)
            speed = speed * info->swim_milli / 1000;
        else
            speed = speed * 100 / (cost && cost < 253 ? cost : 100);
        a->move_acc += speed;
        if (a->move_acc < 3000)
            continue;
        a->move_acc -= 3000;
        int dx = rng_range(&w->rng, -1, 1), dy = rng_range(&w->rng, -1, 1);
        if (nearest >= 0 && distance <= (aggressive ? 6 : 3)) {
            Villager *v = w->villagers[nearest];
            int vx = v->current_x_milli / 1000, vy = v->current_y_milli / 1000, sign = aggressive ? 1 : -1;
            dx = (vx > x ? 1 : vx < x ? -1 : 0) * sign;
            dy = (vy > y ? 1 : vy < y ? -1 : 0) * sign;
        }
        /* 동물도 네 방향 인접 칸만 이동해 모서리 벽을 통과하지 않습니다. */
        if (dx && dy) {
            if ((a->id + w->clock.tick) & 1)
                dx = 0;
            else
                dy = 0;
        }
        int xx = x + dx, yy = y + dy;
        if (grid_in_bounds(&w->grid, xx, yy)) {
            int n = grid_index(&w->grid, xx, yy), region = e->region_map[n];
            CoreHabitatView *r = &e->regions[region];
            int swimming = w->grid.terrain[n] == CORE_TERRAIN_WATER && !w->grid.road[n];
            int capacity =
                region == a->region || (r->count < 7 && r->biomass + info->biomass <= r->capacity &&
                                        (info->behavior != 2 || r->aggressive < 2));
            if (!w->grid.block[n] && (w->grid.cost[n] || (swimming && info->swim_milli)) && capacity &&
                !(info->behavior == 2 && exp_day(w) <= 10 && exp_dist(xx, yy, w->keep_x, w->keep_y) < 12)) {
                if (region != a->region) {
                    CoreHabitatView *old = &e->regions[a->region];
                    old->count--;
                    old->biomass -= info->biomass;
                    old->aggressive -= info->behavior == 2;
                    r->count++;
                    r->biomass += info->biomass;
                    r->aggressive += info->behavior == 2;
                    a->region = region;
                }
                x = xx;
                y = yy;
            }
        }
        a->x_milli = x * 1000 + 500;
        a->y_milli = y * 1000 + 500;
        if (aggressive && nearest >= 0 && info->attack &&
            (exp_day(w) > 10 || exp_dist(x, y, w->keep_x, w->keep_y) >= 12)) {
            Villager *v = w->villagers[nearest];
            int d = exp_dist(x, y, v->current_x_milli / 1000, v->current_y_milli / 1000);
            if (d <= 3 && (!v->workplace || v->workplace->type != CORE_BLD_BARRACKS))
                v->hold_ticks = 12;
            if (d <= 1) {
                a->attack_tick = w->clock.tick;
                a->target_id = v->id;
                a->attack_remaining_milli = 2400;
            }
        }
    }
}
CORE_API int32_t core_expansion_copy_animals(const CoreWorld *w, CoreAnimalView *out, int32_t capacity,
                                             int32_t *count) {
    int i, n = 0;
    if (!w || !count)
        return CORE_ERR_NULL_ARG;
    if (!w->expansion)
        return CORE_ERR_INVALID_ARG;
    for (i = 0; i < w->expansion->animal_count; i++)
        if (w->expansion->animals[i].alive)
            n++;
    *count = n;
    if (capacity < n)
        return CORE_ERR_BUFFER_TOO_SMALL;
    if (n && !out)
        return CORE_ERR_NULL_ARG;
    n = 0;
    for (i = 0; i < w->expansion->animal_count; i++)
        if (w->expansion->animals[i].alive)
            out[n++] = w->expansion->animals[i];
    return CORE_OK;
}
CORE_API int32_t core_expansion_buy_animal(CoreWorld *w, int32_t id, int32_t kind) {
    static const int price[4] = {35, 14, 18, 5};
    Building *b;
    CoreAnimalView *a;
    int k = livestock(kind);
    if (!w)
        return CORE_ERR_NULL_ARG;
    if (!w->expansion || k < 0)
        return CORE_ERR_INVALID_ARG;
    b = entity_find_building(w, id);
    if (!b)
        return CORE_ERR_NOT_FOUND;
    if ((b->type != CORE_BLD_RANCH && b->type != CORE_BLD_PASTURE) || b->state != CORE_BSTATE_ACTIVE ||
        occupancy(w->expansion, id) >= (b->type == CORE_BLD_RANCH ? 6 : 16) ||
        w->bank.amount[CORE_RES_GOLD] < price[k])
        return CORE_ERR_REJECTED;
    a = new_animal(w, kind, id, MATURE[k]);
    if (!a)
        return CORE_ERR_REJECTED;
    (void)expansion_bank_add(w, CORE_RES_GOLD, -price[k] * 1000, CORE_LEDGER_TRADE);
    a->x_milli = (b->x + b->w / 2) * 1000 + 500;
    a->y_milli = (b->y + b->h / 2) * 1000 + 500;
    return CORE_OK;
}
