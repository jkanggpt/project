/* boats — 현행 기능별 회귀 사례 */
#include "fixtures/services.h"

void test_services_bridge_and_boats(void) {
    World *w = fx_services_fresh(16);
    CHECK(w);
    Grid *g = &w->grid;
    static const int dx[4] = {0, 1, 0, -1}, dy[4] = {-1, 0, 1, 0};
    int center = grid_index(g, 20, 20);
    for (int mask = 0; mask < 16; mask++) {
        g->terrain[center] = CORE_TERRAIN_WATER;
        g->road[center] = 3;
        for (int d = 0; d < 4; d++) {
            int p = grid_index(g, 20 + dx[d], 20 + dy[d]);
            g->terrain[p] = CORE_TERRAIN_WATER;
            g->road[p] = (mask & (1 << d)) ? 3 : 0;
            g->occupant[p] = 0;
        }
        CHECK_EQ(core_expansion_bridge_mask(w, 20, 20), (~mask) & 15);
    }
    g->terrain[center] = CORE_TERRAIN_GRASS;
    CHECK_EQ(core_expansion_bridge_mask(w, 20, 20), 0);
    g->road[center] = 0;
    Building *b = fx_services_facility(w, CORE_BLD_PORT, 30, 30);
    CHECK(b);
    for (int y = 28; y < 48; y++)
        for (int x = 30; x < 56; x++) {
            int p = grid_index(g, x, y);
            g->terrain[p] = x >= b->x + b->w ? CORE_TERRAIN_WATER : CORE_TERRAIN_GRASS;
            g->road[p] = 0;
            g->occupant[p] = 0;
            w->expansion->tiles[p].tree_kind = 0;
            w->expansion->tiles[p].tree_logs_milli = 0;
        }
    expansion_water_bodies(w);
    expansion_rebuild(w);
    int water = expansion_adjacent_water_id(w, b);
    CHECK(water > 0);
    w->clock.tick = 54;
    for (int n = 1; n <= 4; n++) {
        Villager *v = w->villagers[n - 1];
        CHECK_EQ(entity_link_worker(b, v, 4), CORE_OK);
        v->at_target = 1;
        v->state = CORE_VS_WORKING;
        v->target = b;
        v->inside_building = b->id;
        v->current_x_milli = b->door_x * 1000 + 500;
        v->current_y_milli = b->door_y * 1000 + 500;
    }
    expansion_boats_tick(w);
    CHECK_EQ(w->expansion->boat_count, 2);
    CHECK_EQ(w->expansion->boats[0].crew_count, 2);
    CHECK_EQ(w->expansion->boats[1].crew_count, 2);
    for (int tick = 0; tick < 40; tick++) {
        w->clock.tick++;
        expansion_boats_tick(w);
        for (int i = 0; i < 2; i++) {
            CoreBoatView *boat = &w->expansion->boats[i];
            int p = grid_index(g, boat->x_milli / 1000, boat->y_milli / 1000);
            CHECK_EQ(g->terrain[p], CORE_TERRAIN_WATER);
            CHECK_EQ(g->road[p], 0);
        }
    }
    for (int i = 0; i < 4; i++)
        CHECK(w->villagers[i]->boat_port_id == b->id);
    w->clock.tick = 108;
    for (int tick = 0; tick < 200; tick++) {
        w->clock.tick++;
        expansion_boats_tick(w);
    }
    for (int i = 0; i < 4; i++)
        CHECK_EQ(w->villagers[i]->boat_port_id, 0);
    core_destroy(w);
}
