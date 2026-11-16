/*
 * jobs.c — 일자리 (설명은 jobs.h)
 */
#include "jobs.h"
#include "expansion.h"

#include <stdlib.h>
#include <string.h>

#include "nature.h"
#include "research.h"

#define STORY_FIRST_PRODUCE_BIT 1
#define STORY_MOVED_IN_BIT 2

int32_t jobs_relevant_stat(const Villager *villager, int32_t building_type)
{
    switch (building_type) {
    case CORE_BLD_FARM:
    case CORE_BLD_LUMBER:
    case CORE_BLD_QUARRY:
    case CORE_BLD_BARRACKS:
        return villager->stats.strength;
    case CORE_BLD_GATHERER:
    case CORE_BLD_HEALER:
        return villager->stats.dexterity;
    case CORE_BLD_MANA_MINE:
    case CORE_BLD_MAGE_TOWER:
    case CORE_BLD_MARKET:
        return villager->stats.intellect;
    default:
        return villager->stats.vitality;
    }
}

int32_t jobs_work_percent(const World *world, const Villager *villager, int32_t building_type)
{
    int32_t trait_bonus = 0;
    int32_t percent;
    int32_t i;
    for (i = 0; i < 2; i++) {
        int32_t trait = villager->traits[i];
        if (trait >= 0 && trait < world->defs.trait_count) {
            trait_bonus += world->defs.traits[trait].work_pct;
        }
    }
    /* 기분 0 → 50%, 기분 50 → 100%, 기분 100 → 150% */
    percent = 50 + villager->mood;
    percent = percent * (100 + trait_bonus) / 100;
    /* 능력치 1 → 84%, 10 → 120% */
    percent = percent * (80 + 4 * jobs_relevant_stat(villager, building_type)) / 100;
    return percent > 0 ? percent : 0;
}

/* 생산한 자원 한 단위를 창고에 넣습니다. 가득 찼으면 버리고 하루에 한 번 알립니다 */
static void deliver(World *world, Building *building, Villager *villager, int32_t resource)
{
    int32_t day = clock_day(&world->clock);
    if (eco_change(&world->bank, &world->ledger, resource, 1, CORE_LEDGER_PRODUCTION) > 0) {
        events_push_produced(&world->events, building->id, resource, 1);
        if (!(villager->story_flags & STORY_FIRST_PRODUCE_BIT)) {
            villager->story_flags |= STORY_FIRST_PRODUCE_BIT;
            events_push_villager(&world->events, CORE_EVT_STORY, villager->id, CORE_STORY_FIRST_PRODUCE, resource,
                                 villager->name);
        }
    } else if (world->storage_full_day[resource] != day) {
        world->storage_full_day[resource] = day;
        events_push_count(&world->events, CORE_EVT_STORAGE_FULL, resource);
    }
}

/* 일꾼 한 명의 한 틱 몫을 건물의 누적값에 더하고, 1000 이 찰 때마다 한 단위씩 내놓습니다 */
static void produce(World *world, Building *building, Villager *villager, int32_t slot, int32_t resource,
                    int32_t rate, int32_t extra_percent)
{
    int64_t amount;
    if (resource < 0 || rate <= 0) {
        return;
    }
    amount = (int64_t)rate * jobs_work_percent(world, villager, building->type) / 100;
    amount = amount * building->productivity / 100;
    amount = amount * extra_percent / 100;
    building->work_milli[slot] += (int32_t)amount;
    while (building->work_milli[slot] >= 1000) {
        building->work_milli[slot] -= 1000;
        deliver(world, building, villager, resource);
    }
}

/* ---- 직업별 작업 함수 -------------------------------------------------- */

typedef void (*JobWorkFn)(World *world, Building *building, Villager *villager);

/* 농장: 계절·날씨·자연 사건을 탑니다 (겨울에는 0, 가뭄 −50%, 풍년 +50% — nature.c) */
static void work_farm(World *world, Building *building, Villager *villager)
{
    const BuildingDef *def = world_building_def(world, building->type);
    produce(world, building, villager, 0, def->produces, def->rate, nature_farm_percent(world));
}

/* 채집 오두막: 약초와 식량을 함께 */
static void work_gatherer(World *world, Building *building, Villager *villager)
{
    const BuildingDef *def = world_building_def(world, building->type);
    produce(world, building, villager, 0, def->produces, def->rate, 100);
    produce(world, building, villager, 1, def->secondary, def->secondary_rate, 100);
}

/* 벌목장, 채석장, 마정석 광산: 지형 생산성에 비례 (생산성은 produce 안에서 곱함) */
static void work_extract(World *world, Building *building, Villager *villager)
{
    const BuildingDef *def = world_building_def(world, building->type);
    produce(world, building, villager, 0, def->produces, def->rate, 100);
}

/* 마탑: 자원이 아니라 연구 점수를 모읍니다 (research.c) */
static void work_research(World *world, Building *building, Villager *villager)
{
    research_work(world, building, villager);
}

