/*
 * villager_ai.c — 주민의 하루 (설명은 villager_ai.h)
 */
#include "villager_ai.h"
#include "expansion.h"

#include <stdlib.h>

#include "difficulty.h"
#include "jobs.h"
#include "nature.h"
#include "population.h"
#include "threat.h"

/* 할 일 계획: 어떤 상태로 어디에 갈지 */
typedef struct Plan {
    int32_t state;
    Building *target;
    int32_t x;
    int32_t y;
    int32_t flags;
} Plan;

/* 광장(성채 문 앞 아래쪽)에서 주민마다 조금씩 다른 자리 */
static const int32_t SQUARE_DX[8] = {0, -1, 1, -2, 0, 2, -1, 1};
static const int32_t SQUARE_DY[8] = {1, 2, 2, 2, 3, 3, 4, 4};

static int32_t clamp100(int32_t value)
{
    return value < 0 ? 0 : (value > 100 ? 100 : value);
}

int32_t vil_walk_speed(const World *world, const Villager *villager)
{
    int32_t speed = VIL_BASE_WALK_MILLI * (90 + villager->stats.vitality * 2) / 100;
    if (villager->needs.hunger == 0 || villager->sick_ticks > 0) {
        speed = speed * 70 / 100; /* 굶거나 아프면 느려짐 */
    }
    if (world->expansion && villager->slow_until_tick > world->clock.tick) speed = speed * 80 / 100;
    return speed * nature_walk_percent(world) / 100; /* 눈·폭풍 */
}

void vil_set_intent(Villager *villager, int32_t state, Building *target, int32_t x, int32_t y, int32_t flags)
{
    int same_place = villager->target == target && villager->target_x == x && villager->target_y == y;
    if (state == villager->state && same_place && flags == villager->intent_flags) {
        return; /* 바뀐 것이 없으면 rev 도 그대로 */
    }
    villager->state = state;
    villager->target = target;
    villager->target_x = x;
    villager->target_y = y;
    villager->intent_flags = flags;
    villager->intent_rev++;
    villager->state_ticks = 0;
    if (flags & CORE_INTENT_HOLD) {
        villager->at_target = 1; /* 제자리에 있으라는 뜻이므로 이미 도착한 것 */
    } else if (!same_place) {
        villager->at_target = 0;
    }
}

/* 칸이 주민이 서 있을 수 있는 곳인가 (걸을 수 있고 건물이 없음) */
static int walkable(const World *world, int32_t x, int32_t y)
{
    const Grid *grid = &world->grid;
    int32_t index;
    uint8_t cost;
    if (!grid_in_bounds(grid, x, y)) {
        return 0;
    }
    index = grid_index(grid, x, y);
    cost = grid->cost[index];
    return cost != CORE_COST_BLOCKED && cost != CORE_COST_WALL;
}

void vil_edge_tile(const World *world, int32_t near_x, int32_t near_y, int32_t *out_x, int32_t *out_y)
{
    const Grid *grid = &world->grid;
    int32_t best_distance = -1;
    int32_t i;
    const Building *keep = world_keep(world);
    *out_x = keep != NULL ? keep->door_x : world->keep_x;
    *out_y = keep != NULL ? keep->door_y : world->keep_y;
    for (i = 0; i < grid->width * 2 + grid->height * 2; i++) {
        int32_t x;
        int32_t y;
        int32_t distance;
        if (i < grid->width) {
            x = i;
            y = 0;
        } else if (i < grid->width * 2) {
            x = i - grid->width;
            y = grid->height - 1;
        } else if (i < grid->width * 2 + grid->height) {
            x = 0;
            y = i - grid->width * 2;
        } else {
            x = grid->width - 1;
            y = i - grid->width * 2 - grid->height;
        }
        if (!walkable(world, x, y)) {
            continue;
        }
        distance = abs(x - near_x) + abs(y - near_y);
        if (best_distance < 0 || distance < best_distance) {
            best_distance = distance;
            *out_x = x;
            *out_y = y;
        }
    }
}

