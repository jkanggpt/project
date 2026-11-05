/*
 * castle_core.h — 마법성채 키우기 C 코어의 공개 API
 *
 * Godot(C#)과 CLI는 이 헤더에 있는 함수와 구조체만 씁니다.
 * - 월드는 불투명 포인터(CoreWorld *)로만 다룹니다. 안을 들여다보지 않습니다.
 * - 데이터는 "뷰 구조체"(int32_t 필드와 고정 크기 UTF-8 char 배열)로
 *   호출한 쪽이 준비한 버퍼에 복사해서 건넵니다. C 메모리를 가리키는 포인터는 밖으로 나가지 않습니다.
 * - 모든 함수는 인자를 검사하고, 문제가 있으면 죽지 않고 음수 오류 코드(CoreResult)를 돌려줍니다.
 * - 구조체나 함수 모양을 바꾸면 CORE_ABI_VERSION 을 올리고 C# 쪽(Views.cs)도 같이 고칩니다.
 */
#ifndef CASTLE_CORE_H
#define CASTLE_CORE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- DLL 내보내기 표시 ------------------------------------------------- */
#if defined(CASTLE_CORE_STATIC)
#  define CORE_API
#elif defined(_WIN32)
#  if defined(CASTLE_CORE_BUILD)
#    define CORE_API __declspec(dllexport)
#  else
#    define CORE_API __declspec(dllimport)
#  endif
#else
#  if defined(CASTLE_CORE_BUILD)
#    define CORE_API __attribute__((visibility("default")))
#  else
#    define CORE_API
#  endif
#endif

/* C#의 Core.ExpectedAbiVersion 과 같아야 합니다. */
#define CORE_ABI_VERSION 1

/* 성채 이름 등 문자열 버퍼 크기 (UTF-8 바이트, 끝의 '\0' 포함) */
#define CORE_NAME_BYTES 48

/* ---- 결과 코드 -------------------------------------------------------- */
typedef enum CoreResult {
    CORE_OK = 0,
    CORE_ERR_NULL_ARG = -1,         /* 꼭 필요한 포인터가 NULL */
    CORE_ERR_INVALID_ARG = -2,      /* 값의 범위가 잘못됨 */
    CORE_ERR_OUT_OF_MEMORY = -3,    /* malloc/calloc/realloc 실패 */
    CORE_ERR_BUFFER_TOO_SMALL = -4, /* 호출한 쪽 버퍼가 작음 */
    CORE_ERR_FILE_OPEN = -5,        /* 파일을 열 수 없음 */
    CORE_ERR_FILE_IO = -6,          /* 읽기/쓰기/이름 바꾸기 실패 */
    CORE_ERR_FORMAT = -7,           /* 세이브 파일 내용이 잘못됨 */
    CORE_ERR_VERSION = -8,          /* 세이브 파일 버전이 다름 */
    CORE_ERR_STRUCT_SIZE = -9       /* ABI 검사: 구조체 크기가 다름 */
} CoreResult;

/* ---- 열거형 (값은 저장 파일과 C#에서 그대로 쓰므로 바꾸지 않습니다) ------ */
typedef enum CoreTerrain {
    CORE_TERRAIN_GRASS = 0,  /* 풀밭 */
    CORE_TERRAIN_FOREST = 1, /* 숲 */
    CORE_TERRAIN_ROCK = 2,   /* 바위 지대 */
    CORE_TERRAIN_WATER = 3,  /* 물 (지나갈 수 없음) */
    CORE_TERRAIN_MANA = 4,   /* 마정석 광맥 */
    CORE_TERRAIN_COUNT = 5
} CoreTerrain;

typedef enum CoreResource {
    CORE_RES_GOLD = 0,  /* 금화 */
    CORE_RES_MANA = 1,  /* 마정석 */
    CORE_RES_FOOD = 2,  /* 식량 */
    CORE_RES_WOOD = 3,  /* 목재 */
    CORE_RES_STONE = 4, /* 석재 */
    CORE_RES_HERBS = 5, /* 약초 */
    CORE_RES_COUNT = 6
} CoreResource;

