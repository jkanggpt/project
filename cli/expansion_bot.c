/* expansion_bot.c — 공개 명령만으로 확장 경제를 운영하는 검사 봇 */
#include "castle_expansion.h"
#include "fortification_plan.h"
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
    uint8_t *cost, *terrain, *roads;
    BotFortificationPlan fort;
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
static int connect(ExpansionBot *bot, int x, int y, int reserve) {
    GamePath path = {0};
    int rc, i;
    bot->finder.normal_cost = 100;
    rc = game_path_find(&bot->finder, bot->cost, bot->summary.map_width, bot->summary.map_height,
                        bot->summary.keep_x, bot->summary.keep_y + 2, x, y, 0, &path);
    if (rc <= 0) {
        game_path_free(&path);
        return 0;
    }
    for (i = 0; i < path.count; i++) {
        int cell = path.cells[i], xx = cell % bot->summary.map_width, yy = cell / bot->summary.map_width;
        int32_t id = 0, reason = 0;
        CoreSummaryView live;
        (void)core_get_summary(bot->world, &live);
        if (live.resources[CORE_RES_LOGS] <= reserve)
            break;
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
                if (bot_fortification_reserved(&bot->fort, x, y, info.w, info.h))
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
                if (info.residents && bot_fortification_inside(&bot->fort, x, y, info.w, info.h, 0))
                    score += 1200;
                else if (info.residents && bot_fortification_inside(&bot->fort, x, y, info.w, info.h, 1))
                    score += 400;
                if (type == CORE_BLD_OUTPOST) {
                    int b;
                    for (b = 0; b < bot->buildings_count; b++)
                        if (bot->buildings[b].type == CORE_BLD_LOGGING_SITE) {
                            int d =
                                abs(bot->buildings[b].door_x - x) + abs(bot->buildings[b].door_y - (y + 1));
                            score -= d * 100;
                            if (d <= 4)
                                score += 4000;
                        }
                }
                if (type == CORE_BLD_WELL) {
                    int h;
                    for (h = 0; h < bot->buildings_count; h++) {
                        const CoreBuildingView *home = &bot->buildings[h];
                        int j, supplied = 0;
                        if (!home->resident_slots || home->state == CORE_BSTATE_RUINED)
                            continue;
                        for (j = 0; j < bot->buildings_count; j++) {
                            const CoreBuildingView *well = &bot->buildings[j];
                            if (well->type == CORE_BLD_WELL && well->state != CORE_BSTATE_RUINED &&
                                abs(well->x - home->x) + abs(well->y - home->y) <= 8) {
                                supplied = 1;
                                break;
                            }
                        }
                        if (!supplied && abs(home->x - x) + abs(home->y - y) <= 8)
                            score += 2000 + home->residents * 200;
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
    (void)connect(bot, bx + info.w / 2, by + info.h, 6);
    {
        int32_t id = 0, reason = 0;
        return core_place_rotated(bot->world, type, bx, by, 0, &id, &reason) == CORE_OK;
    }
}
static int fortify(ExpansionBot *bot, int ring) {
    const int *bounds = bot->fort.bounds[ring];
    int pass, x, y, placed = 0, pending_gates = 0, reserve = 6;
    if (!bot->fort.valid[ring])
        return 0;
    for (pass = 0; pass < 2 && placed < 10; pass++) {
        for (y = bounds[1]; y <= bounds[3] && placed < 10; y++)
            for (x = bounds[0]; x <= bounds[2] && placed < 10; x++) {
                int gate, owner, type, rot = 0;
                int32_t id = 0, reason = 0;
                CoreSummaryView live;
                CorePlacementView preview;
                if (!bot_fortification_boundary(&bot->fort, x, y, ring))
                    continue;
                gate = ((x == bounds[0] || x == bounds[2]) && y == bot->summary.keep_y) ||
                       ((y == bounds[1] || y == bounds[3]) && x == bot->summary.keep_x);
                gate |= bot->roads[y * bot->summary.map_width + x] != 0;
                if (!gate) {
                    (void)core_can_place_rotated(bot->world, CORE_BLD_PALISADE, x, y, 0, &preview);
                    gate = preview.result == CORE_PLACE_BLOCKS_DOOR;
                }
                if (gate != (pass == 0))
                    continue;
                owner = occupied(bot, x, y);
                if (owner) {
                    int i;
                    for (i = 0; i < bot->buildings_count; i++)
                        if (bot->buildings[i].id == owner && bot->buildings[i].state == CORE_BSTATE_RUINED)
                            (void)core_repair(bot->world, owner, &reason);
                    continue;
                }
                (void)core_get_summary(bot->world, &live);
                if (live.resources[CORE_RES_LOGS] < (gate ? 5 : 2) + reserve) {
                    pending_gates |= gate;
                    continue;
                }
                type = gate ? CORE_BLD_PALISADE_GATE : CORE_BLD_PALISADE;
                if (gate && (x == bounds[0] || x == bounds[2]))
                    rot = 1;
                (void)core_can_place_rotated(bot->world, type, x, y, rot, &preview);
                if (preview.result == CORE_PLACE_BLOCKS_DOOR) {
                    type = CORE_BLD_PALISADE_GATE;
                    gate = 1;
                    (void)core_can_place_rotated(bot->world, type, x, y, rot, &preview);
                }
                if (preview.result == CORE_PLACE_OK) {
                    if (core_place_rotated(bot->world, type, x, y, rot, &id, &reason) == CORE_OK)
                        placed++;
                } else if (preview.result == CORE_PLACE_UNREACHABLE) {
                    int xx = x == bounds[0] ? x + 1 : x == bounds[2] ? x - 1 : x;
                    int yy = y == bounds[1] ? y + 1 : y == bounds[3] ? y - 1 : y;
                    if (!bot->tiles[y * bot->summary.map_width + x].explored) {
                        /* 일반 도로를 연장해 다음 시야 갱신에서 경계를 발견합니다. */
                        (void)connect(bot, x, y, gate ? 5 : 2);
                    } else
                        (void)connect(bot, xx, yy, 6);
                    pending_gates |= gate;
                } else
                    pending_gates |= gate;
            }
        /* 비싼 문을 모으는 동안 싼 벽이 잔고를 먼저 쓰는 영구 지연을 막습니다. */
        if (!pass && pending_gates)
            break;
    }
    return placed;
}
static int needs_well(const ExpansionBot *bot) {
    int i, j;
    for (i = 0; i < bot->buildings_count; i++) {
        const CoreBuildingView *home = &bot->buildings[i];
        int supplied = 0;
        if (!home->resident_slots || home->state == CORE_BSTATE_RUINED)
            continue;
        for (j = 0; j < bot->buildings_count; j++) {
            const CoreBuildingView *well = &bot->buildings[j];
            if (well->type == CORE_BLD_WELL && well->state != CORE_BSTATE_RUINED &&
                abs(well->x - home->x) + abs(well->y - home->y) <= 8) {
                supplied = 1;
                break;
            }
        }
        if (!supplied)
            return 1;
    }
    return bot->summary.water_served < bot->summary.permanent_population;
}
static void staffing(ExpansionBot *bot) {
    int priorities[32], n = 0, i, j;
    priorities[n++] = CORE_BLD_LOGGING_SITE;
    priorities[n++] = CORE_BLD_OUTPOST;
    if (bot->summary.resources[CORE_RES_LOGS] < 40 && bot->summary.population >= 6)
        priorities[n++] = CORE_BLD_LOGGING_SITE;
    if (bot->summary.population >= 7 || bot->summary.resources[CORE_RES_FOOD] > 120)
        priorities[n++] = CORE_BLD_STALL;
    if (bot->summary.raid_id || bot->summary.population >= 12)
        priorities[n++] = CORE_BLD_BARRACKS;
    priorities[n++] = CORE_BLD_GATHERER;
    priorities[n++] = CORE_BLD_GARDEN;
    if (bot->summary.season < 2) {
        priorities[n++] = CORE_BLD_CROPLAND;
        priorities[n++] = CORE_BLD_FARM;
    }
    if (bot->summary.resources[CORE_RES_WOOD] < 35 && bot->summary.resources[CORE_RES_LOGS] > 15)
        priorities[n++] = CORE_BLD_SAWMILL;
    if (bot->summary.resources[CORE_RES_STONE] < 35)
        priorities[n++] = CORE_BLD_QUARRY;
    priorities[n++] = CORE_BLD_LUMBER;
    priorities[n++] = CORE_BLD_HEALER;
    priorities[n++] = CORE_BLD_BATHHOUSE;
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
    bot.terrain = (uint8_t *)calloc((size_t)bot.cells, 1);
    bot.roads = (uint8_t *)calloc((size_t)bot.cells, 1);
    if (!bot.buildings || !bot.villagers || !bot.tiles || !bot.cost || !bot.terrain || !bot.roads) {
        rc = CORE_ERR_OUT_OF_MEMORY;
        goto done;
    }
    (void)core_copy_buildings(world, bot.buildings, bot.summary.building_count + 32, &count);
    bot.buildings_count = count;
    (void)core_copy_villagers(world, bot.villagers, bot.summary.population + 8, &count);
    bot.villagers_count = count;
    (void)core_expansion_copy_tiles(world, bot.tiles, bot.cells);
    (void)core_copy_cost_grid(world, bot.cost, bot.cells, NULL);
    (void)core_copy_terrain(world, bot.terrain, bot.cells);
    (void)core_copy_roads(world, bot.roads, bot.cells);
    if (!bot_fortification_plan(bot.terrain, bot.summary.map_width, bot.summary.map_height,
                                bot.summary.keep_x, bot.summary.keep_y, &bot.fort)) {
        rc = CORE_ERR_OUT_OF_MEMORY;
        goto done;
    }
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
    if (bot.summary.rank >= 1) {
        /* 개척지 때 도로 없이 지은 주거도 일반 이주 전에 입구까지 연결합니다. */
        for (i = 0; i < bot.buildings_count; i++) {
            const CoreBuildingView *home = &bot.buildings[i];
            if (home->resident_slots && home->state == CORE_BSTATE_ACTIVE &&
                !bot.roads[home->door_y * bot.summary.map_width + home->door_x]) {
                int32_t road_id = 0, road_reason = 0;
                (void)core_place_rotated(world, CORE_BLD_ROAD, home->door_x, home->door_y, 0,
                                         &road_id, &road_reason);
                (void)connect(&bot, home->door_x, home->door_y, 10);
            }
        }
    }
    if (!count_type(&bot, CORE_BLD_LOGGING_SITE)) {
        (void)build(&bot, CORE_BLD_LOGGING_SITE);
    } else if (!count_type(&bot, CORE_BLD_GATHERER))
        (void)build(&bot, CORE_BLD_GATHERER);
    else if (count_type(&bot, CORE_BLD_GARDEN) < (bot.summary.population + 1) / 2)
        (void)build(&bot, CORE_BLD_GARDEN);
    else if (!count_type(&bot, CORE_BLD_OUTPOST))
        (void)build(&bot, CORE_BLD_OUTPOST);
    else if (bot.summary.housing_capacity <
             (bot.summary.rank ? bot.summary.population + (bot.summary.protected_area >= 25 ? 2 : 0) : 8)) {
        int housing = bot.summary.rank >= 3 && afford(&bot, CORE_BLD_MANSION)   ? CORE_BLD_MANSION
                      : bot.summary.rank >= 2 && afford(&bot, CORE_BLD_HOUSE)   ? CORE_BLD_HOUSE
                      : bot.summary.rank >= 1 && afford(&bot, CORE_BLD_COTTAGE) ? CORE_BLD_COTTAGE
                                                                                : CORE_BLD_SHACK;
        (void)build(&bot, housing);
    } else if (bot.summary.housing_capacity > 0 && needs_well(&bot) && afford(&bot, CORE_BLD_WELL))
        (void)build(&bot, CORE_BLD_WELL);
    else if (bot.summary.rank >= 1 && !count_type(&bot, CORE_BLD_STALL))
        (void)build(&bot, CORE_BLD_STALL);
    else if (bot.summary.rank >= 1 && !count_type(&bot, CORE_BLD_GRANARY))
        (void)build(&bot, CORE_BLD_GRANARY);
    else if (bot.summary.rank == 1 && bot.summary.protected_area >= 25 &&
             count_type(&bot, CORE_BLD_COTTAGE) * 2 < bot.summary.population &&
             bot.summary.average_happiness < 52)
        (void)build(&bot, CORE_BLD_COTTAGE);
    else if (bot.summary.rank >= 1 && bot.summary.protected_area < 25) {
        /* 첫 폐쇄가 끝날 때까지 가공·상급 시설로 원목을 분산하지 않습니다. */
    } else if (bot.summary.rank >= 1 && !count_type(&bot, CORE_BLD_SAWMILL))
        (void)build(&bot, CORE_BLD_SAWMILL);
    else if (bot.summary.rank >= 1 &&
             bot.summary.resource_capacity[CORE_RES_FOOD] < (bot.summary.population + 8) * 144 / 10 + 300) {
        (void)build(&bot, CORE_BLD_GRANARY);
    } else if (bot.summary.rank >= 1) {
        const int essentials[] = {
            CORE_BLD_GRANARY,  CORE_BLD_SAWMILL, CORE_BLD_QUARRY,     CORE_BLD_CROPLAND,
            CORE_BLD_OUTPOST,  CORE_BLD_STALL,   CORE_BLD_LUMBER,     CORE_BLD_KILN,
            CORE_BLD_MINE,     CORE_BLD_SMELTER, CORE_BLD_MARKET,     CORE_BLD_LIBRARY,
            CORE_BLD_BARRACKS, CORE_BLD_TOWER,   CORE_BLD_MAGE_TOWER, CORE_BLD_CROSSBOW_TOWER,
            CORE_BLD_HEALER, CORE_BLD_BATHHOUSE, CORE_BLD_WAREHOUSE};
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
    if (bot.summary.housing_capacity >= bot.summary.population && count_type(&bot, CORE_BLD_GARDEN) >= 2) {
        (void)fortify(&bot, 0);
        for (int ring = 1; ring < 4; ring++)
            if (bot.summary.rank >= ring && bot.summary.protected_area >= (int[]){0, 25, 120, 300}[ring])
                (void)fortify(&bot, ring);
    }
    {
        int required = (int[]){35, 38, 40, 42, 44, 46}[bot.summary.rank], tax = bot.summary.tax;
        if (bot.summary.average_happiness < required && bot.summary.resources[CORE_RES_GOLD] > 120)
            tax = 1;
        else if (bot.summary.resources[CORE_RES_GOLD] < 80 && tax == 1)
            tax = 2;
        else if (bot.summary.average_happiness >= required + 10)
            tax = 3;
        else if (tax == 1 && bot.summary.average_happiness >= required + 5)
            tax = 2;
        else if (tax == 3 && bot.summary.average_happiness < required + 2)
            tax = 2;
        (void)core_expansion_set_tax(world, tax);
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
        int32_t sale = 0, why = 0;
        if (bot.summary.resources[CORE_RES_HERBS] > 25)
            (void)core_trade(world, CORE_RES_HERBS, -10, &sale, &why);
        else if (bot.summary.resources[CORE_RES_FOOD] > bot.summary.population * 10 + 100)
            (void)core_trade(world, CORE_RES_FOOD, -10, &sale, &why);
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
    free(bot.terrain);
    free(bot.roads);
    return rc;
}
