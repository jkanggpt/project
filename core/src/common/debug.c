/* debug.c — 현재 영지의 공용 성능 검사 진입 */
#include "debug.h"
#include "expansion.h"
int debug_stress(World *world,int32_t villagers,int32_t enemies) {
    return expansion_debug_town(world,villagers,enemies);
}
