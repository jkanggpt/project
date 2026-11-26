/* identity — 현재 기능의 단일 구현 */
#include "expansion.h"
#include "placement.h"
#include "rank.h"
#include "villager_ai.h"
#include <stdlib.h>
#include <string.h>

void expansion_choose_identity(World *w, Villager *v) {
    const char *entry = NULL, *name = NULL;
    int sex = 0;
    int start = rng_range(&w->rng, 0, w->defs.name_count - 1);
    for (int attempt = 0; attempt < w->defs.name_count; attempt++) {
        entry = w->defs.names[(start + attempt) % w->defs.name_count].text;
        sex = entry[0] == 'F' && entry[1] == ':';
        name = ((entry[0] == 'M' || entry[0] == 'F') && entry[1] == ':') ? entry + 2 : entry;
        int in_use = 0;
        for (int i = 0; i < w->villager_count; i++)
            in_use |= w->villagers[i] != v && strcmp(w->villagers[i]->name, name) == 0;
        if (!in_use)
            break;
    }
    if (!name)
        name = u8"주민";
    strcpy(v->name, name);
    v->sex = sex;
    v->appearance = rng_range(&w->rng, 0, 23) * 2 + sex;
    for (int attempt = 0; attempt < 4; attempt++) {
        int recent = 0, checked = 0;
        for (int i = w->villager_count - 1; i >= 0 && checked < 8; i--)
            if (w->villagers[i] != v && w->villagers[i]->sex == sex) {
                checked++;
                recent |= w->villagers[i]->appearance == v->appearance;
            }
        if (!recent || rng_range(&w->rng, 0, 3) == 0)
            break;
        v->appearance = rng_range(&w->rng, 0, 23) * 2 + sex;
    }
}
