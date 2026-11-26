/* sim.c — 공용 실행기를 사용하는 콘솔 시뮬레이션과 불변 조건 검사 */
#include "sim.h"
#include "format.h"
#include "platform.h"
#include "session.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct BotContext {
    CoreWorld *world;
    CoreSummaryView summary;
    CoreBuildingView *buildings; /* 동적 배열 */
    int32_t building_count;
    int32_t building_capacity;
    int32_t keep_door_x;
    int32_t keep_door_y;
} BotContext;

static int refresh_buildings(BotContext *bot) {
    int32_t rc;
    (void)core_get_summary(bot->world, &bot->summary);
    rc = core_copy_buildings(bot->world, bot->buildings, bot->building_capacity, &bot->building_count);
    if (rc == CORE_ERR_BUFFER_TOO_SMALL) {
        int32_t capacity = bot->building_count + 32;
        CoreBuildingView *grown =
            (CoreBuildingView *)realloc(bot->buildings, (size_t)capacity * sizeof(CoreBuildingView));
        if (grown == NULL) {
            return 0;
        }
        bot->buildings = grown;
        bot->building_capacity = capacity;
        rc = core_copy_buildings(bot->world, bot->buildings, bot->building_capacity, &bot->building_count);
    }
    if (rc != CORE_OK) {
        return 0;
    }
    bot->keep_door_x = bot->summary.keep_x;
    bot->keep_door_y = bot->summary.keep_y + 2;
    return 1;
}

static void write_csv_row(FILE *out, const CoreSummaryView *summary) {
    int i;
    fprintf(out, "%d,%d,%d", (int)summary->day, (int)summary->year, (int)summary->season);
    for (i = 0; i < CORE_RES_COUNT; i++) {
        fprintf(out, ",%d", (int)summary->resources[i]);
    }
    fprintf(out, ",%d,%d,%d,%d,%d,%d,%d,%d", (int)summary->resource_capacity[CORE_RES_FOOD],
            (int)summary->population, (int)summary->average_mood, (int)summary->homeless,
            (int)summary->employed, (int)summary->building_count, (int)summary->rank,
            (int)summary->threat_strength);
    fprintf(out, ",%d,%d,%d,%d,%d,%d,%d", (int)summary->weather, (int)summary->nature_event,
            (int)summary->sick_count, (int)summary->cold_houses, (int)summary->merchant_days_left,
            (int)summary->threat_type, (int)summary->guards);
    fprintf(out, ",%d,%d,%d\n", (int)summary->defense, (int)summary->research_done, (int)summary->outcome);
}

