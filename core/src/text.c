/*
 * text.c — 문자열 도우미 (설명은 text.h)
 */
#include "text.h"

#include <string.h>

/* UTF-8 바이트열 검사: 잘린 글자, 너무 긴 표현, 서로게이트, 제어 문자를 거부합니다 */
int text_is_valid_name(const char *name, int32_t max_bytes)
{
    const unsigned char *s = (const unsigned char *)name;
    int32_t length = 0;

    while (length < max_bytes && s[length] != '\0') {
        length++;
    }
    if (length == 0 || length >= max_bytes) {
        return 0; /* 비어 있거나 '\0' 자리가 없음 */
    }
    while (*s != '\0') {
        unsigned char c = *s;
        int extra;
        uint32_t code;
        int i;
        if (c < 0x20 || c == 0x7F) {
            return 0; /* 줄바꿈 등 제어 문자 (저장 파일은 한 줄에 한 값) */
        }
        if (c < 0x80) {
            s++;
            continue;
        }
        if ((c & 0xE0) == 0xC0) {
            extra = 1;
            code = c & 0x1F;
        } else if ((c & 0xF0) == 0xE0) {
            extra = 2;
            code = c & 0x0F;
        } else if ((c & 0xF8) == 0xF0) {
            extra = 3;
            code = c & 0x07;
        } else {
            return 0;
        }
        for (i = 1; i <= extra; i++) {
            if ((s[i] & 0xC0) != 0x80) {
                return 0; /* 이어지는 바이트가 아님 ('\0' 도 여기서 걸러짐) */
            }
            code = (code << 6) | (s[i] & 0x3F);
        }
        if ((extra == 1 && code < 0x80) || (extra == 2 && code < 0x800) || (extra == 3 && code < 0x10000) ||
            code > 0x10FFFF || (code >= 0xD800 && code <= 0xDFFF)) {
            return 0;
        }
        s += extra + 1;
    }
    return 1;
}

int text_copy(char *dst, size_t dst_size, const char *src)
{
    size_t length = strlen(src);
    if (dst_size == 0) {
        return 0;
    }
    if (length >= dst_size) {
        dst[0] = '\0';
        return 0;
    }
    memcpy(dst, src, length + 1);
    return 1;
}

char *text_trim(char *s)
{
    size_t length;
    while (*s == ' ' || *s == '\t' || *s == '\r') {
        s++;
    }
    length = strlen(s);
    while (length > 0 && (s[length - 1] == ' ' || s[length - 1] == '\t' || s[length - 1] == '\r')) {
        s[--length] = '\0';
    }
    return s;
}
