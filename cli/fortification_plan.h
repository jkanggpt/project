/* fortification_plan.h — 물 위를 피하는 두 겹 성벽의 결정적 배치 계획 */
#ifndef CASTLE_FORTIFICATION_PLAN_H
#define CASTLE_FORTIFICATION_PLAN_H
#include <stdint.h>
typedef struct BotFortificationPlan {
    int bounds[2][4];
    int valid[2];
} BotFortificationPlan;
int bot_fortification_plan(const uint8_t *terrain, int width, int height, int cx, int cy,
                           BotFortificationPlan *out);
int bot_fortification_boundary(const BotFortificationPlan *plan, int x, int y, int ring);
int bot_fortification_reserved(const BotFortificationPlan *plan, int x, int y, int width, int height);
int bot_fortification_inside(const BotFortificationPlan *plan, int x, int y, int width, int height, int ring);
#endif
