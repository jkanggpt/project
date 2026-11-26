/* storage.c — 한글 사용자·프로젝트 경로를 운영체제 API에서 잃지 않게 변환합니다. */
#if !defined(_WIN32)
#define _POSIX_C_SOURCE 200809L
#endif
#include "storage.h"
#include "platform.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 5105) /* 구형 Windows SDK 헤더에만 적용 */
#endif
/* Windows 기본 형식을 먼저 정의한 뒤 Shell API를 포함합니다. */
#include <windows.h>
#include <shellapi.h>
#ifdef _MSC_VER
#pragma warning(pop)
#endif
#else
#include <sys/stat.h>
#include <unistd.h>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif
#endif

int game_join_path(char *out, size_t size, const char *dir, const char *name) {
    int n = snprintf(out, size, "%s%s%s", dir,
                     dir[0] && dir[strlen(dir) - 1] != '/' && dir[strlen(dir) - 1] != '\\' ? "/" : "", name);
    return n >= 0 && (size_t)n < size;
}
int game_make_dirs(const char *path) {
    char copy[GAME_PATH_BYTES];
    size_t i, length = strlen(path);
    if (!length || length >= sizeof(copy))
        return 0;
    memcpy(copy, path, length + 1);
    for (i = 1; i < length; i++) {
        if (copy[i] == '/' || copy[i] == '\\') {
            char c = copy[i];
            copy[i] = '\0';
            if (!(i == 2 && copy[1] == ':') && platform_make_dir(copy) != 0)
                return 0;
            copy[i] = c;
        }
    }
    return platform_make_dir(copy) == 0;
}
int game_user_directory(char *out, size_t size) {
#ifdef _WIN32
    wchar_t wide[GAME_PATH_BYTES];
    char base[GAME_PATH_BYTES];
    DWORD n = GetEnvironmentVariableW(L"APPDATA", wide, GAME_PATH_BYTES);
    if (!n || n >= GAME_PATH_BYTES ||
        !WideCharToMultiByte(CP_UTF8, 0, wide, -1, base, sizeof(base), NULL, NULL))
        return 0;
    return game_join_path(out, size, base, "MagicCastle");
#else
    const char *base = getenv("XDG_DATA_HOME");
    char fallback[GAME_PATH_BYTES];
    if (!base || !*base) {
        const char *user_home = getenv("HOME");
        if (!user_home || !game_join_path(fallback, sizeof(fallback), user_home, ".local/share"))
            return 0;
        base = fallback;
    }
    return game_join_path(out, size, base, "magic-castle");
#endif
}
int game_executable_directory(char *out, size_t size, const char *argv0) {
    char path[GAME_PATH_BYTES], *slash, *back;
#ifdef _WIN32
    wchar_t wide[GAME_PATH_BYTES];
    DWORD n = GetModuleFileNameW(NULL, wide, GAME_PATH_BYTES);
    (void)argv0;
    if (!n || n >= GAME_PATH_BYTES ||
        !WideCharToMultiByte(CP_UTF8, 0, wide, -1, path, sizeof(path), NULL, NULL))
        return 0;
#elif defined(__APPLE__)
    uint32_t capacity = (uint32_t)sizeof(path);
    (void)argv0;
    if (_NSGetExecutablePath(path, &capacity) != 0)
        return 0;
#else
    ssize_t n = readlink("/proc/self/exe", path, sizeof(path) - 1);
    if (n < 0) {
        if (strlen(argv0) >= sizeof(path))
            return 0;
        strcpy(path, argv0);
    } else
        path[n] = '\0';
#endif
    slash = strrchr(path, '/');
    back = strrchr(path, '\\');
    if (back && (!slash || back > slash))
        slash = back;
    if (!slash)
        return game_join_path(out, size, ".", "");
    *slash = '\0';
    if (strlen(path) >= size)
        return 0;
    strcpy(out, path);
    return 1;
}
void game_settings_default(GameSettings *s) {
    s->master_volume = 0.8f;
    s->sfx_volume = 0.8f;
    s->fullscreen = 0;
    s->edge_scroll = 1;
    s->ui_scale = 1;
    s->low_effects = 0;
}
int game_settings_load(GameSettings *s, const char *path) {
    char line[128], key[64], extra;
    float value;
    FILE *file = platform_fopen_utf8(path, "rb");
    game_settings_default(s);
    if (!file)
        return 0;
    while (fgets(line, sizeof(line), file)) {
        if (sscanf(line, "%63[^=]=%f %c", key, &value, &extra) != 2 || !isfinite(value))
            continue;
        if (strcmp(key, "master") == 0 && value >= 0 && value <= 1)
            s->master_volume = value;
        else if (strcmp(key, "sfx") == 0 && value >= 0 && value <= 1)
            s->sfx_volume = value;
        else if (strcmp(key, "fullscreen") == 0 && (value == 0 || value == 1))
            s->fullscreen = (int)value;
        else if (strcmp(key, "edge_scroll") == 0 && (value == 0 || value == 1))
            s->edge_scroll = (int)value;
        else if (strcmp(key, "ui_scale") == 0 && (value == 1 || value == 2))
            s->ui_scale = (int)value;
        else if (strcmp(key, "low_effects") == 0 && (value == 0 || value == 1))
            s->low_effects = (int)value;
    }
    fclose(file);
    return 1;
}
int game_settings_save(const GameSettings *s, const char *path) {
    char tmp[GAME_PATH_BYTES];
    FILE *file;
    int ok;
    if (snprintf(tmp, sizeof(tmp), "%s.tmp", path) >= (int)sizeof(tmp))
        return 0;
    file = platform_fopen_utf8(tmp, "wb");
    if (!file)
        return 0;
    ok = fprintf(file, "master=%.3f\nsfx=%.3f\nfullscreen=%d\nedge_scroll=%d\nui_scale=%d\nlow_effects=%d\n",
                 (double)s->master_volume, (double)s->sfx_volume, s->fullscreen, s->edge_scroll, s->ui_scale,
                 s->low_effects) > 0;
    if (fclose(file) != 0)
        ok = 0;
    if (ok && platform_replace_file(tmp, path) == 0)
        return 1;
    (void)platform_remove_file(tmp);
    return 0;
}
unsigned char *game_read_bytes(const char *path, int *length) {
    FILE *file = platform_fopen_utf8(path, "rb");
    long n;
    unsigned char *bytes;
    *length = 0;
    if (!file)
        return NULL;
    if (fseek(file, 0, SEEK_END) != 0 || (n = ftell(file)) < 1 || n > 64 * 1024 * 1024 ||
        fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return NULL;
    }
    bytes = (unsigned char *)malloc((size_t)n);
    if (!bytes) {
        fclose(file);
        return NULL;
    }
    if (fread(bytes, 1, (size_t)n, file) != (size_t)n) {
        free(bytes);
        fclose(file);
        return NULL;
    }
    fclose(file);
    *length = (int)n;
    return bytes;
}
int game_write_bytes(const char *path, const void *data, size_t length) {
    FILE *file = platform_fopen_utf8(path, "wb");
    int ok;
    if (!file)
        return 0;
    ok = fwrite(data, 1, length, file) == length;
    if (fclose(file) != 0)
        ok = 0;
    return ok;
}
unsigned long long game_file_modified(const char *path) {
#ifdef _WIN32
    wchar_t wide[GAME_PATH_BYTES];
    WIN32_FILE_ATTRIBUTE_DATA data;
    ULARGE_INTEGER t;
    if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, wide, GAME_PATH_BYTES) ||
        !GetFileAttributesExW(wide, GetFileExInfoStandard, &data))
        return 0;
    t.LowPart = data.ftLastWriteTime.dwLowDateTime;
    t.HighPart = data.ftLastWriteTime.dwHighDateTime;
    return t.QuadPart;
