/*
 * events.c — 피드백 이벤트 큐 (설명은 events.h)
 */
#include "events.h"

#include <string.h>

#include "mem.h"

int events_init(EventQueue *queue, int32_t initial_capacity)
{
    memset(queue, 0, sizeof(*queue));
    if (initial_capacity < 1) {
        initial_capacity = 1;
    }
    queue->items = (Event *)mem_calloc((size_t)initial_capacity, sizeof(Event));
    if (queue->items == NULL) {
        return CORE_ERR_OUT_OF_MEMORY;
    }
    queue->capacity = initial_capacity;
    return CORE_OK;
}

void events_free(EventQueue *queue)
{
    mem_free(queue->items);
    memset(queue, 0, sizeof(*queue));
}

/* 큐를 두 배로 키웁니다. 실패하면 0 을 돌려주고 큐는 그대로입니다. */
static int grow(EventQueue *queue)
{
    int32_t old_capacity = queue->capacity;
    int32_t new_capacity = old_capacity * 2;
    Event *grown;
    int32_t wrapped;

    if (new_capacity > EVENTS_MAX_CAPACITY) {
        return 0;
    }
    /* 임시 포인터에 받아서, 실패해도 원래 배열을 잃지 않게 합니다 */
    grown = (Event *)mem_realloc(queue->items, (size_t)new_capacity * sizeof(Event));
    if (grown == NULL) {
        return 0;
    }
    queue->items = grown;
    queue->capacity = new_capacity;

    /* 원형 버퍼가 끝에서 앞으로 감겨 있었다면, 감긴 부분을 새로 생긴 뒤쪽 공간으로 옮겨 순서를 이어 줍니다.
     * 새 크기가 두 배라서 감긴 부분(최대 old_capacity 개)은 항상 들어갑니다. */
    wrapped = queue->head + queue->count - old_capacity;
    if (wrapped > 0) {
        memcpy(&queue->items[old_capacity], &queue->items[0], (size_t)wrapped * sizeof(Event));
    }
    return 1;
}

void events_push(EventQueue *queue, const Event *event)
{
    int32_t slot;

    if (queue->items == NULL) {
        return;
    }
    if (queue->count == queue->capacity && !grow(queue)) {
        /* 더 키울 수 없으면 가장 오래된 이벤트를 버립니다 (이벤트는 보여 주기용이라 괜찮습니다) */
        queue->head = (queue->head + 1) % queue->capacity;
        queue->count--;
        queue->dropped++;
    }
    slot = (queue->head + queue->count) % queue->capacity;
    queue->items[slot] = *event;
    queue->count++;
}

int events_pop(EventQueue *queue, Event *out)
{
    if (queue->dropped > 0) {
        /* 버린 이벤트가 있었다는 사실을 먼저 알립니다 */
        memset(out, 0, sizeof(*out));
        out->type = CORE_EVT_WARNING;
        out->as.warning.code = CORE_WARN_EVENTS_DROPPED;
        out->as.warning.detail = queue->dropped;
        queue->dropped = 0;
        return 1;
    }
    if (queue->count == 0) {
        return 0;
    }
    *out = queue->items[queue->head];
    queue->head = (queue->head + 1) % queue->capacity;
    queue->count--;
    return 1;
}

void events_push_day(EventQueue *queue, CoreEventType type, int32_t day)
{
    Event event;
    memset(&event, 0, sizeof(event));
    event.type = type;
    event.as.day.day = day;
    events_push(queue, &event);
}

void events_push_season(EventQueue *queue, int32_t season, int32_t year)
{
    Event event;
    memset(&event, 0, sizeof(event));
    event.type = CORE_EVT_SEASON_CHANGED;
    event.as.season.season = season;
    event.as.season.year = year;
    events_push(queue, &event);
}

void events_push_building(EventQueue *queue, CoreEventType type, int32_t id, int32_t building_type, int32_t x,
                          int32_t y)
{
    Event event;
    memset(&event, 0, sizeof(event));
    event.type = type;
    event.as.building.id = id;
    event.as.building.type = building_type;
    event.as.building.x = x;
    event.as.building.y = y;
    events_push(queue, &event);
}

void events_push_produced(EventQueue *queue, int32_t building_id, int32_t resource, int32_t amount)
{
    Event event;
    memset(&event, 0, sizeof(event));
    event.type = CORE_EVT_PRODUCED;
    event.as.produced.building_id = building_id;
    event.as.produced.resource = resource;
    event.as.produced.amount = amount;
    events_push(queue, &event);
}

void events_push_villager(EventQueue *queue, CoreEventType type, int32_t id, int32_t detail, int32_t value,
                          const char *name)
{
    Event event;
    memset(&event, 0, sizeof(event));
    event.type = type;
    event.as.villager.id = id;
    event.as.villager.detail = detail;
    event.as.villager.value = value;
    if (name != NULL) {
        size_t length = strlen(name);
        if (length >= sizeof(event.text)) {
            length = sizeof(event.text) - 1;
        }
        memcpy(event.text, name, length);
    }
    events_push(queue, &event);
}

void events_push_pair(EventQueue *queue, CoreEventType type, int32_t first, int32_t second)
{
    Event event;
    memset(&event, 0, sizeof(event));
    event.type = type;
    event.as.pair.first = first;
    event.as.pair.second = second;
    events_push(queue, &event);
}

void events_push_threat(EventQueue *queue, int32_t threat_type, int32_t direction, int32_t ticks_left, int32_t strength)
{
    Event event;
    memset(&event, 0, sizeof(event));
    event.type = CORE_EVT_THREAT_SCHEDULED;
    event.as.threat.type = threat_type;
    event.as.threat.direction = direction;
    event.as.threat.ticks_left = ticks_left;
    event.as.threat.strength = strength;
    events_push(queue, &event);
}