typedef enum CoreSeason {
    CORE_SEASON_SPRING = 0,
    CORE_SEASON_SUMMER = 1,
    CORE_SEASON_AUTUMN = 2,
    CORE_SEASON_WINTER = 3,
    CORE_SEASON_COUNT = 4
} CoreSeason;

typedef enum CoreDifficulty {
    CORE_DIFF_PEACEFUL = 0, /* 평화 */
    CORE_DIFF_NORMAL = 1,   /* 보통 */
    CORE_DIFF_HARSH = 2,    /* 혹독 */
    CORE_DIFF_COUNT = 3
} CoreDifficulty;

/* 피드백 이벤트 종류. 문장은 Godot/CLI가 이 코드로 만듭니다. (코어는 언어를 모릅니다) */
typedef enum CoreEventType {
    CORE_EVT_NONE = 0,
    CORE_EVT_DAY_STARTED = 1,    /* a = 새 날짜(1부터) */
    CORE_EVT_SEASON_CHANGED = 2, /* a = 계절(CoreSeason), b = 연도 */
    CORE_EVT_NIGHT_FELL = 3,     /* a = 날짜 */
    CORE_EVT_DAWN = 4,           /* a = 날짜 */
    CORE_EVT_WARNING = 5         /* a = 경고 코드(CoreWarning), b = 추가 정보 */
} CoreEventType;

typedef enum CoreWarning {
    CORE_WARN_NONE = 0,
    CORE_WARN_EVENTS_DROPPED = 1 /* 이벤트 큐가 가득 차서 오래된 이벤트를 버림. b = 버린 개수 */
} CoreWarning;

/* ---- 뷰 구조체 (C#의 Views.cs 와 필드 순서·크기가 같아야 함) ------------ */

/* 새 게임 설정 */
typedef struct CoreNewGameParams {
    char castle_name[CORE_NAME_BYTES]; /* UTF-8, '\0'으로 끝남. 1바이트 이상, 제어 문자 없음 */
    int32_t seed;                      /* 0 이상. 같은 시드 → 같은 지도 */
    int32_t difficulty;                /* CoreDifficulty */
} CoreNewGameParams;

/* 게임 전체에 고정된 설정값 (월드가 없어도 읽을 수 있음) */
typedef struct CoreConfigView {
    int32_t abi_version;
    int32_t minutes_per_tick;   /* 한 틱 = 게임 속 10분 */
    int32_t ticks_per_day;      /* 144 */
    int32_t days_per_season;    /* 8 */
    int32_t seasons_per_year;   /* 4 */
    int32_t real_ms_per_tick;   /* 1배속에서 한 틱이 걸리는 실제 시간(ms) */
    int32_t max_speed;          /* 최고 배속 */
    int32_t max_ticks_per_call; /* core_tick 한 번에 진행할 수 있는 최대 틱 */
    int32_t night_start_minute; /* 밤이 시작되는 시각(0시부터 분) */
    int32_t night_end_minute;   /* 밤이 끝나는 시각 */
    int32_t resource_count;
    int32_t terrain_count;
} CoreConfigView;

/* 월드 요약: HUD가 매 틱 읽는 값 */
typedef struct CoreSummaryView {
    int32_t tick;             /* 1일 0시부터 센 총 틱 수 */
    int32_t day;              /* 1일부터 */
    int32_t minute_of_day;    /* 0..1439 */
    int32_t season;           /* CoreSeason */
    int32_t day_of_season;    /* 1..days_per_season */
    int32_t year;             /* 1년부터 */
    int32_t is_night;         /* 1이면 밤 */
    int32_t difficulty;       /* CoreDifficulty */
    int32_t seed;
    int32_t map_width;
    int32_t map_height;
    int32_t keep_x;           /* 성채 중심 타일 */
    int32_t keep_y;
    int32_t keep_size;        /* 성채가 차지하는 정사각형 한 변(타일, 홀수). 중심이 keep_x, keep_y */
    int32_t cost_grid_rev;    /* 이동 비용 격자가 바뀔 때마다 1씩 증가 */
    int32_t storage_capacity; /* 식량·목재·석재·약초 각각의 저장 한도 */
    int32_t resources[CORE_RES_COUNT];
    char castle_name[CORE_NAME_BYTES];
} CoreSummaryView;

