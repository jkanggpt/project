/* 원화의 알파와 프레임 경계를 보존하는 결정적 재료 굽기입니다. */
#ifndef CASTLE_SPRITE_MATERIALS_H
#define CASTLE_SPRITE_MATERIALS_H
int art_sprite_materials(const char *sources, const char *input, const char *output, int staged);
int art_sprite_materials_check(const char *sources, const char *runtime);
int art_sprite_materials_test(void);
int art_sprite_materials_gallery(const char *runtime, const char *output);
#endif
