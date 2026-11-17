/* expansion_balance.c — 확장 저장 진단과 실제 C 이동을 포함한 장기 경제 검사 */
#include "fortification_plan.h"
#include "platform.h"
#include "session.h"
#include "sim.h"
#include "storage.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void print_summary(CoreWorld *world) {
    CoreSummaryView s;
    CoreExpansionRankView rank;
    CoreBuildingView buildings[4096];
    CoreVillagerView villagers[512];
    int32_t n = 0;
    int i;
    (void)core_get_summary(world, &s);
    (void)core_expansion_rank(world, &rank);
    printf("day=%d pop=%d permanent=%d mood=%d rank=%d roads=%d area=%d protected=%d housing=%d water=%d "
           "outcome=%d next_reason=%d\n",
           s.day, s.population, s.permanent_population, s.average_mood, s.rank, s.valid_roads,
           s.protected_area, s.protected_population, s.housing_capacity, s.water_served, s.outcome,
           rank.reason);
    for (i = 0; i < 15; i++)
        printf("%s=%d (%+d) ", core_expansion_resource_key(i), s.resources[i], s.resource_delta[i]);
    puts("");
    (void)core_copy_buildings(world, buildings, 4096, &n);
    for (i = 0; i < n; i++)
        printf("b=%d type=%d x=%d y=%d worker=%d/%d active=%d labor=%d pending=%d\n", buildings[i].id,
               buildings[i].type, buildings[i].x, buildings[i].y, buildings[i].workers,
               buildings[i].worker_slots, buildings[i].state, buildings[i].crop_labor_milli,
               buildings[i].pending_harvest_milli);
    (void)core_copy_villagers(world, villagers, 512, &n);
    for (i = 0; i < n; i++)
        printf("v=%d mood=%d health=%d hunger=%d home=%d job=%d state=%d at=%d inside=%d target=%d,%d\n",
               villagers[i].id, villagers[i].mood, villagers[i].health, villagers[i].hunger,
               villagers[i].home_id, villagers[i].job, villagers[i].state, villagers[i].at_target,
               villagers[i].inside_building, villagers[i].target_x, villagers[i].target_y);
    {
        uint8_t *terrain = (uint8_t *)calloc((size_t)s.map_width * s.map_height, 1);
        BotFortificationPlan plan;
        int r, x, y;
        (void)core_copy_buildings(world, buildings, 4096, &n);
        if (terrain && core_copy_terrain(world, terrain, s.map_width * s.map_height) == CORE_OK &&
            bot_fortification_plan(terrain, s.map_width, s.map_height, s.keep_x, s.keep_y, &plan))
            for (r = 0; r < 2; r++)
                if (plan.valid[r]) {
                    printf("ring=%d bounds=%d,%d..%d,%d\n", r, plan.bounds[r][0], plan.bounds[r][1],
                           plan.bounds[r][2], plan.bounds[r][3]);
                    for (y = plan.bounds[r][1]; y <= plan.bounds[r][3]; y++)
                        for (x = plan.bounds[r][0]; x <= plan.bounds[r][2]; x++)
                            if (bot_fortification_boundary(&plan, x, y, r)) {
                                int b, found = 0;
                                for (b = 0; b < n; b++)
                                    if (x >= buildings[b].x && y >= buildings[b].y &&
                                        x < buildings[b].x + buildings[b].w &&
                                        y < buildings[b].y + buildings[b].h) {
                                        if (!(buildings[b].kind_flags & (CORE_EXP_WALL | CORE_EXP_GATE)) ||
                                            buildings[b].state != CORE_BSTATE_ACTIVE)
                                            printf("boundary=%d,%d owner=%d type=%d state=%d\n", x, y,
                                                   buildings[b].id, buildings[b].type, buildings[b].state);
                                        found = 1;
                                        break;
                                    }
                                if (!found && !r) {
                                    CorePlacementView p;
                                    (void)core_can_place_rotated(world, CORE_BLD_PALISADE, x, y, 0, &p);
                                    printf("missing=%d,%d reason=%d\n", x, y, p.result);
                                }
                            }
                }
        free(terrain);
    }
}
int main(int argc, char **argv) {
    GameArguments args;
    int days = 200, seeds = 48, first_seed = 1, only_difficulty = -1, i, d, failures = 0;
    int require_protection_by = 0, unqualified = 0;
    const char *data = "game/data", *out = "artifacts/expansion/balance";
    if (!game_arguments(&args, argc, argv))
        return 2;
    for (i = 1; i < args.count; i++) {
        const char *key = args.values[i];
        if (i + 1 >= args.count) {
            game_arguments_free(&args);
            return 2;
        }
        if (!strcmp(key, "--inspect")) {
            CoreWorld *world = NULL;
            int32_t line = 0;
            int rc = core_load(args.values[++i], NULL, &world, &line);
            printf("load=%d line=%d\n", rc, line);
            if (world) {
                print_summary(world);
                core_destroy(world);
            }
            game_arguments_free(&args);
            return rc == CORE_OK ? 0 : 1;
        } else if (!strcmp(key, "--days"))
            days = atoi(args.values[++i]);
        else if (!strcmp(key, "--seeds"))
            seeds = atoi(args.values[++i]);
        else if (!strcmp(key, "--seed"))
            first_seed = atoi(args.values[++i]);
        else if (!strcmp(key, "--difficulty"))
            only_difficulty = atoi(args.values[++i]);
        else if (!strcmp(key, "--require-protection-by"))
            require_protection_by = atoi(args.values[++i]);
        else if (!strcmp(key, "--data"))
            data = args.values[++i];
        else if (!strcmp(key, "--out"))
            out = args.values[++i];
        else {
            game_arguments_free(&args);
            return 2;
        }
    }
    if (days < 1 || days > 1000 || seeds < 1 || seeds > 512 || first_seed < 0 || only_difficulty < -1 ||
        only_difficulty > 2 || require_protection_by < 0 || require_protection_by > days) {
        game_arguments_free(&args);
        return 2;
    }
    if (!game_make_dirs(out)) {
        game_arguments_free(&args);
        return 1;
    }
    for (d = 0; d < 3; d++)
        if (only_difficulty < 0 || d == only_difficulty)
            for (i = 0; i < seeds; i++) {
                GameSession *s = (GameSession *)calloc(1, sizeof(*s));
                char path[GAME_PATH_BYTES], save[GAME_PATH_BYTES];
                FILE *csv;
                int tick, last_day = 0, rc, first_protection = 0, peak_area = 0, peak_protected = 0;
                uint64_t before = 0, after = 0;
                if (!s) {
                    failures++;
                    continue;
                }
                rc = game_session_init(s, data);
                s->ruleset = 1;
                if (rc == CORE_OK)
                    rc = game_session_new(s, u8"확장 밸런스", first_seed + i, d);
                (void)snprintf(path, sizeof(path), "%s/d%d_seed%d.csv", out, d, first_seed + i);
                csv = platform_fopen_utf8(path, "wb");
                if (!csv || rc != CORE_OK) {
                    if (csv)
                        fclose(csv);
                    game_session_free(s);
                    free(s);
                    failures++;
                    continue;
                }
                fputs("day,population,mood,rank,roads,area,protected,food,gold,logs,wood,stone,research,"
                      "outcome\n",
                      csv);
                for (tick = 0; tick < days * 144 && s->summary.outcome != CORE_OUTCOME_LOST; tick++) {
                    if (last_day != s->summary.day) {
                        CoreSummaryView *p = &s->summary;
                        if (core_validate(s->world) != 0) {
                            failures++;
                            break;
                        }
                        last_day = p->day;
                        if (p->protected_area > peak_area)
                            peak_area = p->protected_area;
                        if (p->protected_population > peak_protected)
                            peak_protected = p->protected_population;
                        if (!first_protection && p->protected_area >= 25 && p->protected_population > 0)
                            first_protection = p->day;
                        fprintf(csv, "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n", p->day, p->population,
                                p->average_mood, p->rank, p->valid_roads, p->protected_area,
                                p->protected_population, p->resources[2], p->resources[0], p->resources[6],
                                p->resources[3], p->resources[4], p->research_done, p->outcome);
                        if (sim_expansion_bot_daily(s->world) != CORE_OK) {
                            failures++;
                            break;
                        }
                    }
                    rc = game_session_step(s, 10);
                    if (rc != CORE_OK) {
                        failures++;
                        break;
                    }
                    /* 아침의 낮은 기분만 보지 않고 하루 중 충족한 승급을 실행합니다. */
                    if (s->summary.rank_ready) {
                        int32_t why = 0;
                        if (core_rank_up(s->world, &why) == CORE_OK && game_session_refresh(s) != CORE_OK) {
                            failures++;
                            break;
                        }
                    }
                }
                fclose(csv);
                (void)snprintf(save, sizeof(save), "%s/d%d_seed%d.sav", out, d, first_seed + i);
                if (!s->raid_id) {
                    if (core_state_hash(s->world, &before) != CORE_OK ||
                        core_save(s->world, save) != CORE_OK || game_session_load(s, save, NULL) != CORE_OK ||
                        core_state_hash(s->world, &after) != CORE_OK || before != after)
                        failures++;
                }
                if (require_protection_by && (!first_protection || first_protection > require_protection_by))
                    unqualified++;
                printf("difficulty=%d seed=%d day=%d pop=%d rank=%d outcome=%d first_protection=%d "
                       "peak_area=%d peak_protected=%d qualified=%d errors=%d\n",
                       d, first_seed + i, s->summary.day, s->summary.population, s->summary.rank,
                       s->summary.outcome, first_protection, peak_area, peak_protected,
                       first_protection > 0 &&
                           (!require_protection_by || first_protection <= require_protection_by),
                       failures);
                fflush(stdout);
                game_session_free(s);
                free(s);
            }
    game_arguments_free(&args);
    return failures || unqualified ? 1 : 0;
}
