/*
 * format.c — CLI 용 한국어 문구 (설명은 format.h)
 */
#include "format.h"
#include "castle_expansion.h"

#include <stdio.h>
#include <string.h>

static const char *const SEASON_NAMES[CORE_SEASON_COUNT] = {u8"봄", u8"여름", u8"가을", u8"겨울"};
static const char *const DIFFICULTY_NAMES[CORE_DIFF_COUNT] = {u8"평화", u8"보통", u8"혹독"};
static const char *const RESOURCE_NAMES[CORE_RES_COUNT] = {
    u8"금화", u8"마정석", u8"식량", u8"목재", u8"돌",     u8"약초", u8"원목", u8"점토",
    u8"벽돌", u8"철광석", u8"석탄", u8"철괴", u8"금광석", u8"금괴", u8"가죽"};
static const char *const TERRAIN_NAMES[CORE_TERRAIN_COUNT] = {u8"풀밭", u8"숲", u8"바위", u8"물", u8"마정석 광맥"};
static const char *const WEATHER_NAMES[CORE_WEATHER_COUNT] = {u8"맑음", u8"비", u8"눈", u8"폭풍"};
static const char *const NATURE_NAMES[CORE_NATURE_COUNT] = {u8"없음", u8"가뭄", u8"역병", u8"폭풍", u8"혹한", u8"풍년",u8"광산 붕괴",u8"교역 호황"};
static const char *const DENY_REASONS[CORE_DENY_COUNT] = {
    "",
    u8"다 지은 시장이 없습니다",
    u8"시장에 일꾼이 없습니다",
    u8"시장에서 다루지 않는 자원입니다",
    u8"금화가 모자랍니다",
    u8"팔 물건이 모자랍니다",
    u8"창고에 자리가 없습니다",
    u8"상인이 없습니다",
    u8"이미 한 거래입니다",
    u8"고칠 곳이 없습니다",
    u8"아직 짓는 중입니다",
    u8"수량이 범위 밖입니다",
    u8"수리 비용이 모자랍니다",
    u8"습격 중입니다",
    u8"이미 가장 높은 등급입니다",
    u8"승급 조건(인구·행복도·방어·연구)이 모자랍니다",
    u8"마정석이 모자랍니다",
    u8"이미 다른 연구를 하는 중입니다",
    u8"이미 끝낸 연구입니다",
    u8"앞선 연구나 등급이 모자랍니다",
    u8"게임이 끝났습니다",
};
static const char *const THREAT_NAMES[CORE_THREAT_COUNT] = {u8"없음", u8"도적", u8"마물", u8"적대 영주 군대"};
static const char *const DIRECTION_NAMES[CORE_DIR_COUNT] = {u8"북", u8"동", u8"남", u8"서"};
static const char *const LEDGER_NAMES[CORE_LEDGER_COUNT] = {u8"생산", u8"식사", u8"임금", u8"세금", u8"건설", u8"철거 환불", u8"거래",
                                                            u8"난방", u8"약탈", u8"기타", u8"치료", u8"수리", u8"공물", u8"연구"};
static const char *const RANK_NAMES[CORE_RANK_COUNT] = {u8"촌락", u8"마을", u8"요새", u8"성", u8"왕성",u8"왕성"};
static const char *const BRANCH_NAMES[CORE_BRANCH_COUNT] = {u8"경제", u8"방어", u8"마법"};
static const char *const RESEARCH_STATES[] = {u8"잠김", u8"시작할 수 있음", u8"연구 중", u8"끝남"};
static const char *const TUTORIAL_NAMES[CORE_TUTORIAL_COUNT] = {u8"농장 짓기", u8"주택 짓기", u8"주민을 일터에 직접 배치하기",
                                                                u8"성벽 짓기", u8"첫 도적 습격 버티기"};
static const char *const LOSE_REASONS[] = {"", u8"성채가 무너졌습니다", u8"주민이 한 명도 남지 않았습니다"};

/* 연구 키 → 한국어 (정의 파일에서 새 연구를 더하면 키가 그대로 보입니다) */
static const char *const RESEARCH_KEYS[] = {"crop_rotation", "better_tools",  "trade_guild",  "granary",
                                            "sharp_arrows",  "stone_walls",   "drill",        "keep_ballista",
                                            "mana_sense",    "herbal_magic",  "warm_hearth",  "ward"};
