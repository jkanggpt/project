/* expansion_economy.c — 실제 노동, 공유 자연자원, 연료와 가을 일괄 수확 */
#include "expansion.h"
#include <stdlib.h>
#include <string.h>

static int labor(const World *w, const Building *b) { return expansion_labor_milli(w, b); }
static int logistics_near(const World *w, const Building *b) {
    uint8_t *mark = w->grid.scratch_mark;
    int32_t *queue = w->grid.scratch_queue;
    int i, head = 0, tail = 0, start = grid_index(&w->grid, b->door_x, b->door_y), stores = 0;
    static const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
    for (i = 0; i < w->building_count; i++)
        if (w->buildings[i]->type == CORE_BLD_LOGISTICS && w->buildings[i]->state == CORE_BSTATE_ACTIVE)
            stores++;
    if (!stores)
        return 0;
    memset(mark, 0, (size_t)w->expansion->cells);
    mark[start] = 1;
    queue[tail++] = start;
    while (head < tail) {
        int p = queue[head++], k;
        for (i = 0; i < w->building_count; i++) {
            const Building *store = w->buildings[i];
            if (store->type == CORE_BLD_LOGISTICS && store->state == CORE_BSTATE_ACTIVE &&
                grid_index(&w->grid, store->door_x, store->door_y) == p)
                return 1;
        }
        if (mark[p] >= 16)
            continue;
        for (k = 0; k < 4; k++) {
            int x = p % w->grid.width + dx[k], y = p / w->grid.width + dy[k], n;
            if (!grid_in_bounds(&w->grid, x, y))
                continue;
            n = grid_index(&w->grid, x, y);
            if (!mark[n] && w->expansion->connected[n] &&
                (w->grid.road[n] || w->grid.block[n] == GRID_BLOCK_GATE)) {
                mark[n] = (uint8_t)(mark[p] + 1);
                queue[tail++] = n;
            }
        }
    }
    return 0;
}
static int logistics(const World *w, const Building *b) {
    const ExpansionState *e = w->expansion;
    int p = grid_index(&w->grid, b->door_x, b->door_y), d = e->distance[p], value;
    if (expansion_spec(b->type)->rank == 0 && d < 0)
        return 100;
    if (d < 0)
        return 0;
    value = d <= 20 ? 100 : d <= 50 ? 95 : d <= 80 ? 85 : 70;
    if (expansion_has_research(w, 3))
        value += 15;
    if (logistics_near(w, b))
        value += 20;
    return value > 100 ? 100 : value;
}
static int local_bonus(const World *w, const Building *b) {
    if (expansion_spec(b->type)->field != 0) return 100;
    for (int i = 0; i < w->building_count; i++) {
        const Building *a = w->buildings[i];
        if (a->state == CORE_BSTATE_ACTIVE && a->type == CORE_BLD_WINDMILL &&
            expansion_centers_distance(a, b) <= 10) return 140;
    }
    return 100;
}
static int room(const World *w, int resource) {
    int64_t value = (int64_t)eco_bank_limit(&w->bank, resource) * 1000 - expansion_bank_have(w, resource);
    return value > INT32_MAX ? INT32_MAX : value < 0 ? 0 : (int)value;
}
static void logging(World *w, Building *b, int factor) {
    int budget=(int)((int64_t)expansion_spec(b->type)->output[CORE_RES_LOGS]*factor/1000);
    int radius=expansion_spec(b->type)->radius,count=0;
    int *candidates=w->expansion->queue;
    if(budget>room(w,CORE_RES_LOGS))budget=room(w,CORE_RES_LOGS);
    for(int y=b->y-radius;y<b->y+b->h+radius;y++)for(int x=b->x-radius;x<b->x+b->w+radius;x++) {
        if(!grid_in_bounds(&w->grid,x,y)||expansion_center_distance(b,x,y)>radius)continue;
        int p=grid_index(&w->grid,x,y);CoreExpansionTileView *t=&w->expansion->tiles[p];
        if(t->tree_kind>0&&t->tree_logs_milli>0&&t->tree_age>=core_expansion_tree_mature_age(t->tree_kind))candidates[count++]=p;
    }
    while(budget>0&&count>0) {
        int share=budget/count,remainder=budget%count,active=0;
        int start=b->logging_cursor%count;
        for(int k=0;k<count;k++) {
            int n=(start+k)%count;CoreExpansionTileView *t=&w->expansion->tiles[candidates[n]];
            int take=share+(k<remainder);if(take>t->tree_logs_milli)take=t->tree_logs_milli;
            take=expansion_bank_add(w,CORE_RES_LOGS,take,CORE_LEDGER_PRODUCTION);
            budget-=take;t->tree_logs_milli-=take;t->tree_used_milli+=take;
            if(t->tree_kind==4)w->expansion->magic_logs_milli+=take;
            if(!t->tree_logs_milli) {
                int kind=t->tree_kind;
                t->regrow_milli=rng_range(&w->rng,(int[]){6,9,13,12}[kind-1],(int[]){12,16,24,24}[kind-1])*1000;
                t->tree_age=0;w->grid.terrain[candidates[n]]=CORE_TERRAIN_GRASS;
                if(rng_range(&w->rng,0,99)<(int[]){20,10,25,25}[kind-1])
                    (void)expansion_bank_add(w,kind==4?CORE_RES_HERBS:CORE_RES_FOOD,kind==3?2000:1000,CORE_LEDGER_PRODUCTION);
            }
        }
        for(int k=0;k<count;k++)if(w->expansion->tiles[candidates[k]].tree_logs_milli>0)candidates[active++]=candidates[k];
        count=active;b->logging_cursor++;
    }
}

