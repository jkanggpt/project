/*
 * tutorial.c — 튜토리얼 안내 (설명은 tutorial.h)
 *
 * 단계 표 GUIDE_STEPS 의 한 줄이 한 단계입니다. 행동 단계의 done 함수가 1을 돌려주면 다음 단계로 넘어갑니다.
 * 화면은 core_guide_view 로 지금 단계를 받아 안내 창과 강조 표시를 그리기만 합니다.
 */
#include "tutorial.h"
#include "clock.h"
#include "expansion.h"
#include "world.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TUTORIAL_FLAG_ASSIGNED 1
#define GUIDE_ROADS_NEEDED 10
#define GUIDE_VILLAGERS_NEEDED 8
#define GUIDE_HINT_MIN_RING 3
#define GUIDE_HINT_MAX_RING 16

void tutorial_note_assign(World *world) { world->tutorial_flags |= TUTORIAL_FLAG_ASSIGNED; }

/* ---- 확인 함수 ---------------------------------------------------------- */

/* 그 종류의 건물을 하나라도 놓았는가 (짓는 중 포함, 무너진 것 제외) */
static int has_placed(const World *w, int32_t type) {
    for (int32_t i = 0; i < w->building_count; i++) {
        const Building *b = w->buildings[i];
        if (b->type == type && b->state != CORE_BSTATE_RUINED)
            return 1;
    }
    return 0;
}
static int placed_garden(const World *w) { return has_placed(w, CORE_BLD_GARDEN); }
static int placed_shack(const World *w) { return has_placed(w, CORE_BLD_SHACK); }
static int placed_logging(const World *w) { return has_placed(w, CORE_BLD_LOGGING_SITE); }
static int assigned_by_hand(const World *w) {
    for (int32_t i = 0; i < w->villager_count; i++) {
        const Villager *v = w->villagers[i];
        if (v->workplace && (v->flags & CORE_VF_MANUAL))
            return 1;
    }
    return 0;
}
static int32_t roads_progress(const World *w, int32_t *need) {
    *need = GUIDE_ROADS_NEEDED;
    return w->expansion ? w->expansion->effective_roads : 0;
}
static int roads_connected(const World *w) {
    int32_t need;
    return roads_progress(w, &need) >= need;
}
static int32_t defense_progress(const World *w, int32_t *need) {
    *need = 2;
    return has_placed(w, CORE_BLD_OUTPOST) + has_placed(w, CORE_BLD_PALISADE);
}
static int defense_ready(const World *w) {
    int32_t need;
    return defense_progress(w, &need) >= need;
}
static int32_t permanent_progress(const World *w, int32_t *need) {
    int32_t count = 0;
    *need = GUIDE_VILLAGERS_NEEDED;
    for (int32_t i = 0; i < w->villager_count; i++) {
        const Villager *v = w->villagers[i];
        count += !v->contract_end_tick && !v->incoming && v->state != CORE_VS_LEAVING;
    }
    return count;
}
/* 등급 0 = 개척지, 1 = 촌락 */
static int promoted(const World *w) { return w->rank >= 1; }
/* 방어 단계: 초소가 없으면 초소부터, 다음은 목책 */
static int32_t defense_build_type(const World *w) {
    return has_placed(w, CORE_BLD_OUTPOST) ? CORE_BLD_PALISADE : CORE_BLD_OUTPOST;
}

/* 일꾼을 보낼 일터: 다 지은 텃밭·벌목지 중 먼저 지은 것, 없으면 짓는 중인 것 */
static const Building *assign_target(const World *w) {
    const Building *best = NULL;
    for (int32_t i = 0; i < w->building_count; i++) {
        const Building *b = w->buildings[i];
        if ((b->type != CORE_BLD_GARDEN && b->type != CORE_BLD_LOGGING_SITE) ||
            b->state == CORE_BSTATE_RUINED)
            continue;
        if (!best || (b->state == CORE_BSTATE_ACTIVE && best->state != CORE_BSTATE_ACTIVE))
            best = b;
    }
    return best;
}
/* 처음부터 영주관 옆에 있는 우물 */
static const Building *first_well(const World *w) {
    for (int32_t i = 0; i < w->building_count; i++)
        if (w->buildings[i]->type == CORE_BLD_WELL && w->buildings[i]->state != CORE_BSTATE_RUINED)
            return w->buildings[i];
    return NULL;
}

