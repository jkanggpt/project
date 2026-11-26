#ifndef CASTLE_ART_TRACE_H
#define CASTLE_ART_TRACE_H
#include <stddef.h>
unsigned char *art_read_bytes(const char *path, int *size);
int art_write_bytes(const char *path, const void *bytes, size_t size);
#endif