/* 광장의 이 주민 자리 */
static void square_tile(const World *world, const Villager *villager, int32_t *out_x, int32_t *out_y)
{
    const Building *keep = world_keep(world);
    int32_t base_x = keep != NULL ? keep->door_x : world->keep_x;
    int32_t base_y = keep != NULL ? keep->door_y : world->keep_y;
    int32_t i;
    for (i = 0; i < 8; i++) {
        int32_t k = (villager->id + i) % 8;
        int32_t x = base_x + SQUARE_DX[k];
        int32_t y = base_y + SQUARE_DY[k] - 1;
        if (walkable(world, x, y)) {
            *out_x = x;
            *out_y = y;
            return;
        }
    }
    *out_x = base_x;
    *out_y = base_y;
}

/* 주민 위치에서 가장 가까운 다 지은 저장소(성채, 창고) */
static Building *nearest_storage(const World *world, const Villager *villager)
{
    Building *best = NULL;
    int32_t best_distance = 0;
    int32_t i;
    for (i = 0; i < world->building_count; i++) {
        Building *building = world->buildings[i];
        int32_t distance;
        if (building->state != CORE_BSTATE_ACTIVE ||
            (building->type != CORE_BLD_KEEP && building->type != CORE_BLD_WAREHOUSE) || building->door_x < 0) {
            continue;
        }
        distance = abs(building->door_x - villager->loc_x) + abs(building->door_y - villager->loc_y);
        if (best == NULL || distance < best_distance) {
            best = building;
            best_distance = distance;
        }
    }
    return best;
}

static void plan_at(Plan *plan, int32_t state, Building *target, int32_t x, int32_t y, int32_t flags)
{
    plan->state = state;
    plan->target = target;
    plan->x = x;
    plan->y = y;
    plan->flags = flags;
}

static int is_work_time(const World *world)
{
    int32_t minute = clock_minute_of_day(&world->clock);
    return minute >= WORLD_WORK_START_MIN && minute < WORLD_WORK_END_MIN;
}

/* 아픈 주민: 치료소가 열려 있으면 그리 가고, 아니면 밤엔 자고 낮엔 집(없으면 광장)에서 쉽니다 */
static int decide_sick(const World *world, const Villager *villager, Plan *plan)
{
    Building *healer;
    int32_t x;
    int32_t y;
    if (villager->sick_ticks <= 0) {
        return 0;
    }
    healer = is_work_time(world) ? nature_open_healer(world, villager) : NULL;
    if (healer != NULL) {
        plan_at(plan, CORE_VS_SEEK_HEALING, healer, healer->door_x, healer->door_y, 0);
        return 1;
    }
    if (clock_is_night(&world->clock)) {
        return 0; /* 잠은 보통 때와 같음 */
    }
    if (villager->home != NULL && villager->home->state == CORE_BSTATE_ACTIVE) {
        plan_at(plan, CORE_VS_IDLE, villager->home, villager->home->door_x, villager->home->door_y, 0);
    } else {
        square_tile(world, villager, &x, &y);
        plan_at(plan, CORE_VS_IDLE, NULL, x, y, 0);
    }
    return 1;
}

/* 습격 중: 경비병(병영 일꾼)은 병영 문 앞으로 나서고, 나머지는 모두 성채로 피신합니다 */
static int decide_raid(const World *world, const Villager *villager, Plan *plan)
{
    Building *keep;
    if (!threat_raid_active(world)) {
        return 0;
    }
    if (threat_is_guard(world, villager) && villager->workplace->door_x >= 0) {
        plan_at(plan, CORE_VS_GUARD_POST, villager->workplace, villager->workplace->door_x,
                villager->workplace->door_y, 0);
        return 1;
    }
    keep = world_keep(world);
    if (keep == NULL) {
        return 0;
    }
    plan_at(plan, CORE_VS_FLEE, keep, keep->door_x, keep->door_y, 0);
    return 1;
}

