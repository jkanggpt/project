/*
 * console.c — 콘솔 입출력 도우미 (설명은 console.h)
 */
#include "console.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

void console_init(void)
{
#ifdef _WIN32
    /* 한국어 Windows 의 기본 콘솔 코드 페이지는 949 라서 UTF-8 한글이 깨집니다 */
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
}

#ifdef _WIN32
/* 콘솔에서 직접 입력받을 때는 UTF-16 으로 읽어 UTF-8 로 바꿉니다.
 * (일부 Windows 버전은 CP_UTF8 상태에서 fgets 로 한글을 읽으면 글자가 사라집니다) */
static int read_console_utf8(HANDLE input, char *buffer, size_t size)
{
    wchar_t wide[512];
    DWORD read_count = 0;
    int bytes;

    if (!ReadConsoleW(input, wide, (DWORD)(sizeof(wide) / sizeof(wide[0]) - 1), &read_count, NULL)) {
        return 0;
    }
    if (read_count == 0) {
        return 0;
    }
    while (read_count > 0 && (wide[read_count - 1] == L'\n' || wide[read_count - 1] == L'\r')) {
        read_count--;
    }
    wide[read_count] = L'\0';
    bytes = WideCharToMultiByte(CP_UTF8, 0, wide, -1, buffer, (int)size, NULL, NULL);
    if (bytes <= 0) {
        buffer[0] = '\0';
        return -1; /* 버퍼보다 김 */
    }
    return 1;
}
#endif

int console_read_line(char *buffer, size_t size)
{
    size_t length;

#ifdef _WIN32
    {
        HANDLE input = GetStdHandle(STD_INPUT_HANDLE);
        DWORD mode;
        if (input != INVALID_HANDLE_VALUE && GetConsoleMode(input, &mode)) {
            return read_console_utf8(input, buffer, size);
        }
    }
#endif
    if (fgets(buffer, (int)size, stdin) == NULL) {
        return 0;
    }
    length = strlen(buffer);
    if (length > 0 && buffer[length - 1] == '\n') {
        buffer[--length] = '\0';
        if (length > 0 && buffer[length - 1] == '\r') {
            buffer[--length] = '\0';
        }
        return 1;
    }
    if (feof(stdin)) {
        return 1; /* 마지막 줄에 줄바꿈이 없음 */
    }
    /* 버퍼보다 긴 줄: 나머지를 읽어 버립니다 */
    {
        int c;
        while ((c = getchar()) != '\n' && c != EOF) {
        }
    }
    return -1;
}

int read_int(const char *prompt, int min, int max, int *out)
{
    char line[64];
    for (;;) {
        char *end = NULL;
        long value;
        int got;

        printf("%s (%d~%d): ", prompt, min, max);
        fflush(stdout);
        got = console_read_line(line, sizeof(line));
        if (got == 0) {
            printf("\n");
            return 0;
        }
        if (got < 0) {
            printf("  입력이 너무 깁니다. 다시 입력하세요.\n");
            continue;
        }
        errno = 0;
        value = strtol(line, &end, 10);
        while (end != NULL && (*end == ' ' || *end == '\t')) {
            end++;
        }
        if (line[0] == '\0' || end == line || *end != '\0' || errno != 0) {
            printf("  숫자를 입력하세요.\n");
            continue;
        }
        if (value < min || value > max) {
            printf("  %d부터 %d 사이의 숫자를 입력하세요.\n", min, max);
            continue;
        }
        *out = (int)value;
        return 1;
    }
}

int read_text(const char *prompt, const char *default_text, char *buffer, size_t size)
{
    for (;;) {
        int got;
        if (default_text != NULL) {
            printf("%s [엔터 = %s]: ", prompt, default_text);
        } else {
            printf("%s: ", prompt);
        }
        fflush(stdout);
        got = console_read_line(buffer, size);
        if (got == 0) {
            printf("\n");
            return 0;
        }
        if (got < 0) {
            printf("  너무 깁니다. %u바이트보다 짧게 입력하세요.\n", (unsigned)(size - 1));
            continue;
        }
        if (buffer[0] == '\0') {
            if (default_text == NULL) {
                printf("  비워 둘 수 없습니다.\n");
                continue;
            }
            if (strlen(default_text) >= size) {
                return 0;
            }
            strcpy(buffer, default_text);
        }
        return 1;
    }
}