/* 건물 종류 → 작업 함수. NULL 이면 일꾼이 있어도 매 틱 만드는 것이 없음
 * (시장 일꾼은 거래를 열어 두고, 치료소 일꾼은 찾아온 환자를 고치고, 병영 일꾼은 습격 때 경비병이 됩니다) */
static const JobWorkFn JOB_TABLE[CORE_BLD_COUNT] = {
    NULL,          /* 성채 */
    NULL,          /* 주택 */
    work_farm,     /* 농장 */
    work_gatherer, /* 채집 오두막 */
    work_extract,  /* 벌목장 */
    work_extract,  /* 채석장 */
    work_extract,  /* 마정석 광산 */
    NULL,          /* 창고 */
    NULL,          /* 시장 */
    NULL,          /* 병영 */
    NULL,          /* 감시탑 */
    NULL,          /* 성벽 */
    NULL,          /* 성문 */
    NULL,          /* 치료소 */
    work_research, /* 마탑 */
    NULL,          /* 도로 */
};

void jobs_work(World *world, Villager *villager)
{
    Building *building = villager->workplace;
    JobWorkFn work;
    if (building == NULL || building->state != CORE_BSTATE_ACTIVE) {
        return;
    }
    work = JOB_TABLE[building->type];
    if (work != NULL) {
        work(world, building, villager);
    }
}

/* ---- 배치 ------------------------------------------------------------ */

static int32_t manhattan(int32_t ax, int32_t ay, int32_t bx, int32_t by)
{
    return abs(ax - bx) + abs(ay - by);
}

/* 주민이 사는 곳 (집 문 앞, 없으면 성채 문 앞) */
static void home_spot(const World *world, const Villager *villager, int32_t *x, int32_t *y)
{
    const Building *keep = world_keep(world);
    if (villager->home != NULL) {
        *x = villager->home->door_x;
        *y = villager->home->door_y;
    } else if (keep != NULL) {
        *x = keep->door_x;
        *y = keep->door_y;
    } else {
        *x = world->keep_x;
        *y = world->keep_y;
    }
}

static int available_for_work(const Villager *villager)
{
    return villager->workplace == NULL && !(villager->flags & CORE_VF_MANUAL) && villager->state != CORE_VS_LEAVING;
}

/* 이 건물에 가장 알맞은 놀고 있는 주민 (능력치가 높고 집이 가까울수록 좋음) */
static Villager *best_candidate(const World *world, const Building *building)
{
    Villager *best = NULL;
    int32_t best_score = 0;
    int32_t i;
    for (i = 0; i < world->villager_count; i++) {
        Villager *villager = world->villagers[i];
        int32_t hx;
        int32_t hy;
        int32_t score;
        if (!available_for_work(villager)) {
            continue;
        }
        home_spot(world, villager, &hx, &hy);
        score = jobs_relevant_stat(villager, building->type) * 10 -
                manhattan(hx, hy, building->door_x, building->door_y) / 2;
        if (best == NULL || score > best_score || (score == best_score && villager->id < best->id)) {
            best = villager;
            best_score = score;
        }
    }
    return best;
}

/* 건물 하나에 일꾼을 limit 명까지 채웁니다. 놀고 있는 주민이 없으면 0 */
static int fill_up_to(World *world, Building *building, int32_t limit)
{
    const BuildingDef *def = world_building_def(world, building->type);
    if (limit > def->worker_slots) {
        limit = def->worker_slots;
    }
    while (building->worker_count < limit) {
        Villager *villager = best_candidate(world, building);
        if (villager == NULL || entity_link_worker(building, villager, def->worker_slots) != CORE_OK) {
            return 0;
        }
    }
    return 1;
}

/*
 * 놀고 있는 주민을 일자리에 보냅니다. 두 번 돕니다:
 *   1) 아무도 일하지 않는 "종류"의 첫 건물에 한 명씩 (새로 지은 마정석 광산·마탑·시장이 오래된 일터에 밀려
 *      빈 채로 남지 않게. 같은 종류가 이미 돌아가고 있으면 건너뜀 → 농장 여럿이 반쯤 비는 일은 없음)
 *   2) 남은 자리를 지은 순서대로 채움
 */
void jobs_auto_assign(World *world)
{
    if(world->expansion){expansion_assign(world);world->expansion->dirty=1;return;}
    int staffed[CORE_BLD_COUNT];
    int32_t i;
    memset(staffed, 0, sizeof(staffed));
    for (i = 0; i < world->building_count; i++) {
        const Building *building = world->buildings[i];
        if (building->state == CORE_BSTATE_ACTIVE && building->worker_count > 0) {
            staffed[building->type] = 1;
        }
    }
    for (i = 0; i < world->building_count; i++) {
        Building *building = world->buildings[i];
        if (building->state != CORE_BSTATE_ACTIVE || !building->auto_assign || staffed[building->type] ||
            world_building_def(world, building->type)->worker_slots == 0) {
            continue;
        }
        if (!fill_up_to(world, building, 1)) {
            return; /* 놀고 있는 주민이 더 없음 */
        }
        staffed[building->type] = 1;
    }
    for (i = 0; i < world->building_count; i++) {
        Building *building = world->buildings[i];
        if (building->state != CORE_BSTATE_ACTIVE || !building->auto_assign) {
            continue;
        }
        if (!fill_up_to(world, building, INT32_MAX)) {
            return;
        }
    }
}