#else
    struct stat data;
    return stat(path, &data) == 0 ? (unsigned long long)data.st_mtime : 0;
#endif
}

void game_arguments_free(GameArguments *args) {
    int i;
    for (i = 0; i < args->count; i++)
        free(args->values[i]);
    free(args->values);
    memset(args, 0, sizeof(*args));
}
int game_arguments(GameArguments *out, int argc, char **argv) {
    int i;
    memset(out, 0, sizeof(*out));
#ifdef _WIN32
    {
        wchar_t **wide = CommandLineToArgvW(GetCommandLineW(), &argc);
        (void)argv;
        if (!wide)
            return 0;
        out->values = (char **)calloc((size_t)argc + 1, sizeof(char *));
        out->count = argc;
        if (!out->values) {
            LocalFree(wide);
            out->count = 0;
            return 0;
        }
        for (i = 0; i < argc; i++) {
            int n = WideCharToMultiByte(CP_UTF8, 0, wide[i], -1, NULL, 0, NULL, NULL);
            if (n < 1) {
                LocalFree(wide);
                game_arguments_free(out);
                return 0;
            }
            out->values[i] = (char *)malloc((size_t)n);
            if (!out->values[i]) {
                LocalFree(wide);
                game_arguments_free(out);
                return 0;
            }
            (void)WideCharToMultiByte(CP_UTF8, 0, wide[i], -1, out->values[i], n, NULL, NULL);
        }
        LocalFree(wide);
    }
#else
    out->values = (char **)calloc((size_t)argc + 1, sizeof(char *));
    out->count = argc;
    if (!out->values) {
        out->count = 0;
        return 0;
    }
    for (i = 0; i < argc; i++) {
        out->values[i] = (char *)malloc(strlen(argv[i]) + 1);
        if (!out->values[i]) {
            game_arguments_free(out);
            return 0;
        }
        strcpy(out->values[i], argv[i]);
    }
#endif
    return 1;
}