static const char *const RESEARCH_NAMES[] = {u8"윤작", u8"좋은 연장", u8"상인 조합", u8"큰 곳간", u8"날카로운 화살", u8"돌 성벽",
                                             u8"경비 훈련", u8"성채 쇠뇌", u8"마력 감지", u8"약초 마법", u8"따뜻한 불씨", u8"마법 결계"};
/* 효과 설명: 앞 글, 부호, 단위 (예: "농장 생산" "+" "%") */
static const char *const EFFECT_TEXTS[CORE_EFFECT_COUNT][3] = {
    {u8"농장 생산", "+", "%"},          {u8"벌목장·채석장 생산", "+", "%"}, {u8"저장 한도", "+", "%"},
    {u8"시장의 사고팔 때 차이", "-", "%p"}, {u8"감시탑 화살 피해", "+", ""},   {u8"성벽·성문 체력", "+", "%"},
    {u8"경비병 피해", "+", ""},          {u8"성채 망루 피해", "+", ""},       {u8"마정석 광산 생산", "+", "%"},
    {u8"앓는 시간", "-", "%"},           {u8"집 한 채의 난방 목재", "-", ""}, {u8"습격 때 건물 피해", "-", "%"}};

static const char *const BUILDING_NAMES[CORE_BLD_COUNT] = {
    u8"영주관", u8"주택", u8"농장",   u8"채집장", u8"벌목장", u8"채석장", u8"마정석 광산", u8"창고",
    u8"시장",   u8"병영", u8"감시탑", u8"성벽",        u8"성문",   u8"치료소", u8"마탑",        u8"도로"};

static const char *const PLACE_RESULTS[CORE_PLACE_COUNT] = {
    u8"지을 수 있습니다", u8"지을 수 없는 종류입니다", u8"지도 밖입니다", u8"지도 가장자리에 너무 가깝습니다",
    u8"물이나 바위 위에는 지을 수 없습니다", u8"다른 건물이 있습니다", u8"도로가 있습니다 (먼저 철거하세요)",
    u8"다른 건물의 문 앞을 막습니다", u8"문 앞 자리가 막혀 있습니다", u8"근처에 숲이 부족합니다",
    u8"바로 옆에 바위 지대가 있어야 합니다", u8"마정석 광맥 위에 지어야 합니다", u8"등급이 모자랍니다",
    u8"자원이 모자랍니다", u8"이미 있습니다", u8"다른 건물이나 지도 밖으로 가는 길을 막습니다",
    u8"탐험한 땅과 성채 입구의 연결 도로를 확인하세요", u8"수변 시설에 필요한 물이 없습니다",u8"점유할 모든 칸이 현재 시야 안에 있어야 합니다",u8"기존 가판대·시장·교역소를 먼저 철거하세요",u8"습격 중 건설 불가 · 적 철수 완료 후 재개"};

static const char *const VILLAGER_STATES[CORE_VS_COUNT] = {u8"쉬는 중", u8"자는 중",   u8"일하러 가는 중",
                                                            u8"일하는 중", u8"먹으러 감", u8"치료받으러 감",
                                                            u8"피신 중",   u8"경비 중",   u8"떠나는 중"};

/* 특성 키 → 한국어 (정의 파일에서 새 특성을 더하면 키가 그대로 보입니다) */
static const char *const TRAIT_KEYS[] = {"diligent", "lazy",     "glutton",  "coward", "brave",
                                         "mana_affinity", "optimist", "grumbler", "strong", "frugal"};
static const char *const TRAIT_NAMES[] = {u8"근면함", u8"게으름", u8"대식가", u8"겁쟁이", u8"용감함",
                                          u8"마력 친화", u8"낙천가", u8"투덜이", u8"장사", u8"검소함"};

