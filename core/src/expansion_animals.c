/* expansion_animals.c — 실제 야생 개체와 먹이·번식·수용량을 갖는 가축 */
#include "expansion.h"
#include <string.h>

static const int HP[16] = {12, 30, 28, 10, 22, 32, 38, 65, 28, 36, 9, 36, 120, 80, 95, 16};
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
    a->hp = HP[kind];
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
    uint8_t fed[CORE_EXP_MAX_ANIMALS] = {0};
    for (i = 0; i < initial; i++) {
        CoreAnimalView *a = &e->animals[i];
        Building *owner;
        int k, feed, grass = 0, x, y;
        if (!a->alive)
            continue;
        a->age++;
        if (!a->owner)
            continue;
        owner = entity_find_building(w, a->owner);
        if (!owner || owner->state != CORE_BSTATE_ACTIVE) {
            a->owner = 0;
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
        if (expansion_bank_have(w, CORE_RES_FOOD) < feed) {
            if (++a->starve_days >= 2 && rng_range(&w->rng, 0, 99) < 25)
                a->alive = 0;
            continue;
        }
        (void)expansion_bank_add(w, CORE_RES_FOOD, -feed, CORE_LEDGER_CONSUMPTION);
        a->starve_days = 0;
        fed[i] = 1;
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
            (void)expansion_bank_add(w, CORE_RES_FOOD, OUTPUT[k] * factor / 1000, CORE_LEDGER_PRODUCTION);
            if (exp_day(w) >= a->breed_day) {
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
    if (exp_day(w) % 4 == 0 && occupancy(e, 0) < 150) {
        CoreAnimalView *a = new_animal(w, rng_range(&w->rng, 0, 15), 0, 24);
        if (a) {
            a->x_milli = rng_range(&w->rng, 1, w->grid.width - 2) * 1000 + 500;
            a->y_milli = rng_range(&w->rng, 1, w->grid.height - 2) * 1000 + 500;
        }
    }
}
void expansion_animals_tick(World *w) {
    ExpansionState *e = w->expansion;
    int i;
    if (w->clock.tick % 3)
        return;
    for (i = 0; i < e->animal_count; i++) {
        CoreAnimalView *a = &e->animals[i];
        int x, y, dx, dy;
        if (!a->alive)
            continue;
        x = a->x_milli / 1000;
        y = a->y_milli / 1000;
        dx = rng_range(&w->rng, -1, 1);
        dy = rng_range(&w->rng, -1, 1);
        if (a->owner) {
            Building *b = entity_find_building(w, a->owner);
            if (!b)
                continue;
            x = exp_clamp(x + dx, b->x, b->x + b->w - 1);
            y = exp_clamp(y + dy, b->y, b->y + b->h - 1);
        } else {
            int xx = x + dx, yy = y + dy;
            if (grid_in_bounds(&w->grid, xx, yy) && w->grid.cost[grid_index(&w->grid, xx, yy)]) {
                x = xx;
                y = yy;
            }
            if (a->kind >= 11 && (exp_day(w) > 10 || exp_dist(x, y, w->keep_x, w->keep_y) >= 12)) {
                int j;
                for (j = 0; j < w->villager_count; j++) {
                    Villager *v = w->villagers[j];
                    if (!v->inside_building &&
                        exp_dist(x, y, v->current_x_milli / 1000, v->current_y_milli / 1000) <= 1) {
                        int damage = (int[]){5, 15, 9, 12, 4}[a->kind - 11];
                        v->health = exp_clamp(v->health - damage, 1, 100);
                        v->needs.safety = exp_clamp(v->needs.safety - 10, 0, 100);
                        break;
                    }
                }
            }
        }
        a->x_milli = x * 1000 + 500;
        a->y_milli = y * 1000 + 500;
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