/* 지금 무엇을 해야 하는지 고릅니다 (우선순위: 떠남 > 기다림 > 습격 > 식사 > 병 > 잠 > 일 > 쉼) */
static void decide(const World *world, const Villager *villager, Plan *plan)
{
    Building *workplace = villager->workplace;
    int32_t x;
    int32_t y;

    if (villager->state == CORE_VS_LEAVING) {
        plan_at(plan, CORE_VS_LEAVING, NULL, villager->target_x, villager->target_y, villager->intent_flags);
        return;
    }
    if (villager->hold_ticks > 0) {
        plan_at(plan, CORE_VS_IDLE, NULL, villager->loc_x, villager->loc_y, CORE_INTENT_HOLD);
        return;
    }
    if (decide_raid(world, villager, plan)) {
        return;
    }
    if (villager->needs.hunger < VIL_HUNGRY_BELOW && world->bank.amount[CORE_RES_FOOD] > 0) {
        Building *storage = nearest_storage(world, villager);
        if (storage != NULL) {
            plan_at(plan, CORE_VS_EAT, storage, storage->door_x, storage->door_y, 0);
            return;
        }
    }
    if (decide_sick(world, villager, plan)) {
        return;
    }
    if (clock_is_night(&world->clock)) {
        if (villager->home != NULL && villager->home->state == CORE_BSTATE_ACTIVE) {
            plan_at(plan, CORE_VS_SLEEP, villager->home, villager->home->door_x, villager->home->door_y, 0);
        } else {
            square_tile(world, villager, &x, &y);
            plan_at(plan, CORE_VS_SLEEP, NULL, x, y, 0);
        }
        return;
    }
    if (is_work_time(world) && workplace != NULL && workplace->state == CORE_BSTATE_ACTIVE &&
        workplace->door_x >= 0) {
        int arrived = villager->target == workplace && villager->at_target &&
                      (villager->state == CORE_VS_GO_WORK || villager->state == CORE_VS_WORKING);
        plan_at(plan, arrived ? CORE_VS_WORKING : CORE_VS_GO_WORK, workplace, workplace->door_x, workplace->door_y, 0);
        return;
    }
    square_tile(world, villager, &x, &y);
    plan_at(plan, CORE_VS_IDLE, NULL, x, y, 0);
}

/* ---- 상태별 처리 함수 (STATE_TABLE) ------------------------------------ */

typedef void (*StateHandler)(World *world, Villager *villager);

static void on_idle(World *world, Villager *villager)
{
    (void)world;
    (void)villager; /* 광장에서 쉬는 것 자체가 할 일 */
}

static void on_sleep(World *world, Villager *villager)
{
    if (!villager->at_target) {
        return;
    }
    /* 집에서 자면 빨리, 광장에서 자면 천천히 회복 */
    if (villager->home != NULL && villager->target == villager->home) {
        villager->needs.rest = clamp100(villager->needs.rest + 2);
    } else if (clock_minute_of_day(&world->clock) % 20 == 0) {
        villager->needs.rest = clamp100(villager->needs.rest + 2);
    }
}

static void on_go_work(World *world, Villager *villager)
{
    (void)world;
    (void)villager; /* 걷는 중: C 그래픽 프런트엔드 이 움직이고 도착을 알려 줌 */
}

static void on_working(World *world, Villager *villager)
{
    if (villager->at_target) {
        jobs_work(world, villager);
    }
}

/* 저장소 문 앞에 있으면 식량 하나로 한 끼. (도착 보고 때가 아니라 여기서 먹습니다:
 * 성채로 피신했다가 바로 그 자리에서 배가 고파지면 "새로 도착"하는 일이 없기 때문) */
static void on_eat(World *world, Villager *villager)
{
    if (villager->at_target &&
        eco_change(&world->bank, &world->ledger, CORE_RES_FOOD, -1, CORE_LEDGER_CONSUMPTION) < 0) {
        villager->needs.hunger = clamp100(villager->needs.hunger + VIL_MEAL_HUNGER);
    }
}

