/* clay — 현재 기능의 단일 구현 */
#include "expansion.h"
#include "placement.h"
#include "rank.h"
#include "villager_ai.h"
#include <stdlib.h>
#include <string.h>

int expansion_clay_take(World *w, const Building *b, int radius, int requested) {
    ExpansionState *e = w->expansion;
    int count = 0, total = 0,
        room = eco_bank_limit(&w->bank, CORE_RES_CLAY) * 1000 - expansion_bank_have(w, CORE_RES_CLAY);
    if (requested > room)
        requested = room;
    if (requested <= 0)
        return 0;
    int *cells = e->queue;
    for (int y = b->y - radius; y < b->y + b->h + radius; y++)
        for (int x = b->x - radius; x < b->x + b->w + radius; x++)
            if (grid_in_bounds(&w->grid, x, y) && expansion_center_distance(b, x, y) <= radius) {
                int p = grid_index(&w->grid, x, y);
                if (w->grid.terrain[p] != CORE_TERRAIN_WATER && e->tiles[p].clay_milli > 0)
                    cells[count++] = p;
            }
    while (count > 0 && requested > 0) {
        int share = requested / count, remainder = requested % count, active = 0,
            start = (b->id + exp_day(w)) % count;
        for (int k = 0; k < count; k++) {
            int p = cells[(start + k) % count], take = share + (k < remainder);
            if (take > e->tiles[p].clay_milli)
                take = e->tiles[p].clay_milli;
            take = expansion_bank_add(w, CORE_RES_CLAY, take, CORE_LEDGER_PRODUCTION);
            e->tiles[p].clay_milli -= take;
            requested -= take;
            total += take;
        }
        for (int k = 0; k < count; k++)
            if (e->tiles[cells[k]].clay_milli > 0)
                cells[active++] = cells[k];
        count = active;
    }
    return total;
}
