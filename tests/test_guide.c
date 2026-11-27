/* test_guide.c — 튜토리얼 안내: 단계 진행, 권하는 자리, 습격 미루기, 저장 */
#include "expansion.h"
#include "test_util.h"
#include "tutorial.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 튜토리얼 게임과 같은 지도 (game/src/ui/tutorial.c 의 시드·난이도) */
#define GUIDE_TEST_SEED 42

static CoreGuideView guide(World *w) {
    CoreGuideView g;
    memset(&g, 0, sizeof(g));
    (void)core_guide_view(w, &g);
    return g;
}

/* 지금 단계가 권하는 자리에 그 건물을 짓습니다 */
static int build_at_hint(World *w) {
    CoreGuideView g = guide(w);
    int32_t id = 0, reason = 0, rc;
    if (g.target != CORE_GUIDE_TARGET_BUILD || g.hint_x < 0)
        return 0;
    rc = core_place_building(w, g.target_id, g.hint_x, g.hint_y, &id, &reason);
    if (rc != CORE_OK)
        fprintf(stderr, "  step %d: type %d at %d,%d rc=%d reason=%d\n", (int)g.step, (int)g.target_id,
                (int)g.hint_x, (int)g.hint_y, (int)rc, (int)reason);
    return rc == CORE_OK;
}

static int road_ok(const World *w, int32_t x, int32_t y) {
    CorePlacementView v;
    if (core_can_place(w, CORE_BLD_ROAD, x, y, &v) != CORE_OK)
        return 0;
    return v.result == CORE_PLACE_OK || v.result == CORE_PLACE_ALREADY_THERE;
}

/* (x0,y0) 에서 (x1,y1) 까지 흙길을 놓을 수 있는 칸으로 가장 짧은 길을 찾아 한 칸씩 놓습니다 */
static int connect_with_road(World *w, int32_t x0, int32_t y0, int32_t x1, int32_t y1) {
    static const int DX[4] = {1, -1, 0, 0}, DY[4] = {0, 0, 1, -1};
    int32_t width = 0, height = 0, head = 0, tail = 0, found = 0;
    if (core_get_map_size(w, &width, &height) != CORE_OK)
        return 0;
    int32_t cells = width * height, *from = malloc((size_t)cells * sizeof(int32_t)),
            *queue = malloc((size_t)cells * sizeof(int32_t));
    if (!from || !queue) {
        free(from);
        free(queue);
        return 0;
    }
    for (int32_t i = 0; i < cells; i++)
        from[i] = -2;
    from[y0 * width + x0] = -1;
    queue[tail++] = y0 * width + x0;
    while (head < tail && !found) {
        int32_t p = queue[head++];
        for (int k = 0; k < 4; k++) {
            int32_t x = p % width + DX[k], y = p / width + DY[k], n = y * width + x;
            if (x < 0 || y < 0 || x >= width || y >= height || from[n] != -2 || !road_ok(w, x, y))
                continue;
            from[n] = p;
            queue[tail++] = n;
            if (x == x1 && y == y1)
                found = 1;
        }
    }
    int ok = found;
    for (int32_t p = y1 * width + x1; found && p >= 0; p = from[p]) {
        int32_t placed = 0, reason = 0;
        if (core_place_line(w, CORE_BLD_ROAD, p % width, p / width, p % width, p / width, &placed, &reason) !=
                CORE_OK &&
            reason != CORE_PLACE_ALREADY_THERE)
            ok = 0;
    }
    free(from);
    free(queue);
    return ok;
}

/* 안내가 표시하는 대로 영주관 문 앞에서 건물 입구까지 도로를 잇습니다 (연결 도로가 모자라면 다음 건물도) */
static int lay_roads(World *w) {
    CoreGuideView g = guide(w);
    for (int i = 0; i < 6 && g.step == GUIDE_ROADS; i++) {
        if (g.goal_x < 0 || !connect_with_road(w, g.hint_x, g.hint_y, g.goal_x, g.goal_y))
            return 0;
        if (core_tick(w, 1) != CORE_OK) /* 도로망은 다음 틱에 다시 셉니다 */
            return 0;
        g = guide(w);
    }
    return g.step > GUIDE_ROADS;
}

