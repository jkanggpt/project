/*
 * validate.h — 불변 조건 검사기
 *
 * "항상 참이어야 하는 사실"(예: 자원은 0 이상, 지형 값은 범위 안, 비용 격자는 지형과 일치)을 모두 확인하고
 * 어긋난 개수를 돌려줍니다. 테스트, CLI 소크 테스트(매 틱), C 그래픽 프런트엔드 스모크 테스트, 불러오기
 * 검증에서 씁니다.
 */
#ifndef CASTLE_VALIDATE_H
#define CASTLE_VALIDATE_H

#include <stdint.h>

#include "world.h"

/* 위반을 발견할 때마다 불리는 함수 (테스트에서 무엇이 틀렸는지 출력하는 데 씀). NULL 이어도 됩니다 */
typedef void (*ValidateReportFn)(void *context, const char *what);

int32_t validate_world(const World *world, ValidateReportFn report, void *context);

#endif
