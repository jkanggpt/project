/*
 * console.h — 콘솔 입출력 도우미
 *
 * 사용자가 무엇을 입력하든 프로그램이 죽거나 엉뚱하게 동작하지 않도록,
 * 입력은 모두 여기서 검사하고 잘못되면 다시 묻습니다.
 */
#ifndef CASTLE_CLI_CONSOLE_H
#define CASTLE_CLI_CONSOLE_H

#include <stddef.h>

/* Windows 콘솔을 UTF-8 로 맞춥니다 (한글 출력/입력). 다른 OS 에서는 아무 일도 하지 않습니다 */
void console_init(void);

/* 한 줄을 읽어 buffer 에 넣습니다 (줄바꿈 제거).
 * 1 = 읽음, 0 = 입력 끝(EOF), -1 = 줄이 너무 길어 버림 */
int console_read_line(char *buffer, size_t size);

/* min..max 정수를 입력받을 때까지 다시 묻습니다. 성공 1, 입력 끝 0 */
int read_int(const char *prompt, int min, int max, int *out);

/* 글자를 입력받습니다. 빈 줄이면 default_text 를 씁니다 (NULL 이면 다시 묻습니다). 성공 1, 입력 끝 0 */
int read_text(const char *prompt, const char *default_text, char *buffer, size_t size);

#endif
