/*
 * text.h — 문자열 도우미 (UTF-8 이름 검사, 안전한 복사, 앞뒤 공백 자르기)
 *
 * 성채 이름, 주민 이름, 정의 파일의 키처럼 "바깥에서 들어오는 글자"는 모두 여기서 검사합니다.
 */
#ifndef CASTLE_TEXT_H
#define CASTLE_TEXT_H

#include <stddef.h>
#include <stdint.h>

/* 1..max_bytes-1 바이트, 올바른 UTF-8, 제어 문자 없음이면 1 */
int text_is_valid_name(const char *name, int32_t max_bytes);
/* dst(크기 dst_size)에 src 를 복사합니다. 넘치면 0 을 돌려주고 dst 는 빈 문자열 */
int text_copy(char *dst, size_t dst_size, const char *src);
/* 앞뒤 공백(스페이스, 탭, \r)을 잘라 냅니다. s 를 직접 고치고, 시작 위치를 돌려줍니다 */
char *text_trim(char *s);

#endif