/* ---- 단계 표 ------------------------------------------------------------- */

typedef int (*GuideDoneFn)(const World *w);
typedef int32_t (*GuideProgressFn)(const World *w, int32_t *need);
typedef int32_t (*GuideTypeFn)(const World *w);
typedef const Building *(*GuideBuildingFn)(const World *w);

typedef struct GuideStepDef {
    const char *title;
    const char *body;
    int32_t kind;   /* CoreGuideKind */
    int32_t pause;  /* 읽는 동안 시간을 멈추기를 권함 */
    int32_t target; /* CoreGuideTarget */
    int32_t target_id;
    GuideDoneFn done;         /* 행동 단계: 해냈는가 */
    GuideProgressFn progress; /* 진행 표시 (없으면 NULL) */
    const char *progress_label;
    GuideTypeFn build_type;   /* 지을 건물을 그때그때 정함 (없으면 target_id) */
    GuideBuildingFn building; /* 눌러 볼 건물 (BUILDING 목표) */
} GuideStepDef;

static const GuideStepDef GUIDE_STEPS[GUIDE_STEP_COUNT] = {
    [GUIDE_WELCOME] =
        {u8"튜토리얼 영지에 오신 것을 환영합니다",
         u8"이 땅의 새 영주가 되었습니다. 이 안내는 일터와 집을 짓고, 주민을 일하게 하고, 영지를 "
         u8"촌락으로 키우는 법을 하나씩 보여 줍니다. 안내를 읽는 동안에는 시간이 멈춥니다. 언제든 "
         u8"'건너뛰기'로 안내를 끄고 자유롭게 할 수 있습니다.",
         CORE_GUIDE_INFO, 1, CORE_GUIDE_TARGET_NONE, 0, NULL, NULL, NULL, NULL, NULL},
    [GUIDE_CAMERA] =
        {u8"화면 움직이기",
         u8"W·A·S·D나 화살표로 화면을 움직이고, 마우스 휠로 확대·축소합니다. 가운데 단추를 누른 채 "
         u8"끌어도 됩니다. 영주관 둘레를 한 번 둘러보세요. 화면을 움직이면 다음으로 넘어갑니다.",
         CORE_GUIDE_INFO, 1, CORE_GUIDE_TARGET_UI, CORE_GUIDE_UI_MAP, NULL, NULL, NULL, NULL, NULL},
    [GUIDE_TIME] =
        {u8"시간 다루기",
         u8"위쪽의 단추나 스페이스로 시간을 멈추고 다시 흐르게 합니다. 1·2·3은 배속입니다. 멈춘 채로도 "
         u8"건물을 짓고 주민을 고를 수 있습니다. 하루는 1배속으로 2분입니다.",
         CORE_GUIDE_INFO, 0, CORE_GUIDE_TARGET_UI, CORE_GUIDE_UI_SPEED, NULL, NULL, NULL, NULL, NULL},
    [GUIDE_GARDEN] =
        {u8"텃밭 짓기",
         u8"먼저 먹을 것이 필요합니다. 아래 건설 막대에서 '농업' 탭을 누르고 '텃밭' 카드를 고른 뒤, "
         u8"지도에 표시한 자리를 왼쪽 클릭하세요. 미리보기가 초록이면 지을 수 있습니다. 텃밭은 매일 "
         u8"식량을 만듭니다 (겨울에는 절반).",
         CORE_GUIDE_ACTION, 0, CORE_GUIDE_TARGET_BUILD, CORE_BLD_GARDEN, placed_garden, NULL, NULL, NULL,
         NULL},
    [GUIDE_SHACK] = {u8"막사 짓기",
                     u8"주민이 잘 곳이 필요합니다. '주거' 탭의 '막사'를 지으세요. 지을 때 R을 누르면 방향을 "
                     u8"돌릴 수 있습니다.",
                     CORE_GUIDE_ACTION, 0, CORE_GUIDE_TARGET_BUILD, CORE_BLD_SHACK, placed_shack, NULL, NULL,
                     NULL, NULL},
    [GUIDE_WELL] =
        {u8"우물과 물",
         u8"영주관 옆의 우물(표시한 곳)이 반경 8칸 안의 주민 18명에게 물을 줍니다. 집과 일터는 우물 "
         u8"가까이에 두세요. 주민이 늘면 '의료·위생' 탭에서 우물을 더 지어야 합니다. 우물은 촌락으로 "
         u8"오르는 조건이기도 합니다.",
         CORE_GUIDE_INFO, 1, CORE_GUIDE_TARGET_BUILDING, 0, NULL, NULL, NULL, NULL, first_well},
    [GUIDE_LOGGING] =
        {u8"벌목지 짓기",
         u8"'임업' 탭의 '벌목지'를 숲 가까이에 지으세요. 원목은 거의 모든 건물에 들어갑니다. 원목과 "
         u8"제재소에서 다듬은 목재는 서로 다른 자원입니다.",
         CORE_GUIDE_ACTION, 0, CORE_GUIDE_TARGET_BUILD, CORE_BLD_LOGGING_SITE, placed_logging, NULL, NULL,
         NULL, NULL},
    [GUIDE_ASSIGN] =
        {u8"일꾼 직접 보내기",
         u8"건물이 다 지어지면 놀던 주민이 알아서 일하러 갑니다. 직접 정할 수도 있습니다: 표시한 "
         u8"일터를 누르면 오른쪽에 검사창이 열립니다. 일꾼 배치에서 주민을 골라 보내세요. 아직 짓는 "
         u8"중이면 시간을 흘려 다 지어지기를 기다리세요.",
         CORE_GUIDE_ACTION, 0, CORE_GUIDE_TARGET_BUILDING, 0, assigned_by_hand, NULL, NULL, NULL,
         assign_target},
    [GUIDE_ROADS] =
        {u8"도로 잇기",
         u8"주민은 도로 위에서 빨리 걷고, 촌락부터는 건물 입구가 영주관 도로망에 이어져야 합니다. "
         u8"'도로' 탭의 '흙길'을 고르고, 영주관 문 앞(표시한 칸)에서 일터의 입구(두 번째 표시)까지 끌어 "
         u8"이으세요. 영주관 문과 건물 입구 사이에 이어진 도로만 셉니다. 촌락이 되려면 10칸이 필요하니, "
         u8"모자라면 다른 건물 입구도 이으세요.",
         CORE_GUIDE_ACTION, 0, CORE_GUIDE_TARGET_LINE, CORE_BLD_ROAD, roads_connected, roads_progress,
         u8"연결 도로", NULL, NULL},
    [GUIDE_RESOURCES] =
        {u8"자원과 장부",
         u8"위쪽 막대의 숫자가 영지의 자원입니다. 하루에 쓰는 양보다 적으면 빨갛게 바뀝니다. 숫자에 "
         u8"마우스를 올리면 자세한 내용이, '장부' 단추에는 어제 들어오고 나간 내역이 있습니다. "
         u8"주민 한 명은 하루에 식량 1개를 먹습니다.",
         CORE_GUIDE_INFO, 1, CORE_GUIDE_TARGET_UI, CORE_GUIDE_UI_RESOURCES, NULL, NULL, NULL, NULL, NULL},
    [GUIDE_DEFENSE] =
        {u8"방어 준비",
         u8"가끔 적이 영지를 습격합니다. '방어탑' 탭의 '초소'와 '성벽·성문' 탭의 '목책'을 지으세요. "
         u8"초소의 경비가 적을 발견하면 경보가 울리고, 주민은 가까운 보호구역으로 피신합니다. 이 "
         u8"튜토리얼에서는 이 단계를 마칠 때까지 습격이 오지 않습니다.",
         CORE_GUIDE_ACTION, 0, CORE_GUIDE_TARGET_BUILD, CORE_BLD_OUTPOST, defense_ready, defense_progress,
         u8"지은 방어 시설", defense_build_type, NULL},
    [GUIDE_RANK_PANEL] =
        {u8"영지 창",
         u8"위쪽 막대의 '영지' 단추를 누르면 다음 등급의 조건이 나옵니다. 초록은 채운 조건, 빨강은 "
         u8"모자란 조건입니다. 주민에게 받는 세금도 여기서 정합니다.",
         CORE_GUIDE_INFO, 1, CORE_GUIDE_TARGET_UI, CORE_GUIDE_UI_RANK, NULL, NULL, NULL, NULL, NULL},
    [GUIDE_PROMOTE] =
        {u8"촌락으로 승급",
         u8"영구 주민 8명, 평균 행복 40, 연결 도로 10칸, 식량 8일분, 금화 20, 우물을 갖추면 영지 창의 "
         u8"'승급하기'를 누르세요. 이주민은 5~8일째와 10~13일째에 두 명씩 옵니다. 2·3배속으로 "
         u8"시간을 흘려 보내도 됩니다.",
         CORE_GUIDE_ACTION, 0, CORE_GUIDE_TARGET_UI, CORE_GUIDE_UI_RANK, promoted, permanent_progress,
         u8"영구 주민", NULL, NULL},
    [GUIDE_DONE] =
        {u8"튜토리얼 완료",
         u8"촌락이 되었습니다! 안내는 여기까지입니다. 이제 제재소·경작지·저장고로 영지를 넓히세요. 도시가 "
         u8"되면 연구를 시작할 수 있고, 시장과 떠돌이 상인과 거래할 수 있습니다. 조작은 F1 도움말에서 "
         u8"언제든 다시 볼 수 있습니다.",
         CORE_GUIDE_INFO, 1, CORE_GUIDE_TARGET_NONE, 0, NULL, NULL, NULL, NULL, NULL},
};

