/* ui.c — 버튼·입력·스크롤 화면은 C 함수로 구성합니다. */
#include "ui.h"
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
const Color UI_BG = {25, 24, 34, 255}, UI_PANEL = {39, 35, 48, 245}, UI_EDGE = {85, 72, 97, 255};
const Color UI_TEXT = {239, 231, 219, 255}, UI_MUTED = {161, 153, 171, 255}, UI_GOLD = {244, 197, 108, 255},
            UI_GOOD = {133, 209, 151, 255}, UI_BAD = {239, 127, 124, 255};
/* raylib의 scissor는 중첩 스택이 아니므로 부모 영역을 복원합니다. */
static Rectangle clip_stack[16];
static int clip_depth;
void ui_clip_begin(int x, int y, int width, int height) {
    Rectangle area = {(float)x, (float)y, (float)width, (float)height};
    if (clip_depth >= 16) {
        clip_depth++;
        return;
    }
    if (clip_depth)
        area = GetCollisionRec(area, clip_stack[clip_depth - 1]);
    clip_stack[clip_depth++] = area;
    BeginScissorMode((int)area.x, (int)area.y, (int)area.width, (int)area.height);
}
void ui_clip_end(void) {
    Rectangle area;
    if (clip_depth > 16) {
        clip_depth--;
        return;
    }
    if (!clip_depth)
        return;
    if (--clip_depth) {
        area = clip_stack[clip_depth - 1];
        BeginScissorMode((int)area.x, (int)area.y, (int)area.width, (int)area.height);
    } else
        EndScissorMode();
}
void ui_begin(GameUi *ui, GameAssets *assets, float size, float volume) {
    ui->assets = assets;
    ui->font_size = size;
    ui->volume = volume;
    ui->tooltip_bad = 0;
    ui->tooltip[0] = '\0';
    ui->input_blocked = 0;
    ui->consumed = 0;
}
static void textv(GameUi *ui, float x, float y, float size, Color color, const char *format, va_list args) {
    char text[2048];
    (void)vsnprintf(text, sizeof(text), format, args);
    DrawTextEx(ui->assets->font, text, (Vector2){x, y}, size, 0, color);
}
void ui_text(GameUi *ui, float x, float y, Color color, const char *format, ...) {
    va_list args;
    va_start(args, format);
    textv(ui, x, y, ui->font_size, color, format, args);
    va_end(args);
}
void ui_small(GameUi *ui, float x, float y, Color color, const char *format, ...) {
    va_list args;
    va_start(args, format);
    textv(ui, x, y, ui->font_size - 3, color, format, args);
    va_end(args);
}
void ui_wrap(GameUi *ui, Rectangle area, Color color, const char *text) {
    float x = area.x, y = area.y;
    int offset = 0;
    ui_clip_begin((int)area.x, (int)area.y, (int)area.width, (int)area.height);
    while (text[offset] && y + ui->font_size <= area.y + area.height) {
        int bytes = 0, code = GetCodepointNext(text + offset, &bytes);
        char glyph[5] = {0};
        float width;
        if (bytes < 1)
            break;
        if (code == '\n') {
            x = area.x;
            y += ui->font_size + 8;
            offset += bytes;
            continue;
        }
        if (bytes > 4)
            break;
        memcpy(glyph, text + offset, (size_t)bytes);
        width = MeasureTextEx(ui->assets->font, glyph, ui->font_size, 0).x;
        if (x + width > area.x + area.width) {
            x = area.x;
            y += ui->font_size + 8;
        }
        DrawTextEx(ui->assets->font, glyph, (Vector2){x, y}, ui->font_size, 0, color);
        x += width;
        offset += bytes;
    }
    ui_clip_end();
}
float ui_wrap_height(GameUi *ui, float width, const char *text) {
    float x = 0;
    int lines = 1, offset = 0;
    while (text[offset]) {
        int bytes = 0, code = GetCodepointNext(text + offset, &bytes);
        char glyph[5] = {0};
        if (bytes < 1 || bytes > 4)
            break;
        offset += bytes;
        if (code == '\n') {
            lines++;
            x = 0;
            continue;
        }
        memcpy(glyph, text + offset - bytes, (size_t)bytes);
        float w = MeasureTextEx(ui->assets->font, glyph, ui->font_size, 0).x;
        if (x + w > width) {
            lines++;
            x = 0;
        }
        x += w;
    }
    return lines * (ui->font_size + 8) - 8;
}
void ui_box(Rectangle area, Color color) {
    DrawRectangleRec(area, color);
    DrawRectangleLinesEx(area, 1, UI_EDGE);
}
static int button(GameUi *ui, Rectangle area, const char *text, int enabled, int selected,
                  const char *tooltip, const Color *custom) {
    int hover = CheckCollisionPointRec(GetMousePosition(), area), pressed = 0;
    Color fill = custom ? *custom : selected ? (Color){96, 73, 63, 255} : UI_PANEL;
    Vector2 measure = MeasureTextEx(ui->assets->font, text, ui->font_size, 0);
    if (hover && enabled)
        fill = custom ? ColorBrightness(*custom, .08f) : (Color){80, 66, 86, 255};
    if (!enabled)
        fill = (Color){31, 30, 39, 245};
    ui_box(area, fill);
    if (selected)
        DrawRectangleLinesEx(area, 2, custom ? UI_BG : UI_GOLD);
    ui_clip_begin((int)area.x + 2, (int)area.y, (int)area.width - 4, (int)area.height);
    DrawTextEx(ui->assets->font, text,
               (Vector2){area.x + fmaxf(6, (area.width - measure.x) * 0.5f),
                         area.y + (area.height - ui->font_size) * 0.5f},
               ui->font_size, 0, enabled ? (custom ? UI_BG : UI_TEXT) : UI_MUTED);
    ui_clip_end();
    if (hover && tooltip)
        (void)snprintf(ui->tooltip, sizeof(ui->tooltip), "%s", tooltip);
    if (hover && enabled && !ui->input_blocked && !ui->consumed && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        pressed = 1;
        ui->consumed = 1;
        game_sound(ui->assets, SND_CLICK, ui->volume);
    }
    return pressed;
}
int ui_button(GameUi *ui, Rectangle area, const char *text, int enabled, int selected, const char *tip) {
    return button(ui, area, text, enabled, selected, tip, NULL);
}
int ui_color_button(GameUi *ui, Rectangle area, const char *text, int enabled, int selected, Color color) {
    return button(ui, area, text, enabled, selected, NULL, &color);
}
void ui_bar(Rectangle area, float fraction, Color color) {
    DrawRectangleRec(area, (Color){25, 22, 31, 255});
    fraction = fmaxf(0, fminf(1, fraction));
    DrawRectangleRec((Rectangle){area.x, area.y, area.width * fraction, area.height}, color);
    DrawRectangleLinesEx(area, 1, UI_EDGE);
}
static void append_utf8(char *text, size_t size, const char *add, int digits_only) {
    size_t length = strlen(text), i = 0;
    while (add[i]) {
        int bytes = 0, code = GetCodepointNext(add + i, &bytes);
        if (bytes < 1)
            break;
        if (code >= 32 && code != 127 && (!digits_only || (code >= '0' && code <= '9')) &&
            length + (size_t)bytes < size) {
            memcpy(text + length, add + i, (size_t)bytes);
            length += (size_t)bytes;
            text[length] = '\0';
        }
        i += (size_t)bytes;
    }
}
void ui_textbox(GameUi *ui, Rectangle area, int id, char *text, size_t size, int digits_only) {
    int active, code;
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !ui->input_blocked &&
        CheckCollisionPointRec(GetMousePosition(), area))
        ui->focus = id;
    active = ui->focus == id;
    ui_box(area, (Color){24, 23, 32, 255});
    if (active)
        DrawRectangleLinesEx(area, 2, UI_GOLD);
    if (active && !ui->input_blocked) {
        if ((IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) && IsKeyPressed(KEY_V))
            append_utf8(text, size, GetClipboardText(), digits_only);
        while ((code = GetCharPressed()) > 0) {
            int length = 0;
            const char *encoded = CodepointToUTF8(code, &length);
            char copy[5] = {0};
            if (length > 0 && length <= 4) {
                memcpy(copy, encoded, (size_t)length);
                append_utf8(text, size, copy, digits_only);
            }
        }
        if ((IsKeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE)) && text[0]) {
            size_t n = strlen(text);
            do {
                n--;
            } while (n > 0 && ((unsigned char)text[n] & 0xC0) == 0x80);
            text[n] = '\0';
        }
    }
    ui_clip_begin((int)area.x + 5, (int)area.y, (int)area.width - 10, (int)area.height);
    ui_text(ui, area.x + 10, area.y + 9, UI_TEXT, "%s%s", text,
            active && ((int)(GetTime() * 2) % 2) == 0 ? "|" : "");
    ui_clip_end();
}
void ui_slider(GameUi *ui, Rectangle area, float *value) {
    if (!ui->input_blocked && IsMouseButtonDown(MOUSE_BUTTON_LEFT) &&
        CheckCollisionPointRec(GetMousePosition(), area))
        *value = fmaxf(0, fminf(1, (GetMouseX() - area.x) / area.width));
    ui_bar((Rectangle){area.x, area.y + area.height * 0.3f, area.width, area.height * 0.4f}, *value, UI_GOLD);
    DrawCircleV((Vector2){area.x + area.width * (*value), area.y + area.height * 0.5f}, 8, UI_TEXT);
}
void ui_tooltip(GameUi *ui) {
    if (!ui->tooltip[0])
        return;
    unsigned hash = 2166136261u;
    for (const unsigned char *p = (const unsigned char *)ui->tooltip; *p; p++)
        hash = (hash ^ *p) * 16777619u;
    if (hash != ui->tooltip_hash) {
        ui->tooltip_hash = hash;
        ui->tooltip_scroll = 0;
    }
    float width = fminf(520, GetScreenWidth() - 24.0f), content = ui_wrap_height(ui, width - 24, ui->tooltip),
          height = fminf(content + 24, GetScreenHeight() - 24.0f);
    Rectangle area = {fmaxf(8, fminf(GetMouseX() + 12.0f, GetScreenWidth() - width - 8)), GetMouseY() + 22.0f,
                      width, height};
    if (area.y + height > GetScreenHeight() - 8)
        area.y = GetMouseY() - height - 12;
    if (area.y < 8)
        area.y = 8;
    float max = fmaxf(0, content + 24 - height);
    if (max > 0) {
        ui->tooltip_scroll -= GetMouseWheelMove() * 40;
        ui->tooltip_scroll = fmaxf(0, fminf(max, ui->tooltip_scroll));
    }
    ui_box(area, (Color){20, 18, 27, 250});
    ui_clip_begin((int)area.x, (int)area.y, (int)area.width, (int)area.height);
    ui_wrap(
        ui,
        (Rectangle){area.x + 12, area.y + 10 - ui->tooltip_scroll, width - 24, content + ui->font_size + 8},
        UI_TEXT, ui->tooltip);
    if (ui->tooltip_bad) {
        char first[128];
        size_t n = strcspn(ui->tooltip, "\n");
        if (n >= sizeof(first))
            n = sizeof(first) - 1;
        memcpy(first, ui->tooltip, n);
        first[n] = 0;
        DrawRectangle((int)area.x + 10, (int)(area.y + 8 - ui->tooltip_scroll), (int)width - 20,
                      (int)ui->font_size + 4, (Color){20, 18, 27, 255});
        ui_text(ui, area.x + 12, area.y + 10 - ui->tooltip_scroll, UI_BAD, "%s", first);
    }
    if (max > 0) {
        DrawRectangle((int)(area.x + area.width - 5),
                      (int)(area.y + height * ui->tooltip_scroll / (max + height)), 3,
                      (int)(height * height / (max + height)), UI_GOLD);
    }
    ui_clip_end();
}
