/*
 * format.c — CLI 용 한국어 문구 (설명은 format.h)
 */
#include "format.h"

#include <stdio.h>
#include <string.h>

static const char *const SEASON_NAMES[CORE_SEASON_COUNT] = {"봄", "여름", "가을", "겨울"};
static const char *const DIFFICULTY_NAMES[CORE_DIFF_COUNT] = {"평화", "보통", "혹독"};
static const char *const RESOURCE_NAMES[CORE_RES_COUNT] = {"금화", "마정석", "식량", "목재", "석재", "약초"};
static const char *const TERRAIN_NAMES[CORE_TERRAIN_COUNT] = {"풀밭", "숲", "바위", "물", "마정석 광맥"};
static const char *const WEATHER_NAMES[CORE_WEATHER_COUNT] = {"맑음", "비", "눈", "폭풍"};
static const char *const NATURE_NAMES[CORE_NATURE_COUNT] = {"없음", "가뭄", "역병", "폭풍", "혹한", "풍년"};
static const char *const DENY_REASONS[CORE_DENY_COUNT] = {
    "",
    "다 지은 시장이 없습니다",
    "시장에 일꾼이 없습니다",
    "시장에서 다루지 않는 자원입니다",
    "금화가 모자랍니다",
    "팔 물건이 모자랍니다",
    "창고에 자리가 없습니다",
    "상인이 없습니다",
    "이미 한 거래입니다",
    "고칠 곳이 없습니다",
    "아직 짓는 중입니다",
    "수량이 범위 밖입니다",
    "수리 비용이 모자랍니다",
};
static const char *const THREAT_NAMES[CORE_THREAT_COUNT] = {"없음", "도적", "마물", "적대 영주 군대"};
static const char *const DIRECTION_NAMES[CORE_DIR_COUNT] = {"북", "동", "남", "서"};
static const char *const LEDGER_NAMES[CORE_LEDGER_COUNT] = {"생산", "식사", "임금", "세금", "건설", "철거 환불",
                                                            "거래", "난방", "약탈",  "기타", "치료", "수리"};

static const char *const BUILDING_NAMES[CORE_BLD_COUNT] = {
    "성채", "주택", "농장", "채집 오두막", "벌목장", "채석장", "마정석 광산", "창고",
    "시장", "병영", "감시탑", "성벽", "성문", "치료소", "마탑", "도로"};

static const char *const PLACE_RESULTS[CORE_PLACE_COUNT] = {
    "지을 수 있습니다", "지을 수 없는 종류입니다", "지도 밖입니다", "지도 가장자리에 너무 가깝습니다",
    "물이나 바위 위에는 지을 수 없습니다", "다른 건물이 있습니다", "도로가 있습니다 (먼저 철거하세요)",
    "다른 건물의 문 앞을 막습니다", "문 앞 자리가 막혀 있습니다", "근처에 숲이 부족합니다",
    "바로 옆에 바위 지대가 있어야 합니다", "마정석 광맥 위에 지어야 합니다", "등급이 모자랍니다",
    "자원이 모자랍니다", "이미 있습니다", "다른 건물이나 지도 밖으로 가는 길을 막습니다",
    "성채에서 그곳까지 걸어갈 길이 없습니다"};

static const char *const VILLAGER_STATES[CORE_VS_COUNT] = {"쉬는 중", "자는 중",   "일하러 가는 중",
                                                            "일하는 중", "먹으러 감", "치료받으러 감",
                                                            "피신 중",   "경비 중",   "떠나는 중"};

/* 특성 키 → 한국어 (정의 파일에서 새 특성을 더하면 키가 그대로 보입니다) */
static const char *const TRAIT_KEYS[] = {"diligent", "lazy",     "glutton",  "coward", "brave",
                                         "mana_affinity", "optimist", "grumbler", "strong", "frugal"};
static const char *const TRAIT_NAMES[] = {"근면함", "게으름", "대식가", "겁쟁이", "용감함",
                                          "마력 친화", "낙천가", "투덜이", "장사", "검소함"};

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
    return (type >= 0 && type < CORE_BLD_COUNT) ? BUILDING_NAMES[type] : "?";
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
        return "성공했습니다.";
    case CORE_ERR_NULL_ARG:
    case CORE_ERR_INVALID_ARG:
        return "잘못된 값입니다.";
    case CORE_ERR_OUT_OF_MEMORY:
        return "메모리가 부족합니다.";
    case CORE_ERR_FILE_OPEN:
        return "파일을 열 수 없습니다. 경로를 확인하세요.";
    case CORE_ERR_FILE_IO:
        return "파일을 읽거나 쓰는 중에 문제가 생겼습니다.";
    case CORE_ERR_FORMAT:
        return "세이브 파일이 손상되었습니다.";
    case CORE_ERR_VERSION:
        return "다른 버전의 세이브 파일입니다.";
    case CORE_ERR_REJECTED:
        return "규칙상 할 수 없습니다.";
    case CORE_ERR_NOT_FOUND:
        return "그런 번호가 없습니다.";
    default:
        return "알 수 없는 오류입니다.";
    }
}