/* ---- 진행 ---------------------------------------------------------------- */

static int guide_running(const World *w) { return w->guide.active && w->guide.step < GUIDE_STEP_COUNT; }

void guide_tick(World *w) {
    while (guide_running(w)) {
        const GuideStepDef *s = &GUIDE_STEPS[w->guide.step];
        if (s->kind != CORE_GUIDE_ACTION || !s->done(w))
            return;
        w->guide.step++;
    }
}

int guide_holds_raids(const World *w) { return w->guide.active && w->guide.step <= GUIDE_DEFENSE; }

/* ---- 권하는 자리 -------------------------------------------------------- */

/* 영주관 둘레를 안쪽 고리부터 시계 방향으로 돌며 처음으로 지을 수 있는 자리를 찾습니다.
 * 자원이 모자란 것은 괜찮습니다 (자리만 알려 줌). */
static int find_spot(const World *w, int32_t type, int32_t *out_x, int32_t *out_y) {
    const Building *keep = world_keep(w);
    CorePlacementView view;
    if (!keep)
        return 0;
    int32_t cx = keep->x + keep->w / 2, cy = keep->y + keep->h / 2;
    for (int32_t r = GUIDE_HINT_MIN_RING; r <= GUIDE_HINT_MAX_RING; r++) {
        for (int32_t k = 0; k < 8 * r; k++) {
            int32_t side = k / (2 * r), t = k % (2 * r), x, y;
            if (side == 0) {
                x = cx - r + t;
                y = cy - r;
            } else if (side == 1) {
                x = cx + r;
                y = cy - r + t;
            } else if (side == 2) {
                x = cx + r - t;
                y = cy + r;
            } else {
                x = cx - r;
                y = cy + r - t;
            }
            expansion_place_check(w, type, x, y, 0, 0, &view);
            if (view.result == CORE_PLACE_OK || view.result == CORE_PLACE_CANNOT_AFFORD) {
                *out_x = x;
                *out_y = y;
                return 1;
            }
        }
    }
    return 0;
}

