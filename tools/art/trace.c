/* trace.c — 동적 제작 경로를 실제 I/O 성공에 따라 원장에 기록합니다. */
#include "trace.h"
#include "storage.h"
#include <stdio.h>
unsigned char *art_read_bytes(const char *path, int *size) {
    unsigned char *bytes = game_read_bytes(path, size);
    if (bytes)
        fprintf(stderr, "ARTINPUT\t%s\n", path);
    return bytes;
}
int art_write_bytes(const char *path, const void *bytes, size_t size) {
    int ok = game_write_bytes(path, bytes, size);
    if (ok)
        fprintf(stderr, "ARTOUTPUT\t%s\n", path);
    return ok;
}