const char *fmt_josa(const char *word, const char *with_final, const char *without_final)
{
    size_t length = strlen(word);
    size_t start;
    uint32_t code;
    if (length < 3) {
        return without_final;
    }
    /* 마지막 글자의 시작 바이트를 찾습니다 (UTF-8 이어지는 바이트는 10xxxxxx) */
    start = length - 1;
    while (start > 0 && ((unsigned char)word[start] & 0xC0) == 0x80) {
        start--;
    }
    if (length - start != 3) {
        return without_final; /* 한글(3바이트)이 아님 */
    }
    code = ((uint32_t)((unsigned char)word[start] & 0x0F) << 12) |
           ((uint32_t)((unsigned char)word[start + 1] & 0x3F) << 6) | ((unsigned char)word[start + 2] & 0x3F);
    if (code < 0xAC00 || code > 0xD7A3) {
        return without_final;
    }
    /* 한글 음절 = 0xAC00 + (초성×21 + 중성)×28 + 종성. 종성이 0 이면 받침 없음 */
    return (code - 0xAC00) % 28 != 0 ? with_final : without_final;
}

const char *fmt_building_name(int32_t type)
{
    return type>=CORE_LEGACY_BLD_COUNT?core_expansion_building_name(type):((type>=0 && type<CORE_BLD_COUNT)?BUILDING_NAMES[type]:"?");
}

const char *fmt_place_result(int32_t result)
{
    return (result >= 0 && result < CORE_PLACE_COUNT) ? PLACE_RESULTS[result] : "?";
}

const char *fmt_villager_state(int32_t state)
{
    return (state >= 0 && state < CORE_VS_COUNT) ? VILLAGER_STATES[state] : "?";
}

const char *fmt_trait_name(const char *key)
{
    size_t i;
    for (i = 0; i < sizeof(TRAIT_KEYS) / sizeof(TRAIT_KEYS[0]); i++) {
        if (strcmp(TRAIT_KEYS[i], key) == 0) {
            return TRAIT_NAMES[i];
        }
    }
    return key;
}

const char *fmt_weather_name(int32_t weather)
{
    return (weather >= 0 && weather < CORE_WEATHER_COUNT) ? WEATHER_NAMES[weather] : "?";
}

const char *fmt_nature_name(int32_t event)
{
    return (event >= 0 && event < CORE_NATURE_COUNT) ? NATURE_NAMES[event] : "?";
}

const char *fmt_deny_reason(int32_t reason)
{
    return (reason >= 0 && reason < CORE_DENY_COUNT) ? DENY_REASONS[reason] : "?";
}

const char *fmt_ledger_name(int32_t category)
{
    return (category >= 0 && category < CORE_LEDGER_COUNT) ? LEDGER_NAMES[category] : "?";
}

const char *fmt_rank_name(int32_t rank)
{
    return (rank >= 0 && rank < CORE_RANK_COUNT) ? RANK_NAMES[rank] : "?";
}

const char *fmt_branch_name(int32_t branch)
{
    return (branch >= 0 && branch < CORE_BRANCH_COUNT) ? BRANCH_NAMES[branch] : "?";
}

const char *fmt_research_state(int32_t state)
{
    return (state >= 0 && state < (int32_t)(sizeof(RESEARCH_STATES) / sizeof(RESEARCH_STATES[0])))
               ? RESEARCH_STATES[state]
               : "?";
}

const char *fmt_research_name(const char *key)
{
    size_t i;
    for (i = 0; i < sizeof(RESEARCH_KEYS) / sizeof(RESEARCH_KEYS[0]); i++) {
        if (strcmp(RESEARCH_KEYS[i], key) == 0) {
            return RESEARCH_NAMES[i];
        }
    }
    return key;
}

void fmt_effect(int32_t effect, int32_t value, char *out, size_t size)
{
    if (effect < 0 || effect >= CORE_EFFECT_COUNT) {
        snprintf(out, size, "?");
        return;
    }
    snprintf(out, size, "%s %s%d%s", EFFECT_TEXTS[effect][0], EFFECT_TEXTS[effect][1], (int)value,
             EFFECT_TEXTS[effect][2]);
}

const char *fmt_tutorial_name(int32_t step)
{
    return (step >= 0 && step < CORE_TUTORIAL_COUNT) ? TUTORIAL_NAMES[step] : "?";
}

const char *fmt_lose_reason(int32_t reason)
{
    return (reason > 0 && reason < (int32_t)(sizeof(LOSE_REASONS) / sizeof(LOSE_REASONS[0]))) ? LOSE_REASONS[reason]
                                                                                              : "?";
}

const char *fmt_threat_name(int32_t type)
{
    return (type >= 0 && type < CORE_THREAT_COUNT) ? THREAT_NAMES[type] : "?";
}