/* 치료소에 지금 일하는 일꾼(치료사)이 있는가 */
static int healer_on_duty(const Building *healer)
{
    int32_t i;
    for (i = 0; i < healer->worker_count; i++) {
        const Villager *worker = healer->workers[i];
        if (worker->state == CORE_VS_WORKING && worker->at_target) {
            return 1;
        }
    }
    return 0;
}

/* 치료소 문 앞에 도착한 환자: 치료사가 일하는 중이면 한 시간 뒤 약초 하나로 낫습니다 */
static void on_seek_healing(World *world, Villager *villager)
{
    Building *healer = villager->target;
    if (!villager->at_target || healer == NULL || healer->type != CORE_BLD_HEALER || !healer_on_duty(healer) ||
        world->bank.amount[CORE_RES_HERBS] <= 0) {
        return;
    }
    if (++villager->heal_ticks >= NATURE_HEAL_TICKS) {
        (void)eco_change(&world->bank, &world->ledger, CORE_RES_HERBS, -1, CORE_LEDGER_HEALING);
        nature_cure(world, villager, 1);
    }
}

/* 성채 안에 숨어 있음 (안전 욕구는 update_needs 에서). 성채는 저장소이므로 배가 고프면 그 안에서 먹습니다.
 * (습격이 하루 내내 이어져도 피신한 주민이 굶지 않게) */
static void on_flee(World *world, Villager *villager)
{
    if (villager->needs.hunger < VIL_HUNGRY_BELOW) {
        on_eat(world, villager);
    }
}

static void on_guard_post(World *world, Villager *villager)
{
    on_idle(world, villager); /* 싸움은 C 그래픽 프런트엔드(CLI 는 숫자 전투)이 하고, 피해는 보고로 들어옴 */
}

static void on_leaving(World *world, Villager *villager)
{
    (void)world;
    (void)villager; /* 가장자리에 도착하면 vil_arrived 가 목록에서 지움 */
}

static const StateHandler STATE_TABLE[CORE_VS_COUNT] = {
    on_idle,         /* CORE_VS_IDLE */
    on_sleep,        /* CORE_VS_SLEEP */
    on_go_work,      /* CORE_VS_GO_WORK */
    on_working,      /* CORE_VS_WORKING */
    on_eat,          /* CORE_VS_EAT */
    on_seek_healing, /* CORE_VS_SEEK_HEALING */
    on_flee,         /* CORE_VS_FLEE */
    on_guard_post,   /* CORE_VS_GUARD_POST */
    on_leaving,      /* CORE_VS_LEAVING */
};

/* ---- 욕구와 기분 ------------------------------------------------------ */

static int32_t trait_sum_food(const World *world, const Villager *villager)
{
    int32_t total = 0;
    int32_t i;
    for (i = 0; i < 2; i++) {
        int32_t trait = villager->traits[i];
        if (trait >= 0 && trait < world->defs.trait_count) {
            total += world->defs.traits[trait].food_pct;
        }
    }
    return total;
}

static void update_needs(World *world, Villager *villager)
{
    int sleeping = villager->state == CORE_VS_SLEEP && villager->at_target;
    /* 배고픔: 한 틱에 약 2/3 씩 줄고(자는 동안은 절반), 대식가는 더 빨리 줄어듭니다 */
    villager->hunger_acc += 667 * (100 + trait_sum_food(world, villager)) / 100 / (sleeping ? 2 : 1);
    while (villager->hunger_acc >= 1000) {
        villager->hunger_acc -= 1000;
        if (villager->needs.hunger > 0) {
            villager->needs.hunger--;
        }
    }
    /* 피로: 깨어 있으면 줄어듭니다 (자는 동안의 회복은 on_sleep) */
    if (!sleeping) {
        villager->rest_acc += 400;
        while (villager->rest_acc >= 1000) {
            villager->rest_acc -= 1000;
            if (villager->needs.rest > 0) {
                villager->needs.rest--;
            }
        }
    }
    /* 안전: 습격 중 성채에 숨지 못한 주민은 빨리 떨어지고, 습격이 없으면 서서히 회복 */
    if (threat_raid_active(world)) {
        int sheltered = villager->state == CORE_VS_FLEE && villager->at_target;
        if (!sheltered) {
            villager->needs.safety = clamp100(villager->needs.safety - THREAT_UNSHELTERED_SAFETY);
        }
    } else if (villager->needs.safety < 100) {
        villager->needs.safety++;
    }
    villager->needs.housing = villager->home != NULL ? 100 : 25;

    /* 굶주림: 배고픔이 0 이면 여섯 틱(한 시간)마다 건강이 1 줄고(약 4일이면 위험), 먹으면 한 시간에 1 씩 회복.
     * 건강이 절반(약 이틀)까지 떨어지면 죽기 전에 마을을 떠납니다 (vil_tick) */
    if (villager->needs.hunger == 0) {
        if (world->clock.tick % 6 == 0) {
            villager->health--;
        }
    } else if (villager->health < 100 && villager->sick_ticks <= 0 && clock_minute_of_day(&world->clock) % 60 == 0) {
        villager->health++;
    }
    nature_sick_tick(world, villager);
}

