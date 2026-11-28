/* 실제 게임 렌더러·재료 경로로 조명을 검수하는 장면입니다. 저장하지 않습니다. */
#ifndef CASTLE_GAME_REVIEW_H
#define CASTLE_GAME_REVIEW_H
struct GameApp;
/* crowd는 군중 주민 수입니다. 실패하면 0을 돌려줍니다. */
int game_review_start(struct GameApp *app, int seed, int crowd);
void game_review_update(struct GameApp *app);
void game_review_draw_help(struct GameApp *app);
/* U 키로 게임 UI를 숨긴 검수 화면에서는 0입니다. */
int game_review_interface_visible(void);
void game_review_set_clean(int clean);
#endif
