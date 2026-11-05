/*
 * events.h — 피드백 이벤트 큐
 *
 * 소리, 알림 문구, 떠오르는 숫자 같은 "보여 주기"에만 쓰는 이벤트입니다.
 * Godot은 상태를 이벤트가 아니라 매 틱의 스냅숏(뷰 복사)으로 맞추므로, 이벤트를 놓쳐도 게임이 어긋나지 않습니다.
 * 그래서 저장하지 않고, 너무 많이 쌓이면 가장 오래된 것부터 버립니다.
 */
#ifndef CASTLE_EVENTS_H
#define CASTLE_EVENTS_H

#include <stdint.h>

#include "castle_core.h"

#define EVENTS_INITIAL_CAPACITY 32
#define EVENTS_MAX_CAPACITY 4096

/* 이벤트 하나. 종류(type)에 따라 union 의 다른 칸을 씁니다. (태그 붙은 공용체) */
typedef struct Event {
    CoreEventType type;
    union {
        struct {
            int32_t day;
        } day; /* DAY_STARTED, NIGHT_FELL, DAWN */
        struct {
            int32_t season;
            int32_t year;
        } season; /* SEASON_CHANGED */
        struct {
            int32_t code;
            int32_t detail;
        } warning; /* WARNING, DEF_WARNING */
        struct {
            int32_t id;
            int32_t type;
            int32_t x;
            int32_t y;
        } building; /* BUILDING_PLACED, BUILDING_COMPLETED, BUILDING_REMOVED, BUILDING_DAMAGED/RUINED/REPAIRED */
        struct {
            int32_t building_id;
            int32_t resource;
            int32_t amount;
        } produced; /* PRODUCED */
        struct {
            int32_t id;
            int32_t detail;
            int32_t value;
        } villager; /* IMMIGRANT, VILLAGER_*, PATH_BLOCKED, STORY, VILLAGER_SICK, VILLAGER_CURED */
        struct {
            int32_t value;
        } count; /* STORAGE_FULL(자원), WAGES_UNPAID, STARVING, WEATHER_CHANGED, NATURE_FORECAST, NATURE_ENDED,
                    MERCHANT_LEFT, HOUSES_COLD */
        struct {
            int32_t first;
            int32_t second;
        } pair; /* NATURE_STARTED(사건, 날 수), MERCHANT_ARRIVED(날 수, 제안 수), WINTER_WARNING(식량, 목재) */
    } as;
    char text[CORE_NAME_BYTES]; /* 관련 주민 이름 (없으면 빈 문자열) */
} Event;

/* 원형 버퍼. 꽉 차면 realloc 으로 두 배씩 늘리고, 최대 크기에서는 가장 오래된 것을 버립니다. */
typedef struct EventQueue {
    Event *items;
    int32_t capacity;
    int32_t head;    /* 가장 오래된 이벤트의 위치 */
    int32_t count;
    int32_t dropped; /* 버렸지만 아직 경고로 알리지 않은 개수 */
} EventQueue;

int events_init(EventQueue *queue, int32_t initial_capacity); /* CORE_OK 또는 CORE_ERR_OUT_OF_MEMORY */
void events_free(EventQueue *queue);
void events_push(EventQueue *queue, const Event *event);
/* 꺼낼 이벤트가 있으면 out 에 채우고 1, 없으면 0 */
int events_pop(EventQueue *queue, Event *out);

/* 자주 쓰는 이벤트를 만드는 도우미 */
void events_push_day(EventQueue *queue, CoreEventType type, int32_t day);
void events_push_season(EventQueue *queue, int32_t season, int32_t year);
void events_push_building(EventQueue *queue, CoreEventType type, int32_t id, int32_t building_type, int32_t x,
                          int32_t y);
void events_push_produced(EventQueue *queue, int32_t building_id, int32_t resource, int32_t amount);
void events_push_villager(EventQueue *queue, CoreEventType type, int32_t id, int32_t detail, int32_t value,
                          const char *name);
void events_push_count(EventQueue *queue, CoreEventType type, int32_t value);
void events_push_warning(EventQueue *queue, CoreEventType type, int32_t code, int32_t detail);
void events_push_pair(EventQueue *queue, CoreEventType type, int32_t first, int32_t second);

/* 내부 이벤트(공용체)를 C#에 건넬 평평한 뷰로 바꿉니다. */
void events_to_view(const Event *event, CoreEventView *out);

#endif