void events_push_raid(EventQueue *queue, CoreEventType type, int32_t a, int32_t b, int32_t c, int32_t d)
{
    Event event;
    memset(&event, 0, sizeof(event));
    event.type = type;
    event.as.raid.a = a;
    event.as.raid.b = b;
    event.as.raid.c = c;
    event.as.raid.d = d;
    events_push(queue, &event);
}

void events_push_progress(EventQueue *queue, CoreEventType type, int32_t a, int32_t b, int32_t c, const char *text)
{
    Event event;
    memset(&event, 0, sizeof(event));
    event.type = type;
    event.as.progress.a = a;
    event.as.progress.b = b;
    event.as.progress.c = c;
    if (text != NULL) {
        strncpy(event.text, text, sizeof(event.text) - 1);
    }
    events_push(queue, &event);
}

void events_push_count(EventQueue *queue, CoreEventType type, int32_t value)
{
    Event event;
    memset(&event, 0, sizeof(event));
    event.type = type;
    event.as.count.value = value;
    events_push(queue, &event);
}

void events_push_warning(EventQueue *queue, CoreEventType type, int32_t code, int32_t detail)
{
    Event event;
    memset(&event, 0, sizeof(event));
    event.type = type;
    event.as.warning.code = code;
    event.as.warning.detail = detail;
    events_push(queue, &event);
}

void events_to_view(const Event *event, CoreEventView *out)
{
    memset(out, 0, sizeof(*out));
    out->type = (int32_t)event->type;
    /* 종류(태그)를 보고 공용체의 어느 칸이 유효한지 정합니다 */
    switch (event->type) {
    case CORE_EVT_DAY_STARTED:
    case CORE_EVT_NIGHT_FELL:
    case CORE_EVT_DAWN:
        out->a = event->as.day.day;
        break;
    case CORE_EVT_SEASON_CHANGED:
        out->a = event->as.season.season;
        out->b = event->as.season.year;
        break;
    case CORE_EVT_WARNING:
    case CORE_EVT_DEF_WARNING:
        out->a = event->as.warning.code;
        out->b = event->as.warning.detail;
        break;
    case CORE_EVT_BUILDING_PLACED:
    case CORE_EVT_BUILDING_COMPLETED:
    case CORE_EVT_BUILDING_REMOVED:
    case CORE_EVT_BUILDING_DAMAGED:
    case CORE_EVT_BUILDING_RUINED:
    case CORE_EVT_BUILDING_REPAIRED:
        out->a = event->as.building.id;
        out->b = event->as.building.type;
        out->c = event->as.building.x;
        out->d = event->as.building.y;
        break;
    case CORE_EVT_PRODUCED:
        out->a = event->as.produced.building_id;
        out->b = event->as.produced.resource;
        out->c = event->as.produced.amount;
        break;
    case CORE_EVT_IMMIGRANT:
    case CORE_EVT_VILLAGER_LEAVING:
    case CORE_EVT_VILLAGER_LEFT:
    case CORE_EVT_VILLAGER_DIED:
    case CORE_EVT_PATH_BLOCKED:
    case CORE_EVT_STORY:
    case CORE_EVT_VILLAGER_SICK:
    case CORE_EVT_VILLAGER_CURED:
        out->a = event->as.villager.id;
        out->b = event->as.villager.detail;
        out->c = event->as.villager.value;
        break;
    case CORE_EVT_STORAGE_FULL:
    case CORE_EVT_WAGES_UNPAID:
    case CORE_EVT_STARVING:
    case CORE_EVT_WEATHER_CHANGED:
    case CORE_EVT_NATURE_FORECAST:
    case CORE_EVT_NATURE_ENDED:
    case CORE_EVT_MERCHANT_LEFT:
    case CORE_EVT_HOUSES_COLD:
        out->a = event->as.count.value;
        break;
    case CORE_EVT_NATURE_STARTED:
    case CORE_EVT_MERCHANT_ARRIVED:
    case CORE_EVT_WINTER_WARNING:
    case CORE_EVT_THREAT_NEAR:
        out->a = event->as.pair.first;
        out->b = event->as.pair.second;
        break;
    case CORE_EVT_THREAT_SCHEDULED:
        out->a = event->as.threat.type;
        out->b = event->as.threat.direction;
        out->c = event->as.threat.ticks_left;
        out->d = event->as.threat.strength;
        break;
    case CORE_EVT_RAID_STARTED:
    case CORE_EVT_RAID_ENDED:
    case CORE_EVT_ENEMY_KILLED:
    case CORE_EVT_LOOT_STOLEN:
        out->a = event->as.raid.a;
        out->b = event->as.raid.b;
        out->c = event->as.raid.c;
        out->d = event->as.raid.d;
        break;
    case CORE_EVT_RESEARCH_STARTED:
    case CORE_EVT_RESEARCH_DONE:
    case CORE_EVT_RANK_READY:
    case CORE_EVT_RANK_UP:
    case CORE_EVT_VICTORY:
    case CORE_EVT_DEFEAT:
    case CORE_EVT_TUTORIAL_STEP:
    case CORE_EVT_TUTORIAL_DONE:
        out->a = event->as.progress.a;
        out->b = event->as.progress.b;
        out->c = event->as.progress.c;
        break;
    case CORE_EVT_NONE:
    default:
        break;
    }
    memcpy(out->text, event->text, sizeof(out->text));
    out->text[sizeof(out->text) - 1] = '\0';
}
