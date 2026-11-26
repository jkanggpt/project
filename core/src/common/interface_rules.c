/* interface_rules — 현재 기능의 단일 구현 */
#include "expansion.h"
#include "placement.h"
#include "rank.h"
#include "villager_ai.h"
#include <stdlib.h>
#include <string.h>

void expansion_notice(World *w, int code, int a, int b, const char *message) {
    char text[CORE_NAME_BYTES] = {0};
    size_t n = strlen(message);
    if (n >= sizeof(text))
        n = sizeof(text) - 1;
    while (n && ((unsigned char)message[n] & 0xc0) == 0x80)
        n--;
    memcpy(text, message, n);
    events_push_progress(&w->events, CORE_EVT_EXPANSION_NOTICE, code, a, b, text);
}

CORE_API int32_t core_expansion_ui_color_group(int32_t type) {
    const ExpansionBuildingSpec *s = expansion_spec(type);
    if (!s)
        return -1;
    if (type == CORE_BLD_WELL || type == CORE_BLD_WATER_TOWER || s->category == CORE_CAT_HOUSING ||
        s->category == CORE_CAT_ROAD)
        return 0;
    if (type == CORE_BLD_KEEP || type == CORE_BLD_TAVERN || type == CORE_BLD_THEATRE ||
        type == CORE_BLD_CHURCH || type == CORE_BLD_ART_MUSEUM || s->field == 9)
        return 5;
    if (s->field == 0 || s->field == 1 || s->field == 3 || s->field == 5)
        return 1;
    if (s->field == 2 || s->field == 4 || s->field == 6 || s->category == CORE_CAT_STORAGE)
        return 2;
    if (s->field == 7 || s->category == CORE_CAT_DEFENSE)
        return 3;
    if (s->field == 8 || s->field == 10 || s->field == 11)
        return 4;
    return 0;
}
