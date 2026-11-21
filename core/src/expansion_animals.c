/* expansion_animals.c — 실제 야생 개체와 먹이·번식·수용량을 갖는 가축 */
#include "expansion.h"
#include "population.h"
#include "placement.h"
#include <string.h>

static const int MATURE[4] = {24, 12, 16, 8}, PERIOD[4] = {24, 12, 16, 8}, BABIES[4] = {1, 2, 1, 2};
static const int LIFE[4] = {72, 30, 48, 28};
static const int KINDS[4] = {7, 9, 8, 10}, FEED[4] = {600, 450, 350, 150}, OUTPUT[4] = {1000, 0, 450, 350},
                 MEAT[4] = {30, 18, 12, 2}, LEATHER[4] = {6, 3, 3, 0};
static int livestock(int kind) {
    int i;
    for (i = 0; i < 4; i++)
        if (KINDS[i] == kind)
            return i;
    return -1;
}
CORE_API int32_t core_expansion_hit_animal(CoreWorld *w,int32_t id,int32_t damage,int32_t source_kind,int32_t source_id) {
    if(!w)return CORE_ERR_NULL_ARG;
    if(!w->expansion||source_kind<1||source_kind>2)return CORE_ERR_INVALID_ARG;
    if(source_kind==1?!entity_find_building(w,source_id):!entity_find_villager(w,source_id))return CORE_ERR_NOT_FOUND;
    for(int i=0;i<w->expansion->animal_count;i++) {
        CoreAnimalView *a=&w->expansion->animals[i];
        if(a->id!=id||!a->alive)continue;
        if(a->owner)return CORE_ERR_REJECTED;
        if(damage>0) {
            a->attacker_kind=source_kind;a->attacker_id=source_id;a->retaliation_until=w->clock.tick+144;
            a->retaliate_id=source_kind==2?source_id:0;a->threat_until_tick=w->clock.tick+144;
        }
        return core_expansion_damage_animal(w,id,damage);
    }
    return CORE_ERR_NOT_FOUND;
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
    (void)expansion_bank_add(w, CORE_RES_FOOD, MEAT[k] * 1000 * (1000 + exp_clamp((a->age - MATURE[k]) * 1000 / (LIFE[k] - MATURE[k]), 0, 1000)) / 1000, CORE_LEDGER_PRODUCTION);
    (void)expansion_bank_add(w, CORE_RES_LEATHER, LEATHER[k] * 1000, CORE_LEDGER_PRODUCTION);
}
void expansion_animal_daily(World *w) {
    ExpansionState *e = w->expansion;
    int initial = e->animal_count;
    uint16_t fed[CORE_EXP_MAX_ANIMALS] = {0};
    for (int j = 0; j < w->building_count; j++) w->buildings[j]->feed_milli = 0;
    /* 사료는 전날 잔고로 지불하고, 그 다음 나이를 증가시킵니다. */
    for (int i = 0; i < initial; i++) {
        CoreAnimalView *a = &e->animals[i];
        if (!a->alive) continue;
        if (!a->owner) { a->age++; continue; }
        Building *b = entity_find_building(w, a->owner);
        int k = livestock(a->kind);
        if (!b || b->state != CORE_BSTATE_ACTIVE || k < 0) { a->alive = 0; continue; }
        int feed = FEED[k] / (a->age < MATURE[k] ? 2 : 1), grass = 0;
        if (b->type == CORE_BLD_PASTURE && (k == 0 || k == 2) && exp_season(w) != 3) {
            for (int y = b->y - 3; y < b->y + b->h + 3; y++)
                for (int x = b->x - 3; x < b->x + b->w + 3; x++)
                    if (grid_in_bounds(&w->grid, x, y) && expansion_center_distance(b,x,y) <= 3 &&
                        e->tiles[grid_index(&w->grid,x,y)].biome == CORE_BIOME_GRASS) grass++;
            if (grass >= 12) feed /= 2;
        }
        int paid = exp_clamp(expansion_bank_have(w, CORE_RES_FOOD), 0, feed);
        (void)expansion_bank_add(w, CORE_RES_FOOD, -paid, CORE_LEDGER_CONSUMPTION);
        b->feed_milli += paid;
        fed[i] = (uint16_t)(paid * 1000 / feed);
        a->age++;
        if (paid < feed) {
            if (++a->starve_days >= 2) a->hp -= 5;
            if (a->hp <= 0) { a->hp = 0; a->alive = 0; }
        } else a->starve_days = 0;
    }
    for (int i = 0; i < initial; i++) {
        CoreAnimalView *a = &e->animals[i];
        if (!a->alive || !a->owner) continue;
        Building *b = entity_find_building(w, a->owner);
        int k = livestock(a->kind), factor = expansion_labor_milli(w,b);
        if (k < 0) continue;
        if (a->age >= MATURE[k]) {
            int age = exp_clamp((a->age - MATURE[k]) * 1000 / (LIFE[k] - MATURE[k]),0,1000);
            int out = (int)((int64_t)OUTPUT[k] * (1000 + age * 3 / 4) * factor * fed[i] / 1000000000LL);
            (void)expansion_bank_add(w, CORE_RES_FOOD, out, CORE_LEDGER_PRODUCTION);
        }
        if (a->age >= LIFE[k]) slaughter(w,a);
    }
    for (int j = 0; j < w->building_count; j++) {
        Building *b = w->buildings[j];
        if (b->state != CORE_BSTATE_ACTIVE || (b->type != CORE_BLD_RANCH && b->type != CORE_BLD_PASTURE)) continue;
        int factor = expansion_labor_milli(w,b), cap = b->type == CORE_BLD_RANCH ? 6 : 16;
        for (int k = 0; k < 4; k++) {
            int adults = 0, full = 1;
            for (int i = 0; i < initial; i++)
                if (e->animals[i].alive && e->animals[i].owner == b->id && e->animals[i].kind == KINDS[k]) {
                    adults += e->animals[i].age >= MATURE[k];
                    full &= fed[i] == 1000;
                }
            if (adults < 2 || !full || !factor) continue;
            b->breeding_progress[k] = exp_clamp(b->breeding_progress[k] + exp_clamp(expansion_sqrt_milli(factor),0,2500),0,PERIOD[k] * 1000);
            if (b->breeding_progress[k] < PERIOD[k] * 1000 || cap - occupancy(e,b->id) < BABIES[k]) continue;
            int free = CORE_EXP_MAX_ANIMALS - e->animal_count;
            for (int i = 0; i < e->animal_count; i++) free += !e->animals[i].alive;
            if (free < BABIES[k]) continue;
            for (int n = 0; n < BABIES[k]; n++) {
                CoreAnimalView *baby = new_animal(w,KINDS[k],b->id,0);
                baby->x_milli = (b->x + b->w / 2) * 1000 + 500;
                baby->y_milli = (b->y + b->h / 2) * 1000 + 500;
            }
            b->breeding_progress[k] -= PERIOD[k] * 1000;
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
        if (!a->hp) {
            a->alive = 0;
            a->attack_remaining_milli = 0;
            if (!a->owner) {
                (void)expansion_bank_add(w, CORE_RES_FOOD, EXP_ANIMALS[a->kind].food * 1000,
                                         CORE_LEDGER_PRODUCTION);
                (void)expansion_bank_add(w, CORE_RES_LEATHER, EXP_ANIMALS[a->kind].leather * 1000,
                                         CORE_LEDGER_PRODUCTION);
                (void)expansion_bank_add(w, CORE_RES_MANA, EXP_ANIMALS[a->kind].mana_drop * 1000, CORE_LEDGER_PRODUCTION);
                if (a->kind >= 24) {
                    static const int lo[6] = {35,24,35,45,50,30}, hi[6] = {55,40,55,65,70,50};
                    int *timers=a->phantom_slot?w->expansion->phantom_second_day:w->expansion->phantom_next_day;
                    timers[a->kind-24]=exp_day(w)+rng_range(&w->rng,lo[a->kind-24],hi[a->kind-24]);
                }
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
    if(a->target_id<0) {
        Building *b=entity_find_building(w,-a->target_id);
        if(!a->alive||!b||b->state!=CORE_BSTATE_ACTIVE||a->attacker_kind!=1||a->attacker_id!=b->id||a->retaliation_until<w->clock.tick)return;
        int x=a->x_milli/1000,y=a->y_milli/1000;
        int distance=(x<b->x?b->x-x:x>=b->x+b->w?x-b->x-b->w+1:0)+(y<b->y?b->y-y:y>=b->y+b->h?y-b->y-b->h+1:0);
        if(distance<=EXP_ANIMALS[a->kind].attack_range) {
            b->damage_kind=2;b->damage_source=a->kind;b->damage_tick=w->clock.tick;
            placement_damage(w,b,EXP_ANIMALS[a->kind].attack);
        }
        return;
    }
    Villager *v = entity_find_villager(w, a->target_id);
    if (!a->alive || !v || v->inside_building || v->incoming || v->state == CORE_VS_LEAVING ||
        exp_dist(a->x_milli / 1000, a->y_milli / 1000, v->current_x_milli / 1000, v->current_y_milli / 1000) >
            EXP_ANIMALS[a->kind].attack_range)
        return;
    a->threat_until_tick = w->clock.tick + 144;
    int damage = EXP_ANIMALS[a->kind].attack;
    if(v->workplace && v->workplace->type==CORE_BLD_BARRACKS)
        damage=(damage*100+exp_clamp(100+8*(v->stats.vitality-4),76,148)/2)/exp_clamp(100+8*(v->stats.vitality-4),76,148);
    v->damage_kind=2;v->damage_source=a->kind;
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
        if(a->retaliation_until<w->clock.tick){a->attacker_kind=0;a->attacker_id=0;a->retaliate_id=0;}
        a->attack_cooldown_milli = exp_clamp(a->attack_cooldown_milli - 10000, 0, 60000);
        /* 순수 코어 호출자는 세부 시간을 보고하지 않으므로 다음 틱에서 미처리 타격을 마칩니다. */
        if (a->attack_remaining_milli > 0 && a->attack_tick < w->clock.tick)
            animal_impact(w, a);
        if(a->attacker_kind==1 && info->attack) {
            Building *b=entity_find_building(w,a->attacker_id);
            if(!b||b->state!=CORE_BSTATE_ACTIVE){a->attacker_kind=0;a->attacker_id=0;continue;}
            int building_distance=(x<b->x?b->x-x:x>=b->x+b->w?x-b->x-b->w+1:0)+(y<b->y?b->y-y:y>=b->y+b->h?y-b->y-b->h+1:0);
            if(building_distance<=info->attack_range && !a->attack_cooldown_milli && !a->attack_remaining_milli) {
                a->target_id=-b->id;a->attack_tick=w->clock.tick;a->attack_remaining_milli=2400;a->attack_cooldown_milli=info->attack_minutes*1000;
            } else if(building_distance>info->attack_range) {
                int xx=x+(b->door_x>x?1:b->door_x<x?-1:0),yy=y;
                if(xx==x)yy+=b->door_y>y?1:b->door_y<y?-1:0;
                if(grid_in_bounds(&w->grid,xx,yy)&&!w->grid.block[grid_index(&w->grid,xx,yy)]&&w->grid.cost[grid_index(&w->grid,xx,yy)]) {
                    a->x_milli=xx*1000+500;a->y_milli=yy*1000+500;
                }
            }
            continue;
        }
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
                region == a->region || (r->count < r->count_cap && r->biomass + info->biomass <= r->capacity &&
                                        (info->behavior != 2 || a->kind >= 24 || r->aggressive < exp_clamp((r->count_cap + 3) / 4, 2, 100)));
            if (!w->grid.block[n] && (w->grid.cost[n] || (swimming && info->swim_milli)) && capacity &&
                !(info->behavior == 2 && exp_day(w) <= 10 && exp_dist(xx, yy, w->keep_x, w->keep_y) < 12)) {
                if (region != a->region) {
                    CoreHabitatView *old = &e->regions[a->region];
                    old->count--;
                    old->biomass -= info->biomass;
                    old->aggressive -= a->kind < 24 && info->behavior == 2;
                    r->count++;
                    r->biomass += info->biomass;
                    r->aggressive += a->kind < 24 && info->behavior == 2;
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
            if (d <= info->attack_range && !a->attack_cooldown_milli && !a->attack_remaining_milli) {
                a->attack_cooldown_milli = info->attack_minutes * 1000;
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
    a = new_animal(w, kind, id, 0);
    if (!a)
        return CORE_ERR_REJECTED;
    (void)expansion_bank_add(w, CORE_RES_GOLD, -price[k] * 1000, CORE_LEDGER_TRADE);
    a->x_milli = (b->x + b->w / 2) * 1000 + 500;
    a->y_milli = (b->y + b->h / 2) * 1000 + 500;
    return CORE_OK;
}
