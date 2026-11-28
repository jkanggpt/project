/* expansion_raids.c — 고정 능력치 28종·계절 편성·고유 보상과 동적 소환 */
#include "expansion.h"
#include "castle_test_tools.h"
#include "mem.h"
#include <stdlib.h>
#include <string.h>
static const EnemyDef RAIDERS[28] = {
    {"outlaw", 45, 5, 20, 1350, 1, CORE_BEHAVIOR_MONSTER, -1, 0, 0},
    {"plunderer", 50, 5, 30, 1100, 4, CORE_BEHAVIOR_MONSTER, -1, 0, 0},
    {"captain", 110, 10, 20, 1150, 1, CORE_BEHAVIOR_MONSTER, -1, 0, 0},
    {"bandit_chief", 235, 17, 30, 1120, 1, CORE_BEHAVIOR_MONSTER, -1, 0, 0},
    {"goblin", 38, 5, 20, 1300, 1, CORE_BEHAVIOR_MONSTER, -1, 0, 0},
    {"hobgoblin", 90, 8, 20, 1000, 1, CORE_BEHAVIOR_MONSTER, -1, 0, 0},
    {"goblin_shaman", 70, 8, 30, 900, 4, CORE_BEHAVIOR_MONSTER, -1, 0, 0},
    {"orc", 170, 15, 30, 850, 1, CORE_BEHAVIOR_MONSTER, -1, 0, 0},
    {"ogre", 300, 23, 40, 650, 1, CORE_BEHAVIOR_MONSTER, -1, 0, 0},
    {"skeleton", 65, 7, 20, 950, 1, CORE_BEHAVIOR_MONSTER, -1, 0, 0},
    {"skeleton_archer", 48, 6, 30, 900, 4, CORE_BEHAVIOR_MONSTER, -1, 0, 0},
    {"ghoul", 95, 7, 20, 1400, 1, CORE_BEHAVIOR_MONSTER, -1, 0, 0},
    {"wraith", 120, 10, 30, 1150, 2, CORE_BEHAVIOR_MONSTER, -1, 0, 0},
    {"necromancer", 150, 12, 30, 750, 4, CORE_BEHAVIOR_MONSTER, -1, 0, 0},
    {"leshy", 70, 6, 30, 950, 1, CORE_BEHAVIOR_MONSTER, -1, 0, 0},
    {"lizardman", 90, 9, 20, 1150, 1, CORE_BEHAVIOR_MONSTER, -1, 0, 0},
    {"druid", 105, 9, 30, 920, 4, CORE_BEHAVIOR_MONSTER, -1, 0, 0},
    {"drake", 185, 17, 30, 1100, 1, CORE_BEHAVIOR_MONSTER, -1, 0, 0},
    {"treant", 390, 27, 40, 520, 1, CORE_BEHAVIOR_MONSTER, -1, 0, 0},
    {"northern_raider", 75, 9, 20, 1120, 1, CORE_BEHAVIOR_MONSTER, -1, 0, 0},
    {"ice_shield", 145, 8, 30, 750, 1, CORE_BEHAVIOR_MONSTER, -1, 0, 0},
    {"frost_shaman", 100, 10, 30, 850, 4, CORE_BEHAVIOR_MONSTER, -1, 0, 0},
    {"yeti", 300, 24, 40, 800, 1, CORE_BEHAVIOR_MONSTER, -1, 0, 0},
    {"believer", 55, 6, 20, 1080, 1, CORE_BEHAVIOR_MONSTER, -1, 0, 0},
    {"martyr", 65, 4, 20, 1220, 1, CORE_BEHAVIOR_MONSTER, -1, 0, 0},
    {"flame_priest", 85, 10, 30, 900, 4, CORE_BEHAVIOR_MONSTER, -1, 0, 0},
    {"paladin", 190, 16, 30, 900, 1, CORE_BEHAVIOR_MONSTER, -1, 0, 0},
    {"inquisitor", 245, 18, 30, 950, 4, CORE_BEHAVIOR_MONSTER, -1, 0, 0},
};
static const CoreRaiderInfo RAIDER_INFO[28] = {
    {0, 5, 1, 950, 1450, 0, u8"무법자"},          {0, 6, 1, 1000, 1500, 0, u8"약탈자"},
    {0, 13, 48, 1150, 1650, 0, u8"행동대장"},     {0, 27, 108, 1300, 1850, 1, u8"도적 두목"},
    {1, 4, 1, 850, 1100, 0, u8"고블린"},          {1, 9, 32, 1100, 1550, 0, u8"홉고블린"},
    {1, 13, 44, 950, 1450, 0, u8"고블린 주술사"}, {1, 17, 60, 1650, 2000, 0, u8"오크"},
    {1, 28, 108, 2500, 2700, 2, u8"오우거"},      {2, 6, 1, 950, 1550, 0, u8"해골 병사"},
    {2, 7, 1, 950, 1550, 0, u8"해골 궁수"},       {2, 11, 34, 1200, 1450, 0, u8"구울"},
    {2, 17, 72, 1050, 1900, 3, u8"망령"},         {2, 27, 105, 1150, 1750, 4, u8"네크로맨서"},
    {3, 6, 1, 1250, 1200, 0, u8"레쉬"},           {3, 9, 1, 1250, 1550, 0, u8"리자드맨"},
    {3, 17, 48, 1100, 1650, 5, u8"드루이드"},     {3, 20, 70, 2350, 1650, 6, u8"드레이크"},
    {3, 32, 120, 2600, 3000, 2, u8"트렌트"},      {4, 7, 1, 1100, 1600, 0, u8"약탈전사"},
    {4, 13, 32, 1250, 1650, 0, u8"얼음방패병"},   {4, 18, 56, 1100, 1700, 7, u8"냉기 주술사"},
    {4, 30, 105, 2350, 2550, 8, u8"예티"},        {5, 5, 1, 1000, 1500, 0, u8"신자"},
    {5, 8, 1, 1000, 1550, 9, u8"순교자"},         {5, 17, 50, 1150, 1650, 0, u8"불꽃 사제"},
    {5, 22, 76, 1350, 1850, 0, u8"성기사"},       {5, 34, 120, 1300, 1900, 10, u8"이단 심문관"},
};
void expansion_raider_definitions(Defs *defs) { memcpy(defs->enemies, RAIDERS, sizeof(RAIDERS)); }
CORE_API int32_t core_expansion_raider_info(int32_t type, CoreRaiderInfo *out) {
    if (!out)
        return CORE_ERR_NULL_ARG;
    if (type < 0 || type >= 28)
        return CORE_ERR_INVALID_ARG;
    *out = RAIDER_INFO[type];
    return CORE_OK;
}
static int reserve(Raid *r, int needed) {
    if (needed <= r->enemy_capacity)
        return 1;
    int cap = r->enemy_capacity ? r->enemy_capacity : 32;
    while (cap < needed) {
        if (cap > INT32_MAX / 2 / (int)sizeof(Enemy))
            return 0;
        cap *= 2;
    }
    Enemy *p = mem_realloc(r->enemies, (size_t)cap * sizeof(*p));
    if (!p)
        return 0;
    r->enemies = p;
    r->enemy_capacity = cap;
    return 1;
}
CORE_API int32_t core_expansion_summon(CoreWorld *w, int32_t parent_id, int32_t *id) {
    if (!w || !id)
        return CORE_ERR_NULL_ARG;
    if (!w->expansion || !w->threat.raid.id)
        return CORE_ERR_REJECTED;
    Raid *r = &w->threat.raid;
    if (parent_id < 1 || parent_id > r->enemy_count || r->enemies[parent_id - 1].state != CORE_ENEMY_ALIVE ||
        r->enemies[parent_id - 1].type != 13)
        return CORE_ERR_REJECTED;
    if (!reserve(r, r->enemy_count + 1)) {
        w->expansion->deferred_summons++;
        return CORE_ERR_OUT_OF_MEMORY;
    }
    *id = ++r->enemy_count;
    r->enemies[*id - 1] = (Enemy){*id, 9, CORE_ENEMY_ALIVE, parent_id, 0, {0}};
    w->expansion->summons_total++;
    return CORE_OK;
}
CORE_API int32_t core_expansion_slow(CoreWorld *w, int32_t id) {
    if (!w)
        return CORE_ERR_NULL_ARG;
    Villager *v = entity_find_villager(w, id);
    if (!v)
        return CORE_ERR_NOT_FOUND;
    if (!rng_chance_permille(&w->rng, 250))
        return CORE_IGNORED;
    v->slow_until_tick = w->clock.tick + 3;
    return CORE_OK;
}
static int elite(int type) {
    return type == 3 || type == 8 || type == 13 || type == 18 || type == 22 || type == 27;
}
void expansion_raid_discover(World *w) {
    Raid *r = &w->threat.raid;
    if (!r->id || r->first_discovered || r->phase == CORE_RAID_RETREAT)
        return;
    r->first_discovered = 1;
    r->phase = CORE_RAID_ASSAULT;
    r->started_tick = w->clock.tick;
    r->assault_end_tick = w->clock.tick + 288;
    w->expansion->alarm = 1;
    events_push_raid(&w->events, CORE_EVT_RAID_STARTED, r->id, r->type, r->initial_count, r->direction);
    for (int i = 0; i < w->villager_count; i++)
        expansion_mood_event(w, w->villagers[i], 15, -3, r->id, 1000000000, 0);
}
CORE_API int32_t core_expansion_engaged(CoreWorld *w, int32_t id) {
    if (!w)
        return CORE_ERR_NULL_ARG;
    Raid *r = &w->threat.raid;
    if (!w->expansion || !r->id || id < 1 || id > r->enemy_count ||
        r->enemies[id - 1].state != CORE_ENEMY_ALIVE)
        return CORE_ERR_REJECTED;
    if (r->phase == CORE_RAID_RETREAT)
        return CORE_IGNORED;
    expansion_raid_discover(w);
    return CORE_OK;
}
void expansion_raids_tick(World *w) {
    ExpansionState *e = w->expansion;
    Raid *r = &w->threat.raid;
    if (r->id) {
        if (!r->initial_count)
            r->initial_count = r->enemy_count;
        /* 이전 v10의 미발견 습격도 접근 단계로 정규화합니다. */
        if (!r->first_discovered && r->phase != CORE_RAID_RETREAT) {
            r->phase = CORE_RAID_APPROACH;
            r->assault_end_tick = 0;
        }
        if (r->first_discovered && r->phase != CORE_RAID_RETREAT) {
            r->phase = CORE_RAID_ASSAULT;
            if (!r->assault_end_tick || r->assault_end_tick > r->started_tick + 288)
                r->assault_end_tick = r->started_tick + 288;
            if (w->clock.tick >= r->assault_end_tick)
                r->phase = CORE_RAID_RETREAT;
        }
        return;
    }
    if (w->test_flags & CORE_TEST_NO_RAIDS)
        return;
    if (!e->next_raid_tick) {
        int first = (int[]){28, 18, 14}[w->difficulty];
        e->next_raid_tick = (first - 1) * 144 + rng_range(&w->rng, 0, 3) * 144 + rng_range(&w->rng, 36, 138);
    }
    if (w->clock.tick < e->next_raid_tick)
        return;
    if (guide_holds_raids(w)) {
        /* 튜토리얼 방어 단계 전에는 습격을 하루씩 미룹니다 (난수를 쓰지 않아 일반 게임과 결과가 같음) */
        e->next_raid_tick = w->clock.tick + 144;
        return;
    }
    int day = exp_day(w), n = day > 18 ? day - 18 : 0;
    int budget = exp_clamp(
        (int)(((240000LL + 2500LL * n + 18LL * n * n) * (int[]){80, 100, 125}[w->difficulty] + 500000) /
              1000000),
        22, 150);
    int weights[6] = {100, 100, 100, 100, 100, 100}, season = exp_season(w);
    if (season == 0) {
        weights[3] = 110;
        weights[1] = 105;
    }
    if (season == 1) {
        weights[0] = 110;
        weights[5] = 105;
    }
    if (season == 2) {
        weights[2] = 110;
        weights[3] = 105;
    }
    if (season == 3) {
        weights[4] = 115;
        weights[2] = 105;
    }
    int sum = 0;
    for (int i = 0; i < 6; i++)
        sum += weights[i];
    int roll = rng_range(&w->rng, 1, sum), faction = 0;
    while (faction < 5 && (roll -= weights[faction]) > 0)
        faction++;
    if (!reserve(r, 24)) {
        e->error = CORE_ERR_OUT_OF_MEMORY;
        return;
    }
    r->enemy_count = 0;
    int used_elite = 0, ranged = 0, limit = w->threat.raids_done < 2 ? rng_range(&w->rng, 3, 5) : 24;
    for (int i = 0; i < limit; i++) {
        int choices[28], count = 0, minimum = 1000;
        for (int t = 0; t < 28; t++)
            if (RAIDER_INFO[t].faction == faction && RAIDER_INFO[t].unlock == 1 &&
                RAIDER_INFO[t].cost < minimum)
                minimum = RAIDER_INFO[t].cost;
        for (int t = 0; t < 28; t++)
            if (RAIDER_INFO[t].faction == faction && RAIDER_INFO[t].unlock <= day &&
                RAIDER_INFO[t].cost <= budget && !(used_elite && elite(t)) &&
                !(w->threat.raids_done < 2 && RAIDER_INFO[t].unlock != 1) &&
                !(w->threat.raids_done < 2 && i < 2 && RAIDER_INFO[t].cost > budget - minimum * (2 - i)) &&
                !(w->threat.raids_done >= 2 && RAIDERS[t].range > 1 && ranged * 5 >= ((i + 1) * 2)))
                choices[count++] = t;
        if (!count)
            break;
        int t = choices[rng_range(&w->rng, 0, count - 1)];
        r->enemies[r->enemy_count++] = (Enemy){i + 1, t, CORE_ENEMY_ALIVE, 0, 0, {0}};
        budget -= RAIDER_INFO[t].cost;
        used_elite |= elite(t);
        ranged += RAIDERS[t].range > 1;
    }
    if (!r->enemy_count)
        return;
    int x = 0, y = 0;
    if (!expansion_edge(w, rng_range(&w->rng, 0, w->grid.width - 1),
                        rng_range(&w->rng, 0, w->grid.height - 1), &x, &y))
        return;
    /* 선택한 외곽 방향을 따라 영지에 도달 가능한 25~40칸 지점을 우선합니다. */
    int ex = x - w->keep_x, ey = y - w->keep_y, length = abs(ex) + abs(ey), best = INT32_MAX, bx = x, by = y;
    int tx = w->keep_x + (length ? ex * 32 / length : 32), ty = w->keep_y + (length ? ey * 32 / length : 0);
    Building *keep = world_keep(w);
    for (int p = 0; p < e->cells; p++) {
        int xx = p % w->grid.width, yy = p / w->grid.width, d = expansion_center_distance(keep, xx, yy);
        if (d < 25 || e->protected_cells[p] || w->grid.occupant[p] || !w->grid.cost[p] ||
            !w->grid.scratch_mark[p])
            continue;
        int score = exp_dist(xx, yy, tx, ty) + (d > 40 ? 1000 + d : 0) + (e->visible[p] ? 50 : 0);
        if (score < best) {
            best = score;
            bx = xx;
            by = yy;
        }
    }
    /* 실제 지도 가장자리 진입점을 유지합니다. 가까운 지점으로 순간 생성하지 않습니다. */
    (void)bx;
    (void)by;
    r->id = w->threat.next_raid_id++;
    r->type = 1 + faction;
    r->direction = rng_range(&w->rng, 0, 3);
    r->spawn_x = x;
    r->spawn_y = y;
    r->started_tick = w->clock.tick;
    r->killed = r->escaped = r->stolen = r->fallen = 0;
    r->phase = CORE_RAID_APPROACH;
    r->assault_end_tick = 0;
    r->initial_count = r->enemy_count;
    r->first_discovered = 0;
    memset(r->initial_types, 0, sizeof(r->initial_types));
    for (int i = 0; i < r->enemy_count; i++)
        r->initial_types[r->enemies[i].type]++;
    int interval = rng_range(&w->rng, (int[]){15, 12, 10}[w->difficulty], (int[]){22, 18, 15}[w->difficulty]);
    e->next_raid_tick = (day + interval - 1) * 144 + rng_range(&w->rng, 36, 138);
}
static void drop(World *w, int resource, int lo, int hi, int chance) {
    if (rng_range(&w->rng, 1, 100) > chance)
        return;
    int n = rng_range(&w->rng, lo, hi);
    (void)expansion_bank_add(w, resource, n * 1000, CORE_LEDGER_RAID);
    w->expansion->raid_drops[resource] += n;
}
void expansion_raider_drops(World *w, int type) {
    switch (type) {
    case 0:
        drop(w, CORE_RES_GOLD, 2, 4, 100);
        break;
    case 1:
        drop(w, CORE_RES_GOLD, 3, 5, 100);
        drop(w, CORE_RES_LEATHER, 1, 1, 30);
        break;
    case 2:
        drop(w, CORE_RES_GOLD, 7, 11, 100);
        drop(w, CORE_RES_IRON, 1, 1, 25);
        break;
    case 3:
        drop(w, CORE_RES_GOLD, 15, 22, 100);
        drop(w, CORE_RES_IRON, 2, 2, 40);
        break;
    case 4:
        drop(w, CORE_RES_GOLD, 1, 3, 100);
        break;
    case 5:
        drop(w, CORE_RES_GOLD, 2, 4, 100);
        drop(w, CORE_RES_IRON_ORE, 1, 1, 40);
        break;
    case 6:
        drop(w, CORE_RES_HERBS, 1, 2, 100);
        drop(w, CORE_RES_MANA, 1, 1, 20);
        break;
    case 7:
        drop(w, CORE_RES_GOLD, 4, 7, 100);
        drop(w, CORE_RES_IRON_ORE, 2, 2, 40);
        break;
    case 8:
        drop(w, CORE_RES_FOOD, 5, 9, 100);
        drop(w, CORE_RES_LEATHER, 3, 3, 100);
        drop(w, CORE_RES_GOLD, 2, 5, 100);
        break;
    case 9:
        drop(w, CORE_RES_IRON_ORE, 1, 1, 35);
        break;
    case 10:
        drop(w, CORE_RES_LOGS, 1, 1, 30);
        drop(w, CORE_RES_IRON_ORE, 1, 1, 30);
        break;
    case 11:
        drop(w, CORE_RES_HERBS, 1, 1, 50);
        break;
    case 12:
        drop(w, CORE_RES_MANA, 1, 1, 25);
        break;
    case 13:
        drop(w, CORE_RES_GOLD, 4, 7, 100);
        drop(w, CORE_RES_MANA, 1, 1, 40);
        break;
    case 14:
        drop(w, CORE_RES_LOGS, 2, 4, 100);
        drop(w, CORE_RES_HERBS, 1, 1, 30);
        break;
    case 15:
        drop(w, CORE_RES_LEATHER, 2, 2, 100);
        drop(w, CORE_RES_FOOD, 2, 2, 100);
        break;
    case 16:
        drop(w, CORE_RES_HERBS, 2, 2, 100);
        drop(w, CORE_RES_MANA, 1, 1, 35);
        break;
    case 17:
        drop(w, CORE_RES_FOOD, 5, 5, 100);
        drop(w, CORE_RES_LEATHER, 4, 4, 100);
        drop(w, CORE_RES_MANA, 1, 1, 10);
        break;
    case 18:
        drop(w, CORE_RES_LOGS, 12, 18, 100);
        drop(w, CORE_RES_HERBS, 2, 2, 100);
        drop(w, CORE_RES_MANA, 1, 1, 30);
        break;
    case 19:
        drop(w, CORE_RES_GOLD, 2, 5, 100);
        drop(w, CORE_RES_LEATHER, 1, 1, 100);
        break;
    case 20:
        drop(w, CORE_RES_GOLD, 3, 5, 100);
        drop(w, CORE_RES_IRON, 1, 1, 25);
        break;
    case 21:
        drop(w, CORE_RES_HERBS, 2, 2, 100);
        drop(w, CORE_RES_MANA, 1, 1, 35);
        break;
    case 22:
        drop(w, CORE_RES_FOOD, 8, 8, 100);
        drop(w, CORE_RES_LEATHER, 6, 6, 100);
        drop(w, CORE_RES_MANA, 1, 1, 15);
        break;
    case 23:
        drop(w, CORE_RES_GOLD, 1, 3, 100);
        break;
    case 24:
        drop(w, CORE_RES_GOLD, 2, 4, 100);
        drop(w, CORE_RES_COAL, 1, 1, 30);
        break;
    case 25:
        drop(w, CORE_RES_GOLD, 4, 7, 100);
        drop(w, CORE_RES_MANA, 1, 1, 35);
        break;
    case 26:
        drop(w, CORE_RES_GOLD, 9, 13, 100);
        drop(w, CORE_RES_IRON, 1, 2, 100);
        break;
    case 27:
        drop(w, CORE_RES_GOLD, 12, 18, 100);
        drop(w, CORE_RES_MANA, 1, 1, 50);
        break;
    default:
        break;
    }
}
