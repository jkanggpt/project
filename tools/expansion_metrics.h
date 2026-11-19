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
} MetricContext;
static int metric_open_details(MetricContext *ctx, const char *out, int difficulty, int seed) {
    static const char *const names[] = {"immigration", "promotion", "habitats", "people", "prices", "hubs"};
    static const char *const headers[] = {
        "day,last_arrival_day,cooldown_remaining,next_check_day,is_eligible,all_rejection_reasons,status,"
        "group_size,arrival_type,permanent_pop_before_arrivals_estimate,permanent_pop_after,special_waves,refill_day\n",
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
        "worker_hours\n"};
    FILE **files[] = {&ctx->immigration, &ctx->promotion, &ctx->habitats,
                      &ctx->people,      &ctx->prices,    &ctx->hubs};
    char path[GAME_PATH_BYTES];
    for (int i = 0; i < 6; i++) {
        snprintf(path, sizeof(path), "%s/d%d_seed%d.%s.csv", out, difficulty, seed, names[i]);
        *files[i] = platform_fopen_utf8(path, "wb");
        if (!*files[i])
            return 0;
        fputs(headers[i], *files[i]);
    }
    return 1;
}
static void metric_close_details(MetricContext *ctx) {
    FILE *files[] = {ctx->immigration, ctx->promotion, ctx->habitats, ctx->people, ctx->prices, ctx->hubs};
    for (int i = 0; i < 6; i++)
        if (files[i])
            fclose(files[i]);
}
static int metric_facility(const World *w, int type) {
    for (int i = 0; i < w->building_count; i++)
        if (w->buildings[i]->type == type && w->buildings[i]->state == CORE_BSTATE_ACTIVE)
            return 1;
    return 0;
}
static void metric_details(MetricContext *ctx, const CoreSummaryView *s, const CoreImmigrationView *imm,
                           const CoreExpansionRankView *rank) {
    const World *w = ctx->world;
    const ExpansionState *e = w->expansion;
    int injured = 0, facility = 1, target = rank->target;
    if (ctx->immigration)
        fprintf(ctx->immigration, "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n", s->day, imm->last_day,
                exp_clamp(imm->last_day + 4 - s->day, 0, 4), imm->next_day, imm->reasons == 0, imm->reasons,
                imm->status, e->report_arrivals, e->report_arrival_type,
                s->permanent_population - e->report_arrivals + e->report_departures, s->permanent_population,
                imm->special_waves, imm->refill_day);
    if (target == 1)
        facility = metric_facility(w, CORE_BLD_WELL);
    else if (target == 2)
        facility = metric_facility(w, CORE_BLD_GRANARY) && metric_facility(w, CORE_BLD_CROPLAND) &&
                   (metric_facility(w, CORE_BLD_OUTPOST) || metric_facility(w, CORE_BLD_WATCHTOWER) ||
                    metric_facility(w, CORE_BLD_TOWER) || metric_facility(w, CORE_BLD_BARRACKS));
    else if (target == 3)
        facility = metric_facility(w, CORE_BLD_SAWMILL) && metric_facility(w, CORE_BLD_MARKET) &&
                   (metric_facility(w, CORE_BLD_KILN) || metric_facility(w, CORE_BLD_SMELTER));
    else if (target == 4)
        facility = metric_facility(w, CORE_BLD_MINE) || metric_facility(w, CORE_BLD_MANA_MINE);
    else if (target == 5)
        facility = metric_facility(w, CORE_BLD_BARRACKS) &&
                   (metric_facility(w, CORE_BLD_CROSSBOW_TOWER) ||
                    metric_facility(w, CORE_BLD_CANNON_TOWER) || metric_facility(w, CORE_BLD_ARCANE_TOWER));
    if (ctx->promotion)
        fprintf(ctx->promotion, "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n", s->day, target,
                s->permanent_population < rank->population, s->average_happiness < rank->mood,
                s->valid_roads < rank->roads, s->protected_area < rank->area,
                s->protected_population * 100 < s->permanent_population * rank->protected_pct,
                s->research_done < rank->research, s->resources[CORE_RES_GOLD] < rank->gold,
                s->resources[CORE_RES_MANA] < rank->mana, s->resources[CORE_RES_GOLD_BAR] < rank->gold_bars,
                !facility,
                target == 1 && expansion_bank_have(w, CORE_RES_FOOD) < s->permanent_population * 9600,
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
    for (int kind = 0; kind < 24 && ctx->people; kind++) {
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
static void metric_header(FILE *f) {
    fputs(
        "seed,difficulty,day,year,season,rank,promotion_state,permanent_pop,contract_pop,immigration_arrived,"
        "immigration_type,voluntary_departures,voluntary_departures_blocked_floor8,deaths,avg_happiness,avg_"
        "mood,food,gold,wood,coal,livestock_count,wild_count,wild_biomass_total,habitat_regions,habitat_"
        "overcapacity,active_water_bodies,valid_roads,protected_area,protected_pop_pct,completed_research,"
        "idle_travel_seconds,warehouse_hub_visits,manor_hub_visits,market_price_food,market_price_iron,"
        "market_volume_food,raid_active,sim_ms,render_ms,outcome,population,logs,stone,farm_worker_hours\n",
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
    fprintf(ctx->events, "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n", exp_day(ctx->world),
            ctx->world->clock.tick, event->type, event->a, event->b, event->c, imm.reasons, imm.status,
            rank.reason, ctx->world->expansion->arrived_today, ctx->world->expansion->hub_visits,
            ctx->world->expansion->manor_visits, ctx->world->expansion->saved_travel_ticks, quote.rate_bp,
            quote.pressure_bp, ctx->no_progress, ctx->world->expansion->spawn_attempts,
            ctx->world->expansion->spawn_capacity_rejected, ctx->world->expansion->spawn_tile_rejected,
            ctx->world->expansion->spawn_species_rejected);
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
            "d,%d,%.2f,%.2f,%d,%d,%.3f,0,%d,%d,%d,%d,%.2f\n",
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
}
#endif