static int ore_value(int res) {
    return res == CORE_RES_STONE      ? 1
           : res == CORE_RES_COAL     ? 2
           : res == CORE_RES_IRON_ORE ? 3
           : res == CORE_RES_GOLD_ORE ? 8
           : res == CORE_RES_MANA     ? 10
                                      : 0;
}
static int mine_accept(int type, int res) {
    return type == CORE_BLD_QUARRY ? res == CORE_RES_STONE
           : type == CORE_BLD_MINE
               ? (res == CORE_RES_STONE || res == CORE_RES_COAL || res == CORE_RES_IRON_ORE)
               : ore_value(res) > 0;
}
static void mining(World *w, Building *b, int factor) {
    int radius=expansion_spec(b->type)->radius;
    int budget=(int)((int64_t)(b->type==CORE_BLD_QUARRY?8000:b->type==CORE_BLD_MINE?24000:50000)*factor/1000);
    if(b->type!=CORE_BLD_QUARRY&&expansion_event_active(w,CORE_NATURE_MINE_COLLAPSE))budget=budget*60/100;
    int64_t weight=0;
    for(int y=b->y-radius;y<b->y+b->h+radius;y++)for(int x=b->x-radius;x<b->x+b->w+radius;x++) {
        if(!grid_in_bounds(&w->grid,x,y)||expansion_center_distance(b,x,y)>radius)continue;
        CoreExpansionTileView *t=&w->expansion->tiles[grid_index(&w->grid,x,y)];
        weight+=t->stone_milli;
        if(mine_accept(b->type,t->ore_kind))weight+=(int64_t)t->ore_milli*ore_value(t->ore_kind);
    }
    int remaining=budget;if(!weight)return;
    for(int y=b->y-radius;y<b->y+b->h+radius;y++)for(int x=b->x-radius;x<b->x+b->w+radius;x++) {
        if(!grid_in_bounds(&w->grid,x,y)||expansion_center_distance(b,x,y)>radius)continue;
        CoreExpansionTileView *t=&w->expansion->tiles[grid_index(&w->grid,x,y)];
        for(int layer=0;layer<2;layer++) {
            int res=layer?t->ore_kind:CORE_RES_STONE;int *stock=layer?&t->ore_milli:&t->stone_milli;
            int value=ore_value(res);if(!value||!mine_accept(b->type,res)||*stock<=0)continue;
            int take=(int)((int64_t)budget*(*stock)/weight);
            if(take>*stock)take=*stock;
            if(take>remaining/value)take=remaining/value;
            take=expansion_bank_add(w,res,take,CORE_LEDGER_PRODUCTION);*stock-=take;remaining-=take*value;
        }
    }
}