const char *fmt_direction_name(int32_t direction)
{
    return (direction >= 0 && direction < CORE_DIR_COUNT) ? DIRECTION_NAMES[direction] : "?";
}

const char *fmt_season_name(int32_t season)
{
    return (season >= 0 && season < CORE_SEASON_COUNT) ? SEASON_NAMES[season] : "?";
}

const char *fmt_difficulty_name(int32_t difficulty)
{
    return (difficulty >= 0 && difficulty < CORE_DIFF_COUNT) ? DIFFICULTY_NAMES[difficulty] : "?";
}

const char *fmt_resource_name(int32_t resource)
{
    return (resource >= 0 && resource < CORE_RES_COUNT) ? RESOURCE_NAMES[resource] : "?";
}

const char *fmt_terrain_name(int32_t terrain)
{
    return (terrain >= 0 && terrain < CORE_TERRAIN_COUNT) ? TERRAIN_NAMES[terrain] : "?";
}

const char *fmt_result_message(int32_t code)
{
    switch (code) {
    case CORE_OK:
        return u8"성공했습니다.";
    case CORE_ERR_NULL_ARG:
    case CORE_ERR_INVALID_ARG:
        return u8"잘못된 값입니다.";
    case CORE_ERR_OUT_OF_MEMORY:
        return u8"메모리가 부족합니다.";
    case CORE_ERR_FILE_OPEN:
        return u8"파일을 열 수 없습니다. 경로를 확인하세요.";
    case CORE_ERR_FILE_IO:
        return u8"파일을 읽거나 쓰는 중에 문제가 생겼습니다.";
    case CORE_ERR_FORMAT:
        return u8"세이브 파일이 손상되었습니다.";
    case CORE_ERR_VERSION:
        return u8"다른 버전의 저장입니다. v1.2.1은 생산·동물·습격 변경으로 저장 v9부터 지원합니다. 원본은 "
               u8"보존됩니다.";
    case CORE_ERR_REJECTED:
        return u8"규칙상 할 수 없습니다.";
    case CORE_ERR_NOT_FOUND:
        return u8"그런 번호가 없습니다.";
    default:
        return u8"알 수 없는 오류입니다.";
    }
}

/* ---- 이벤트 종류별 문장 만들기 (종류 번호 → 함수 표) ---------------------- */

typedef void (*EventFormatter)(const CoreEventView *event, char *out, size_t size);

static void format_unknown(const CoreEventView *event, char *out, size_t size)
{
    snprintf(out, size, u8"(알 수 없는 사건 %d)", (int)event->type);
}

static void format_day_started(const CoreEventView *event, char *out, size_t size)
{
    snprintf(out, size, u8"%d일째가 시작되었습니다.", (int)event->a);
}

static void format_season_changed(const CoreEventView *event, char *out, size_t size)
{
    /* 봄, 여름, 가을, 겨울 모두 받침으로 끝나므로 조사는 늘 '이' */
    snprintf(out, size, u8"%d년 %s이 왔습니다.", (int)event->b, fmt_season_name(event->a));
}

static void format_night_fell(const CoreEventView *event, char *out, size_t size)
{
    (void)event;
    snprintf(out, size, u8"밤이 되었습니다.");
}

static void format_dawn(const CoreEventView *event, char *out, size_t size)
{
    (void)event;
    snprintf(out, size, u8"새벽이 밝아 옵니다.");
}

static void format_warning(const CoreEventView *event, char *out, size_t size)
{
    if (event->a == CORE_WARN_EVENTS_DROPPED) {
        snprintf(out, size, u8"(알림 %d개를 표시하지 못했습니다)", (int)event->b);
    } else {
        snprintf(out, size, u8"(경고 %d)", (int)event->a);
    }
}