static uint32_t soak_random(uint32_t *state) {
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

static int32_t soak_pick(uint32_t *state, int32_t lo, int32_t hi) {
    return lo + (int32_t)(soak_random(state) % (uint32_t)(hi - lo + 1));
}

static int poke_invalid_calls(CoreWorld *world) {
    uint8_t tiny[4];
    uint64_t before = 0;
    uint64_t after = 1;
    int32_t id = 0;
    int32_t reason = 0;
    int ok = 1;
    (void)core_state_hash(world, &before);
    ok &= core_tick(world, 0) == CORE_ERR_INVALID_ARG;
    ok &= core_tick(world, -3) == CORE_ERR_INVALID_ARG;
    ok &= core_copy_terrain(world, tiny, (int32_t)sizeof(tiny)) == CORE_ERR_BUFFER_TOO_SMALL;
    ok &= core_get_summary(world, NULL) == CORE_ERR_NULL_ARG;
    ok &= core_save(world, "") == CORE_ERR_INVALID_ARG;
    ok &= core_place_building(world, CORE_BLD_KEEP, 10, 10, &id, &reason) == CORE_ERR_REJECTED;
    ok &= core_place_building(world, 999, 10, 10, &id, &reason) == CORE_ERR_REJECTED;
    ok &= core_place_building(world, CORE_BLD_HOUSE, -50, 9999, &id, &reason) == CORE_ERR_REJECTED;
    ok &= core_demolish(world, -1) == CORE_ERR_NOT_FOUND;
    ok &= core_assign_worker(world, 999999, 1) == CORE_ERR_NOT_FOUND;
    ok &= core_villager_arrived(world, 999999, 1) == CORE_ERR_NOT_FOUND;
    ok &= core_set_auto_assign(world, 1, 7) == CORE_ERR_INVALID_ARG;
    ok &= core_report_building_damage(world, 1, 1, -5) == CORE_ERR_INVALID_ARG;
    ok &= core_report_enemy_killed(world, 99999, 1) == CORE_IGNORED;
    ok &= core_report_raid_ended(world, -1) == CORE_IGNORED;
    ok &= core_report_loot(world, 99999, 1, 1, NULL) == CORE_IGNORED;
    (void)core_state_hash(world, &after);
    return ok && before == after;
}

static void random_commands(BotContext *bot, uint32_t *random_state) {
    CoreVillagerView *views;
    int32_t villager_count = 0;
    int32_t i;
    int32_t id;
    int32_t reason;
    int32_t placed;
    if (!refresh_buildings(bot)) {
        return;
    }
    for (i = 0; i < 6; i++) {
        int32_t type = soak_pick(random_state, 1, CORE_BLD_COUNT - 1);
        int32_t x = bot->summary.keep_x + soak_pick(random_state, -16, 16);
        int32_t y = bot->summary.keep_y + soak_pick(random_state, -16, 16);
        (void)core_place_building(bot->world, type, x, y, &id, &reason);
    }
    (void)core_place_line(bot->world, soak_pick(random_state, 0, 1) ? CORE_BLD_ROAD : CORE_BLD_WALL,
                          bot->summary.keep_x + soak_pick(random_state, -12, 12),
                          bot->summary.keep_y + soak_pick(random_state, -12, 12),
                          bot->summary.keep_x + soak_pick(random_state, -12, 12),
                          bot->summary.keep_y + soak_pick(random_state, -12, 12), &placed, &reason);
    if (bot->building_count > 1 && soak_pick(random_state, 0, 3) == 0) {
        (void)core_demolish(bot->world,
                            bot->buildings[soak_pick(random_state, 0, bot->building_count - 1)].id);
    }
    (void)core_remove_road(bot->world, bot->summary.keep_x + soak_pick(random_state, -12, 12),
                           bot->summary.keep_y + soak_pick(random_state, -12, 12));
    views =
        (CoreVillagerView *)malloc((size_t)(bot->summary.villager_count_hint + 1) * sizeof(CoreVillagerView));
    if (views != NULL &&
        core_copy_villagers(bot->world, views, bot->summary.villager_count_hint + 1, &villager_count) ==
            CORE_OK &&
        villager_count > 0 && bot->building_count > 0) {
        const CoreVillagerView *v = &views[soak_pick(random_state, 0, villager_count - 1)];
        int32_t target = bot->buildings[soak_pick(random_state, 0, bot->building_count - 1)].id;
        int32_t gold;
        (void)core_trade(bot->world, soak_pick(random_state, 0, CORE_RES_COUNT - 1),
                         soak_pick(random_state, 0, 1) ? soak_pick(random_state, 1, 4)
                                                       : -soak_pick(random_state, 1, 4),
                         &gold, &reason);
        (void)core_merchant_accept(bot->world, soak_pick(random_state, 0, CORE_MERCHANT_MAX_DEALS - 1),
                                   &reason);
        (void)core_repair(bot->world, target, &reason);
        (void)core_start_research(bot->world, soak_pick(random_state, -2, CORE_MAX_RESEARCH + 2), &reason);
        (void)core_rank_up(bot->world, &reason);
        switch (soak_pick(random_state, 0, 4)) {
        case 0:
            (void)core_assign_worker(bot->world, v->id, target);
            break;
        case 1:
            (void)core_unassign_worker(bot->world, v->id);
            break;
        case 2:
            (void)core_release_worker(bot->world, v->id);
            break;
        case 3:
            (void)core_villager_arrived(bot->world, v->id, v->intent_rev - 1); /* 오래된 보고 */
            break;
        default:
            (void)core_villager_unreachable(bot->world, v->id, v->intent_rev);
            break;
        }
    }
    free(views);
}

static double now_ms(void) {
#if defined(TIME_UTC)
    struct timespec ts;
    if (timespec_get(&ts, TIME_UTC) != 0) {
        return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1000000.0;
    }
#endif
    return (double)clock() * 1000.0 / CLOCKS_PER_SEC;
}

int sim_bot_exists(const char *name) {
    return name && (!strcmp(name, "idle") || !strcmp(name, "reasonable"));
}
int sim_bot_daily(CoreWorld *world, const char *name) {
    if (!world || !sim_bot_exists(name))
        return CORE_ERR_INVALID_ARG;
    return strcmp(name, "idle") ? sim_expansion_bot_daily(world) : CORE_OK;
}
void sim_print_bots(void) { printf("idle, reasonable\n"); }
static GameSession *open_session(const CoreDefs *defs, int seed, int difficulty, const char *name) {
    GameSession *s = (GameSession *)calloc(1, sizeof(*s));
    if (!s)
        return NULL;
    if (game_session_init_shared(s, defs) != CORE_OK ||
        game_session_new(s, name, seed, difficulty) != CORE_OK) {
        game_session_free(s);
        free(s);
        return NULL;
    }
    return s;
}
static void close_session(GameSession *s) {
    if (s) {
        game_session_free(s);
        free(s);
    }
}
static int session_tick(GameSession *s) { return game_session_step(s, 1440.0f / s->config.ticks_per_day); }
static void print_csv_header(FILE *out) {
    fprintf(
        out,
        "day,year,season,gold,mana,food,wood,stone,herbs,logs,clay,bricks,iron_ore,coal,iron,gold_ore,gold_"
        "bar,leather,capacity,population,mood,homeless,employed,buildings,rank,threat_strength,weather,"
        "nature_event,sick,cold_houses,merchant,threat_type,guards,defense,research_done,outcome\n");
}
int sim_run_simulation(int days, int32_t seed, int difficulty, const char *bot_name, const char *csv_path,
                       const CoreDefs *defs, int print_events) {
    GameSession *s;
    FILE *out = stdout;
    int day, tick, status = 0;
    if (!sim_bot_exists(bot_name))
        return 1;
    s = open_session(defs, seed, difficulty, "Simulation");
    if (!s)
        return 1;
    if (csv_path) {
        out = platform_fopen_utf8(csv_path, "wb");
        if (!out) {
            close_session(s);
            return 1;
        }
    }
    print_csv_header(out);
    write_csv_row(out, &s->summary);
    for (day = 0; day < days && !status && s->summary.outcome != CORE_OUTCOME_LOST; day++) {
        if (sim_bot_daily(s->world, bot_name) != CORE_OK) {
            status = 1;
            break;
        }
        for (tick = 0; tick < s->config.ticks_per_day; tick++) {
            CoreEventView event;
            int rc;
            if (strcmp(bot_name, "idle") && s->summary.minute_of_day == 720)
                sim_expansion_bot_trade(s->world);
            rc = session_tick(s);
            if (rc == CORE_ERR_REJECTED && s->summary.outcome == CORE_OUTCOME_LOST)
                break;
            if (rc != CORE_OK || core_validate(s->world)) {
                status = 1;
                break;
            }
            while (core_poll_event(s->world, &event) == 1)
                if (print_events && event.type != CORE_EVT_PRODUCED) {
                    char text[256];
                    fmt_event(&event, text, sizeof(text));
                    fprintf(stderr, "[%d] %s\n", s->summary.day, text);
                }
        }
        write_csv_row(out, &s->summary);
    }
    if (out != stdout && fclose(out))
        status = 1;
    printf(u8"시뮬레이션 종료: %d일, 주민 %d명, 등급 %s, 오류 %d\n", s->summary.day, s->summary.population,
           fmt_rank_name(s->summary.rank), status);
    close_session(s);
    return status;
}
static int session_roundtrip(GameSession *s, const char *path) {
    uint64_t before = 0, after = 1;
    int rc = game_session_checkpoint(s, 0);
    if (rc == CORE_OK)
        rc = core_save(s->world, path);
    if (rc == CORE_ERR_REJECTED)
        return 1;
    if (rc != CORE_OK || core_state_hash(s->world, &before) != CORE_OK)
        return 0;
    if (game_session_load(s, path, NULL) != CORE_OK || core_state_hash(s->world, &after) != CORE_OK)
        return 0;
    return before == after && !core_validate(s->world);
}
static int soak_one_seed(int days, int32_t seed, const CoreDefs *defs, long *ticks_run) {
    GameSession *s = open_session(defs, seed, seed % CORE_DIFF_COUNT, "Soak");
    BotContext bot = {0};
    uint32_t random_state = (uint32_t)seed * 2654435761u + 1u;
    long done, budget = (long)days * 144;
    int failures = 0, restarts = 0, last_day;
    char path[80], backup[88];
    if (!s)
        return 1;
    snprintf(path, sizeof(path), "soak_%d.sav", (int)seed);
    snprintf(backup, sizeof(backup), "%s.bak", path);
    last_day = s->summary.day;
    for (done = 0; done < budget && !failures;) {
        uint32_t roll = soak_random(&random_state);
        int rc;
        if (s->summary.outcome == CORE_OUTCOME_LOST) {
            if (!session_roundtrip(s, path)) {
                failures++;
                break;
            }
            restarts++;
            if (game_session_new(s, "Soak", seed + 100000 * restarts, seed % CORE_DIFF_COUNT) != CORE_OK) {
                failures++;
                break;
            }
            last_day = s->summary.day;
        }
        rc = session_tick(s);
        if (rc != CORE_OK || core_validate(s->world)) {
            failures++;
            break;
        }
        done++;
        (*ticks_run)++;
        if (last_day != s->summary.day) {
            last_day = s->summary.day;
            bot.world = s->world;
            if (roll % 3)
                sim_bot_daily(s->world, "reasonable");
            else
                random_commands(&bot, &random_state);
            if (roll % 10 == 0 && !session_roundtrip(s, path))
                failures++;
            if (roll % 13 == 0 && !poke_invalid_calls(s->world))
                failures++;
        }
        if (roll % 7 == 0) {
            CoreEventView event;
            while (core_poll_event(s->world, &event) == 1) {
            }
        }
    }
    if (failures)
        fprintf(stderr, "soak seed=%d tick=%ld day=%d error=%d\n", seed, done, s->summary.day, s->last_error);
    free(bot.buildings);
    close_session(s);
    remove(path);
    remove(backup);
    return failures;
}
int sim_run_soak(int days, int seeds, int32_t first_seed, const CoreDefs *defs) {
    int i;
    int failed_seeds = 0;
    long ticks_run = 0;
    for (i = 0; i < seeds; i++) {
        int32_t seed = first_seed + i;
        if (soak_one_seed(days, seed, defs, &ticks_run) != 0) {
            failed_seeds++;
        }
    }
    if (failed_seeds == 0) {
        printf(u8"소크 테스트 통과: 시드 %d개 × %d일, 틱 %ld번, 불변 조건 위반 0\n", seeds, days, ticks_run);
        return 0;
    }
    printf(u8"소크 테스트 실패: 시드 %d개 중 %d개에서 문제\n", seeds, failed_seeds);
    return 1;
}
int sim_run_bench(int villagers, int enemies, int32_t seed, const CoreDefs *defs) {
    GameSession *s = open_session(defs, seed, CORE_DIFF_NORMAL, "Bench");
    double total = 0, slowest = 0;
    int tick, measured = 0, rc = CORE_OK;
    if (!s)
        return 1;
    rc = core_debug_stress(s->world, villagers, enemies);
    if (rc == CORE_OK)
        rc = game_session_refresh(s);
    for (tick = 0; rc == CORE_OK && tick < s->config.ticks_per_day * 6; tick++) {
        double start = now_ms(), spent;
        rc = session_tick(s);
        spent = now_ms() - start;
        if (tick >= s->config.ticks_per_day) {
            total += spent;
            if (spent > slowest)
                slowest = spent;
            measured++;
        }
    }
    printf(u8"공용 이동·전투 포함: %d틱 평균 %.3f ms 최대 %.3f ms, 오류 %d\n", measured,
           measured ? total / measured : 0, slowest, rc);
    close_session(s);
    return rc != CORE_OK;
}
