/* expansion_save.c — 확장 상태의 명시적 정수 직렬화·해시·범위 검사 */
#include "expansion.h"
#include "text.h"
#include <errno.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

typedef struct Field {
    size_t offset;
    int count;
} Field;
#define F(T, name, count)                                                                                    \
    { offsetof(T, name), count }
static const Field STATE[] = {F(ExpansionState, tax, 1),
                              F(ExpansionState, tax_day, 1),
                              F(ExpansionState, fuel_milli, 1),
                              F(ExpansionState, magic_logs_milli, 1),
                              F(ExpansionState, fractional, 15),
                              F(ExpansionState, trade_stock, 15),
                              F(ExpansionState, trade_used, 1),
                              F(ExpansionState, starter_waves, 1),
                              F(ExpansionState, immigration_day, 1),
                              F(ExpansionState, disaster_day, 1),
                              F(ExpansionState, next_person_id, 1),
                              F(ExpansionState, candidate_day, 1),
                              F(ExpansionState, animal_count, 1),
                              F(ExpansionState, next_animal_id, 1),
                              F(ExpansionState, candidate_count, 1),
                              F(ExpansionState, archive_count, 1)};
static const Field BUILDING[] = {F(Building, rotation, 1),
                                 F(Building, completed_day, 1),
                                 F(Building, mode, 1),
                                 F(Building, mode_day, 1),
                                 F(Building, operating, 1),
                                 F(Building, blocked_reason, 1),
                                 F(Building, daily_work_milli, 1),
                                 F(Building, crop_labor_milli, 1),
                                 F(Building, crop_year, 1),
                                 F(Building, crop_drought, 1),
                                 F(Building, pending_harvest_milli, 1),
                                 F(Building, harvest_expiry, 1),
                                 F(Building, processing_milli, 15),
                                 F(Building, hunts_milli, 1),
                                 F(Building, shots, 1),
                                 F(Building, visits_today, 1)};
static const Field VILLAGER[] = {
    F(Villager, stats.agility, 1),   F(Villager, initial_stats, 5),     F(Villager, stat_xp, 5),
    F(Villager, skill_minutes, 12),  F(Villager, contract_end_tick, 1), F(Villager, personal_id, 1),
    F(Villager, water, 1),           F(Villager, hygiene, 1),           F(Villager, culture_until, 1),
    F(Villager, culture_bonus, 1),   F(Villager, visit_day, 1),         F(Villager, inside_building, 1),
    F(Villager, current_x_milli, 1), F(Villager, current_y_milli, 1),   F(Villager, meal_day, 1)};
static const Field TILE[] = {
    F(CoreExpansionTileView, biome, 1),           F(CoreExpansionTileView, tree_kind, 1),
    F(CoreExpansionTileView, tree_age, 1),        F(CoreExpansionTileView, tree_logs_milli, 1),
    F(CoreExpansionTileView, tree_used_milli, 1), F(CoreExpansionTileView, regrow_milli, 1),
    F(CoreExpansionTileView, ore_kind, 1),        F(CoreExpansionTileView, ore_milli, 1),
    F(CoreExpansionTileView, water_id, 1),        F(CoreExpansionTileView, explored, 1)};
static const Field ANIMAL[] = {F(CoreAnimalView, id, 1),          F(CoreAnimalView, kind, 1),
                               F(CoreAnimalView, x_milli, 1),     F(CoreAnimalView, y_milli, 1),
                               F(CoreAnimalView, hp, 1),          F(CoreAnimalView, age, 1),
                               F(CoreAnimalView, owner, 1),       F(CoreAnimalView, breed_day, 1),
                               F(CoreAnimalView, starve_days, 1), F(CoreAnimalView, alive, 1)};
static const Field CANDIDATE[] = {F(CoreCandidateView, person_id, 1), F(CoreCandidateView, fee, 1),
                                  F(CoreCandidateView, field, 1),     F(CoreCandidateView, age, 1),
                                  F(CoreCandidateView, stats, 5),     F(CoreCandidateView, initial, 5),
                                  F(CoreCandidateView, xp, 5),        F(CoreCandidateView, skills, 12),
                                  F(CoreCandidateView, traits, 2),    F(CoreCandidateView, hired, 1)};