static void format_building_event(const CoreEventView *event,char *out,size_t size) {
    const char *name=core_expansion_building_name(event->b);
    const char *verb=event->type==CORE_EVT_BUILDING_PLACED?u8"착공":event->type==CORE_EVT_BUILDING_COMPLETED?u8"완공":event->type==CORE_EVT_BUILDING_DAMAGED?u8"피해":event->type==CORE_EVT_BUILDING_RUINED?u8"파괴":event->type==CORE_EVT_BUILDING_REPAIRED?u8"수리":u8"철거";
    char source[160]={0};
    if(event->cause_kind==1) {CoreRaiderInfo info;if(core_expansion_raider_info(event->source_type-1,&info)==CORE_OK)snprintf(source,sizeof(source),u8"%s의 공격",info.name);else snprintf(source,sizeof(source),u8"습격 적의 공격");}
    else if(event->cause_kind==2){CoreAnimalInfo info;if(core_expansion_animal_info(event->source_type-1,&info)==CORE_OK)snprintf(source,sizeof(source),u8"%s의 반격",info.name);}
    else if(event->cause_kind==3)snprintf(source,sizeof(source),u8"폭풍");
    if(event->type==CORE_EVT_BUILDING_RUINED)snprintf(out,size,u8"%s#%d %s · %s (%d일 %02d:%02d)",name,event->type_serial?event->type_serial:event->a,verb,source[0]?source:u8"원인 확인 불가",event->event_tick/144+1,event->event_tick%144/6,event->event_tick%6*10);
    else snprintf(out,size,u8"%s#%d %s (%d,%d)",name,event->type_serial?event->type_serial:event->a,verb,event->c,event->d);
}

static void format_produced(const CoreEventView *event, char *out, size_t size)
{
    snprintf(out, size, "+%d %s", (int)event->c, fmt_resource_name(event->b));
}

static void format_villager_event(const CoreEventView *event, char *out, size_t size)
{
    const char *ga = fmt_josa(event->text, u8"이", u8"가");
    switch (event->type) {
    case CORE_EVT_IMMIGRANT:
        snprintf(out, size, u8"%s%s 마을에 이주해 왔습니다.", event->text, ga);
        break;
    case CORE_EVT_VILLAGER_LEAVING:
        snprintf(out, size, u8"%s%s 마을을 떠나기로 했습니다.", event->text, ga);
        break;
    case CORE_EVT_VILLAGER_LEFT:
        snprintf(out, size, u8"%s%s 마을을 떠났습니다.", event->text, ga);
        break;
    case CORE_EVT_VILLAGER_DIED:
        if((event->b==CORE_DEATH_RAID || event->b==CORE_DEATH_WILDLIFE) && event->c>0 && (event->cause_kind==1 || event->cause_kind==2)) {
            CoreRaiderInfo enemy;CoreAnimalInfo animal;
            const char *source=u8"공격자";
            if(event->cause_kind==1 && core_expansion_raider_info(event->c-1,&enemy)==CORE_OK)source=enemy.name;
            if(event->cause_kind==2 && core_expansion_animal_info(event->c-1,&animal)==CORE_OK)source=animal.name;
            snprintf(out,size,u8"%s%s %s #%d의 공격으로 사망했습니다. (%d일 %02d:%02d)",
                event->text,ga,source,event->source_id,event->event_tick/144+1,(event->event_tick%144)/6,(event->event_tick%6)*10);
            break;
        }
        snprintf(out, size, u8"%s%s %s세상을 떠났습니다.", event->text, ga,
                 event->b == CORE_DEATH_STARVATION ? u8"굶주림으로 "
                 : event->b == CORE_DEATH_ILLNESS  ? u8"병으로 "
                 : event->b == CORE_DEATH_RAID     ? u8"습격에 쓰러져 "
                                                   : "");
        break;
    case CORE_EVT_PATH_BLOCKED:
        snprintf(out, size, u8"%s: 길이 막혔습니다.", event->text);
        break;
    case CORE_EVT_VILLAGER_SICK:
        snprintf(out, size, u8"%s%s 병에 걸렸습니다.", event->text, ga);
        break;
    case CORE_EVT_VILLAGER_CURED:
        snprintf(out, size, "%s%s %s", event->text, ga, event->b ? u8"치료소에서 나았습니다." : u8"병이 저절로 나았습니다.");
        break;
    default:
        snprintf(out, size, "%s", event->text);
        break;
    }
}

static void format_story(const CoreEventView *event, char *out, size_t size)
{
    const char *ga = fmt_josa(event->text, u8"이", u8"가");
    switch (event->b) {
    case CORE_STORY_FIRST_PRODUCE: {
        const char *resource = fmt_resource_name(event->c);
        snprintf(out, size, u8"%s%s 처음으로 %s%s 거뒀습니다.", event->text, ga, resource, fmt_josa(resource, u8"을", u8"를"));
        break;
    }
    case CORE_STORY_MOVED_IN:
        snprintf(out, size, u8"%s%s 새 집으로 이사했습니다.", event->text, ga);
        break;
    case CORE_STORY_NEW_JOB:
        snprintf(out, size, u8"%s%s %s에서 일하게 되었습니다.", event->text, ga, fmt_building_name(event->c));
        break;
    default:
        snprintf(out, size, "%s", event->text);
        break;
    }
}