void test_guide_start_and_info_steps(void) {
    World *w = test_make_world(GUIDE_TEST_SEED, CORE_DIFF_PEACEFUL);
    CHECK(w);
    CoreGuideView g = guide(w);
    CHECK_EQ(g.active, 0); /* 일반 게임 */
    CHECK_EQ(g.count, GUIDE_STEP_COUNT);
    CHECK_EQ(core_guide_next(w), CORE_ERR_REJECTED);
    CHECK_EQ(core_guide_skip(w), CORE_ERR_REJECTED);
    CHECK_EQ(core_guide_view(NULL, &g), CORE_ERR_NULL_ARG);
    CHECK_EQ(core_guide_start(w), CORE_OK);
    CHECK_EQ(core_guide_start(w), CORE_ERR_REJECTED); /* 두 번 켜지 않음 */
    g = guide(w);
    CHECK_EQ(g.active, 1);
    CHECK_EQ(g.step, GUIDE_WELCOME);
    CHECK_EQ(g.kind, CORE_GUIDE_INFO);
    CHECK_EQ(g.pause, 1);
    CHECK(g.title[0] && g.body[0]);
    CHECK_EQ(core_guide_next(w), CORE_OK);
    CHECK_EQ(guide(w).target_id, CORE_GUIDE_UI_MAP);
    CHECK_EQ(core_guide_next(w), CORE_OK);
    CHECK_EQ(guide(w).target_id, CORE_GUIDE_UI_SPEED);
    CHECK_EQ(core_guide_next(w), CORE_OK);
    g = guide(w);
    CHECK_EQ(g.step, GUIDE_GARDEN);
    CHECK_EQ(g.kind, CORE_GUIDE_ACTION);
    CHECK_EQ(g.target, CORE_GUIDE_TARGET_BUILD);
    CHECK_EQ(g.target_id, CORE_BLD_GARDEN);
    CHECK(g.hint_x >= 0 && g.hint_y >= 0 && g.hint_w == 2 && g.hint_h == 2);
    CHECK_EQ(core_guide_next(w), CORE_ERR_REJECTED); /* 행동 단계는 해내야 넘어감 */
    CHECK_EQ(test_validate_verbose(w), 0);
    CHECK_EQ(core_guide_skip(w), CORE_OK);
    CHECK_EQ(guide(w).active, 0);
    CHECK_EQ(test_validate_verbose(w), 0);
    core_destroy(w);

    /* 하루가 지난 게임에서는 켜지 않음 */
    w = test_make_world(GUIDE_TEST_SEED, CORE_DIFF_PEACEFUL);
    CHECK(w);
    CHECK_EQ(core_tick(w, 144), CORE_OK);
    CHECK_EQ(core_guide_start(w), CORE_ERR_REJECTED);
    core_destroy(w);
}

/* 공개 API만으로 튜토리얼을 처음부터 끝까지 (권하는 자리가 실제로 지을 수 있는 자리인지도 확인) */
void test_guide_full_playthrough(void) {
    World *w = test_make_world(GUIDE_TEST_SEED, CORE_DIFF_PEACEFUL);
    CHECK(w);
    CHECK_EQ(core_guide_start(w), CORE_OK);
    for (int i = 0; i < 3; i++)
        CHECK_EQ(core_guide_next(w), CORE_OK);
    CHECK_EQ(guide(w).step, GUIDE_GARDEN);
    CHECK(build_at_hint(w));
    CHECK_EQ(guide(w).step, GUIDE_SHACK);
    CHECK(build_at_hint(w));
    CoreGuideView g = guide(w);
    CHECK_EQ(g.step, GUIDE_WELL); /* 처음부터 있는 우물을 가리키는 정보 단계 */
    CHECK_EQ(g.target, CORE_GUIDE_TARGET_BUILDING);
    CHECK(entity_find_building(w, g.target_id) &&
          entity_find_building(w, g.target_id)->type == CORE_BLD_WELL);
    CHECK_EQ(core_guide_next(w), CORE_OK);
    CHECK_EQ(guide(w).step, GUIDE_LOGGING);
    CHECK(build_at_hint(w));
    g = guide(w);
    CHECK_EQ(g.step, GUIDE_ASSIGN);
    CHECK_EQ(g.target, CORE_GUIDE_TARGET_BUILDING);
    Building *work = entity_find_building(w, g.target_id);
    CHECK(work);
    for (int t = 0; t < 144 * 3 && work->state != CORE_BSTATE_ACTIVE; t += 6)
        CHECK_EQ(core_tick(w, 6), CORE_OK);
    CHECK_EQ(work->state, CORE_BSTATE_ACTIVE);
    int assigned = 0;
    for (int i = 0; i < w->villager_count && !assigned; i++)
        assigned = core_assign_worker(w, w->villagers[i]->id, work->id) == CORE_OK;
    CHECK(assigned);
    g = guide(w);
    CHECK_EQ(g.step, GUIDE_ROADS);
    CHECK_EQ(g.target, CORE_GUIDE_TARGET_LINE);
    CHECK_EQ(g.progress_need, 10);
    CHECK(lay_roads(w));
    CHECK_EQ(guide(w).step, GUIDE_RESOURCES);
    CHECK(guide_holds_raids(w));
    CHECK_EQ(core_guide_next(w), CORE_OK);
    g = guide(w);
    CHECK_EQ(g.step, GUIDE_DEFENSE);
    CHECK_EQ(g.target_id, CORE_BLD_OUTPOST);
    CHECK_EQ(g.progress, 0);
    CHECK(build_at_hint(w));
    g = guide(w);
    CHECK_EQ(g.step, GUIDE_DEFENSE);
    CHECK_EQ(g.target_id, CORE_BLD_PALISADE); /* 초소 다음은 목책 */
    CHECK_EQ(g.progress, 1);
    CHECK(build_at_hint(w));
    CHECK_EQ(guide(w).step, GUIDE_RANK_PANEL);
    CHECK(!guide_holds_raids(w)); /* 방어를 마치면 습격이 다시 옴 */
    CHECK_EQ(core_guide_next(w), CORE_OK);
    g = guide(w);
    CHECK_EQ(g.step, GUIDE_PROMOTE);
    CHECK_EQ(g.progress_need, 8);
    CHECK_EQ(test_validate_verbose(w), 0);
    w->rank = 1; /* 승급 조건을 채우는 며칠은 다른 테스트가 봅니다 */
    g = guide(w);
    CHECK_EQ(g.step, GUIDE_DONE);
    CHECK_EQ(g.kind, CORE_GUIDE_INFO);
    CHECK_EQ(core_guide_next(w), CORE_OK);
    g = guide(w);
    CHECK_EQ(g.active, 1);
    CHECK_EQ(g.finished, 1);
    CHECK_EQ(g.step, GUIDE_STEP_COUNT);
    CHECK_EQ(core_guide_next(w), CORE_ERR_REJECTED);
    CHECK_EQ(test_validate_verbose(w), 0);
    core_destroy(w);
}