static int fish_request(const World *w, const Building *b, int *water) {
    int *counts = w->grid.scratch_queue, water_count = w->expansion->water_count, x, y, id, chosen = 0,
        best = 0, desire;
    int port = b->type == CORE_BLD_PORT, radius = port ? 1 : 5;
    *water = 0;
    if (!water_count || !b->operating)
        return 0;

    memset(counts, 0, (size_t)water_count * sizeof(*counts));
    for (y = b->y - radius; y < b->y + b->h + radius; y++)
        for (x = b->x - radius; x < b->x + b->w + radius; x++) {
            int dx = x < b->x ? b->x - x : x >= b->x + b->w ? x - (b->x + b->w - 1) : 0;
            int dy = y < b->y ? b->y - y : y >= b->y + b->h ? y - (b->y + b->h - 1) : 0;
            if (!grid_in_bounds(&w->grid, x, y) ||
                (port ? dx + dy != 1 : expansion_center_distance(b, x, y) > 5))
                continue;
            id = w->expansion->tiles[grid_index(&w->grid, x, y)].water_id;
            if (id)
                counts[id - 1]++;
        }
    for (id = 1; id <= water_count; id++)
        if (counts[id - 1]) {
            int score = port ? w->expansion->water_area[id - 1] : counts[id - 1];
            if (score > best) {
                best = score;
                chosen = id;
            }
        }
    if (!chosen)
        return 0;
    *water = chosen;
    desire = port ? exp_clamp(750 + w->expansion->water_area[chosen - 1] * 1000 / 400, 750, 1400)
                  : exp_clamp(best * 1000 / 12, 250, 1000);
    return (int)((int64_t)3000 * b->daily_work_milli * desire / (540000LL * 1000));
}
static void fishing(World *w) {
    int i, j;
    for (i = 0; i < w->building_count; i++) {
        Building *b = w->buildings[i];
        int water, request, total = 0, prefix = 0, cap;
        if (b->state != CORE_BSTATE_ACTIVE || (b->type != CORE_BLD_FISHING_SPOT && b->type != CORE_BLD_PORT))
            continue;
        request = fish_request(w, b, &water);
        if (!request)
            continue;
        for (j = 0; j < w->building_count; j++) {
            Building *other = w->buildings[j];
            int id, n;
            if (other->state != CORE_BSTATE_ACTIVE ||
                (other->type != CORE_BLD_FISHING_SPOT && other->type != CORE_BLD_PORT))
                continue;
            n = fish_request(w, other, &id);
            if (id == water) {
                total += n;
                if(other->id<b->id)prefix+=n;
            }
        }
        cap = exp_clamp(5000 + w->expansion->water_area[water - 1] * 220, 12000, 130000);
        cap = cap * (exp_season(w) == 1 ? 110 : exp_season(w) == 3 ? 75 : 100) / 100;
        cap = cap * (w->expansion->water_cold[water - 1] ? 90 : 110) / 100;
        if (total > cap)
            request = (int)((int64_t)(prefix+request)*cap/total-(int64_t)prefix*cap/total);
        (void)expansion_bank_add(w, CORE_RES_FOOD, request, CORE_LEDGER_PRODUCTION);
    }
}
int expansion_recipe_plan(const World *w,const Building *b,int factor,int attendance,int constrained,int *in,int *out,int *fuel_out) {
    const ExpansionBuildingSpec *s = expansion_spec(b->type);
    int fuel=s->fuel,r,ratio=1000;
    int inputs_fixed = s->research > 0 || expansion_service_shift(b->type);
    int input_factor = inputs_fixed ? attendance : factor;
    for (r = 0; r < CORE_RES_COUNT; r++) {
        in[r] = s->input[r];
        out[r] = s->output[r];
    }
    if (b->type == CORE_BLD_SMELTER && b->mode) {
        in[CORE_RES_IRON_ORE] = 0;
        in[CORE_RES_GOLD_ORE] = 4000;
        out[CORE_RES_IRON] = 0;
        out[CORE_RES_GOLD_BAR] = 2000;
    }
    if (b->type == CORE_BLD_SMELTER && !b->mode && expansion_has_research(w, 5))
        out[CORE_RES_IRON] = 5000;
    if (b->type == CORE_BLD_GARDEN && exp_season(w) == 3)
        out[CORE_RES_FOOD] /= 2;
    if (b->type == CORE_BLD_ORCHARD &&
        (exp_day(w) - b->completed_day < 24 || (exp_season(w) != 1 && exp_season(w) != 2)))
        {memset(in,0,CORE_RES_COUNT*sizeof(int));memset(out,0,CORE_RES_COUNT*sizeof(int));*fuel_out=0;return 0;}
    if (b->type == CORE_BLD_GREENHOUSE && exp_season(w) == 3)
        fuel = 1000;
    if (expansion_event_active(w, CORE_NATURE_COLD_SNAP) && b->type == CORE_BLD_GREENHOUSE) fuel = fuel * 130 / 100;
    if (expansion_has_research(w, 22))
        for (r = 0; r < CORE_RES_COUNT; r++)
            if (r == CORE_RES_MANA)
                in[r] = in[r] * 90 / 100;
    for (r = 0; r < CORE_RES_COUNT; r++) {
        in[r] = (int)((int64_t)in[r] * input_factor / 1000);
        out[r] = (int)((int64_t)out[r] * factor / 1000);
        if (constrained && in[r] > 0 && (int64_t)in[r] * ratio / 1000 > expansion_bank_have(w, r))
            ratio = expansion_bank_have(w, r) * 1000 / in[r];
        if (constrained && out[r] > 0 && (int64_t)out[r] * ratio / 1000 > room(w, r))
            ratio = room(w, r) * 1000 / out[r];
    }
    fuel = (int)((int64_t)fuel * (inputs_fixed ? attendance : factor) / 1000);
    if (constrained && fuel && (int64_t)fuel * ratio / 1000 > expansion_fuel((World *)w, 0, 0))
        ratio = expansion_fuel((World *)w, 0, 0) * 1000 / fuel;
    *fuel_out=fuel;return ratio;
}
static void recipe(World *w,Building *b,int factor) {
    const ExpansionBuildingSpec *s=expansion_spec(b->type);
    int in[CORE_RES_COUNT],out[CORE_RES_COUNT],fuel=0,r;
    int attendance=s->workers?exp_clamp(b->actual_work_milli/(s->workers*540),0,1000):0;
    int ratio=expansion_recipe_plan(w,b,factor,attendance,1,in,out,&fuel);
    if (ratio <= 0) {
        b->operating = 0;
        b->blocked_reason = CORE_DENY_NO_RESOURCES;
        return;
    }
    b->operating = 1;
    for (r = 0; r < CORE_RES_COUNT; r++)
        if (in[r]) {
            int amount = in[r] * ratio / 1000;
            if (r == CORE_RES_LOGS && b->type == CORE_BLD_SAWMILL) {
                int magic = amount < w->expansion->magic_logs_milli ? amount : w->expansion->magic_logs_milli;
                w->expansion->magic_logs_milli -= magic;
                b->processing_milli[CORE_RES_MANA] += magic;
                while (b->processing_milli[CORE_RES_MANA] >= 10000) {
                    b->processing_milli[CORE_RES_MANA] -= 10000;
                    (void)expansion_bank_add(w, CORE_RES_MANA, 1000, CORE_LEDGER_PRODUCTION);
                }
            }
            (void)expansion_bank_add(w, r, -amount, CORE_LEDGER_PRODUCTION);
        }
    if (fuel)
        (void)expansion_fuel(w, fuel * ratio / 1000, 1);
    for (r = 0; r < CORE_RES_COUNT; r++)
        if (out[r])
            (void)expansion_bank_add(w, r, out[r] * ratio / 1000, CORE_LEDGER_PRODUCTION);
    if (s->research && w->research.active >= 0) {
        int research_factor=factor;
        if(b->type==CORE_BLD_LIBRARY || b->type==CORE_BLD_ACADEMY)
            research_factor=s->workers?exp_clamp(b->research_work_milli/(s->workers*540),0,7500):0;
        int points = (int)((int64_t)s->research * research_factor * ratio / 1000000);
        const ResearchDef *research = &w->defs.research[w->research.active];
        if (b->type == CORE_BLD_MAGE_TOWER && expansion_has_research(w, 20))
            points = points * 120 / 100;
        if (b->type == CORE_BLD_GRAND_MAGE_TOWER && expansion_has_research(w, 23))
            points = points * 120 / 100;
        w->research.progress_milli += points;
        if (w->research.progress_milli >= research->points * 1000) {
            int index = w->research.active;
            w->research.done |= 1u << index;
            w->research.done_count++;
            w->research.active = -1;
            w->research.progress_milli = 0;
            expansion_apply_research(w);
            events_push_progress(&w->events, CORE_EVT_RESEARCH_DONE, index, w->research.done_count, 0, NULL);
        }
    }
}
void expansion_produce(World *w) {
    int i;
    for (i = 0; i < w->building_count; i++) {
        Building *b = w->buildings[i];
        const ExpansionBuildingSpec *s = expansion_spec(b->type);
        int64_t amount, baseline;
        if (!s->annual_yield || b->crop_year == exp_year(w) + 1)
            continue;
        baseline = (int64_t)s->workers * 540 * 24 * 1000;
        int64_t work=b->crop_labor_milli;
        if(work>baseline*12)work=baseline*12;
        amount = (int64_t)s->annual_yield * work * local_bonus(w,b) *
            (expansion_has_research(w,0)?112:100) *
            (expansion_has_research(w,4) && b->type!=CORE_BLD_IRRIGATED_FARM?115:100) /
            (baseline*1000000) * 1000;
        b->pending_harvest_milli += (int)amount;
        b->harvest_expiry = exp_day(w) + 4;
        b->crop_labor_milli = 0;
        b->crop_work_carry = 0;
        b->crop_drought = 0;
        b->crop_year = exp_year(w) + 1;
    }
}
void expansion_collect_harvest(World *w) {
    int i;
    for (i = 0; i < w->building_count; i++) {
        Building *b = w->buildings[i];
        if (!b->pending_harvest_milli)
            continue;
        if (exp_day(w) >= b->harvest_expiry) {
            b->pending_harvest_milli = 0;
            continue;
        }
        b->pending_harvest_milli -=
            expansion_bank_add(w, CORE_RES_FOOD, b->pending_harvest_milli, CORE_LEDGER_PRODUCTION);
    }
}
int expansion_production_factor(const World *w,const Building *b) {
    const ExpansionBuildingSpec *s=expansion_spec(b->type);
    if(b->state!=CORE_BSTATE_ACTIVE || !b->operating)return 0;
    int factor=labor(w,b)*logistics(w,b)/100;
    if(s->field==0 && !s->annual_yield && s->workers)
        factor=(int)((int64_t)b->agriculture_work_milli*1000/(s->workers*540000LL));
    if(!s->annual_yield)factor=factor*local_bonus(w,b)/100;
    return factor;
}
void expansion_daily(World *w) {
    int id, i;
    expansion_collect_harvest(w);
    /* 사료는 당일 생산 전에 차감해 같은 날의 축산물로 사료를 복제하지 않습니다. */
    expansion_animal_daily(w);
    for (id = 1; id < w->next_building_id; id++) {
        Building *b = entity_find_building(w, id);
        const ExpansionBuildingSpec *s;
        int factor;
        if (!b || b->state != CORE_BSTATE_ACTIVE)
            continue;
        s = expansion_spec(b->type);
        b->blocked_reason = 0;
        b->operating = b->daily_work_milli>0 || expansion_qualified(w, b);
        if ((b->type == CORE_BLD_HEALER || b->type == CORE_BLD_HOSPITAL) && !b->operating)
            b->blocked_reason = CORE_DENY_REQUIREMENTS;
        if (s->upkeep && !((b->type == CORE_BLD_HEALER || b->type == CORE_BLD_HOSPITAL) && !b->operating)) {
            if (w->bank.amount[CORE_RES_GOLD] >= s->upkeep)
                (void)expansion_bank_add(w, CORE_RES_GOLD, -s->upkeep * 1000, CORE_LEDGER_WAGES);
            else {
                b->blocked_reason = CORE_DENY_NO_GOLD;
                b->operating = 0;
            }
        }
        b->flags &= ~CORE_BF_COLD;
        if (exp_season(w) == 3 && s->heating_milli && (!s->residents || b->resident_count)) {
            int fuel = s->heating_milli * (expansion_has_research(w, 19) ? 80 : 100) / 100;
            if (expansion_event_active(w, CORE_NATURE_COLD_SNAP)) fuel = fuel * 130 / 100;
            if (expansion_fuel(w, 0, 0) < fuel)
                b->flags |= CORE_BF_COLD;
            else
                (void)expansion_fuel(w, fuel, 1);
        }
        factor=expansion_production_factor(w,b);
        if (factor > 0) {
            if (b->type == CORE_BLD_LOGGING_SITE || b->type == CORE_BLD_LUMBER)
                logging(w, b, factor);
            else if (s->field == 2)
                mining(w, b, factor);
            else if (b->type == CORE_BLD_GATHERER) {
                b->hunts_milli += 4 * factor;
                while (b->hunts_milli >= 1000) {
                    int roll = rng_range(&w->rng, 0, 99),
                        biome=w->expansion->tiles[grid_index(&w->grid,b->x,b->y)].biome,
                        food=biome==CORE_BIOME_SWAMP?15:biome==CORE_BIOME_WETLAND?20:55,
                        herbs=biome==CORE_BIOME_SWAMP?30:35+(biome==CORE_BIOME_WETLAND?5:0),
                        res=roll<food?CORE_RES_FOOD:roll<food+herbs?CORE_RES_HERBS:CORE_RES_CLAY;
                    b->hunts_milli -= 1000;
                    (void)expansion_bank_add(w, res, 1000, CORE_LEDGER_PRODUCTION);
                }
            } else if (s->field == 5) {
                /* 수렵 보상은 실제 화살 명중·처치에서만 발생합니다. */
            } else if (b->type != CORE_BLD_FISHING_SPOT && b->type != CORE_BLD_PORT && !s->annual_yield)
                recipe(w, b, factor);
        }
    }
    fishing(w);
    for (i = 0; i < w->building_count; i++) {
        w->buildings[i]->research_work_milli=0;
        w->buildings[i]->daily_work_milli = 0;
        w->buildings[i]->actual_work_milli = 0;
        w->buildings[i]->agriculture_work_milli = 0;
    }
    expansion_collect_harvest(w);
}
void expansion_growth(World *w) {
    static const int logs[4] = {12000,28000,50000,24000};
    static const int weights[10][3] = {{65,20,15},{35,10,55},{10,90,0},{35,55,10},{70,20,10},
                                     {15,85,0},{0,0,0},{0,0,0},{40,25,35},{65,0,35}};
    ExpansionState *e = w->expansion;
    for (int i = 0; i < e->cells; i++) {
        CoreExpansionTileView *t = &e->tiles[i];
        t->tree_used_milli = 0;
        if (!t->tree_kind) continue;
        int power = 0;
        if (t->tree_kind != 4)
            for (int j = 0; j < w->building_count; j++) {
                Building *b = w->buildings[j];
                if (b->type == CORE_BLD_FORESTRY && b->state == CORE_BSTATE_ACTIVE &&
                    expansion_center_distance(b, i % w->grid.width, i / w->grid.width) <= 8) {
                    int p = expansion_sqrt_milli(labor(w, b));
                    if (p > power) power = p;
                }
            }
        if (t->regrow_milli) {
            t->regrow_milli = exp_clamp(t->regrow_milli - 1000 - power * 7 / 10, 0, 60000);
            if (!t->regrow_milli && !w->grid.occupant[i] && !w->grid.road[i]) {
                t->tree_age = 1000; t->tree_logs_milli = logs[t->tree_kind - 1];
                w->grid.terrain[i] = CORE_TERRAIN_FOREST;
            }
        } else if (t->tree_age)
            t->tree_age = exp_clamp(t->tree_age + 1000 + power * 8 / 10, 0, core_expansion_tree_mature_age(t->tree_kind));
    }
    for (int j = 0; j < w->building_count; j++) {
        Building *b = w->buildings[j];
        if (b->type != CORE_BLD_FORESTRY || b->state != CORE_BSTATE_ACTIVE) continue;
        b->forestry_carry_milli += 3 * expansion_sqrt_milli(labor(w, b));
        int count = 0;
        for (int i = 0; i < e->cells; i++) {
            int biome = e->tiles[i].biome;
            if (!e->tiles[i].tree_kind && !w->grid.occupant[i] && !w->grid.road[i] &&
                w->grid.terrain[i] == CORE_TERRAIN_GRASS && weights[biome][0] + weights[biome][1] + weights[biome][2] &&
                expansion_center_distance(b, i % w->grid.width, i / w->grid.width) <= 8)
                e->queue[count++] = i;
        }
        while (count && b->forestry_carry_milli >= 1000) {
            int choice = rng_range(&w->rng, 0, count - 1), p = e->queue[choice];
            e->queue[choice] = e->queue[--count];
            CoreExpansionTileView *t = &e->tiles[p];
            int roll = rng_range(&w->rng, 1, 100), kind = 0;
            while (kind < 2 && (roll -= weights[t->biome][kind]) > 0) kind++;
            t->tree_kind = kind + 1; t->tree_age = 1000; t->tree_logs_milli = logs[kind];
            w->grid.terrain[p] = CORE_TERRAIN_FOREST;
            b->forestry_carry_milli -= 1000;
        }
        b->forestry_carry_milli %= 1000;
    }
    grid_rebuild_costs(&w->grid);
    expansion_terrain_costs(w);
}
