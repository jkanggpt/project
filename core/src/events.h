/*
 * events.h — 피드백 이벤트 큐
 *
 * 소리, 알림 문구, 떠오르는 숫자 같은 "보여 주기"에만 쓰는 이벤트입니다.
 * C 그래픽 프런트엔드은 상태를 이벤트가 아니라 매 틱의 스냅숏(뷰 복사)으로 맞추므로, 이벤트를 놓쳐도 게임이 어긋나지 않습니다.
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
        } pair; /* NATURE_STARTED(사건, 날 수), MERCHANT_ARRIVED(날 수, 제안 수), WINTER_WARNING(식량, 목재),
                   THREAT_NEAR(종류, 방향) */
        struct {
            int32_t type;
            int32_t direction;
            int32_t ticks_left;
            int32_t strength;
        } threat; /* THREAT_SCHEDULED */
        struct {
            int32_t a;
            int32_t b;
            int32_t c;
            int32_t d;
        } raid; /* RAID_STARTED(id, 종류, 적 수, 방향), RAID_ENDED(쓰러뜨림, 전체, 빼앗김, 쓰러진 주민),
                   ENEMY_KILLED(적 id, 종류, 마정석), LOOT_STOLEN(자원, 양, 건물 id) */
        struct {
            int32_t a;
            int32_t b;
            int32_t c;
        } progress; /* RESEARCH_STARTED(번호), RESEARCH_DONE(번호, 끝낸 수), RANK_READY(등급), RANK_UP(등급),
                       VICTORY(날짜), DEFEAT(원인, 날짜), TUTORIAL_STEP(목표, 끝낸 수, 전체), TUTORIAL_DONE(금화, 마정석) */
    } as;
    char text[CORE_NAME_BYTES]; /* 관련 주민 이름 (없으면 빈 문자열) */
    int32_t cause_kind,source_id,event_tick,type_serial,source_type;
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
void events_push_threat(EventQueue *queue, int32_t threat_type, int32_t direction, int32_t ticks_left, int32_t strength);
void events_push_raid(EventQueue *queue, CoreEventType type, int32_t a, int32_t b, int32_t c, int32_t d);
/* text: 연구 키 등 (NULL 이면 빈 문자열) */
void events_push_progress(EventQueue *queue, CoreEventType type, int32_t a, int32_t b, int32_t c, const char *text);

/* 내부 이벤트(공용체)를 외부 호출자에 건넬 평평한 뷰로 바꿉니다. */
void events_to_view(const Event *event, CoreEventView *out);

#endif
