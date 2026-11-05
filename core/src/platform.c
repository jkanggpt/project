/*
 * platform.c — 파일 처리 감싸개 (설명은 platform.h)
 *
 * Windows 와 그 밖의 운영체제(Linux, macOS) 구현이 #ifdef 로 나뉩니다.
 * Linux 에서는 Windows 쪽을 실행할 수 없으므로, MinGW 교차 컴파일과 GitHub Actions 의 Windows 작업이
 * 한글 경로 테스트로 이 부분을 확인합니다.
 */
#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
/* -std=c11 (확장 끔) 에서도 mkdir, stat 같은 POSIX 함수를 쓰기 위해 */
#define _POSIX_C_SOURCE 200809L
#endif

#include "platform.h"

#include <string.h>

#include "mem.h"

#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <direct.h>
#include <windows.h>

/* UTF-8 문자열을 새로 할당한 UTF-16 문자열로 바꿉니다. 호출한 쪽이 mem_free 합니다. 실패하면 NULL */
static wchar_t *utf8_to_wide(const char *text)
{
    int length;
    wchar_t *wide;

    length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, -1, NULL, 0);
    if (length <= 0) {
        return NULL;
    }
    wide = (wchar_t *)mem_malloc((size_t)length * sizeof(wchar_t));
    if (wide == NULL) {
        return NULL;
    }
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, -1, wide, length) != length) {
        mem_free(wide);
        return NULL;
    }
    return wide;
}

FILE *platform_fopen_utf8(const char *path, const char *mode)
{
    wchar_t *wide_path = utf8_to_wide(path);
    wchar_t *wide_mode = utf8_to_wide(mode);
    FILE *file = NULL;

    if (wide_path != NULL && wide_mode != NULL) {
        file = _wfopen(wide_path, wide_mode);
    }
    mem_free(wide_path);
    mem_free(wide_mode);
    return file;
}

int platform_replace_file(const char *src, const char *dst)
{
    wchar_t *wide_src = utf8_to_wide(src);
    wchar_t *wide_dst = utf8_to_wide(dst);
    int result = -1;

    /* rename() 은 Windows 에서 대상 파일이 있으면 실패하므로 MoveFileExW 를 씁니다 */
    if (wide_src != NULL && wide_dst != NULL &&
        MoveFileExW(wide_src, wide_dst, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        result = 0;
    }
    mem_free(wide_src);
    mem_free(wide_dst);
    return result;
}

int platform_copy_file(const char *src, const char *dst)
{
    wchar_t *wide_src = utf8_to_wide(src);
    wchar_t *wide_dst = utf8_to_wide(dst);
    int result = -1;

    if (wide_src != NULL && wide_dst != NULL && CopyFileW(wide_src, wide_dst, FALSE)) {
        result = 0;
    }
    mem_free(wide_src);
    mem_free(wide_dst);
    return result;
}

int platform_remove_file(const char *path)
{
    wchar_t *wide = utf8_to_wide(path);
    int result = -1;
    if (wide != NULL && DeleteFileW(wide)) {
        result = 0;
    }
    mem_free(wide);
    return result;
}

int platform_file_exists(const char *path)
{
    wchar_t *wide = utf8_to_wide(path);
    int exists = 0;
    if (wide != NULL) {
        DWORD attributes = GetFileAttributesW(wide);
        exists = attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_DIRECTORY);
    }
    mem_free(wide);
    return exists;
}

int platform_make_dir(const char *path)
{
    wchar_t *wide = utf8_to_wide(path);
    int result = -1;
    if (wide != NULL) {
        if (CreateDirectoryW(wide, NULL) || GetLastError() == ERROR_ALREADY_EXISTS) {
            result = 0;
        }
    }
    mem_free(wide);
    return result;
}

#else /* Linux, macOS: 경로가 이미 UTF-8 바이트열이라 표준 함수를 그대로 씁니다 */

#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>

FILE *platform_fopen_utf8(const char *path, const char *mode)
{
    return fopen(path, mode);
}

int platform_replace_file(const char *src, const char *dst)
{
    /* POSIX rename() 은 대상이 있으면 원자적으로 바꿔치기합니다 */
    return rename(src, dst) == 0 ? 0 : -1;
}

int platform_copy_file(const char *src, const char *dst)
{
    FILE *in = fopen(src, "rb");
    FILE *out;
    char buffer[4096];
    size_t got;
    int result = 0;

    if (in == NULL) {
        return -1;
    }
    out = fopen(dst, "wb");
    if (out == NULL) {
        fclose(in);
        return -1;
    }
    while ((got = fread(buffer, 1, sizeof(buffer), in)) > 0) {
        if (fwrite(buffer, 1, got, out) != got) {
            result = -1;
            break;
        }
    }
    if (ferror(in)) {
        result = -1;
    }
    fclose(in);
    if (fclose(out) != 0) {
        result = -1;
    }
    return result;
}

int platform_remove_file(const char *path)
{
    return remove(path) == 0 ? 0 : -1;
}

int platform_file_exists(const char *path)
{
    struct stat info;
    return stat(path, &info) == 0 && S_ISREG(info.st_mode);
}

int platform_make_dir(const char *path)
{
    if (mkdir(path, 0755) == 0 || errno == EEXIST) {
        return 0;
    }
    return -1;
}

#endif