int32_t vil_mood_terms(const World *world, const Villager *villager, int32_t terms[CORE_MOOD_TERM_COUNT])
{
    int32_t total = 0;
    int32_t i;

    for (i = 0; i < CORE_MOOD_TERM_COUNT; i++) {
        terms[i] = 0;
    }
    terms[CORE_MOOD_BASE] = difficulty_preset(world->difficulty)->base_mood;
    terms[CORE_MOOD_HUNGER] = (villager->needs.hunger - 50) / 4 - (villager->needs.hunger == 0 ? 10 : 0);
    terms[CORE_MOOD_REST] = (villager->needs.rest - 50) / 5;
    terms[CORE_MOOD_SAFETY] = (villager->needs.safety - 50) / 5;
    terms[CORE_MOOD_HOUSING] = villager->home != NULL ? 5 : -12;
    for (i = 0; i < 2; i++) {
        int32_t trait = villager->traits[i];
        if (trait >= 0 && trait < world->defs.trait_count) {
            terms[CORE_MOOD_TRAITS] += world->defs.traits[trait].mood;
        }
    }
    if (villager->workplace != NULL) {
        terms[CORE_MOOD_WAGES] = (villager->flags & CORE_VF_UNPAID) ? -15 : 5;
    }
    terms[CORE_MOOD_MOURNING] = -world->mourning;
    if (villager->home != NULL && (villager->home->flags & CORE_BF_COLD)) {
        terms[CORE_MOOD_COLD] = -15; /* 난방을 못 한 집 */
    } else if (villager->home == NULL && clock_season(&world->clock) == CORE_SEASON_WINTER) {
        terms[CORE_MOOD_COLD] = -10; /* 겨울에 한데서 잠 */
    }
    if (villager->sick_ticks > 0) {
        terms[CORE_MOOD_SICKNESS] = -10;
    }
    for (i = 0; i < CORE_MOOD_TERM_COUNT; i++) {
        total += terms[i];
    }
    return clamp100(total);
}

/* 한 시간마다: 기분이 목표 쪽으로 2 씩 움직이고, 오래 우울하면 떠납니다 */
static void hourly(World *world, Villager *villager)
{
    int32_t terms[CORE_MOOD_TERM_COUNT];
    int32_t target = vil_mood_terms(world, villager, terms);
    if (villager->mood < target) {
        villager->mood += (target - villager->mood) < 2 ? (target - villager->mood) : 2;
    } else if (villager->mood > target) {
        villager->mood -= (villager->mood - target) < 2 ? (villager->mood - target) : 2;
    }
    if (villager->mood < VIL_UNHAPPY_BELOW) {
        villager->low_mood_ticks += 6;
    } else {
        villager->low_mood_ticks = 0;
    }
    if (villager->low_mood_ticks >= VIL_LEAVE_AFTER_TICKS && villager->state != CORE_VS_LEAVING) {
        population_begin_leaving(world, villager);
    }
}