/* ---- 이벤트 종류별 문장 만들기 (종류 번호 → 함수 표) ---------------------- */

typedef void (*EventFormatter)(const CoreEventView *event, char *out, size_t size);

static void format_unknown(const CoreEventView *event, char *out, size_t size)
{
    snprintf(out, size, "(알 수 없는 사건 %d)", (int)event->type);
}

static void format_day_started(const CoreEventView *event, char *out, size_t size)
{
    snprintf(out, size, "%d일째가 시작되었습니다.", (int)event->a);
}

static void format_season_changed(const CoreEventView *event, char *out, size_t size)
{
    /* 봄, 여름, 가을, 겨울 모두 받침으로 끝나므로 조사는 늘 '이' */
    snprintf(out, size, "%d년 %s이 왔습니다.", (int)event->b, fmt_season_name(event->a));
}

static void format_night_fell(const CoreEventView *event, char *out, size_t size)
{
    (void)event;
    snprintf(out, size, "밤이 되었습니다.");
}

static void format_dawn(const CoreEventView *event, char *out, size_t size)
{
    (void)event;
    snprintf(out, size, "새벽이 밝아 옵니다.");
}

static void format_warning(const CoreEventView *event, char *out, size_t size)
{
    if (event->a == CORE_WARN_EVENTS_DROPPED) {
        snprintf(out, size, "(알림 %d개를 표시하지 못했습니다)", (int)event->b);
    } else {
        snprintf(out, size, "(경고 %d)", (int)event->a);
    }
}

static void format_building_event(const CoreEventView *event, char *out, size_t size)
{
    const char *verb = event->type == CORE_EVT_BUILDING_PLACED      ? "짓기 시작했습니다"
                       : event->type == CORE_EVT_BUILDING_COMPLETED ? "다 지었습니다"
                       : event->type == CORE_EVT_BUILDING_DAMAGED   ? "폭풍이 부쉈습니다"
                       : event->type == CORE_EVT_BUILDING_RUINED    ? "폭풍이 무너뜨렸습니다"
                       : event->type == CORE_EVT_BUILDING_REPAIRED  ? "고쳤습니다"
                                                                    : "철거했습니다";
    const char *name = fmt_building_name(event->b);
    snprintf(out, size, "%s(%d, %d)%s %s.", name, (int)event->c, (int)event->d, fmt_josa(name, "을", "를"), verb);
}

static void format_produced(const CoreEventView *event, char *out, size_t size)
{
    snprintf(out, size, "+%d %s", (int)event->c, fmt_resource_name(event->b));
}

static void format_villager_event(const CoreEventView *event, char *out, size_t size)
{
    const char *ga = fmt_josa(event->text, "이", "가");
    switch (event->type) {
    case CORE_EVT_IMMIGRANT:
        snprintf(out, size, "%s%s 마을에 이주해 왔습니다.", event->text, ga);
        break;
    case CORE_EVT_VILLAGER_LEAVING:
        snprintf(out, size, "%s%s 마을을 떠나기로 했습니다.", event->text, ga);
        break;
    case CORE_EVT_VILLAGER_LEFT:
        snprintf(out, size, "%s%s 마을을 떠났습니다.", event->text, ga);
        break;
    case CORE_EVT_VILLAGER_DIED:
        snprintf(out, size, "%s%s %s세상을 떠났습니다.", event->text, ga,
                 event->b == CORE_DEATH_STARVATION ? "굶주림으로 "
                 : event->b == CORE_DEATH_ILLNESS  ? "병으로 "
                 : event->b == CORE_DEATH_RAID     ? "습격에 쓰러져 "
                                                   : "");
        break;
    case CORE_EVT_PATH_BLOCKED:
        snprintf(out, size, "%s: 길이 막혔습니다.", event->text);
        break;
    case CORE_EVT_VILLAGER_SICK:
        snprintf(out, size, "%s%s 병에 걸렸습니다.", event->text, ga);
        break;
    case CORE_EVT_VILLAGER_CURED:
        snprintf(out, size, "%s%s %s", event->text, ga, event->b ? "치료소에서 나았습니다." : "병이 저절로 나았습니다.");
        break;
    default:
        snprintf(out, size, "%s", event->text);
        break;
    }
}