/* 피드백 이벤트 하나 (소리, 알림 문구용). 놓쳐도 상태가 어긋나지 않습니다. */
typedef struct CoreEventView {
    int32_t type; /* CoreEventType */
    int32_t a;
    int32_t b;
    int32_t c;
    int32_t d;
} CoreEventView;

/* ABI 검사용: C가 아는 구조체 크기. C#이 Marshal.SizeOf 와 비교합니다. */
typedef struct CoreStructSizes {
    int32_t new_game_params;
    int32_t config_view;
    int32_t summary_view;
    int32_t event_view;
} CoreStructSizes;

/* 불투명 월드 핸들 */
typedef struct CoreWorld CoreWorld;

/* ---- 버전, ABI ------------------------------------------------------- */
CORE_API int32_t core_abi_version(void);
/* out_size 는 호출한 쪽이 아는 sizeof(CoreStructSizes). 다르면 CORE_ERR_STRUCT_SIZE */
CORE_API int32_t core_get_struct_sizes(CoreStructSizes *out, int32_t out_size);
CORE_API int32_t core_get_config(CoreConfigView *out);
/* 결과 코드의 영어 이름 (로그용). 정적 문자열이므로 해제하지 않습니다. */
CORE_API const char *core_result_name(int32_t code);

/* ---- 생성, 해제 ------------------------------------------------------- */
CORE_API int32_t core_create(const CoreNewGameParams *params, CoreWorld **out_world);
CORE_API void core_destroy(CoreWorld *world);

/* ---- 시간 ------------------------------------------------------------ */
/* ticks: 1..max_ticks_per_call */
CORE_API int32_t core_tick(CoreWorld *world, int32_t ticks);

/* ---- 읽기 ------------------------------------------------------------ */
CORE_API int32_t core_get_summary(const CoreWorld *world, CoreSummaryView *out);
CORE_API int32_t core_get_map_size(const CoreWorld *world, int32_t *out_width, int32_t *out_height);
/* 지형 격자를 행 우선(y*width + x)으로 복사. capacity >= width*height 여야 함 */
CORE_API int32_t core_copy_terrain(const CoreWorld *world, uint8_t *out, int32_t capacity);
/* 이동 비용 격자 복사. 0 = 지나갈 수 없음, 10 = 보통 풀밭. out_rev 는 NULL 가능 */
CORE_API int32_t core_copy_cost_grid(const CoreWorld *world, uint8_t *out, int32_t capacity,
                                     int32_t *out_rev);

/* ---- 피드백 이벤트 ---------------------------------------------------- */
/* 이벤트가 있으면 out 에 채우고 1, 없으면 0, 오류면 음수 */
CORE_API int32_t core_poll_event(CoreWorld *world, CoreEventView *out);

/* ---- 저장, 불러오기 ---------------------------------------------------- */
/* 임시 파일에 쓰고 바꿔치기합니다. 기존 파일은 <path>.bak 으로 남깁니다. 경로는 UTF-8 */
CORE_API int32_t core_save(const CoreWorld *world, const char *path_utf8);
/* 성공하면 *out_world 에 새 월드. 실패하면 *out_world 는 NULL, out_error_line 에 문제 줄 번호(없으면 0) */
CORE_API int32_t core_load(const char *path_utf8, CoreWorld **out_world, int32_t *out_error_line);

/* ---- 디버그 ---------------------------------------------------------- */
/* 불변 조건 위반 개수 (0이면 정상), 월드가 NULL 이면 음수 */
CORE_API int32_t core_validate(const CoreWorld *world);
/* 저장되는 상태 전체의 64비트 해시. 저장→불러오기 후 같아야 합니다. */
CORE_API int32_t core_state_hash(const CoreWorld *world, uint64_t *out_hash);

#ifdef __cplusplus
}
#endif

#endif /* CASTLE_CORE_H */
