/* check_v124_rules.c — 분리 난수·서식 추첨·상인 방문/저장 실제 표본 */
#include "expansion.h"
#include "platform.h"
#include "storage.h"
#include "v124_weights_expected.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int cycle(const CoreWanderingMerchantView *m) {
    unsigned char edge[15][15] = {{0}};
    for (int i = 0; i < m->offer_count; i++) {
        CoreBarterRecipe r;
        core_expansion_barter_recipe(m->recipe_ids[i], &r);
        edge[r.pay_resource][r.get_resource] = 1;
    }
    for (int k = 0; k < 15; k++)
        for (int i = 0; i < 15; i++)
            for (int j = 0; j < 15; j++)
                edge[i][j] |= edge[i][k] && edge[k][j];
    for (int i = 0; i < 15; i++)
        if (edge[i][i])
            return 1;
    return 0;
}
int main(int argc, char **argv) {
    const char *directory = argc > 1 ? argv[1] : "artifacts/v124/samples";
    int visits = argc > 2 ? atoi(argv[2]) : 1000;
    if (visits < 1 || visits > 100000)
        return 2;
    game_make_dirs(directory);
    char path[1024];
    snprintf(path, sizeof(path), "%s/species_samples.csv", directory);
    FILE *species = platform_fopen_utf8(path, "wb");
    snprintf(path, sizeof(path), "%s/forecast_samples.csv", directory);
    FILE *forecast = platform_fopen_utf8(path, "wb");
    snprintf(path, sizeof(path), "%s/merchant_samples.csv", directory);
    FILE *merchant = platform_fopen_utf8(path, "wb");
    if (!species || !forecast || !merchant)
        return 2;
    CoreNewGameParams p = {0};
    p.ruleset = 1;
    p.seed = 42;
    strcpy(p.castle_name, u8"전수 표본");
    World *w = NULL;
    if (core_create(&p, NULL, &w) != CORE_OK)
        return 2;
    int errors = 0, seen[85] = {0};
    fputs("biome,kind,expected_pct,observed_pct,samples,error\n", species);
    int tile = grid_index(&w->grid, 10, 10);
    CoreExpansionTileView saved = w->expansion->tiles[tile];
    int terrain = w->grid.terrain[tile], cold = w->expansion->water_cold[0];
    for (int biome = 0; biome < 11; biome++) {
        int count[35] = {0};
        w->expansion->tiles[tile].biome = biome == 10 ? 7 : biome;
        w->expansion->tiles[tile].water_id = biome == 7 || biome == 10 ? 1 : 0;
        w->expansion->water_cold[0] = biome == 7;
        w->grid.terrain[tile] = biome == 7 || biome == 10 ? CORE_TERRAIN_WATER : CORE_TERRAIN_GRASS;
        for (int i = 0; i < 100000; i++) {
            int kind = expansion_wild_kind(w, 10, 10);
            if (kind < 0 || kind >= 35) {
                errors++;
                break;
            }
            count[kind]++;
        }
        for (int kind = 0; kind < 35; kind++) {
            double actual = count[kind] / 1000.0;
            int bad = fabs(actual - EXPECTED_WEIGHTS[biome][kind]) > 1;
            errors += bad;
            fprintf(species, "%d,%d,%d,%.3f,100000,%d\n", biome, kind, EXPECTED_WEIGHTS[biome][kind], actual,
                    bad);
        }
    }
    w->expansion->tiles[tile] = saved;
    w->grid.terrain[tile] = (uint8_t)terrain;
    w->expansion->water_cold[0] = (uint8_t)cold;
    fputs("band,kind,expected_pct,observed_pct,samples,error\n", forecast);
    Rng rng;
    rng_seed(&rng, 124, 245);
    for (int band = 0; band < 4; band++)
        for (int kind = 0; kind < 3; kind++) {
            int chance = expansion_forecast_probability(band, kind), count = 0;
            for (int i = 0; i < 100000; i++)
                count += rng_range(&rng, 0, 99) < chance;
            int bad = abs(count - chance * 1000) > 1000;
            errors += bad;
            fprintf(forecast, "%d,%d,%d,%.3f,100000,%d\n", band, kind, chance, count / 1000.0, bad);
        }
    fputs("visit,rank,offers,mana,cycle,delay_days,save_hash_equal,error\n", merchant);
    for (int i = 0; i < visits; i++) {
        w->rank = i % 5 + 1;
        CoreWanderingMerchantView *m = &w->expansion->wandering;
        memset(m, 0, sizeof(*m));
        m->visit_id = i;
        m->next_target_tick = 1;
        w->clock.tick = 1;
        expansion_merchant_tick(w);
        int bad = m->state == CORE_MERCHANT_DORMANT, offers = m->offer_count, mana = 0, cyc = cycle(m);
        for (int k = 0; k < offers; k++) {
            CoreBarterRecipe r;
            core_expansion_barter_recipe(m->recipe_ids[k], &r);
            bad += r.rank > w->rank;
            mana += r.get_resource == CORE_RES_MANA;
            seen[r.id - 1]++;
            for (int j = k + 1; j < offers; j++)
                bad += m->recipe_ids[k] == m->recipe_ids[j];
        }
        bad +=
            offers != (int[]){0, 3, 4, 5, 5, 6}[w->rank] || mana != (int[]){0, 1, 1, 2, 2, 3}[w->rank] || cyc;
        m->x_milli = m->target_x * 1000 + 500;
        m->y_milli = m->target_y * 1000 + 500;
        expansion_merchant_tick(w);
        bad += m->state != CORE_MERCHANT_TRADING;
        snprintf(path, sizeof(path), "%s/merchant.sav", directory);
        uint64_t before = 0, after = 1;
        World *copy = NULL;
        int32_t line = 0;
        core_state_hash(w, &before);
        int rc = core_save(w, path);
        if (rc == CORE_OK)
            rc = core_load(path, NULL, &copy, &line);
        if (rc == CORE_OK)
            core_state_hash(copy, &after);
        int equal = rc == CORE_OK && before == after;
        bad += !equal;
        core_destroy(copy);
        int arrived = m->arrived_tick;
        w->clock.tick = m->expires_tick;
        expansion_merchant_tick(w);
        m->x_milli = m->entry_x * 1000 + 500;
        m->y_milli = m->entry_y * 1000 + 500;
        expansion_merchant_tick(w);
        int delay = (m->next_target_tick - arrived) / 144;
        bad += delay < 24 || delay > 36;
        errors += bad;
        fprintf(merchant, "%d,%d,%d,%d,%d,%d,%d,%d\n", i + 1, w->rank, offers, mana, cyc, delay, equal, bad);
        if (i % 100 == 99) {
            printf("visits=%d errors=%d\n", i + 1, errors);
            fflush(stdout);
        }
        if (bad) {
            fprintf(stderr, "visit failure i=%d state=%d rc=%d line=%d validate=%d\n", i, m->state, rc, line,
                    core_validate(w));
            break;
        }
    }
    snprintf(path, sizeof(path), "%s/recipe_frequency.csv", directory);
    FILE *frequency = platform_fopen_utf8(path, "wb");
    if (frequency) {
        fputs("recipe,offers\n", frequency);
        for (int id = 0; id < 85; id++)
            fprintf(frequency, "%d,%d\n", id + 1, seen[id]);
        fclose(frequency);
    }
    fclose(species);
    fclose(forecast);
    fclose(merchant);
    core_destroy(w);
    printf("v124 samples errors=%d\n", errors);
    return errors ? 1 : 0;
}
