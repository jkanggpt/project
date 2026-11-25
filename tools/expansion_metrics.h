/* expansion_metrics.h — 무창 검사의 일별 상태와 사건 진단 CSV */
#ifndef CASTLE_EXPANSION_METRICS_H
#define CASTLE_EXPANSION_METRICS_H
#include "entity.h"
#include "expansion.h"
#include "platform.h"
#include "storage.h"
#include <stdio.h>
typedef struct MetricContext {
    FILE *events;
    World *world;
    int day, pop, rank, research, no_progress;
    FILE *immigration, *promotion, *habitats, *people, *prices, *hubs;
    FILE *animals, *production, *workers, *raids;
} MetricContext;
static int metric_open_details(MetricContext *ctx, const char *out, int difficulty, int seed) {
    static const char *const names[] = {"immigration", "promotion", "habitats",   "people",  "prices",
                                        "hubs",        "animals",   "production", "workers", "raids"};
    static const char *const headers[] = {
        "day,last_arrival_day,cooldown_remaining,next_check_day,is_eligible,all_rejection_reasons,status,"
        "group_size,arrival_type,permanent_pop_before_arrivals_estimate,permanent_pop_after,special_waves,"
        "refill_day\n",
        "day,target_rank,lacks_population,lacks_happiness,lacks_valid_road,lacks_protected_area,lacks_"
        "protected_pop_pct,lacks_research,lacks_gold,lacks_mana,lacks_gold_bars,lacks_building,lacks_food,"
        "promotion_remaining_ticks,no_progress_days\n",
        "day,habitat_id,biome,target_count,actual_count,biomass_used,biomass_cap,hunted_12_days,predators,"
        "pressure_until,spawn_attempts_global,rejected_capacity_global,rejected_tiles_global,rejected_"
        "species_global\n",
        "day,kind,count,wild,livestock,injured_residents,deaths_total\n",
        "day,resource,buy_unit_cents,sell_unit_cents,base_buy_price,season_bp,event_bp,trade_pressure_bp,"
        "effective_bp,net_volume_today,price_clamped,stock,daily_remaining\n",
        "day,warehouse_visits_total,manor_visits_total,path_cost_saved_estimate,travel_game_seconds,farm_"
        "worker_hours\n",
        "day,id,kind,owner,alive,age,hp,starve_days,x_milli,y_milli,threat_until_tick,attack_cooldown_"
        "milli\n",
        "day,id,type,workers,actual_minutes,effective_minutes,crop_eff_minutes,crop_carry,pending_harvest,"
        "service_credit,feed_milli,breed_cow,breed_pig,breed_sheep,breed_chicken\n",
        "day,id,sex,appearance,job,state,at_target,inside,x_milli,y_milli,target_x,target_y,actual_minutes,"
        "efficiency_percent,health,hunger,mood,happiness,rest,water,safety,mission,shift,duty,leisure_day,"
        "education_day,workplace_serial\n",
        "day,tick,raid_id,base_alive,summoned_alive,summons_total,deferred_summons,killed,escaped,gold_drop,"
        "mana_drop,food_drop,wood_drop,stone_drop,herbs_drop,logs_drop,clay_drop,bricks_drop,iron_ore_drop,"
        "coal_drop,iron_drop,gold_ore_drop,gold_bar_drop,leather_drop\n"};
    FILE **files[] = {&ctx->immigration, &ctx->promotion, &ctx->habitats,   &ctx->people,  &ctx->prices,
                      &ctx->hubs,        &ctx->animals,   &ctx->production, &ctx->workers, &ctx->raids};
    char path[GAME_PATH_BYTES];
    for (int i = 0; i < 10; i++) {
        snprintf(path, sizeof(path), "%s/d%d_seed%d.%s.csv", out, difficulty, seed, names[i]);
        *files[i] = platform_fopen_utf8(path, "wb");
        if (!*files[i])
            return 0;
        fputs(headers[i], *files[i]);
    }
    return 1;
}
static void metric_close_details(MetricContext *ctx) {
    FILE *files[] = {ctx->immigration, ctx->promotion, ctx->habitats,   ctx->people,  ctx->prices,
                     ctx->hubs,        ctx->animals,   ctx->production, ctx->workers, ctx->raids};
    for (int i = 0; i < 10; i++)
        if (files[i])
            fclose(files[i]);
}
static void metric_details(MetricContext *ctx, const CoreSummaryView *s, const CoreImmigrationView *imm,
                           const CoreExpansionRankView *rank) {
    const World *w = ctx->world;
    const ExpansionState *e = w->expansion;
    int injured = 0, facility = 1, target = rank->target;
    if (ctx->immigration)
        fprintf(ctx->immigration, "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n", s->day, imm->last_day,
                exp_clamp(imm->next_day - s->day, 0, 3), imm->next_day, imm->reasons == 0, imm->reasons,
                imm->status, e->report_arrivals, e->report_arrival_type,
                s->permanent_population - e->report_arrivals + e->report_departures, s->permanent_population,
                imm->special_waves, imm->refill_day);
    CoreRankRequirementView conditions[24];
    int nc = core_expansion_rank_requirements(w, conditions, 24);
    for (int c = 0; c < nc; c++)
        if (conditions[c].kind == 2 && !conditions[c].satisfied)
            facility = 0;
    if (ctx->promotion)
        fprintf(ctx->promotion, "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n", s->day, target,
                s->permanent_population < rank->population, s->average_happiness < rank->mood,
                s->valid_roads < rank->roads, s->protected_area < rank->area,
                s->protected_population * 100 < s->permanent_population * rank->protected_pct,
                s->research_done < rank->research, s->resources[CORE_RES_GOLD] < rank->gold,
                s->resources[CORE_RES_MANA] < rank->mana, s->resources[CORE_RES_GOLD_BAR] < rank->gold_bars,
                !facility,
                target == 1 && expansion_bank_have(w, CORE_RES_FOOD) < s->permanent_population * 8000,
                e->promotion_remaining, ctx->no_progress);
    for (int i = 0; i < e->region_count && ctx->habitats; i++) {
        const CoreHabitatView *r = &e->regions[i];
        int hunted = 0;
        for (int j = 0; j < 12; j++)
            hunted += e->region_hunt_days[i][j];
        fprintf(ctx->habitats, "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n", s->day, r->id, r->biome,
                r->target, r->count, r->biomass, r->capacity, hunted, r->aggressive, r->pressure_until,
                e->spawn_attempts, e->spawn_capacity_rejected, e->spawn_tile_rejected,
                e->spawn_species_rejected);
    }
    for (int i = 0; i < w->villager_count; i++)
        injured += w->villagers[i]->health < 100;
    for (int kind = 0; kind < CORE_EXP_ANIMALS && ctx->people; kind++) {
        int wild = 0, livestock = 0;
        for (int i = 0; i < e->animal_count; i++)
            if (e->animals[i].alive && e->animals[i].kind == kind) {
                wild += !e->animals[i].owner;
                livestock += e->animals[i].owner != 0;
            }
        fprintf(ctx->people, "%d,%d,%d,%d,%d,%d,%d\n", s->day, kind, wild + livestock, wild, livestock,
                injured, w->stats.villagers_died);
    }
    if (ctx->prices) {
        CoreMarketView market;
        core_get_market(w, &market);
        for (int r = 1; r < 15; r++) {
            CoreTradeQuote buy, sell;
            core_expansion_quote(w, r, 1, &buy);
            core_expansion_quote(w, r, -1, &sell);
            fprintf(ctx->prices, "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n", s->day, r, buy.unit_cents,
                    sell.unit_cents, market.base_price[r], buy.season_bp, buy.event_bp, buy.pressure_bp,
                    buy.rate_bp, e->trade_volume[r],
                    buy.rate_bp != buy.season_bp + buy.event_bp + buy.pressure_bp, buy.stock, buy.remaining);
        }
    }
    if (ctx->hubs)
        fprintf(ctx->hubs, "%d,%d,%d,%d,%.2f,%.2f\n", s->day, e->hub_visits, e->manor_visits,
                e->saved_travel_ticks, e->last_travel_minutes * 60.0, e->last_farm_minutes / 60.0);
}
/* 퇴근 직전의 실제 근무 누적과 개체 상태를 별도로 남깁니다. */
static void metric_work_details(MetricContext *ctx) {
    const World *w = ctx->world;
    const ExpansionState *e = w->expansion;
    int day = exp_day(w);
    for (int i = 0; i < e->animal_count && ctx->animals; i++) {
        const CoreAnimalView *a = &e->animals[i];
        if (!a->owner && !expansion_is_phantom(a->kind))
            continue;
        fprintf(ctx->animals, "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n", day, a->id, a->kind, a->owner,
                a->alive, a->age, a->hp, a->starve_days, a->x_milli, a->y_milli, a->threat_until_tick,
                a->attack_cooldown_milli);
    }
    for (int i = 0; i < w->building_count && ctx->production; i++) {
        const Building *b = w->buildings[i];
        if (expansion_spec(b->type)->flags & (CORE_EXP_WALL | CORE_EXP_GATE))
            continue;
        fprintf(ctx->production, "%d,%d,%d,%d,%.3f,%.3f,%.3f,%d,%d,%d,%d,%d,%d,%d,%d\n", day, b->id, b->type,
                b->worker_count, b->actual_work_milli / 1000.0, b->daily_work_milli / 1000.0,
                b->crop_labor_milli / 1000.0, b->crop_work_carry, b->pending_harvest_milli,
                b->service_credit_milli, b->feed_milli, b->breeding_progress[0], b->breeding_progress[1],
                b->breeding_progress[2], b->breeding_progress[3]);
    }
    for (int i = 0; i < w->villager_count && ctx->workers; i++) {
        const Villager *v = w->villagers[i];
        int job = v->workplace ? v->workplace->type : -1;
        fprintf(ctx->workers,
                "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n", day,
                v->id, v->sex, v->appearance, job, v->state, v->at_target, v->inside_building,
                v->current_x_milli, v->current_y_milli, v->target_x, v->target_y, v->work_day_minutes,
                job >= 0 ? expansion_work_percent(w, v, job) : 0, v->health, v->needs.hunger, v->mood,
                v->happiness, v->needs.rest, v->water, v->needs.safety, v->mission, v->shift_kind,
                v->duty_minutes, v->leisure_day, v->education_day,
                v->workplace ? v->workplace->type_serial : 0);
    }
}
static void metric_raid_details(MetricContext *ctx) {
    if (!ctx->raids)
        return;
    const World *w = ctx->world;
    const Raid *r = &w->threat.raid;
    int base = 0, summoned = 0;
    for (int i = 0; i < r->enemy_count; i++)
        if (r->enemies[i].state == CORE_ENEMY_ALIVE) {
            base += !r->enemies[i].summoner_id;
            summoned += r->enemies[i].summoner_id != 0;
        }
    fprintf(ctx->raids, "%d,%d,%d,%d,%d,%d,%d,%d,%d", exp_day(w), w->clock.tick, r->id, base, summoned,
            w->expansion->summons_total, w->expansion->deferred_summons, r->killed, r->escaped);
    for (int i = 0; i < 15; i++)
        fprintf(ctx->raids, ",%d", w->expansion->raid_drops[i]);
    fputc('\n', ctx->raids);
}
static void metric_header(FILE *f) {
    fputs(
        "seed,difficulty,day,year,season,rank,promotion_state,permanent_pop,contract_pop,immigration_arrived,"
        "immigration_type,voluntary_departures,voluntary_departures_blocked_floor8,deaths,avg_happiness,avg_"
        "mood,food,gold,wood,coal,livestock_count,wild_count,wild_biomass_total,habitat_regions,habitat_"
        "overcapacity,active_water_bodies,valid_roads,protected_area,protected_pop_pct,completed_research,"
        "idle_travel_seconds,warehouse_hub_visits,manor_hub_visits,market_price_food,market_price_iron,"
        "market_volume_food,raid_active,sim_ms,render_ms,outcome,population,logs,stone,farm_worker_hours,"
        "arrivals_block_reason,housing_free,water_coverage,food_days,heal_treated,healing_herbs_used,clinic_"
        "treated,fisher_food,lake_quota,hunted_today,predicted_event_total,false_prediction_count,merchant_"
        "visit_id,merchant_offers,traded_recipe_id,harvest_total,harvest_released,harvest_deferred,cash_"
        "balance,frame_p95_ms\n",
        f);
}
static void metric_event(void *context, const CoreEventView *event) {
    MetricContext *ctx = (MetricContext *)context;
    CoreImmigrationView imm;
    CoreExpansionRankView rank;
    CoreTradeQuote quote;
    if (!ctx->events)
        return;
    CoreSummaryView s;
    core_get_summary(ctx->world, &s);
    if (event->type == -1 && ctx->day != s.day) {
        ctx->no_progress =
            s.permanent_population > ctx->pop || s.rank > ctx->rank || s.research_done > ctx->research
                ? 0
                : ctx->no_progress + 1;
        ctx->day = s.day;
        ctx->pop = s.permanent_population;
        ctx->rank = s.rank;
        ctx->research = s.research_done;
    }
    core_expansion_immigration(ctx->world, &imm);
    core_expansion_rank(ctx->world, &rank);
    core_expansion_quote(ctx->world, CORE_RES_FOOD, 1, &quote);
    if (event->type == -1)
        metric_details(ctx, &s, &imm, &rank);
    if (event->type == -1 || event->type == CORE_EVT_RAID_STARTED || event->type == CORE_EVT_RAID_ENDED ||
        event->type == CORE_EVT_ENEMY_KILLED)
        metric_raid_details(ctx);
    fprintf(ctx->events, "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
            exp_day(ctx->world), ctx->world->clock.tick, event->type, event->a, event->b, event->c,
            imm.reasons, imm.status, rank.reason, ctx->world->expansion->arrived_today,
            ctx->world->expansion->hub_visits, ctx->world->expansion->manor_visits,
            ctx->world->expansion->saved_travel_ticks, quote.rate_bp, quote.pressure_bp, ctx->no_progress,
            ctx->world->expansion->spawn_attempts, ctx->world->expansion->spawn_capacity_rejected,
            ctx->world->expansion->spawn_tile_rejected, ctx->world->expansion->spawn_species_rejected,
            event->cause_kind, event->source_id, event->source_type, event->event_tick);
}
static void metric_row(FILE *f, const World *w, double sim_ms) {
    CoreSummaryView s;
    CoreTradeQuote food, iron;
    const ExpansionState *e = w->expansion;
    int livestock = 0;
    double travel = e->last_travel_minutes * 60.0, farm = e->last_farm_minutes / 60.0;
    core_get_summary(w, &s);
    core_expansion_quote(w, CORE_RES_FOOD, 1, &food);
    core_expansion_quote(w, CORE_RES_IRON, 1, &iron);
    for (int i = 0; i < e->animal_count; i++)
        livestock += e->animals[i].alive && e->animals[i].owner;
    fprintf(f,
            "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%.2f,%"
            "d,%d,%.2f,%.2f,%d,%d,%.3f,,%d,%d,%d,%d,%.2f",
            s.seed, s.difficulty, s.day, s.year, s.season, s.rank, s.promotion_state, s.permanent_population,
            s.contractors, e->report_arrivals, e->report_arrival_type, e->report_departures, e->report_floor8,
            e->report_deaths, s.average_happiness, s.average_mood, s.resources[CORE_RES_FOOD],
            s.resources[CORE_RES_GOLD], s.resources[CORE_RES_WOOD], s.resources[CORE_RES_COAL], livestock,
            s.wild_count, s.wild_biomass, s.habitat_regions, s.habitat_overcapacity, e->water_count,
            s.valid_roads, s.protected_area,
            s.permanent_population ? s.protected_population * 100 / s.permanent_population : 0,
            s.research_done, travel, s.warehouse_hub_visits, s.manor_hub_visits, food.unit_cents / 100.0,
            iron.unit_cents / 100.0, e->trade_volume[CORE_RES_FOOD], s.raid_id != 0, sim_ms, s.outcome,
            s.population, s.resources[CORE_RES_LOGS], s.resources[CORE_RES_STONE], farm);
    CoreImmigrationView immigration;
    core_expansion_immigration(w, &immigration);
    int treated = 0, clinic = 0, predictions = 0, false_predictions = 0, hunted = 0;
    int64_t total = 0, released = 0, deferred = 0, quota = 0;
    for (int i = 0; i < w->building_count; i++) {
        const Building *b = w->buildings[i];
        if (expansion_is_medical(b->type)) {
            treated += b->visits_today;
            if (b->type == CORE_BLD_CLINIC)
                clinic += b->visits_today;
        }
        if (expansion_spec(b->type)->annual_yield) {
            total += b->annual_harvest_total;
            released += b->annual_harvest_released;
            deferred += expansion_harvest_deferred(b) + b->pending_harvest_milli;
        }
        predictions += b->forecast_count;
        for (int j = 0; j < b->forecast_count; j++)
            false_predictions += !b->forecast_meta[j][1];
    }
    for (int i = 0; i < e->region_count; i++)
        hunted += e->region_hunt_days[i][s.day % 12];
    for (int i = 1; i <= e->water_count; i++) {
        CoreWaterView water;
        if (core_expansion_water(w, i, &water) == CORE_OK)
            quota += water.capacity_milli;
    }
    fprintf(f, ",%d,%d,%.3f,%.3f,%d,%d,%d,,%lld,%d,%d,%d,%d,%d,%d,%.3f,%.3f,%.3f,%d,\n", immigration.reasons,
            immigration.vacant, s.permanent_population ? s.water_served / (double)s.permanent_population : 0,
            s.permanent_population ? s.resources[CORE_RES_FOOD] / (double)s.permanent_population : 0, treated,
            -w->ledger.today[CORE_RES_HERBS][CORE_LEDGER_HEALING], clinic, (long long)quota, hunted,
            predictions, false_predictions, e->wandering.visit_id, e->wandering.offer_count,
            e->wandering.last_offer, total / 1000.0, released / 1000.0, deferred / 1000.0,
            s.resources[CORE_RES_GOLD]);
}
#endif