/* 도로를 이을 건물: 아직 영주관 도로망에 입구가 닿지 않은 다 지은 시설 가운데 영주관 문에서 가장 먼 것.
 * 멀수록 이어진 도로가 많이 세어집니다. */
static const Building *unconnected_door(const World *w) {
    const Building *keep = world_keep(w), *best = NULL;
    const ExpansionState *e = w->expansion;
    int32_t best_distance = -1;
    if (!keep || !e)
        return NULL;
    for (int32_t i = 0; i < w->building_count; i++) {
        const Building *b = w->buildings[i];
        int32_t distance;
        if (b == keep || b->state != CORE_BSTATE_ACTIVE ||
            (expansion_spec(b->type)->flags & (CORE_EXP_WALL | CORE_EXP_GATE | CORE_EXP_ROAD)) ||
            !grid_in_bounds(&w->grid, b->door_x, b->door_y) ||
            e->connected[grid_index(&w->grid, b->door_x, b->door_y)])
            continue;
        distance = abs(b->door_x - keep->door_x) + abs(b->door_y - keep->door_y);
        if (distance > best_distance) {
            best_distance = distance;
            best = b;
        }
    }
    return best;
}

static void fill_target(World *w, const GuideStepDef *s, CoreGuideView *out) {
    out->target = s->target;
    out->target_id = s->target_id;
    out->hint_x = out->hint_y = out->goal_x = out->goal_y = -1;
    out->hint_w = out->hint_h = 1;
    if (s->target == CORE_GUIDE_TARGET_BUILD) {
        int32_t type = s->build_type ? s->build_type(w) : s->target_id;
        CoreExpansionBuildingInfo info;
        GuideState *g = &w->guide;
        out->target_id = type;
        if (core_expansion_building_info(type, &info) == CORE_OK) {
            out->hint_w = info.w;
            out->hint_h = info.h;
        }
        if (!g->hint_valid || g->hint_step != g->step || g->hint_type != type ||
            g->hint_revision != w->grid.cost_rev) {
            g->hint_valid = 1;
            g->hint_step = g->step;
            g->hint_type = type;
            g->hint_revision = w->grid.cost_rev;
            if (!find_spot(w, type, &g->hint_x, &g->hint_y))
                g->hint_x = g->hint_y = -1;
        }
        out->hint_x = g->hint_x;
        out->hint_y = g->hint_y;
    } else if (s->target == CORE_GUIDE_TARGET_BUILDING) {
        const Building *b = s->building ? s->building(w) : NULL;
        if (b) {
            out->target_id = b->id;
            out->hint_x = b->x;
            out->hint_y = b->y;
            out->hint_w = b->w;
            out->hint_h = b->h;
        }
    } else if (s->target == CORE_GUIDE_TARGET_LINE) {
        const Building *keep = world_keep(w), *goal = unconnected_door(w);
        if (keep) {
            out->hint_x = keep->door_x;
            out->hint_y = keep->door_y;
        }
        if (goal) {
            out->goal_x = goal->door_x;
            out->goal_y = goal->door_y;
        }
    }
}

