/* service_capacity — 현재 기능의 단일 구현 */
#include "expansion.h"
#include "placement.h"
#include "rank.h"
#include "villager_ai.h"
#include <stdlib.h>
#include <string.h>

int expansion_event_active(const World *w, int event) {
    if (w->nature.event == event)
        return 1;
    if (!w->expansion)
        return 0;
    if (event == CORE_NATURE_PLAGUE)
        return w->expansion->plague_days > 0;
    return w->expansion->economic_event == event && w->expansion->economic_days > 0;
}

int expansion_agriculture_event(const World *w, int type) {
    int irrigated = type == CORE_BLD_IRRIGATED_FARM;
    if (expansion_event_active(w, CORE_NATURE_STORM))
        return irrigated ? 900 : type == CORE_BLD_GREENHOUSE ? 1000 : 800;
    if (expansion_event_active(w, CORE_NATURE_DROUGHT))
        return irrigated                     ? (expansion_has_research(w, 2) ? 750 : 650)
               : type == CORE_BLD_GREENHOUSE ? 850
                                             : 250;
    return expansion_event_active(w, CORE_NATURE_BUMPER) ? 2400 : 1000;
}

int expansion_service_base(const Building *b, int workers) {
    switch (b->type) {
    case CORE_BLD_HEALER:
        return workers ? 2 : 0;
    case CORE_BLD_CLINIC:
        return workers >= 2 ? 4 : workers ? 3 : 0;
    case CORE_BLD_HOSPITAL:
        return workers >= 3 ? 8 : workers == 2 ? 5 : 0;
    case CORE_BLD_TAVERN:
        return 8;
    case CORE_BLD_BATHHOUSE:
        return 12;
    case CORE_BLD_THEATRE:
        return 15;
    case CORE_BLD_ART_MUSEUM:
        return workers >= 2 ? 30 : workers ? 15 : 0;
    case CORE_BLD_CHURCH:
        return 12;
    case CORE_BLD_SCHOOL:
        return 8;
    case CORE_BLD_LIBRARY:
        return 6;
    case CORE_BLD_ACADEMY:
        return 12;
    default:
        return 0;
    }
}

int expansion_service_shift(int type) {
    return type == CORE_BLD_TAVERN || type == CORE_BLD_BATHHOUSE || type == CORE_BLD_THEATRE ||
           type == CORE_BLD_CHURCH || type == CORE_BLD_SCHOOL || type == CORE_BLD_LIBRARY ||
           type == CORE_BLD_ACADEMY || type == CORE_BLD_ART_MUSEUM;
}

CORE_API int32_t core_expansion_events(const CoreWorld *w, CoreEventLayers *out) {
    if (!w || !out)
        return CORE_ERR_NULL_ARG;
    if (!w->expansion)
        return CORE_ERR_INVALID_ARG;
    *out = (CoreEventLayers){w->expansion->plague_days, w->expansion->economic_event,
                             w->expansion->economic_days};
    return CORE_OK;
}