static void format_count(const CoreEventView *event, char *out, size_t size)
{
    switch (event->type) {
    case CORE_EVT_STORAGE_FULL:
        snprintf(out, size, u8"%s 창고가 가득 찼습니다.", fmt_resource_name(event->a));
        break;
    case CORE_EVT_WAGES_UNPAID:
        snprintf(out, size, u8"금화가 모자라 %d명이 임금을 받지 못했습니다.", (int)event->a);
        break;
    case CORE_EVT_STARVING:
        snprintf(out, size, u8"주민 %d명이 굶고 있습니다!", (int)event->a);
        break;
    case CORE_EVT_WEATHER_CHANGED:
        snprintf(out, size, u8"날씨: %s", fmt_weather_name(event->a));
        break;
    case CORE_EVT_NATURE_FORECAST: {
        const char *name = fmt_nature_name(event->a);
        snprintf(out, size, u8"예보: 내일 %s%s 옵니다.", name, fmt_josa(name, u8"이", u8"가"));
        break;
    }
    case CORE_EVT_NATURE_ENDED: {
        const char *name = fmt_nature_name(event->a);
        snprintf(out, size, u8"%s%s 끝났습니다.", name, fmt_josa(name, u8"이", u8"가"));
        break;
    }
    case CORE_EVT_MERCHANT_LEFT:
        snprintf(out, size, u8"떠돌이 상인이 떠났습니다.");
        break;
    case CORE_EVT_HOUSES_COLD:
        snprintf(out, size, u8"목재가 모자라 집 %d채가 지난밤 난방을 못 했습니다.", (int)event->a);
        break;
    default:
        snprintf(out, size, "(%d)", (int)event->a);
        break;
    }
}

static void format_pair(const CoreEventView *event, char *out, size_t size)
{
    switch (event->type) {
    case CORE_EVT_NATURE_STARTED: {
        const char *name = fmt_nature_name(event->a);
        snprintf(out, size, u8"%s%s 시작되었습니다 (%d일).", name, fmt_josa(name, u8"이", u8"가"), (int)event->b);
        break;
    }
    case CORE_EVT_MERCHANT_ARRIVED:
        snprintf(out, size, u8"떠돌이 상인이 왔습니다! %d일 머물며 거래 %d개를 제안합니다.", (int)event->a,
                 (int)event->b);
        break;
    case CORE_EVT_WINTER_WARNING:
        snprintf(out, size, u8"겨울 준비가 모자랍니다: 겨울을 나려면 식량 %d, 목재 %d 정도가 있어야 합니다.", (int)event->a, (int)event->b);
        break;
    default:
        snprintf(out, size, "(%d, %d)", (int)event->a, (int)event->b);
        break;
    }
}

static void format_threat(const CoreEventView *event, char *out, size_t size)
{
    switch (event->type) {
    case CORE_EVT_THREAT_SCHEDULED:
        snprintf(out, size, u8"위협 예고: %s %d명이 %s쪽에서 %d일 %d시간 뒤에 옵니다.", fmt_threat_name(event->a),
                 (int)event->d, fmt_direction_name(event->b), (int)(event->c / 144), (int)(event->c % 144 / 6));
        break;
    case CORE_EVT_THREAT_NEAR:
        snprintf(out, size, u8"경보! %s%s %s쪽에서 다가옵니다. 반나절 안에 도착합니다.", fmt_threat_name(event->a),
                 fmt_josa(fmt_threat_name(event->a), u8"이", u8"가"), fmt_direction_name(event->b));
        break;
    case CORE_EVT_RAID_STARTED:
        snprintf(out, size, u8"습격! %s %d명이 %s쪽에 나타났습니다. 주민들이 성채로 피합니다.", fmt_threat_name(event->b),
                 (int)event->c, fmt_direction_name(event->d));
        break;
    case CORE_EVT_RAID_ENDED:
        snprintf(out, size, u8"습격이 끝났습니다: 적 %d/%d명을 쓰러뜨림, 빼앗긴 자원 %d, 쓰러진 주민 %d명.", (int)event->a,
                 (int)event->b, (int)event->c, (int)event->d);
        break;
    case CORE_EVT_ENEMY_KILLED:
        if (event->c > 0) {
            snprintf(out, size, u8"적을 쓰러뜨렸습니다. 마정석 +%d", (int)event->c);
        } else {
            snprintf(out, size, u8"적을 쓰러뜨렸습니다.");
        }
        break;
    case CORE_EVT_LOOT_STOLEN:
        snprintf(out, size, u8"적이 %s %d개를 훔쳐 갔습니다!", fmt_resource_name(event->a), (int)event->b);
        break;
    default:
        snprintf(out, size, "(%d)", (int)event->a);
        break;
    }
}