void jobs_assign_housing(World *world)
{
    int32_t i;
    for (i = 0; i < world->villager_count; i++) {
        Villager *villager = world->villagers[i];
        Building *best = NULL;
        int32_t best_distance = 0;
        int32_t wx;
        int32_t wy;
        int32_t j;
        if (villager->home != NULL || villager->state == CORE_VS_LEAVING) {
            continue;
        }
        /* 일터에서 가까운 집 (일터가 없으면 성채에서 가까운 집) */
        if (villager->workplace != NULL) {
            wx = villager->workplace->door_x;
            wy = villager->workplace->door_y;
        } else {
            home_spot(world, villager, &wx, &wy);
        }
        for (j = 0; j < world->building_count; j++) {
            Building *house = world->buildings[j];
            const BuildingDef *def = world_building_def(world, house->type);
            int32_t distance;
            if (house->state != CORE_BSTATE_ACTIVE || house->resident_count >= def->resident_slots) {
                continue;
            }
            distance = manhattan(wx, wy, house->door_x, house->door_y);
            if (best == NULL || distance < best_distance) {
                best = house;
                best_distance = distance;
            }
        }
        if (best != NULL &&
            entity_link_resident(best, villager, world_building_def(world, best->type)->resident_slots) == CORE_OK &&
            !(villager->story_flags & STORY_MOVED_IN_BIT)) {
            villager->story_flags |= STORY_MOVED_IN_BIT;
            events_push_villager(&world->events, CORE_EVT_STORY, villager->id, CORE_STORY_MOVED_IN, best->id,
                                 villager->name);
        }
    }
}

void jobs_on_buildings_changed(World *world)
{
    if(world->expansion){expansion_assign(world);world->expansion->dirty=1;return;}
    jobs_assign_housing(world);
    jobs_auto_assign(world);
}

int jobs_assign_manual(World *world, Villager *villager, Building *building)
{
    const BuildingDef *def = world_building_def(world, building->type);
    int rc;
    if (building->state != CORE_BSTATE_ACTIVE || def->worker_slots <= 0 || villager->state == CORE_VS_LEAVING) {
        return CORE_ERR_REJECTED;
    }
    if (villager->workplace == building) {
        villager->flags |= CORE_VF_MANUAL;
        return CORE_OK;
    }
    rc = entity_link_worker(building, villager, def->worker_slots);
    if (rc == CORE_OK) {
        villager->flags |= CORE_VF_MANUAL;
        if(world->expansion)world->expansion->dirty=1;
        events_push_villager(&world->events, CORE_EVT_STORY, villager->id, CORE_STORY_NEW_JOB, building->type,
                             villager->name);
    }
    return rc;
}

void jobs_unassign_manual(World *world, Villager *villager)
{
    if(world->expansion)world->expansion->dirty=1;
    (void)world;
    entity_unlink_worker(villager);
    villager->flags |= CORE_VF_MANUAL; /* 자동 배치가 다시 넣지 않도록 */
}

void jobs_release_manual(World *world, Villager *villager)
{
    villager->flags &= ~CORE_VF_MANUAL;
    jobs_auto_assign(world);
}

void jobs_pay_wages_and_taxes(World *world)
{
    int32_t unpaid = 0;
    int32_t i;
    /* 세금을 먼저 걷어야 그 돈으로 임금을 줄 수 있습니다 */
    for (i = 0; i < world->villager_count; i++) {
        if (world->villagers[i]->home != NULL) {
            (void)eco_change(&world->bank, &world->ledger, CORE_RES_GOLD, JOBS_TAX_PER_HOUSED, CORE_LEDGER_TAXES);
        }
    }
    for (i = 0; i < world->villager_count; i++) {
        Villager *villager = world->villagers[i];
        int32_t wage;
        if (villager->workplace == NULL || villager->workplace->state != CORE_BSTATE_ACTIVE) {
            villager->flags &= ~CORE_VF_UNPAID;
            continue;
        }
        wage = world_building_def(world, villager->workplace->type)->wage;
        if (world->bank.amount[CORE_RES_GOLD] >= wage) {
            (void)eco_change(&world->bank, &world->ledger, CORE_RES_GOLD, -wage, CORE_LEDGER_WAGES);
            villager->flags &= ~CORE_VF_UNPAID;
        } else {
            villager->flags |= CORE_VF_UNPAID;
            unpaid++;
        }
    }
    if (unpaid > 0) {
        events_push_count(&world->events, CORE_EVT_WAGES_UNPAID, unpaid);
    }
}
