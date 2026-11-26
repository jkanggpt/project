/* storage.h — UTF-8 경로, 사용자 저장 폴더, 설정. 그래픽 라이브러리와 독립적입니다. */
#ifndef CASTLE_GAME_STORAGE_H
#define CASTLE_GAME_STORAGE_H
#include <stddef.h>
#define GAME_PATH_BYTES 2048
typedef struct GameSettings {
    float master_volume, sfx_volume;
    int fullscreen, edge_scroll, ui_scale, low_effects;
} GameSettings;
int game_join_path(char *out, size_t size, const char *dir, const char *name);
int game_make_dirs(const char *path);
int game_user_directory(char *out, size_t size);
int game_executable_directory(char *out, size_t size, const char *argv0);
void game_settings_default(GameSettings *settings);
int game_settings_load(GameSettings *settings, const char *path);
int game_settings_save(const GameSettings *settings, const char *path);
unsigned char *game_read_bytes(const char *path, int *length);
int game_write_bytes(const char *path, const void *data, size_t length);
unsigned long long game_file_modified(const char *path);
typedef struct GameArguments {
    int count;
    char **values;
} GameArguments;
int game_arguments(GameArguments *out, int argc, char **argv);
void game_arguments_free(GameArguments *args);
#endif
