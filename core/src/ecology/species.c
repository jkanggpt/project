/* animal_classes — 현재 기능의 단일 구현 */
#include "expansion.h"
#include "placement.h"
#include "rank.h"
#include "villager_ai.h"
#include <stdlib.h>
#include <string.h>

int expansion_is_phantom(int kind) { return kind >= 24 && kind <= 29; }

CORE_API int32_t core_expansion_animal_class(int32_t kind) {
    if (kind < 0 || kind >= CORE_EXP_ANIMALS)
        return CORE_ERR_INVALID_ARG;
    return EXP_ANIMALS[kind].livestock ? 2 : expansion_is_phantom(kind) ? 1 : 0;
}
