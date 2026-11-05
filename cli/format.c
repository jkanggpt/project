/*
 * format.c — CLI 용 한국어 문구 (설명은 format.h)
 */
#include "format.h"

#include <stdio.h>

static const char *const SEASON_NAMES[CORE_SEASON_COUNT] = {"봄", "여름", "가을", "겨울"};
static const char *const DIFFICULTY_NAMES[CORE_DIFF_COUNT] = {"평화", "보통", "혹독"};
static const char *const RESOURCE_NAMES[CORE_RES_COUNT] = {"금화", "마정석", "식량", "목재", "석재", "약초"};
static const char *const TERRAIN_NAMES[CORE_TERRAIN_COUNT] = {"풀밭", "숲", "바위", "물", "마정석 광맥"};

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

/* 순서는 CoreEventType 과 같아야 합니다 */
static const EventFormatter EVENT_FORMATTERS[] = {
    format_unknown,        /* CORE_EVT_NONE */
    format_day_started,    /* CORE_EVT_DAY_STARTED */
    format_season_changed, /* CORE_EVT_SEASON_CHANGED */
    format_night_fell,     /* CORE_EVT_NIGHT_FELL */
    format_dawn,           /* CORE_EVT_DAWN */
    format_warning,        /* CORE_EVT_WARNING */
};
#define EVENT_FORMATTER_COUNT ((int32_t)(sizeof(EVENT_FORMATTERS) / sizeof(EVENT_FORMATTERS[0])))

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
