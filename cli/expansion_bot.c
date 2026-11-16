/* expansion_bot.c — 공개 명령만으로 확장 경제를 운영하는 검사 봇 */
#include "castle_expansion.h"
#include "path.h"
#include "sim.h"
#include <stdlib.h>
#include <string.h>

typedef struct ExpansionBot {
    CoreWorld *world;
    CoreSummaryView summary;
    CoreBuildingView *buildings;
    CoreVillagerView *villagers;
    CoreExpansionTileView *tiles;
    uint8_t *cost;
    int buildings_count, villagers_count, cells;
    GamePathfinder finder;
} ExpansionBot;

static int count_type(const ExpansionBot *bot, int type) {
    int i, count = 0;
    for (i = 0; i < bot->buildings_count; i++)
        count += bot->buildings[i].type == type && bot->buildings[i].state != CORE_BSTATE_RUINED;
    return count;
}
static int occupied(const ExpansionBot *bot, int x, int y) {
    int i;
    for (i = 0; i < bot->buildings_count; i++) {
        const CoreBuildingView *b = &bot->buildings[i];
        if (x >= b->x && y >= b->y && x < b->x + b->w && y < b->y + b->h)
            return b->id;
    }
    return 0;
}
static int afford(const ExpansionBot *bot, int type) {
    CoreBuildingDefView def;
    int r;
    if (core_get_building_def(bot->world, type, &def) != CORE_OK || !def.available)
        return 0;
    for (r = 0; r < 15; r++)
        if (bot->summary.resources[r] < def.cost[r])
            return 0;
    return 1;
}
static int connect(ExpansionBot *bot, int x, int y) {
    GamePath path = {0};
    int rc, i, limit = bot->summary.resources[CORE_RES_LOGS] > 60 ? 60 : 20;
    rc = game_path_find(&bot->finder, bot->cost, bot->summary.map_width, bot->summary.map_height,
                        bot->summary.keep_x, bot->summary.keep_y + 2, x, y, 0, &path);
    if (rc <= 0) {
        game_path_free(&path);
        return 0;
    }
    for (i = 0; i < path.count && i < limit; i++) {
        int cell = path.cells[i], xx = cell % bot->summary.map_width, yy = cell / bot->summary.map_width;
        int32_t id = 0, reason = 0;
        if (!occupied(bot, xx, yy))
            (void)core_place_rotated(bot->world, CORE_BLD_ROAD, xx, yy, 0, &id, &reason);
    }
    game_path_free(&path);
    return 1;
}
static int build(ExpansionBot *bot, int type) {
    CoreExpansionBuildingInfo info;
    int radius, dx, dy, best = -100000, bx = 0, by = 0;
    CorePlacementView preview;
    if (!afford(bot, type))
        return 0;
    (void)core_expansion_building_info(type, &info);
    for (radius = 3; radius <= 24; radius++)
        for (dy = -radius; dy <= radius; dy++)
            for (dx = -radius; dx <= radius; dx++) {
                int x = bot->summary.keep_x + dx, y = bot->summary.keep_y + dy, score = 1000 - radius * 20,
                    xx, yy, valid = 1;
                if (abs(dx) != radius && abs(dy) != radius)
                    continue;
                /* 영구 성벽 자리를 건물로 막지 않습니다. */
                if ((dx <= 14 && dx + info.w > 14) || (dx <= -14 && dx + info.w > -14) ||
                    (dy <= 14 && dy + info.h > 14) || (dy <= -14 && dy + info.h > -14))
                    continue;
                if (x < 2 || y < 2 || x + info.w >= bot->summary.map_width - 1 ||
                    y + info.h >= bot->summary.map_height - 1)
                    continue;
                for (yy = y; yy < y + info.h; yy++)
                    for (xx = x; xx < x + info.w; xx++)
                        if (occupied(bot, xx, yy) || !bot->cost[yy * bot->summary.map_width + xx] ||
                            !bot->tiles[yy * bot->summary.map_width + xx].explored)
                            valid = 0;
                if (!valid)
                    continue;
                (void)core_can_place_rotated(bot->world, type, x, y, 0, &preview);
                if (preview.result != CORE_PLACE_OK && preview.result != CORE_PLACE_UNREACHABLE)
                    continue;
                if (type == CORE_BLD_LOGGING_SITE || type == CORE_BLD_LUMBER || type == CORE_BLD_QUARRY ||
                    type == CORE_BLD_MINE) {
                    for (yy = y + info.h / 2 - info.radius; yy <= y + info.h / 2 + info.radius; yy++)
                        for (xx = x + info.w / 2 - info.radius; xx <= x + info.w / 2 + info.radius; xx++)
                            if (xx >= 0 && yy >= 0 && xx < bot->summary.map_width &&
                                yy < bot->summary.map_height) {
                                CoreExpansionTileView *tile = &bot->tiles[yy * bot->summary.map_width + xx];
                                if (abs(xx - (x + info.w / 2)) + abs(yy - (y + info.h / 2)) > info.radius ||
                                    (xx >= x && xx < x + info.w && yy >= y && yy < y + info.h))
                                    continue;
                                if (info.job_field == 1 && tile->tree_kind >= 1 && tile->tree_kind <= 4 &&
                                    tile->tree_logs_milli > 0 &&
                                    tile->tree_age >=
                                        (int[]){28000, 48000, 84000, 112000}[tile->tree_kind - 1])
                                    score += 300;
                                if (info.job_field == 2 && tile->ore_milli > 0)
                                    score += 150;
                            }
                }
                if (type == CORE_BLD_WELL) {
                    int h;
                    for (h = 0; h < bot->buildings_count; h++) {
                        const CoreBuildingView *home = &bot->buildings[h];
                        if (home->resident_slots && abs(home->x - x) + abs(home->y - y) <= 8)
                            score += 120 + home->residents * 100;
                    }
                }
                if (score > best) {
                    best = score;
                    bx = x;
                    by = y;
                }
                if (best > 0 && radius > 8)
                    break;
            }
    if (best == -100000)
        return 0;
    (void)connect(bot, bx + info.w / 2, by + info.h);
    {
        int32_t id = 0, reason = 0;
        return core_place_rotated(bot->world, type, bx, by, 0, &id, &reason) == CORE_OK;
    }
}
static void staffing(ExpansionBot *bot) {
    int priorities[32], n = 0, i, j;
    priorities[n++] = CORE_BLD_LOGGING_SITE;
    priorities[n++] = CORE_BLD_STALL;
    if (bot->summary.resources[CORE_RES_WOOD] < 35 && bot->summary.resources[CORE_RES_LOGS] > 15)
        priorities[n++] = CORE_BLD_SAWMILL;
    if (bot->summary.resources[CORE_RES_STONE] < 35)
        priorities[n++] = CORE_BLD_QUARRY;
    priorities[n++] = CORE_BLD_GATHERER;
    priorities[n++] = CORE_BLD_GARDEN;
    if (bot->summary.season < 2) {
        priorities[n++] = CORE_BLD_CROPLAND;
        priorities[n++] = CORE_BLD_FARM;
    }
    priorities[n++] = CORE_BLD_LUMBER;
    priorities[n++] = CORE_BLD_MINE;
    priorities[n++] = CORE_BLD_SMELTER;
    priorities[n++] = CORE_BLD_KILN;
    priorities[n++] = CORE_BLD_LIBRARY;
    priorities[n++] = CORE_BLD_MARKET;
    priorities[n++] = CORE_BLD_OUTPOST;
    priorities[n++] = CORE_BLD_TOWER;
    priorities[n++] = CORE_BLD_BARRACKS;
    for (i = 0; i < bot->buildings_count; i++)
        if (bot->buildings[i].worker_slots)
            (void)core_set_auto_assign(bot->world, bot->buildings[i].id, 0);
    for (i = 0; i < bot->villagers_count; i++)
        (void)core_unassign_worker(bot->world, bot->villagers[i].id);
    /* 필요한 시설에 첫 일꾼을 나눈 뒤 식량 시설을 우선 채웁니다. */
    {
        int next = 0, pass;
        for (pass = 0; pass < 5; pass++)
            for (i = 0; i < n; i++)
                for (j = 0; j < bot->buildings_count && next < bot->villagers_count; j++) {
                    CoreBuildingView *b = &bot->buildings[j];
                    int32_t rc;
                    if (b->type != priorities[i] || b->state != CORE_BSTATE_ACTIVE || b->worker_slots <= pass)
                        continue;
                    if (pass > 0 && b->type != CORE_BLD_GARDEN && b->type != CORE_BLD_CROPLAND &&
                        b->type != CORE_BLD_FARM && b->type != CORE_BLD_LOGGING_SITE)
                        continue;
                    rc = core_assign_worker(bot->world, bot->villagers[next].id, b->id);
                    if (rc == CORE_OK)
                        next++;
                }
    }
}
int sim_expansion_bot_daily(CoreWorld *world) {
    ExpansionBot bot;
    int32_t count = 0;
    int rc = CORE_OK, i;
    CoreExpansionRankView rank;
    memset(&bot, 0, sizeof(bot));
    bot.world = world;
    if (core_get_summary(world, &bot.summary) != CORE_OK || !bot.summary.ruleset)
        return CORE_ERR_INVALID_ARG;
    bot.cells = bot.summary.map_width * bot.summary.map_height;
    bot.buildings =
        (CoreBuildingView *)calloc((size_t)bot.summary.building_count + 32, sizeof(*bot.buildings));
    bot.villagers = (CoreVillagerView *)calloc((size_t)bot.summary.population + 8, sizeof(*bot.villagers));
    bot.tiles = (CoreExpansionTileView *)calloc((size_t)bot.cells, sizeof(*bot.tiles));
    bot.cost = (uint8_t *)calloc((size_t)bot.cells, 1);
    if (!bot.buildings || !bot.villagers || !bot.tiles || !bot.cost) {
        rc = CORE_ERR_OUT_OF_MEMORY;
        goto done;
    }
    (void)core_copy_buildings(world, bot.buildings, bot.summary.building_count + 32, &count);
    bot.buildings_count = count;
    (void)core_copy_villagers(world, bot.villagers, bot.summary.population + 8, &count);
    bot.villagers_count = count;
    (void)core_expansion_copy_tiles(world, bot.tiles, bot.cells);
    (void)core_copy_cost_grid(world, bot.cost, bot.cells, NULL);
    /* 길을 깔 경로는 야외 밭·우물의 점유 칸도 돌아갑니다. 주민의 보행 격자와 구별합니다. */
    for (i = 0; i < bot.buildings_count; i++) {
        const CoreBuildingView *b = &bot.buildings[i];
        int x, y;
        if (b->kind_flags & CORE_EXP_GATE)
            continue;
        for (y = b->y; y < b->y + b->h; y++)
            for (x = b->x; x < b->x + b->w; x++)
                bot.cost[y * bot.summary.map_width + x] = 0;
    }
    if (bot.summary.housing_capacity > 0 && bot.summary.water_served < bot.summary.permanent_population &&
        afford(&bot, CORE_BLD_WELL))
        (void)build(&bot, CORE_BLD_WELL);
    else if (!count_type(&bot, CORE_BLD_LOGGING_SITE)) {
        (void)connect(&bot, bot.summary.keep_x + 10, bot.summary.keep_y + 2);
        (void)build(&bot, CORE_BLD_LOGGING_SITE);
    } else if (!count_type(&bot, CORE_BLD_GATHERER))
        (void)build(&bot, CORE_BLD_GATHERER);
    else if (count_type(&bot, CORE_BLD_GARDEN) < (bot.summary.population + 2) / 3)
        (void)build(&bot, CORE_BLD_GARDEN);
    else if (bot.summary.rank >= 1 && !count_type(&bot, CORE_BLD_STALL))
        (void)build(&bot, CORE_BLD_STALL);
    else if (bot.summary.rank >= 1 && !count_type(&bot, CORE_BLD_GRANARY))
        (void)build(&bot, CORE_BLD_GRANARY);
    else if (bot.summary.rank >= 1 && !count_type(&bot, CORE_BLD_SAWMILL))
        (void)build(&bot, CORE_BLD_SAWMILL);
    else if (bot.summary.housing_capacity < (bot.summary.rank ? bot.summary.population + 4 : 8)) {
        int housing = bot.summary.rank >= 3 && afford(&bot, CORE_BLD_MANSION) ? CORE_BLD_MANSION
                      : bot.summary.rank >= 2 && afford(&bot, CORE_BLD_HOUSE) ? CORE_BLD_HOUSE
                      : bot.summary.rank >= 1                                 ? CORE_BLD_COTTAGE
                                                                              : CORE_BLD_SHACK;
        (void)build(&bot, housing);
    } else if (bot.summary.rank >= 1) {
        const int essentials[] = {
            CORE_BLD_GRANARY,  CORE_BLD_SAWMILL, CORE_BLD_QUARRY,     CORE_BLD_CROPLAND,
            CORE_BLD_OUTPOST,  CORE_BLD_STALL,   CORE_BLD_LUMBER,     CORE_BLD_KILN,
            CORE_BLD_MINE,     CORE_BLD_SMELTER, CORE_BLD_MARKET,     CORE_BLD_LIBRARY,
            CORE_BLD_BARRACKS, CORE_BLD_TOWER,   CORE_BLD_MAGE_TOWER, CORE_BLD_CROSSBOW_TOWER};
        for (i = 0; i < (int)(sizeof(essentials) / sizeof(essentials[0])); i++)
            if (!count_type(&bot, essentials[i]) && afford(&bot, essentials[i])) {
                (void)build(&bot, essentials[i]);
                break;
            }
    }
    staffing(&bot);
    (void)core_expansion_rank(world, &rank);
    if (rank.ready) {
        int32_t reason = 0;
        (void)core_rank_up(world, &reason);
    }
    if (bot.summary.rank >= 1 && count_type(&bot, CORE_BLD_GRANARY) && count_type(&bot, CORE_BLD_SAWMILL) &&
        bot.summary.resources[CORE_RES_LOGS] > 70) {
        int placed = 0, d;
        for (d = -14; d <= 14 && placed < 12; d++) {
            int side;
            for (side = 0; side < 4 && placed < 12; side++) {
                int x = bot.summary.keep_x + (side < 2    ? d
                                              : side == 2 ? -14
                                                          : 14),
                    y = bot.summary.keep_y + (side >= 2   ? d
                                              : side == 0 ? -14
                                                          : 14);
                CorePlacementView p;
                int32_t id = 0, reason = 0;
                int type = (d == 0) ? CORE_BLD_PALISADE_GATE : CORE_BLD_PALISADE;
                (void)core_can_place_rotated(world, type, x, y, 0, &p);
                if (p.result == CORE_PLACE_OK &&
                    core_place_rotated(world, type, x, y, 0, &id, &reason) == CORE_OK)
                    placed++;
                else if (p.result == CORE_PLACE_UNREACHABLE)
                    (void)connect(&bot, x, y);
            }
        }
    }
    if (bot.summary.rank >= 3 && bot.summary.research_active < 0)
        for (i = 0; i < 24; i++) {
            CoreResearchView view;
            int32_t reason = 0;
            if (core_get_research(world, i, &view) == CORE_OK && view.state == CORE_RSTATE_AVAILABLE &&
                core_start_research(world, i, &reason) == CORE_OK)
                break;
        }
    if (bot.summary.market_open) {
        if (bot.summary.resources[CORE_RES_LOGS] < 20 && bot.summary.resources[CORE_RES_GOLD] > 45) {
            int32_t gold = 0, reason = 0;
            (void)core_trade(world, CORE_RES_LOGS, 10, &gold, &reason);
        }
        const int resources[] = {CORE_RES_WOOD, CORE_RES_STONE, CORE_RES_CLAY,   CORE_RES_IRON_ORE,
                                 CORE_RES_COAL, CORE_RES_IRON,  CORE_RES_LEATHER};
        for (i = 0; i < 7; i++) {
            int r = resources[i];
            int32_t gold = 0, reason = 0;
            if (bot.summary.resources[r] < 10 && bot.summary.resources[CORE_RES_GOLD] > 90)
                (void)core_trade(world, r, 5, &gold, &reason);
        }
    }
done:
    game_pathfinder_free(&bot.finder);
    free(bot.buildings);
    free(bot.villagers);
    free(bot.tiles);
    free(bot.cost);
    return rc;
}