static void format_story(const CoreEventView *event, char *out, size_t size)
{
    const char *ga = fmt_josa(event->text, "이", "가");
    switch (event->b) {
    case CORE_STORY_FIRST_PRODUCE: {
        const char *resource = fmt_resource_name(event->c);
        snprintf(out, size, "%s%s 처음으로 %s%s 거뒀습니다.", event->text, ga, resource, fmt_josa(resource, "을", "를"));
        break;
    }
    case CORE_STORY_MOVED_IN:
        snprintf(out, size, "%s%s 새 집으로 이사했습니다.", event->text, ga);
        break;
    case CORE_STORY_NEW_JOB:
        snprintf(out, size, "%s%s %s에서 일하게 되었습니다.", event->text, ga, fmt_building_name(event->c));
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
        snprintf(out, size, "%s 창고가 가득 찼습니다.", fmt_resource_name(event->a));
        break;
    case CORE_EVT_WAGES_UNPAID:
        snprintf(out, size, "금화가 모자라 %d명이 임금을 받지 못했습니다.", (int)event->a);
        break;
    case CORE_EVT_STARVING:
        snprintf(out, size, "주민 %d명이 굶고 있습니다!", (int)event->a);
        break;
    case CORE_EVT_WEATHER_CHANGED:
        snprintf(out, size, "날씨: %s", fmt_weather_name(event->a));
        break;
    case CORE_EVT_NATURE_FORECAST: {
        const char *name = fmt_nature_name(event->a);
        snprintf(out, size, "예보: 내일 %s%s 옵니다.", name, fmt_josa(name, "이", "가"));
        break;
    }
    case CORE_EVT_NATURE_ENDED: {
        const char *name = fmt_nature_name(event->a);
        snprintf(out, size, "%s%s 끝났습니다.", name, fmt_josa(name, "이", "가"));
        break;
    }
    case CORE_EVT_MERCHANT_LEFT:
        snprintf(out, size, "떠돌이 상인이 떠났습니다.");
        break;
    case CORE_EVT_HOUSES_COLD:
        snprintf(out, size, "목재가 모자라 집 %d채가 지난밤 난방을 못 했습니다.", (int)event->a);
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
        snprintf(out, size, "%s%s 시작되었습니다 (%d일).", name, fmt_josa(name, "이", "가"), (int)event->b);
        break;
    }
    case CORE_EVT_MERCHANT_ARRIVED:
        snprintf(out, size, "떠돌이 상인이 왔습니다! %d일 머물며 거래 %d개를 제안합니다.", (int)event->a,
                 (int)event->b);
        break;
    case CORE_EVT_WINTER_WARNING:
        snprintf(out, size, "겨울 준비가 모자랍니다: 겨울을 나려면 식량 %d, 목재 %d 정도가 있어야 합니다.", (int)event->a, (int)event->b);
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
        snprintf(out, size, "위협 예고: %s %d명이 %s쪽에서 %d일 %d시간 뒤에 옵니다.", fmt_threat_name(event->a),
                 (int)event->d, fmt_direction_name(event->b), (int)(event->c / 144), (int)(event->c % 144 / 6));
        break;
    case CORE_EVT_THREAT_NEAR:
        snprintf(out, size, "경보! %s%s %s쪽에서 다가옵니다. 반나절 안에 도착합니다.", fmt_threat_name(event->a),
                 fmt_josa(fmt_threat_name(event->a), "이", "가"), fmt_direction_name(event->b));
        break;
    case CORE_EVT_RAID_STARTED:
        snprintf(out, size, "습격! %s %d명이 %s쪽에 나타났습니다. 주민들이 성채로 피합니다.", fmt_threat_name(event->b),
                 (int)event->c, fmt_direction_name(event->d));
        break;
    case CORE_EVT_RAID_ENDED:
        snprintf(out, size, "습격이 끝났습니다: 적 %d/%d명을 쓰러뜨림, 빼앗긴 자원 %d, 쓰러진 주민 %d명.", (int)event->a,
                 (int)event->b, (int)event->c, (int)event->d);
        break;
    case CORE_EVT_ENEMY_KILLED:
        if (event->c > 0) {
            snprintf(out, size, "적을 쓰러뜨렸습니다. 마정석 +%d", (int)event->c);
        } else {
            snprintf(out, size, "적을 쓰러뜨렸습니다.");
        }
        break;
    case CORE_EVT_LOOT_STOLEN:
        snprintf(out, size, "적이 %s %d개를 훔쳐 갔습니다!", fmt_resource_name(event->a), (int)event->b);
        break;
    default:
        snprintf(out, size, "(%d)", (int)event->a);
        break;
    }
}

static void format_def_warning(const CoreEventView *event, char *out, size_t size)
{
    static const char *const FILES[CORE_DATA_COUNT] = {"buildings.def", "traits.def", "names.txt", "enemies.def"};
    const char *file = (event->a >= 0 && event->a < CORE_DATA_COUNT) ? FILES[event->a] : "?";
    snprintf(out, size, "정의 파일 %s 의 %d번째 줄이 잘못되어 기본값을 씁니다.", file, (int)event->b);
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
};
#define EVENT_FORMATTER_COUNT ((int32_t)(sizeof(EVENT_FORMATTERS) / sizeof(EVENT_FORMATTERS[0])))
_Static_assert(sizeof(EVENT_FORMATTERS) / sizeof(EVENT_FORMATTERS[0]) == CORE_EVT_THREAT_NEAR + 1,
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
    snprintf(out, size, "%d년 %s %d일 (%d일째) %02d:%02d%s", (int)summary->year, fmt_season_name(summary->season),
             (int)summary->day_of_season, (int)summary->day, (int)(summary->minute_of_day / 60),
             (int)(summary->minute_of_day % 60), summary->is_night ? " 밤" : "");
}