static void refresh_flags(Villager *villager)
{
    villager->flags &= ~(CORE_VF_STARVING | CORE_VF_HOMELESS | CORE_VF_UNHAPPY);
    if (villager->needs.hunger == 0) {
        villager->flags |= CORE_VF_STARVING;
    }
    if (villager->home == NULL) {
        villager->flags |= CORE_VF_HOMELESS;
    }
    if (villager->mood < VIL_UNHAPPY_BELOW) {
        villager->flags |= CORE_VF_UNHAPPY;
    }
}

/* 길이 막혔을 때: 잠시 그 자리에서 기다리게 합니다. 떠나려던 주민은 그냥 떠난 것으로 칩니다 */
static void fall_back(World *world, Villager *villager)
{
    int32_t day = clock_day(&world->clock);
    if (villager->state == CORE_VS_LEAVING) {
        population_remove_departed(world, villager);
        return;
    }
    villager->hold_ticks = VIL_HOLD_TICKS;
    vil_set_intent(villager, CORE_VS_IDLE, NULL, villager->loc_x, villager->loc_y, CORE_INTENT_HOLD);
    if (villager->blocked_day != day) {
        villager->blocked_day = day;
        events_push_villager(&world->events, CORE_EVT_PATH_BLOCKED, villager->id, 0, 0, villager->name);
    }
}

void vil_tick(World *world, Villager *villager)
{
    Plan plan;

    update_needs(world, villager);
    if (villager->health <= 0) {
        population_kill(world, villager, villager->sick_ticks > 0 ? CORE_DEATH_ILLNESS : CORE_DEATH_STARVATION);
        return;
    }
    if (villager->needs.hunger == 0 && villager->health <= VIL_STARVING_LEAVE_HEALTH &&
        villager->state != CORE_VS_LEAVING) {
        population_begin_leaving(world, villager);
        return;
    }
    if (clock_minute_of_day(&world->clock) % 60 == 0) {
        hourly(world, villager);
        if (villager->state == CORE_VS_LEAVING) {
            return; /* 방금 떠나기로 함: 의도는 population_begin_leaving 이 정함 */
        }
    }
    refresh_flags(villager);
    if (villager->hold_ticks > 0) {
        villager->hold_ticks--;
    }
    if (!villager->at_target && ++villager->state_ticks > VIL_WALK_TIMEOUT_TICKS) {
        fall_back(world, villager); /* 하루 내내 도착하지 못함: 길이 없는 것으로 봄 */
        return;
    }
    decide(world, villager, &plan);
    vil_set_intent(villager, plan.state, plan.target, plan.x, plan.y,
                   plan.flags | (villager->intent_flags & CORE_INTENT_SPAWN_EDGE));
    STATE_TABLE[villager->state](world, villager);
}

int vil_arrived(World *world, Villager *villager, int32_t intent_rev)
{
    if (intent_rev != villager->intent_rev) {
        return CORE_IGNORED; /* 걷는 사이에 의도가 바뀜: 오래된 보고 */
    }
    villager->at_target = 1;
    villager->loc_x = villager->target_x;
    villager->loc_y = villager->target_y;
    villager->intent_flags &= ~CORE_INTENT_SPAWN_EDGE;
    if (world->expansion && villager->incoming && villager->target == world_keep(world))
        expansion_arrival_join(world, villager);
    if(world->expansion && villager->target && villager->state!=CORE_VS_GUARD_POST && !(expansion_spec(villager->target->type)->flags&CORE_EXP_OUTDOOR))villager->inside_building=villager->target->id;
    switch (villager->state) {
    case CORE_VS_LEAVING:
        if(world->expansion && villager->contract_end_tick>world->clock.tick)
            villager->intent_flags|=CORE_INTENT_HOLD;
        else
            population_remove_departed(world, villager);
        break;
    default:
        break;
    }
    return CORE_OK;
}

int vil_unreachable(World *world, Villager *villager, int32_t intent_rev)
{
    if (intent_rev != villager->intent_rev) {
        return CORE_IGNORED;
    }
    if (world->expansion) {
        villager->hub_revision = -1;
        villager->hub_id = 0;
    }
    fall_back(world, villager);
    return CORE_OK;
}