/* 방어 단계 전에는 습격 차례가 와도 하루씩 미루고, 안내를 끄면 다시 옴 */
void test_guide_holds_raids_until_defense(void) {
    World *w = test_make_world(GUIDE_TEST_SEED, CORE_DIFF_PEACEFUL);
    CHECK(w);
    CHECK_EQ(core_guide_start(w), CORE_OK);
    CHECK(guide_holds_raids(w));
    w->expansion->next_raid_tick = w->clock.tick + 1;
    CHECK_EQ(core_tick(w, 2), CORE_OK);
    CHECK_EQ(w->threat.raid.id, 0);
    CHECK(w->expansion->next_raid_tick > w->clock.tick + 100);
    CHECK_EQ(core_guide_skip(w), CORE_OK);
    CHECK(!guide_holds_raids(w));
    w->expansion->next_raid_tick = w->clock.tick + 1;
    CHECK_EQ(core_tick(w, 2), CORE_OK);
    CHECK(w->threat.raid.id != 0);
    core_destroy(w);
}

/* 저장 → 불러오기 후 같은 단계, 같은 해시. 일반 게임의 해시는 안내 상태와 상관없음 */
void test_guide_save_roundtrip(void) {
    char path[160];
    World *w = test_make_world(GUIDE_TEST_SEED, CORE_DIFF_PEACEFUL), *loaded = NULL;
    uint64_t before = 0, after = 0, plain = 0;
    int32_t line = 0;
    CHECK(w);
    CHECK_EQ(core_state_hash(w, &plain), CORE_OK);
    CHECK_EQ(core_guide_start(w), CORE_OK);
    CHECK_EQ(core_guide_next(w), CORE_OK);
    CHECK_EQ(core_guide_next(w), CORE_OK);
    CHECK_EQ(core_state_hash(w, &before), CORE_OK);
    CHECK(before != plain);
    (void)snprintf(path, sizeof(path), "%s.sav", test_current_name());
    CHECK_EQ(core_save(w, path), CORE_OK);
    CHECK_EQ(core_load(path, NULL, &loaded, &line), CORE_OK);
    CHECK(loaded);
    CHECK_EQ(core_state_hash(loaded, &after), CORE_OK);
    CHECK(before == after);
    CHECK_EQ(guide(loaded).step, GUIDE_TIME);
    CHECK_EQ(guide(loaded).active, 1);
    CHECK_EQ(core_guide_next(loaded), CORE_OK);
    CHECK_EQ(guide(loaded).step, GUIDE_GARDEN);
    core_destroy(loaded);
    core_destroy(w);
    (void)remove(path);
}
