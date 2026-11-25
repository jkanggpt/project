/* expansion_v124.c — 등대·이주·분산 수확과 구저장 이관의 공통 규칙 */
#include "expansion.h"
#include <string.h>

int expansion_is_medical(int type) {
    return type == CORE_BLD_HEALER || type == CORE_BLD_CLINIC || type == CORE_BLD_HOSPITAL;
}
int expansion_is_phantom(int kind) { return kind >= 24 && kind <= 29; }
CORE_API int32_t core_expansion_animal_class(int32_t kind) {
    if (kind < 0 || kind >= CORE_EXP_ANIMALS)
        return CORE_ERR_INVALID_ARG;
    return EXP_ANIMALS[kind].livestock ? 2 : expansion_is_phantom(kind) ? 1 : 0;
}
int expansion_lighthouse_bonus(const World *w, const Building *b) {
    int water = expansion_adjacent_water_id(w, b);
    if (!water)
        return 100;
    for (int i = 0; i < w->building_count; i++) {
        const Building *light = w->buildings[i];
        if (light->type == CORE_BLD_LIGHTHOUSE && light->state == CORE_BSTATE_ACTIVE && light->operating &&
            expansion_centers_distance(light, b) <= 10 && expansion_adjacent_water_id(w, light) == water)
            return 115;
    }
    return 100;
}
int expansion_immigration_wait(World *w) {
    static const int three_day[6] = {0, 35, 40, 45, 50, 55};
    return rng_range(&w->rng, 0, 99) < three_day[exp_clamp(w->rank, 0, 5)] ? 3 : 2;
}
void expansion_immigration_limits(int rank, int vacant, int winter, int *lo, int *hi) {
    static const int minimum[6] = {2, 2, 3, 3, 4, 4}, maximum[6] = {2, 4, 5, 6, 6, 7};
    *hi = maximum[rank] < vacant ? maximum[rank] : vacant;
    *lo = minimum[rank] < *hi ? minimum[rank] : *hi;
    if (winter) {
        *lo = (*lo + 1) / 2;
        *hi = (*hi + 1) / 2;
    }
}
int expansion_immigration_count(World *w, int vacant) {
    int lo, hi;
    expansion_immigration_limits(w->rank, vacant, 0, &lo, &hi);
    if (hi <= 0)
        return 0;
    int count = rng_range(&w->rng, lo, hi);
    return exp_season(w) == 3 ? (count + 1) / 2 : count;
}

