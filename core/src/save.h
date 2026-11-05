/*
 * save.h — 저장과 불러오기
 *
 * 형식: 사람이 읽을 수 있는 텍스트. 첫 줄은 "MAGICCASTLE_SAVE <버전>", 그 뒤로 [섹션] 과 키=값 줄.
 * 포인터는 절대 저장하지 않습니다 (다음 실행에서는 주소가 달라지므로). id 를 저장하고
 * 불러온 뒤 id 로 포인터를 다시 이어 줍니다. 정의(건물 수치 등)는 저장하지 않고 불러올 때 받은 것을 씁니다.
 */
#ifndef CASTLE_SAVE_H
#define CASTLE_SAVE_H

#include <stdint.h>

#include "world.h"

#define SAVE_MAGIC "MAGICCASTLE_SAVE"
#define SAVE_VERSION 2

/* 임시 파일(<path>.tmp)에 쓰고, 기존 파일은 <path>.bak 으로 복사한 뒤, 임시 파일로 바꿔치기합니다 */
int save_write(const World *world, const char *path);

/* 성공하면 *out_world 에 새 월드. 실패하면 부분적으로 읽은 것을 모두 해제하고 *out_world = NULL,
 * *out_line 에 문제가 된 줄 번호(파일을 못 열었으면 0) */
int save_read(const char *path, const Defs *defs, World **out_world, int32_t *out_line);

#endif
