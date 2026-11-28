/* castle_test_tools.h — 일반 규칙과 저장 형식에 속하지 않는 실행 중 검사 도구 */
#ifndef CASTLE_TEST_TOOLS_H
#define CASTLE_TEST_TOOLS_H
#include "castle_core.h"

#ifdef __cplusplus
extern "C" {
#endif

enum CoreTestFlag {
    CORE_TEST_ENABLED = 1 << 0,
    CORE_TEST_FREE_BUILD = 1 << 1,
    CORE_TEST_INSTANT_BUILD = 1 << 2,
    CORE_TEST_UNLOCK_BUILD = 1 << 3,
    CORE_TEST_REVEAL = 1 << 4,
    CORE_TEST_NO_RAIDS = 1 << 5,
    CORE_TEST_SUSTAIN = 1 << 6,
    CORE_TEST_RESOURCES = 1 << 7,
    CORE_TEST_ALL_FLAGS = (1 << 8) - 1
};

/* 설정은 저장하거나 상태 해시에 넣지 않습니다. weather=-1이면 자연 날씨를 씁니다.
 * ENABLED가 없으면 다른 비트도 없어야 합니다. 일반 세계에서는 변경 명령을 거절합니다. */
CORE_API int32_t core_test_configure(CoreWorld *world, int32_t flags, int32_t weather);
CORE_API int32_t core_test_read(const CoreWorld *world, int32_t *flags, int32_t *weather);
/* 자원은 해당 저장 한도까지 지정하며, 범위를 넘으면 아무것도 바꾸지 않습니다. */
CORE_API int32_t core_test_resource(CoreWorld *world, int32_t resource, int32_t amount);
/* 시각은 10분 단위로 앞으로 옮깁니다. 건너뛴 동안 생산·소비·결산을 실행하지 않습니다. */
CORE_API int32_t core_test_clock(CoreWorld *world, int32_t day, int32_t minute);
CORE_API int32_t core_test_rank(CoreWorld *world, int32_t rank);
/* index=-1이면 모든 연구를 완료합니다. 이미 완료한 연구도 안전하게 다시 계산합니다. */
CORE_API int32_t core_test_research(CoreWorld *world, int32_t index);
CORE_API int32_t core_test_finish_buildings(CoreWorld *world);
CORE_API int32_t core_test_restore(CoreWorld *world);
/* 주민 수 1~300명. 주거·일터는 자동으로 만들지 않습니다. */
CORE_API int32_t core_test_population(CoreWorld *world, int32_t count);
/* count=0이면 현재 습격을 끝냅니다. 그 외에는 세력·방향별 1~64명을 생성합니다. */
CORE_API int32_t core_test_raid(CoreWorld *world, int32_t type, int32_t direction, int32_t count);

#ifdef __cplusplus
}
#endif
#endif