static void format_progress(const CoreEventView *event, char *out, size_t size)
{
    switch (event->type) {
    case CORE_EVT_RESEARCH_STARTED:
        snprintf(out, size, u8"연구 시작: %s. 마탑의 마법사들이 연구합니다.", fmt_research_name(event->text));
        break;
    case CORE_EVT_RESEARCH_DONE:
        snprintf(out, size, u8"연구 끝: %s! (끝낸 연구 %d개)", fmt_research_name(event->text), (int)event->b);
        break;
    case CORE_EVT_RANK_READY:
        snprintf(out, size, u8"%s%s 될 조건을 채웠습니다. 공물을 바치면 승급합니다.", fmt_rank_name(event->a),
                 fmt_josa(fmt_rank_name(event->a), u8"이", u8"가"));
        break;
    case CORE_EVT_RANK_UP:
        snprintf(out, size, u8"승급! 이제 %s입니다.", fmt_rank_name(event->a));
        break;
    case CORE_EVT_VICTORY:
        snprintf(out, size, u8"승리! %d일째에 왕성이 되었습니다. 계속 다스릴 수 있습니다.", (int)event->a);
        break;
    case CORE_EVT_DEFEAT:
        snprintf(out, size, u8"패배: %s. (%d일째)", fmt_lose_reason(event->a), (int)event->b);
        break;
    case CORE_EVT_TUTORIAL_STEP:
        snprintf(out, size, u8"목표 달성: %s (%d/%d)", fmt_tutorial_name(event->a), (int)event->b, (int)event->c);
        break;
    case CORE_EVT_EXPANSION_NOTICE:
        (void)snprintf(out, size, "%s", event->text);
        break;
    case CORE_EVT_TUTORIAL_DONE:
        snprintf(out, size, u8"처음 목표를 모두 이루었습니다! 보상: 금화 +%d, 마정석 +%d", (int)event->a, (int)event->b);
        break;
    default:
        snprintf(out, size, "(%d)", (int)event->a);
        break;
    }
}

static void format_def_warning(const CoreEventView *event, char *out, size_t size)
{
    static const char *const FILES[CORE_DATA_COUNT] = {"buildings.def", "traits.def",   "names.txt",
                                                       "enemies.def",   "research.def", "ranks.def"};
    const char *file = (event->a >= 0 && event->a < CORE_DATA_COUNT) ? FILES[event->a] : "?";
    snprintf(out, size, u8"정의 파일 %s 의 %d번째 줄이 잘못되어 기본값을 씁니다.", file, (int)event->b);
}

