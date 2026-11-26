/* environment — 현행 기능별 회귀 사례 */
#include "fixtures/services.h"

void test_services_world_and_clay(void) {
    World *w = fx_services_fresh(42);
    CHECK(w);
    CHECK_EQ(w->grid.width, 160);
    CHECK_EQ(w->expansion->water_count, 20);
    int water = 0;
    for (int p = 0; p < w->expansion->cells; p++) {
        CoreExpansionTileView *t = &w->expansion->tiles[p];
        if (w->grid.terrain[p] == CORE_TERRAIN_WATER) {
            water++;
            CHECK_EQ(t->clay_milli, 0);
            CHECK(exp_dist(p % 160, p / 160, w->keep_x, w->keep_y) > 12);
        } else {
            CHECK(t->clay_milli > 0);
            CHECK_EQ(t->clay_milli, t->clay_initial_milli);
        }
    }
    CHECK_EQ(water, 2304);
    CHECK_EQ(core_validate(w), 0);
    Building b = {0};
    b.id = 18;
    b.x = w->keep_x;
    b.y = w->keep_y;
    b.w = b.h = 2;
    int before = 0, after = 0;
    for (int p = 0; p < w->expansion->cells; p++)
        before += w->expansion->tiles[p].clay_milli / 1000;
    CHECK_EQ(expansion_clay_take(w, &b, 4, 8000), 8000);
    int64_t exact = 0;
    for (int p = 0; p < w->expansion->cells; p++) {
        after += w->expansion->tiles[p].clay_milli / 1000;
        exact += w->expansion->tiles[p].clay_initial_milli - w->expansion->tiles[p].clay_milli;
    }
    CHECK_EQ(exact, 8000);
    CHECK(after <= before);
    uint64_t hash, copyhash;
    World *copy = NULL;
    int32_t line = 0;
    core_state_hash(w, &hash);
    CHECK_EQ(core_save(w, "services_roundtrip.sav"), CORE_OK);
    CHECK_EQ(core_load("services_roundtrip.sav", NULL, &copy, &line), CORE_OK);
    core_state_hash(copy, &copyhash);
    CHECK_EQ(hash, copyhash);
    core_destroy(copy);
    core_destroy(w);
    remove("services_roundtrip.sav");
    remove("services_roundtrip.sav.bak");
}
