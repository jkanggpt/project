/* fortification_plan.c — 누적합으로 물을 피해 성채를 감싸는 폐곡선을 고릅니다. */
#include "fortification_plan.h"
#include "castle_core.h"
#include <limits.h>
#include <stdlib.h>
#include <string.h>
static int sum(const int *p, int stride, int x, int y, int xx, int yy) {
    return p[(yy + 1) * stride + xx + 1] - p[y * stride + xx + 1] - p[(yy + 1) * stride + x] +
           p[y * stride + x];
}
static int select_rect(const int *prefix, int width, int height, int cx, int cy, int minimum, int radius,
                       const int *inner, int out[4]) {
    int l, r, t, b, best = INT_MAX, found = 0, stride = width + 1;
    /* 첫 경계는 성채·문·주거 공간을 품고 시작 우물(cx+4,cy+2)을 피합니다. */
    int ml = inner ? cx - inner[0] + 5 : 6, mr = inner ? inner[2] - cx + 5 : 6;
    int mt = inner ? cy - inner[1] + 5 : 6, mb = inner ? inner[3] - cy + 5 : 6;
    for (l = ml; l <= radius; l++)
        for (r = mr; r <= radius; r++)
            for (t = mt; t <= radius; t++)
                for (b = mb; b <= radius; b++) {
                    int x = cx - l, y = cy - t, xx = cx + r, yy = cy + b, score;
                    if ((!inner && r == 4) || x < 2 || y < 2 || xx >= width - 2 || yy >= height - 2 ||
                        (l + r - 1) * (t + b - 1) < minimum)
                        continue;
                    score = (l + r + t + b) * 100 + abs(l - r) * 3 + abs(t - b) * 3 + abs(l + r - t - b);
                    if (score >= best)
                        continue;
                    if (sum(prefix, stride, x, y, xx, y) || sum(prefix, stride, x, yy, xx, yy) ||
                        sum(prefix, stride, x, y, x, yy) || sum(prefix, stride, xx, y, xx, yy))
                        continue;
                    out[0] = x;
                    out[1] = y;
                    out[2] = xx;
                    out[3] = yy;
                    best = score;
                    found = 1;
                }
    return found;
}
int bot_fortification_plan(const uint8_t *terrain, int width, int height, int cx, int cy,
                           BotFortificationPlan *out) {
    int *p, x, y, stride = width + 1;
    if (!terrain || !out || width < 16 || height < 16)
        return 0;
    memset(out, 0, sizeof(*out));
    p = (int *)calloc((size_t)(width + 1) * (height + 1), sizeof(int));
    if (!p)
        return 0;
    for (y = 0; y < height; y++)
        for (x = 0; x < width; x++)
            p[(y + 1) * stride + x + 1] = (terrain[y * width + x] == CORE_TERRAIN_WATER) +
                                          p[y * stride + x + 1] + p[(y + 1) * stride + x] - p[y * stride + x];
    out->valid[0] = select_rect(p, width, height, cx, cy, 25, 11, NULL, out->bounds[0]);
    for (int ring = 1; ring < 4 && out->valid[ring - 1]; ring++) {
        int excluded = 0;
        static const int goals[4] = {25, 120, 300, 600};
        for (int n = 0; n < ring; n++) {
            const int *b = out->bounds[n];
            excluded += 2 * (b[2] - b[0] + b[3] - b[1]);
        }
        out->valid[ring] = select_rect(p, width, height, cx, cy, goals[ring] + excluded, 25,
                                       out->bounds[ring - 1], out->bounds[ring]);
    }
    free(p);
    return out->valid[0];
}
int bot_fortification_boundary(const BotFortificationPlan *p, int x, int y, int ring) {
    const int *b;
    if (ring < 0 || ring > 3 || !p->valid[ring])
        return 0;
    b = p->bounds[ring];
    return x >= b[0] && x <= b[2] && y >= b[1] && y <= b[3] &&
           (x == b[0] || x == b[2] || y == b[1] || y == b[3]);
}
int bot_fortification_reserved(const BotFortificationPlan *p, int x, int y, int width, int height) {
    int r, xx, yy;
    for (r = 0; r < 4; r++)
        for (yy = y; yy < y + height; yy++)
            for (xx = x; xx < x + width; xx++)
                if (bot_fortification_boundary(p, xx, yy, r))
                    return 1;
    return 0;
}
int bot_fortification_inside(const BotFortificationPlan *p, int x, int y, int width, int height, int ring) {
    const int *b;
    if (ring < 0 || ring > 3 || !p->valid[ring])
        return 0;
    b = p->bounds[ring];
    return x > b[0] && y > b[1] && x + width - 1 < b[2] && y + height - 1 < b[3];
}