#define COUNT(a) ((int)(sizeof(a) / sizeof((a)[0])))
static int32_t get_int(const void *object, size_t offset) {
    int32_t value;
    memcpy(&value, (const char *)object + offset, sizeof(value));
    return value;
}
static void write_fields(FILE *f, const void *object, const Field *fields, int count) {
    int i, j;
    for (i = 0; i < count; i++)
        for (j = 0; j < fields[i].count; j++)
            fprintf(f, ",%d", (int)get_int(object, fields[i].offset + (size_t)j * 4));
}
static uint64_t hash_int(uint64_t hash, int32_t value) {
    int i;
    for (i = 0; i < 4; i++) {
        hash ^= (uint8_t)((uint32_t)value >> (i * 8));
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}
static uint64_t hash_fields(uint64_t hash, const void *object, const Field *fields, int count) {
    int i, j;
    for (i = 0; i < count; i++)
        for (j = 0; j < fields[i].count; j++)
            hash = hash_int(hash, get_int(object, fields[i].offset + (size_t)j * 4));
    return hash;
}
int expansion_save(FILE *f, const World *w) {
    ExpansionState *e = w->expansion;
    int i;
    if (!e)
        return CORE_OK;
    fputs("[expansion]\nstate=1", f);
    write_fields(f, e, STATE, COUNT(STATE));
    fputc('\n', f);
    for (i = 0; i < e->cells; i++) {
        fprintf(f, "tile=%d", i);
        write_fields(f, &e->tiles[i], TILE, COUNT(TILE));
        fputc('\n', f);
    }
    for (i = 0; i < w->building_count; i++) {
        fprintf(f, "building=%d", w->buildings[i]->id);
        write_fields(f, w->buildings[i], BUILDING, COUNT(BUILDING));
        fputc('\n', f);
    }
    for (i = 0; i < w->villager_count; i++) {
        fprintf(f, "villager=%d", w->villagers[i]->id);
        write_fields(f, w->villagers[i], VILLAGER, COUNT(VILLAGER));
        fputc('\n', f);
    }
    for (i = 0; i < e->animal_count; i++) {
        fprintf(f, "animal=%d", i);
        write_fields(f, &e->animals[i], ANIMAL, COUNT(ANIMAL));
        fputc('\n', f);
    }
    for (i = 0; i < e->candidate_count; i++) {
        fprintf(f, "candidate=%d", i);
        write_fields(f, &e->candidates[i], CANDIDATE, COUNT(CANDIDATE));
        fprintf(f, ",%s\n", e->candidates[i].name);
    }
    for (i = 0; i < e->archive_count; i++) {
        fprintf(f, "archive=%d", i);
        write_fields(f, &e->archive[i], CANDIDATE, COUNT(CANDIDATE));
        fprintf(f, ",%s\n", e->archive[i].name);
    }
    return ferror(f) ? CORE_ERR_FILE_IO : CORE_OK;
}
static int read_int(char **cursor, int32_t *out, int comma) {
    char *end;
    long value;
    if (comma) {
        if (**cursor != ',')
            return 0;
        (*cursor)++;
    }
    if (!**cursor || **cursor == ' ' || **cursor == '\t')
        return 0;
    errno = 0;
    value = strtol(*cursor, &end, 10);
    if (errno || end == *cursor || value < INT32_MIN || value > INT32_MAX)
        return 0;
    *out = (int32_t)value;
    *cursor = end;
    return 1;
}
static int read_fields(char **cursor, void *object, const Field *fields, int count) {
    int i, j;
    for (i = 0; i < count; i++)
        for (j = 0; j < fields[i].count; j++) {
            int32_t value;
            if (!read_int(cursor, &value, 1))
                return 0;
            memcpy((char *)object + fields[i].offset + (size_t)j * 4, &value, 4);
        }
    return 1;
}
int expansion_load_line(World *w, const char *key, char *value) {
    ExpansionState *e = w->expansion;
    int32_t index;
    char *cursor = value;
    if (!e || !read_int(&cursor, &index, 0))
        return CORE_ERR_FORMAT;
    if (strcmp(key, "state") == 0) {
        if (e->load_mask || index != 1 || !read_fields(&cursor, e, STATE, COUNT(STATE)) || *cursor)
            return CORE_ERR_FORMAT;
        if (e->animal_count < 0 || e->animal_count > 300 || e->candidate_count < 0 ||
            e->candidate_count > 15 || e->archive_count < 0 || e->archive_count > 64)
            return CORE_ERR_FORMAT;
        e->load_mask = 1;
        return CORE_OK;
    }
    if (!e->load_mask)
        return CORE_ERR_FORMAT;
    if (strcmp(key, "tile") == 0) {
        if (index < 0 || index >= e->cells || e->visible[index] ||
            !read_fields(&cursor, &e->tiles[index], TILE, COUNT(TILE)) || *cursor)
            return CORE_ERR_FORMAT;
        e->visible[index] = 1;
        e->loaded_cells++;
        return CORE_OK;
    }
    if (strcmp(key, "building") == 0) {
        Building *b = entity_find_building(w, index);
        if (!b || b->expansion_loaded || !read_fields(&cursor, b, BUILDING, COUNT(BUILDING)) || *cursor)
            return CORE_ERR_FORMAT;
        b->expansion_loaded = 1;
        e->loaded_buildings++;
        return CORE_OK;
    }
    if (strcmp(key, "villager") == 0) {
        Villager *v = entity_find_villager(w, index);
        if (!v || v->expansion_loaded || !read_fields(&cursor, v, VILLAGER, COUNT(VILLAGER)) || *cursor)
            return CORE_ERR_FORMAT;
        v->expansion_loaded = 1;
        e->loaded_villagers++;
        return CORE_OK;
    }
    if (strcmp(key, "animal") == 0) {
        if (index != e->loaded_animals || index >= e->animal_count ||
            !read_fields(&cursor, &e->animals[index], ANIMAL, COUNT(ANIMAL)) || *cursor)
            return CORE_ERR_FORMAT;
        e->loaded_animals++;
        return CORE_OK;
    }
    if (strcmp(key, "candidate") == 0 || strcmp(key, "archive") == 0) {
        int archive = strcmp(key, "archive") == 0;
        int *loaded = archive ? &e->loaded_archive : &e->loaded_candidates;
        CoreCandidateView *c;
        if (index != *loaded || index >= (archive ? e->archive_count : e->candidate_count))
            return CORE_ERR_FORMAT;
        c = archive ? &e->archive[index] : &e->candidates[index];
        if (!read_fields(&cursor, c, CANDIDATE, COUNT(CANDIDATE)) || *cursor++ != ',' ||
            !text_is_valid_name(cursor, CORE_NAME_BYTES))
            return CORE_ERR_FORMAT;
        strcpy(c->name, cursor);
        (*loaded)++;
        return CORE_OK;
    }
    return CORE_ERR_FORMAT;
}
int expansion_finish_load(World *w) {
    ExpansionState *e = w->expansion;
    if (!e->load_mask || e->loaded_cells != e->cells || e->loaded_animals != e->animal_count ||
        e->loaded_candidates != e->candidate_count || e->loaded_archive != e->archive_count ||
        e->loaded_buildings != w->building_count || e->loaded_villagers != w->villager_count)
        return CORE_ERR_FORMAT;
    if (expansion_validate(w))
        return CORE_ERR_FORMAT;
    w->bank.individual_limits = 1;
    expansion_capacity(w);
    expansion_water_bodies(w);
    expansion_rebuild(w);
    expansion_visibility(w);
    return CORE_OK;
}
uint64_t expansion_hash(const World *w, uint64_t hash) {
    const ExpansionState *e = w->expansion;
    int i;
    hash = hash_int(hash, 1);
    hash = hash_fields(hash, e, STATE, COUNT(STATE));
    for (i = 0; i < e->cells; i++)
        hash = hash_fields(hash, &e->tiles[i], TILE, COUNT(TILE));
    for (i = 0; i < w->building_count; i++)
        hash = hash_fields(hash, w->buildings[i], BUILDING, COUNT(BUILDING));
    for (i = 0; i < w->villager_count; i++)
        hash = hash_fields(hash, w->villagers[i], VILLAGER, COUNT(VILLAGER));
    for (i = 0; i < e->animal_count; i++)
        hash = hash_fields(hash, &e->animals[i], ANIMAL, COUNT(ANIMAL));
    for (i = 0; i < e->candidate_count + e->archive_count; i++) {
        const CoreCandidateView *c =
            i < e->candidate_count ? &e->candidates[i] : &e->archive[i - e->candidate_count];
        const unsigned char *s = (const unsigned char *)c->name;
        hash = hash_fields(hash, c, CANDIDATE, COUNT(CANDIDATE));
        while (*s) {
            hash ^= *s++;
            hash *= UINT64_C(1099511628211);
        }
    }
    return hash;
}
int expansion_validate(const World *w) {
    const ExpansionState *e = w->expansion;
    int bad = 0, i, j;
    if (!e || e->cells != w->grid.width * w->grid.height || !e->tiles || w->rank < 0 || w->rank > 5 ||
        w->clock.tick < 0 || !world_keep(w))
        return 1;
    bad += e->tax < 1 || e->tax > 4 || e->fuel_milli < 0 || e->fuel_milli >= 2000 ||
           e->magic_logs_milli < 0 || e->magic_logs_milli > expansion_bank_have(w, CORE_RES_LOGS);
    if (e->animal_count < 0 || e->animal_count > 300 || e->candidate_count < 0 || e->candidate_count > 15 ||
        e->archive_count < 0 || e->archive_count > 64)
        return bad + 1;
    bad += e->starter_waves < 0 || e->starter_waves > 2 || e->immigration_day < 0 ||
           e->immigration_day > CLOCK_MAX_DAYS || e->next_person_id < 1 || e->next_animal_id < 1 ||
           e->trade_used < 0 || e->trade_used > 260;
    for (i = 0; i < 15; i++)
        bad += w->bank.amount[i] < 0 || w->bank.amount[i] > ECO_MAX_AMOUNT || e->fractional[i] < 0 ||
               e->fractional[i] >= 1000 || e->trade_stock[i] < 0 || e->trade_stock[i] > 1000000;
    for (i = 0; i < e->cells; i++) {
        const CoreExpansionTileView *t = &e->tiles[i];
        bad += t->biome < 0 || t->biome > 7 || t->tree_kind < 0 || t->tree_kind > 4 || t->tree_age < 0 ||
               t->tree_age > 112000 || t->tree_logs_milli < 0 || t->tree_logs_milli > 50000 ||
               t->regrow_milli < 0 || t->regrow_milli > 60000 || t->tree_used_milli < 0 ||
               t->tree_used_milli > 1000 || t->ore_kind < -1 || t->ore_kind >= 15 || t->ore_milli < 0 ||
               t->ore_milli > 1800000 || t->explored < 0 || t->explored > 1 || w->grid.road[i] > 3;
    }
    for (i = 0; i < w->building_count; i++) {
        const Building *b = w->buildings[i];
        const BuildingDef *d = world_building_def(w, b->type);
        if (!d) {
            bad++;
            continue;
        }
        bad += b->rotation < 0 || b->rotation > 3 || b->w != ((b->rotation & 1) ? d->h : d->w) ||
               b->h != ((b->rotation & 1) ? d->w : d->h) || b->x < 1 || b->y < 1 ||
               b->x + b->w >= w->grid.width || b->y + b->h >= w->grid.height ||
               b->worker_count > d->worker_slots || b->resident_count > d->resident_slots || b->hp < 0 ||
               b->hp > d->max_hp || b->daily_work_milli < 0 || b->crop_labor_milli < 0 ||
               b->pending_harvest_milli < 0 || b->pending_harvest_milli > 10000000 || b->mode < 0 ||
               b->mode > 1;
        for (j = i + 1; j < w->building_count; j++) {
            const Building *a = w->buildings[j];
            bad += b->id == a->id ||
                   (b->x < a->x + a->w && b->x + b->w > a->x && b->y < a->y + a->h && b->y + b->h > a->y);
        }
    }
    for (i = 0; i < w->villager_count; i++) {
        const Villager *v = w->villagers[i];
        int stats[5] = {v->stats.strength, v->stats.agility, v->stats.vitality, v->stats.dexterity,
                        v->stats.intellect};
        bad += v->id <= 0 || v->personal_id <= 0 || v->health < 1 || v->health > 100 || v->mood < 0 ||
               v->mood > 100 || v->current_x_milli < 0 || v->current_x_milli >= w->grid.width * 1000 ||
               v->current_y_milli < 0 || v->current_y_milli >= w->grid.height * 1000 ||
               (v->inside_building && !entity_find_building(w, v->inside_building));
        for (j = 0; j < 5; j++)
            bad += stats[j] < 1 || stats[j] > 10 || v->initial_stats[j] < 1 || v->initial_stats[j] > 9 ||
                   stats[j] > v->initial_stats[j] + 3 || v->stat_xp[j] < 0;
        for (j = 0; j < 12; j++)
            bad += v->skill_minutes[j] < 0;
        if (v->home) {
            int found = 0;
            for (j = 0; j < v->home->resident_count; j++)
                found += v->home->residents[j] == v;
            bad += found != 1;
        }
        if (v->workplace) {
            int found = 0;
            for (j = 0; j < v->workplace->worker_count; j++)
                found += v->workplace->workers[j] == v;
            bad += found != 1;
        }
    }
    for (i = 0; i < e->animal_count && i < 300; i++) {
        const CoreAnimalView *a = &e->animals[i];
        bad += a->kind < 0 || a->kind > 15 || a->id <= 0 || a->alive < 0 || a->alive > 1 || a->x_milli < 0 ||
               a->y_milli < 0 || a->x_milli >= w->grid.width * 1000 || a->y_milli >= w->grid.height * 1000 ||
               (a->owner && !entity_find_building(w, a->owner));
        if (a->kind >= 0 && a->kind < 16) {
            bad += a->hp < 0 ||
                   a->hp > (int[]){12, 30, 28, 10, 22, 32, 38, 65, 28, 36, 9, 36, 120, 80, 95, 16}[a->kind] ||
                   (a->alive && a->hp < 1);
        }
        bad += a->id >= e->next_animal_id || a->age < 0 || a->starve_days < 0 || a->breed_day < 0;
        for (j = i + 1; j < e->animal_count; j++)
            bad += a->id == e->animals[j].id;
    }
    if (w->research.done & 0xff000000u)
        bad++;
    if (w->research.active < -1 || w->research.active >= 24)
        bad++;
    for (i = 0; i < e->candidate_count + e->archive_count; i++) {
        const CoreCandidateView *c =
            i < e->candidate_count ? &e->candidates[i] : &e->archive[i - e->candidate_count];
        bad += c->person_id < 1 || c->person_id >= e->next_person_id || c->fee < 1 || c->fee > 1000000 ||
               c->field < 0 || c->field >= 12 || c->age < 0 || c->age > 120 || c->hired < 0 || c->hired > 1 ||
               !text_is_valid_name(c->name, CORE_NAME_BYTES);
        for (j = 0; j < 5; j++)
            bad += c->initial[j] < 1 || c->initial[j] > 9 || c->stats[j] < c->initial[j] ||
                   c->stats[j] > 10 || c->stats[j] > c->initial[j] + 3 || c->xp[j] < 0 || c->xp[j] > 300000;
        for (j = 0; j < 12; j++)
            bad += c->skills[j] < 0;
        for (j = 0; j < 2; j++)
            bad += c->traits[j] < -1 || c->traits[j] >= w->defs.trait_count;
    }
    return bad;
}
