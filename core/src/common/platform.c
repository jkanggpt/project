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
#ifdef _MSC_VER
/* 구형 Windows SDK 10.0.19041 헤더의 매크로 경고만 한정해 제외합니다. */
#pragma warning(push)
#pragma warning(disable : 5105)
#endif
#include <windows.h>
#ifdef _MSC_VER
#pragma warning(pop)
#endif

/* UTF-8 경로를 UTF-16 으로 바꾼 결과.
 * 보통 길이의 경로는 구조체 안의 배열(스택)에 담고, 아주 긴 경로만 힙에 할당합니다.
 * 그래서 메모리가 부족해도 흔한 경로는 열 수 있고, 오류도 "파일을 못 엶"으로 잘못 보고되지 않습니다. */
typedef struct WideText {
    wchar_t local[MAX_PATH + 1];
    wchar_t *heap;
    wchar_t *text; /* local 또는 heap 을 가리킴. 실패하면 NULL */
} WideText;

/* 성공하면 1. 실패(잘못된 UTF-8, 메모리 부족)하면 0 이고 wide->text 는 NULL */
static int wide_begin(WideText *wide, const char *utf8) {
    int length;

    wide->heap = NULL;
    wide->text = NULL;
    length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8, -1, NULL, 0);
    if (length <= 0) {
        return 0;
    }
    if (length <= (int)(sizeof(wide->local) / sizeof(wide->local[0]))) {
        wide->text = wide->local;
    } else {
        wide->heap = (wchar_t *)mem_malloc((size_t)length * sizeof(wchar_t));
        if (wide->heap == NULL) {
            return 0;
        }
        wide->text = wide->heap;
    }
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8, -1, wide->text, length) != length) {
        mem_free(wide->heap);
        wide->heap = NULL;
        wide->text = NULL;
        return 0;
    }
    return 1;
}

static void wide_end(WideText *wide) {
    mem_free(wide->heap);
    wide->heap = NULL;
    wide->text = NULL;
}

FILE *platform_fopen_utf8(const char *path, const char *mode) {
    WideText wide_path;
    WideText wide_mode;
    FILE *file = NULL;

    if (wide_begin(&wide_path, path) && wide_begin(&wide_mode, mode)) {
        file = _wfopen(wide_path.text, wide_mode.text);
        wide_end(&wide_mode);
    }
    wide_end(&wide_path);
    return file;
}

int platform_replace_file(const char *src, const char *dst) {
    WideText wide_src;
    WideText wide_dst;
    int result = -1;

    /* rename() 은 Windows 에서 대상 파일이 있으면 실패하므로 MoveFileExW 를 씁니다 */
    if (wide_begin(&wide_src, src) && wide_begin(&wide_dst, dst)) {
        if (MoveFileExW(wide_src.text, wide_dst.text, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
            result = 0;
        }
        wide_end(&wide_dst);
    }
    wide_end(&wide_src);
    return result;
}

int platform_copy_file(const char *src, const char *dst) {
    WideText wide_src;
    WideText wide_dst;
    int result = -1;

    if (wide_begin(&wide_src, src) && wide_begin(&wide_dst, dst)) {
        if (CopyFileW(wide_src.text, wide_dst.text, FALSE)) {
            result = 0;
        }
        wide_end(&wide_dst);
    }
    wide_end(&wide_src);
    return result;
}

int platform_remove_file(const char *path) {
    WideText wide;
    int result = -1;
    if (wide_begin(&wide, path) && DeleteFileW(wide.text)) {
        result = 0;
    }
    wide_end(&wide);
    return result;
}

int platform_file_exists(const char *path) {
    WideText wide;
    int exists = 0;
    if (wide_begin(&wide, path)) {
        DWORD attributes = GetFileAttributesW(wide.text);
        exists = attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_DIRECTORY);
    }
    wide_end(&wide);
    return exists;
}

int platform_make_dir(const char *path) {
    WideText wide;
    int result = -1;
    if (wide_begin(&wide, path)) {
        if (CreateDirectoryW(wide.text, NULL) || GetLastError() == ERROR_ALREADY_EXISTS) {
            result = 0;
        }
    }
    wide_end(&wide);
    return result;
}

#else /* Linux, macOS: 경로가 이미 UTF-8 바이트열이라 표준 함수를 그대로 씁니다 */

#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>

FILE *platform_fopen_utf8(const char *path, const char *mode) { return fopen(path, mode); }

int platform_replace_file(const char *src, const char *dst) {
    /* POSIX rename() 은 대상이 있으면 원자적으로 바꿔치기합니다 */
    return rename(src, dst) == 0 ? 0 : -1;
}

int platform_copy_file(const char *src, const char *dst) {
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

int platform_remove_file(const char *path) { return remove(path) == 0 ? 0 : -1; }

int platform_file_exists(const char *path) {
    struct stat info;
    return stat(path, &info) == 0 && S_ISREG(info.st_mode);
}

int platform_make_dir(const char *path) {
    if (mkdir(path, 0755) == 0 || errno == EEXIST) {
        return 0;
    }
    return -1;
}

#endif
