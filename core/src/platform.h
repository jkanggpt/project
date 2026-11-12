/*
 * platform.h — 운영체제마다 다른 파일 처리를 감추는 얇은 층
 *
 * 경로는 언제나 UTF-8 로 받습니다.
 * Windows 에서는 사용자 폴더 이름에 한글이 있을 수 있는데, 그냥 fopen 은 UTF-8 경로를 이해하지 못합니다.
 * 그래서 Windows 에서는 UTF-16 으로 바꿔서 _wfopen, MoveFileExW 등을 씁니다.
 */
#ifndef CASTLE_PLATFORM_H
#define CASTLE_PLATFORM_H

#include <stdio.h>

/* 실패하면 NULL */
FILE *platform_fopen_utf8(const char *path, const char *mode);
/* src 를 dst 로 옮깁니다. dst 가 있으면 한 번에 바꿔치기합니다. 성공 0, 실패 -1 */
int platform_replace_file(const char *src, const char *dst);
/* src 를 dst 로 복사합니다 (dst 가 있으면 덮어씀). 성공 0, 실패 -1 */
int platform_copy_file(const char *src, const char *dst);
/* 성공 0, 실패 -1 */
int platform_remove_file(const char *path);
/* 있으면 1 */
int platform_file_exists(const char *path);
/* 폴더를 만듭니다. 이미 있어도 성공. 성공 0, 실패 -1 */
int platform_make_dir(const char *path);

#endif
