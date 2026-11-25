/* ui.h — 한국어 글꼴을 쓰는 작은 즉시 모드 UI */
#ifndef CASTLE_GAME_UI_H
#define CASTLE_GAME_UI_H
#include "assets.h"
typedef struct GameUi {
    GameAssets *assets;
    float font_size, volume;
    int focus, input_blocked, consumed;
    char tooltip[4096];
    unsigned tooltip_hash;
    float tooltip_scroll;
    int tooltip_bad;
} GameUi;
extern const Color UI_BG,UI_PANEL,UI_EDGE,UI_TEXT,UI_MUTED,UI_GOLD,UI_GOOD,UI_BAD;
void ui_begin(GameUi *ui,GameAssets *assets,float size,float volume);
void ui_clip_begin(int x,int y,int width,int height);
void ui_clip_end(void);
void ui_clip_begin(int x,int y,int width,int height);
void ui_clip_end(void);
void ui_text(GameUi *ui,float x,float y,Color color,const char *format,...);
void ui_small(GameUi *ui,float x,float y,Color color,const char *format,...);
void ui_wrap(GameUi *ui,Rectangle area,Color color,const char *text);
float ui_wrap_height(GameUi *ui,float width,const char *text);
void ui_box(Rectangle area,Color color);
int ui_button(GameUi *ui,Rectangle area,const char *text,int enabled,int selected,const char *tooltip);
int ui_color_button(GameUi *ui,Rectangle area,const char *text,int enabled,int selected,Color color);
void ui_bar(Rectangle area,float fraction,Color color);
void ui_textbox(GameUi *ui,Rectangle area,int id,char *text,size_t size,int digits_only);
void ui_slider(GameUi *ui,Rectangle area,float *value);
void ui_tooltip(GameUi *ui);
#endif