/* 순서는 CoreEventType 과 같아야 합니다 */
static const EventFormatter EVENT_FORMATTERS[] = {
    format_unknown,        /* CORE_EVT_NONE */
    format_day_started,    /* CORE_EVT_DAY_STARTED */
    format_season_changed, /* CORE_EVT_SEASON_CHANGED */
    format_night_fell,     /* CORE_EVT_NIGHT_FELL */
    format_dawn,           /* CORE_EVT_DAWN */
    format_warning,        /* CORE_EVT_WARNING */
    format_building_event, /* CORE_EVT_BUILDING_PLACED */
    format_building_event, /* CORE_EVT_BUILDING_COMPLETED */
    format_building_event, /* CORE_EVT_BUILDING_REMOVED */
    format_produced,       /* CORE_EVT_PRODUCED */
    format_villager_event, /* CORE_EVT_IMMIGRANT */
    format_villager_event, /* CORE_EVT_VILLAGER_LEAVING */
    format_villager_event, /* CORE_EVT_VILLAGER_LEFT */
    format_villager_event, /* CORE_EVT_VILLAGER_DIED */
    format_villager_event, /* CORE_EVT_PATH_BLOCKED */
    format_count,          /* CORE_EVT_STORAGE_FULL */
    format_count,          /* CORE_EVT_WAGES_UNPAID */
    format_story,          /* CORE_EVT_STORY */
    format_count,          /* CORE_EVT_STARVING */
    format_def_warning,    /* CORE_EVT_DEF_WARNING */
    format_count,          /* CORE_EVT_WEATHER_CHANGED */
    format_count,          /* CORE_EVT_NATURE_FORECAST */
    format_pair,           /* CORE_EVT_NATURE_STARTED */
    format_count,          /* CORE_EVT_NATURE_ENDED */
    format_building_event, /* CORE_EVT_BUILDING_DAMAGED */
    format_building_event, /* CORE_EVT_BUILDING_RUINED */
    format_building_event, /* CORE_EVT_BUILDING_REPAIRED */
    format_villager_event, /* CORE_EVT_VILLAGER_SICK */
    format_villager_event, /* CORE_EVT_VILLAGER_CURED */
    format_pair,           /* CORE_EVT_MERCHANT_ARRIVED */
    format_count,          /* CORE_EVT_MERCHANT_LEFT */
    format_pair,           /* CORE_EVT_WINTER_WARNING */
    format_count,          /* CORE_EVT_HOUSES_COLD */
    format_threat,         /* CORE_EVT_THREAT_SCHEDULED */
    format_threat,         /* CORE_EVT_RAID_STARTED */
    format_threat,         /* CORE_EVT_RAID_ENDED */
    format_threat,         /* CORE_EVT_ENEMY_KILLED */
    format_threat,         /* CORE_EVT_LOOT_STOLEN */
    format_threat,         /* CORE_EVT_THREAT_NEAR */
    format_progress,       /* CORE_EVT_RESEARCH_STARTED */
    format_progress,       /* CORE_EVT_RESEARCH_DONE */
    format_progress,       /* CORE_EVT_RANK_READY */
    format_progress,       /* CORE_EVT_RANK_UP */
    format_progress,       /* CORE_EVT_VICTORY */
    format_progress,       /* CORE_EVT_DEFEAT */
    format_progress,       /* CORE_EVT_TUTORIAL_STEP */
    format_progress,       /* CORE_EVT_TUTORIAL_DONE */
    format_progress,       /* CORE_EVT_EXPANSION_NOTICE */
};
#define EVENT_FORMATTER_COUNT ((int32_t)(sizeof(EVENT_FORMATTERS) / sizeof(EVENT_FORMATTERS[0])))
_Static_assert(sizeof(EVENT_FORMATTERS) / sizeof(EVENT_FORMATTERS[0]) == CORE_EVT_EXPANSION_NOTICE + 1,
               "every event type needs a formatter");

void fmt_event(const CoreEventView *event, char *out, size_t size)
{
    EventFormatter formatter = format_unknown;
    if (event->type >= 0 && event->type < EVENT_FORMATTER_COUNT) {
        formatter = EVENT_FORMATTERS[event->type];
    }
    formatter(event, out, size);
}

void fmt_date(const CoreSummaryView *summary, char *out, size_t size)
{
    snprintf(out, size, u8"%d년 %s %d일 (%d일째) %02d:%02d%s", (int)summary->year, fmt_season_name(summary->season),
             (int)summary->day_of_season, (int)summary->day, (int)(summary->minute_of_day / 60),
             (int)(summary->minute_of_day % 60), summary->is_night ? u8" 밤" : "");
}

void fmt_save_info(const CoreSaveInfoView *info, char *out, size_t size)
{
    snprintf(out, size, u8"%s — %d년 %s, %d일째 %02d:%02d · %s · 주민 %d명 · %s%s", info->castle_name, (int)info->year,
             fmt_season_name(info->season), (int)info->day, (int)(info->minute_of_day / 60),
             (int)(info->minute_of_day % 60), fmt_rank_name(info->rank), (int)info->population,
             fmt_difficulty_name(info->difficulty),
             info->outcome == CORE_OUTCOME_WON ? u8" · 승리" : (info->outcome == CORE_OUTCOME_LOST ? u8" · 패배" : ""));
}