/* 미수령 수확은 연도에 관계없이 보존합니다. 두 필드는 수학적 하위/상위 32비트입니다. */
int64_t expansion_harvest_deferred(const Building *b) {
    return (int64_t)(((uint64_t)(uint32_t)b->harvest_deferred_hi << 32) | (uint32_t)b->harvest_deferred_lo);
}
void expansion_harvest_defer(Building *b, int64_t amount) {
    int64_t old=expansion_harvest_deferred(b);
    int64_t value=amount>0 && old>INT64_MAX-amount?INT64_MAX:amount<0 && amount < -old?0:old+amount;
    b->harvest_deferred_lo = (int32_t)(uint32_t)value;
    b->harvest_deferred_hi = (int32_t)((uint64_t)value >> 32);
}
void expansion_harvest_release(World *w) {
    static const int cumulative[6] = {10, 25, 45, 70, 90, 100};
    int day = exp_day(w), year_day = (day - 1) % 48 + 1;
    if (year_day < 25 || year_day > 30 || exp_minute(w) < 480)
        return;
    for (int i = 0; i < w->building_count; i++) {
        Building *b = w->buildings[i];
        if (b->annual_harvest_year != exp_year(w) || b->annual_harvest_day >= day)
            continue;
        int target = (int)((int64_t)b->annual_harvest_total * cumulative[year_day - 25] / 100);
        expansion_harvest_defer(b, target - b->annual_harvest_released);
        b->annual_harvest_released = target;
        b->annual_harvest_day = day;
    }
}
void expansion_v124_migrate(World *w) {
    ExpansionState *e = w->expansion;
    if (w->generation_version >= 12)
        return;
    memset(e->region_hunt_days, 0, sizeof(e->region_hunt_days));
    memset(e->region_pressure_until, 0, sizeof(e->region_pressure_until));
    for (int i = 0; i < e->region_count; i++)
        e->regions[i].pressure_until = 0;
    expansion_fill_definitions(&w->defs, 1);
    memcpy(w->base_buildings, w->defs.buildings, sizeof(w->base_buildings));
    expansion_apply_research(w);
    for (int i = 0; i < w->building_count; i++) {
        Building *b = w->buildings[i];
        if (b->hp > w->defs.buildings[b->type].max_hp)
            b->hp = w->defs.buildings[b->type].max_hp;
        if (b->type == CORE_BLD_BARRACKS)
            while (b->worker_count > 3) {
                Villager *remove = b->workers[0];
                int score = expansion_work_percent(w, remove, b->type);
                for (int k = 1; k < b->worker_count; k++) {
                    Villager *v = b->workers[k];
                    int value = expansion_work_percent(w, v, b->type);
                    if (value < score || (value == score && v->id > remove->id)) {
                        remove = v;
                        score = value;
                    }
                }
                entity_unlink_worker(remove);
            }
        b->forecast_count = b->forecast_day = b->forecast_horizon = 0;
        memset(b->forecast_entries, 0, sizeof(b->forecast_entries));
        memset(b->forecast_meta, 0, sizeof(b->forecast_meta));
    }
    for (int i = 0; i < e->animal_count; i++) {
        CoreAnimalView *a = &e->animals[i];
        if (a->kind >= 0 && a->kind < CORE_EXP_ANIMALS && a->hp > EXP_ANIMALS[a->kind].hp)
            a->hp = EXP_ANIMALS[a->kind].hp;
    }
    for (int i = 0; i < e->boat_count; i++) {
        e->boats[i].previous_x_milli=e->boats[i].x_milli;
        e->boats[i].previous_y_milli=e->boats[i].y_milli;
        e->boats[i].boat_kind = 1;
        e->boats[i].cruise_ticks = 3 + (e->boats[i].slot % 4);
    }
}

CORE_API int32_t core_expansion_medical(const CoreWorld *w,int32_t id,CoreMedicalView *out) {
    if(!w||!out)return CORE_ERR_NULL_ARG;
    const Building *b=entity_find_building(w,id);
    if(!w->expansion||!b||!expansion_is_medical(b->type))return CORE_ERR_INVALID_ARG;
    *out=(CoreMedicalView){0};
    for(int i=0;i<b->worker_count;i++)if(expansion_working_present(w,b,b->workers[i])) {
        int intellect=b->workers[i]->stats.intellect;
        out->workers++;out->intellect_sum+=intellect;if(intellect>out->highest)out->highest=intellect;
    }
    out->qualified=expansion_qualified_present(w,b);
    out->base_patients=out->qualified?expansion_service_base(b,out->workers):0;
    out->herbs=b->type==CORE_BLD_HOSPITAL?3:2;
    out->treated=b->visits_today;out->credit_milli=b->service_credit_milli;
    out->required_workers=b->type==CORE_BLD_HOSPITAL?2:1;
    out->required_highest=b->type==CORE_BLD_HOSPITAL?6:b->type==CORE_BLD_CLINIC?5:4;
    out->required_sum=b->type==CORE_BLD_HOSPITAL?10:b->type==CORE_BLD_CLINIC?7:0;
    for(int i=0;i<w->villager_count;i++)if(w->villagers[i]->target==b && w->villagers[i]->state==CORE_VS_SEEK_HEALING)out->waiting++;
    return CORE_OK;
}
CORE_API int32_t core_expansion_harvest(const CoreWorld *w,int32_t id,CoreHarvestView *out) {
    if(!w||!out)return CORE_ERR_NULL_ARG;
    const Building *b=entity_find_building(w,id);
    if(!w->expansion||!b||!expansion_spec(b->type)->annual_yield)return CORE_ERR_INVALID_ARG;
    *out=(CoreHarvestView){b->annual_harvest_total,b->annual_harvest_released,b->annual_harvest_year,b->annual_harvest_day,expansion_harvest_deferred(b)+b->pending_harvest_milli};
    return CORE_OK;
}
