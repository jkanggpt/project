/* 기존 픽셀 원화를 보존하는 조명 표본입니다. */
#ifndef CASTLE_LIGHTING_PIXELS_H
#define CASTLE_LIGHTING_PIXELS_H
#define PIXEL_LIGHT_CELL 320
#define PIXEL_LIGHT_WIDTH 960
#define PIXEL_LIGHT_HEIGHT 640
int art_pixel_reference(const char *root, const char *output);
int art_pixel_lighting(const char *source, const char *output);
int art_pixel_scene(const char *root, const char *output);
int art_pixel_check(const char *root);
#endif