/* ---- 공개 API ------------------------------------------------------------ */

CORE_API int32_t core_guide_start(CoreWorld *w) {
    if (!w)
        return CORE_ERR_NULL_ARG;
    if (w->guide.active || clock_day(&w->clock) > 1 || w->outcome != CORE_OUTCOME_NONE)
        return CORE_ERR_REJECTED;
    memset(&w->guide, 0, sizeof(w->guide));
    w->guide.active = 1;
    return CORE_OK;
}

CORE_API int32_t core_guide_view(CoreWorld *w, CoreGuideView *out) {
    if (!w || !out)
        return CORE_ERR_NULL_ARG;
    memset(out, 0, sizeof(*out));
    out->count = GUIDE_STEP_COUNT;
    out->hint_x = out->hint_y = out->goal_x = out->goal_y = -1;
    if (!w->guide.active)
        return CORE_OK;
    guide_tick(w);
    out->active = 1;
    out->step = w->guide.step;
    if (w->guide.step >= GUIDE_STEP_COUNT) {
        out->finished = 1;
        return CORE_OK;
    }
    const GuideStepDef *s = &GUIDE_STEPS[w->guide.step];
    out->kind = s->kind;
    out->pause = s->pause;
    if (s->progress) {
        out->progress = s->progress(w, &out->progress_need);
        (void)snprintf(out->progress_label, sizeof(out->progress_label), "%s", s->progress_label);
    }
    fill_target(w, s, out);
    (void)snprintf(out->title, sizeof(out->title), "%s", s->title);
    (void)snprintf(out->body, sizeof(out->body), "%s", s->body);
    return CORE_OK;
}

CORE_API int32_t core_guide_next(CoreWorld *w) {
    if (!w)
        return CORE_ERR_NULL_ARG;
    if (!guide_running(w) || GUIDE_STEPS[w->guide.step].kind != CORE_GUIDE_INFO)
        return CORE_ERR_REJECTED;
    w->guide.step++;
    guide_tick(w); /* 이미 해 둔 행동 단계는 바로 넘깁니다 */
    return CORE_OK;
}

CORE_API int32_t core_guide_skip(CoreWorld *w) {
    if (!w)
        return CORE_ERR_NULL_ARG;
    if (!w->guide.active)
        return CORE_ERR_REJECTED;
    memset(&w->guide, 0, sizeof(w->guide));
    return CORE_OK;
}
