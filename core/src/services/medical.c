/* medical — 현재 기능의 단일 구현 */
#include "expansion.h"
#include "placement.h"
#include "rank.h"
#include "villager_ai.h"
#include <stdlib.h>
#include <string.h>

int expansion_is_medical(int type) {
    return type == CORE_BLD_HEALER || type == CORE_BLD_CLINIC || type == CORE_BLD_HOSPITAL;
}

CORE_API int32_t core_expansion_medical(const CoreWorld *w, int32_t id, CoreMedicalView *out) {
    if (!w || !out)
        return CORE_ERR_NULL_ARG;
    const Building *b = entity_find_building(w, id);
    if (!w->expansion || !b || !expansion_is_medical(b->type))
        return CORE_ERR_INVALID_ARG;
    *out = (CoreMedicalView){0};
    for (int i = 0; i < b->worker_count; i++)
        if (expansion_working_present(w, b, b->workers[i])) {
            int intellect = b->workers[i]->stats.intellect;
            out->workers++;
            out->intellect_sum += intellect;
            if (intellect > out->highest)
                out->highest = intellect;
        }
    out->qualified = expansion_qualified_present(w, b);
    out->base_patients = out->qualified ? expansion_service_base(b, out->workers) : 0;
    out->herbs = b->type == CORE_BLD_HOSPITAL ? 3 : 2;
    out->treated = b->visits_today;
    out->credit_milli = b->service_credit_milli;
    out->required_workers = b->type == CORE_BLD_HOSPITAL ? 2 : 1;
    out->required_highest = b->type == CORE_BLD_HOSPITAL ? 6 : b->type == CORE_BLD_CLINIC ? 5 : 4;
    out->required_sum = b->type == CORE_BLD_HOSPITAL ? 10 : b->type == CORE_BLD_CLINIC ? 7 : 0;
    for (int i = 0; i < w->villager_count; i++)
        if (w->villagers[i]->target == b && w->villagers[i]->state == CORE_VS_SEEK_HEALING)
            out->waiting++;
    return CORE_OK;
}
