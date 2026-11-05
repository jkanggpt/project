/*
 * castle_core.h — 마법성채 키우기 C 코어의 공개 API
 *
 * Godot(C#)과 CLI는 이 헤더에 있는 함수와 구조체만 씁니다.
 * - 월드는 불투명 포인터(CoreWorld *)로만 다룹니다. 안을 들여다보지 않습니다.
 * - 데이터는 "뷰 구조체"(int32_t 필드와 고정 크기 UTF-8 char 배열)로
 *   호출한 쪽이 준비한 버퍼에 복사해서 건넵니다. C 메모리를 가리키는 포인터는 밖으로 나가지 않습니다.
 * - 모든 함수는 인자를 검사하고, 문제가 있으면 죽지 않고 음수 오류 코드(CoreResult)를 돌려줍니다.
 * - 구조체나 함수 모양을 바꾸면 CORE_ABI_VERSION 을 올리고 C# 쪽(Native.cs)도 같이 고칩니다.
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

/* C#의 CoreBridge.ExpectedAbiVersion 과 같아야 합니다. */
#define CORE_ABI_VERSION 2

/* 성채·주민 이름 버퍼 크기 (UTF-8 바이트, 끝의 '\0' 포함) */
#define CORE_NAME_BYTES 48
/* 정의 키(영어 식별자, 예: "farm") 버퍼 크기 */
#define CORE_KEY_BYTES 24

/* ---- 결과 코드 -------------------------------------------------------- */
typedef enum CoreResult {
    CORE_IGNORED = 1,               /* 오류는 아니지만 무시됨 (예: 오래된 intentRev 보고) */
    CORE_OK = 0,
    CORE_ERR_NULL_ARG = -1,         /* 꼭 필요한 포인터가 NULL */
    CORE_ERR_INVALID_ARG = -2,      /* 값의 범위가 잘못됨 */
    CORE_ERR_OUT_OF_MEMORY = -3,    /* malloc/calloc/realloc 실패 */
    CORE_ERR_BUFFER_TOO_SMALL = -4, /* 호출한 쪽 버퍼가 작음 */
    CORE_ERR_FILE_OPEN = -5,        /* 파일을 열 수 없음 */
    CORE_ERR_FILE_IO = -6,          /* 읽기/쓰기/이름 바꾸기 실패 */
    CORE_ERR_FORMAT = -7,           /* 세이브·정의 파일 내용이 잘못됨 */
    CORE_ERR_VERSION = -8,          /* 세이브 파일 버전이 다름 */
    CORE_ERR_STRUCT_SIZE = -9,      /* ABI 검사: 구조체 크기가 다름 */
    CORE_ERR_REJECTED = -10,        /* 게임 규칙상 할 수 없음 (이유는 out_reason) */
    CORE_ERR_NOT_FOUND = -11        /* 그런 id 의 주민·건물이 없음 */
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

/* 건물 종류. 도로(ROAD)는 건물이 아니라 지도의 한 층이지만, 짓는 방법이 같아서 같은 번호 체계를 씁니다. */
typedef enum CoreBuildingType {
    CORE_BLD_KEEP = 0,       /* 성채 (처음부터 있음, 저장소, 무너지면 패배) */
    CORE_BLD_HOUSE = 1,      /* 주택 */
    CORE_BLD_FARM = 2,       /* 농장 */
    CORE_BLD_GATHERER = 3,   /* 채집 오두막 */
    CORE_BLD_LUMBER = 4,     /* 벌목장 */
    CORE_BLD_QUARRY = 5,     /* 채석장 */
    CORE_BLD_MANA_MINE = 6,  /* 마정석 광산 */
    CORE_BLD_WAREHOUSE = 7,  /* 창고 */
    CORE_BLD_MARKET = 8,     /* 시장 */
    CORE_BLD_BARRACKS = 9,   /* 병영 */
    CORE_BLD_TOWER = 10,     /* 감시탑 */
    CORE_BLD_WALL = 11,      /* 성벽 */
    CORE_BLD_GATE = 12,      /* 성문 */
    CORE_BLD_HEALER = 13,    /* 치료소 */
    CORE_BLD_MAGE_TOWER = 14,/* 마탑 */
    CORE_BLD_ROAD = 15,      /* 도로 */
    CORE_BLD_COUNT = 16
} CoreBuildingType;

typedef enum CoreBuildingState {
    CORE_BSTATE_CONSTRUCTING = 0, /* 짓는 중 */
    CORE_BSTATE_ACTIVE = 1,       /* 다 지음 */
    CORE_BSTATE_RUINED = 2        /* 무너짐 (수리 가능, 마일스톤 4) */
} CoreBuildingState;

/* 건설 막대의 분류 */
typedef enum CoreBuildCategory {
    CORE_CAT_SPECIAL = 0,   /* 성채 (짓지 않음) */
    CORE_CAT_HOUSING = 1,   /* 주거 */
    CORE_CAT_RESOURCE = 2,  /* 자원 */
    CORE_CAT_STORAGE = 3,   /* 저장·거래 */
    CORE_CAT_DEFENSE = 4,   /* 방어 */
    CORE_CAT_SERVICE = 5,   /* 치료·마법 */
    CORE_CAT_ROAD = 6,      /* 도로 */
    CORE_CAT_COUNT = 7
} CoreBuildCategory;

/* 배치 규칙 (정의 파일의 placement 값) */
typedef enum CorePlacementRule {
    CORE_PLACE_ANY = 0,
    CORE_PLACE_NEAR_FOREST = 1, /* 작업 반경 안에 숲이 min_terrain 칸 이상 */
    CORE_PLACE_ADJ_ROCK = 2,    /* 바로 옆에 바위 지대가 있어야 함 */
    CORE_PLACE_ON_MANA = 3      /* 마정석 광맥 위에 지어야 함 */
} CorePlacementRule;

/* 배치 검사 결과 (core_can_place, core_place_* 의 out_reason) */
typedef enum CorePlaceResult {
    CORE_PLACE_OK = 0,
    CORE_PLACE_BAD_TYPE = 1,       /* 지을 수 없는 종류 (성채 등) */
    CORE_PLACE_OUT_OF_BOUNDS = 2,  /* 지도 밖 */
    CORE_PLACE_TOO_CLOSE_EDGE = 3, /* 지도 가장자리에 너무 가까움 */
    CORE_PLACE_BAD_TERRAIN = 4,    /* 물이나 바위 위 */
    CORE_PLACE_OCCUPIED = 5,       /* 다른 건물이 있음 */
    CORE_PLACE_ROAD_IN_WAY = 6,    /* 도로 위 (도로를 먼저 철거) */
    CORE_PLACE_BLOCKS_DOOR = 7,    /* 다른 건물의 문 앞을 막음 */
    CORE_PLACE_NO_DOOR = 8,        /* 이 건물의 문 앞이 막혀 있음 */
    CORE_PLACE_NEEDS_FOREST = 9,   /* 근처에 숲이 부족함 */
    CORE_PLACE_NEEDS_ROCK = 10,    /* 옆에 바위 지대가 없음 */
    CORE_PLACE_NEEDS_MANA = 11,    /* 마정석 광맥 위가 아님 */
    CORE_PLACE_LOCKED = 12,        /* 등급이 모자람 */
    CORE_PLACE_CANNOT_AFFORD = 13, /* 자원이 모자람 */
    CORE_PLACE_ALREADY_THERE = 14, /* 이미 같은 도로/성벽이 있음 */
    CORE_PLACE_BLOCKS_PATH = 15,   /* 지으면 다른 건물이나 지도 밖으로 가는 길이 끊김 */
    CORE_PLACE_UNREACHABLE = 16,   /* 성채에서 그 자리까지 걸어갈 길이 없음 */
    CORE_PLACE_COUNT = 17
} CorePlaceResult;

/* 주민 상태 (상태마다 C 의 처리 함수가 하나씩 있음) */
typedef enum CoreVillagerState {
    CORE_VS_IDLE = 0,         /* 광장에서 쉼 */
    CORE_VS_SLEEP = 1,        /* 집(없으면 광장)에서 잠 */
    CORE_VS_GO_WORK = 2,      /* 일터로 가는 중 */
    CORE_VS_WORKING = 3,      /* 일하는 중 (이때만 생산) */
    CORE_VS_EAT = 4,          /* 저장소로 가서 먹음 */
    CORE_VS_SEEK_HEALING = 5, /* 치료소로 감 (마일스톤 3) */
    CORE_VS_FLEE = 6,         /* 습격 중 성채로 피신 (마일스톤 4) */
    CORE_VS_GUARD_POST = 7,   /* 경비병 위치 (마일스톤 4) */
    CORE_VS_LEAVING = 8,      /* 마을을 떠나 지도 가장자리로 감 */
    CORE_VS_COUNT = 9
} CoreVillagerState;

/* 의도 플래그 (CoreVillagerView.intent_flags) */
#define CORE_INTENT_HOLD 1     /* 움직이지 말고 지금 자리에 있기 (길이 막혔을 때) */
#define CORE_INTENT_SPAWN_EDGE 2 /* 지도 가장자리에 처음 나타남 (이주민) */

/* 주민 상태 플래그 (CoreVillagerView.flags) */
#define CORE_VF_MANUAL 1     /* 플레이어가 직접 배치함 (자동 배치가 건드리지 않음) */
#define CORE_VF_UNPAID 2     /* 오늘 임금을 못 받음 */
#define CORE_VF_STARVING 4   /* 배고픔이 0 */
#define CORE_VF_HOMELESS 8   /* 집이 없음 */
#define CORE_VF_UNHAPPY 16   /* 기분이 낮아 떠날 위기 */
#define CORE_VF_SICK 32      /* 병 (마일스톤 3) */

/* 기분 항목 (기분 툴팁의 줄) */
typedef enum CoreMoodTerm {
    CORE_MOOD_BASE = 0,
    CORE_MOOD_HUNGER = 1,
    CORE_MOOD_REST = 2,
    CORE_MOOD_SAFETY = 3,
    CORE_MOOD_HOUSING = 4,
    CORE_MOOD_TRAITS = 5,
    CORE_MOOD_WAGES = 6,
    CORE_MOOD_MOURNING = 7,
    CORE_MOOD_TERM_COUNT = 8
} CoreMoodTerm;

/* 죽음의 원인 */
typedef enum CoreDeathCause {
    CORE_DEATH_STARVATION = 1,
    CORE_DEATH_ILLNESS = 2, /* 마일스톤 3 */
    CORE_DEATH_RAID = 3     /* 마일스톤 4 */
} CoreDeathCause;

/* 주민 이야기 (알림 문장) */
typedef enum CoreStory {
    CORE_STORY_FIRST_PRODUCE = 1, /* c = 자원: "OO이 첫 수확을 거뒀습니다" */
    CORE_STORY_MOVED_IN = 2,      /* 집이 생김 */
    CORE_STORY_NEW_JOB = 3        /* c = 건물 종류 */
} CoreStory;

/* 정의 파일 번호 (core_defs_load_* 의 file_id) */
typedef enum CoreDataFile {
    CORE_DATA_BUILDINGS = 0, /* data/buildings.def : [building 키] */
    CORE_DATA_TRAITS = 1,    /* data/traits.def    : [trait 키] */
    CORE_DATA_NAMES = 2,     /* data/names.txt     : 한 줄에 이름 하나 */
    CORE_DATA_COUNT = 3
} CoreDataFile;

/* 피드백 이벤트 종류. 문장은 Godot/CLI가 이 코드로 만듭니다. (코어는 언어를 모릅니다) */
typedef enum CoreEventType {
    CORE_EVT_NONE = 0,
    CORE_EVT_DAY_STARTED = 1,        /* a = 새 날짜(1부터) */
    CORE_EVT_SEASON_CHANGED = 2,     /* a = 계절(CoreSeason), b = 연도 */
    CORE_EVT_NIGHT_FELL = 3,         /* a = 날짜 */
    CORE_EVT_DAWN = 4,               /* a = 날짜 */
    CORE_EVT_WARNING = 5,            /* a = 경고 코드(CoreWarning), b = 추가 정보 */
    CORE_EVT_BUILDING_PLACED = 6,    /* a = 건물 id, b = 종류, c = x, d = y */
    CORE_EVT_BUILDING_COMPLETED = 7, /* a = 건물 id, b = 종류, c = x, d = y */
    CORE_EVT_BUILDING_REMOVED = 8,   /* a = 건물 id, b = 종류, c = x, d = y */
    CORE_EVT_PRODUCED = 9,           /* a = 건물 id, b = 자원, c = 양 */
    CORE_EVT_IMMIGRANT = 10,         /* a = 주민 id, text = 이름 */
    CORE_EVT_VILLAGER_LEAVING = 11,  /* a = 주민 id, text = 이름 */
    CORE_EVT_VILLAGER_LEFT = 12,     /* a = 주민 id, text = 이름 */
    CORE_EVT_VILLAGER_DIED = 13,     /* a = 주민 id, b = CoreDeathCause, text = 이름 */
    CORE_EVT_PATH_BLOCKED = 14,      /* a = 주민 id, text = 이름 */
    CORE_EVT_STORAGE_FULL = 15,      /* a = 자원 */
    CORE_EVT_WAGES_UNPAID = 16,      /* a = 임금을 못 받은 사람 수 */
    CORE_EVT_STORY = 17,             /* a = 주민 id, b = CoreStory, c = 값, text = 이름 */
    CORE_EVT_STARVING = 18,          /* a = 배고픔이 0 인 주민 수 */
    CORE_EVT_DEF_WARNING = 19        /* a = 정의 파일 번호(CoreDataFile), b = 줄 번호 (기본값을 씀) */
} CoreEventType;

typedef enum CoreWarning {
    CORE_WARN_NONE = 0,
    CORE_WARN_EVENTS_DROPPED = 1 /* 이벤트 큐가 가득 차서 오래된 이벤트를 버림. b = 버린 개수 */
} CoreWarning;

/* 이동 비용 격자의 특별한 값 (core_copy_cost_grid) */
#define CORE_COST_BLOCKED 0  /* 지나갈 수 없음 (물, 건물) */
#define CORE_COST_NORMAL 10  /* 보통 풀밭 */
#define CORE_COST_GATE 253   /* 성문: 주민은 지나가고 적은 막힘 */
#define CORE_COST_WALL 254   /* 성벽: 주민은 못 지나감, 적은 부수고 지나감 */

/* ---- 뷰 구조체 (C#의 Native.cs 와 필드 순서·크기가 같아야 함) ------------ */

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
    int32_t building_type_count;
    int32_t villager_state_count;
    int32_t walk_milli_tiles_per_tick; /* 보통 풀밭(비용 10)에서 주민이 한 틱에 걷는 거리 × 1000 */
    int32_t work_start_minute;  /* 일 시작 시각 */
    int32_t work_end_minute;    /* 일 끝 시각 */
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
    int32_t cost_grid_rev;    /* 이동 비용 격자(도로, 건물 포함)가 바뀔 때마다 1씩 증가 */
    int32_t population;       /* 주민 수 */
    int32_t housing_capacity; /* 다 지은 주택의 자리 수 */
    int32_t homeless;         /* 집 없는 주민 수 */
    int32_t employed;         /* 일터가 있는 주민 수 */
    int32_t job_slots;        /* 다 지은 건물의 일자리 수 */
    int32_t average_mood;     /* 평균 기분 0..100 (주민이 없으면 0) */
    int32_t building_count;
    int32_t villager_count_hint; /* core_copy_villagers 에 필요한 버퍼 크기 */
    int32_t building_count_hint; /* core_copy_buildings 에 필요한 버퍼 크기 */
    int32_t rank;             /* 등급 (마일스톤 5) */
    int32_t resources[CORE_RES_COUNT];
    int32_t resource_capacity[CORE_RES_COUNT]; /* 자원별 저장 한도. 0 이면 한도 없음 (금화, 마정석) */
    int32_t resource_delta[CORE_RES_COUNT];    /* 어제 하루 동안의 순변화 */
    char castle_name[CORE_NAME_BYTES];
} CoreSummaryView;

/* 피드백 이벤트 하나 (소리, 알림 문구용). 놓쳐도 상태가 어긋나지 않습니다. */
typedef struct CoreEventView {
    int32_t type; /* CoreEventType */
    int32_t a;
    int32_t b;
    int32_t c;
    int32_t d;
    char text[CORE_NAME_BYTES]; /* 관련 주민의 이름 (없으면 빈 문자열) */
} CoreEventView;

/* 주민 하나 */
typedef struct CoreVillagerView {
    int32_t id;
    int32_t state;            /* CoreVillagerState */
    int32_t intent_rev;       /* 의도가 바뀔 때마다 1 증가. 도착 보고에 그대로 돌려줌 */
    int32_t intent_flags;     /* CORE_INTENT_* */
    int32_t target_building;  /* 목표 건물 id (0 = 없음) */
    int32_t target_x;         /* 목표 칸 (건물이면 그 건물의 문 앞) */
    int32_t target_y;
    int32_t at_target;        /* 1 이면 목표에 도착해 있음 */
    int32_t loc_x;            /* 마지막으로 도착한 칸 (불러온 뒤 여기서 나타남) */
    int32_t loc_y;
    int32_t home_id;          /* 0 = 없음 */
    int32_t workplace_id;     /* 0 = 없음 */
    int32_t job;              /* 일터 건물 종류, 없으면 -1 */
    int32_t mood;             /* 0..100 */
    int32_t mood_target;      /* 기분이 향하는 값 */
    int32_t health;           /* 0..100 */
    int32_t hunger;           /* 0..100 (100 = 배부름) */
    int32_t rest;             /* 0..100 */
    int32_t safety;           /* 0..100 */
    int32_t housing;          /* 0..100 */
    int32_t strength;         /* 힘 1..10 */
    int32_t intellect;        /* 지능 */
    int32_t dexterity;        /* 손재주 */
    int32_t vitality;         /* 체력 */
    int32_t trait_a;          /* 특성 번호 (core_get_trait_def), -1 = 없음 */
    int32_t trait_b;
    int32_t age;
    int32_t flags;            /* CORE_VF_* */
    int32_t walk_speed;       /* 이 주민이 보통 풀밭에서 한 틱에 걷는 거리 × 1000 */
    char name[CORE_NAME_BYTES];
} CoreVillagerView;

/* 건물 하나 */
typedef struct CoreBuildingView {
    int32_t id;
    int32_t type;             /* CoreBuildingType */
    int32_t x;                /* 왼쪽 위 칸 */
    int32_t y;
    int32_t w;
    int32_t h;
    int32_t state;            /* CoreBuildingState */
    int32_t hp;
    int32_t max_hp;
    int32_t build_progress;   /* 지은 틱 수 */
    int32_t build_total;      /* 다 짓는 데 필요한 틱 수 */
    int32_t workers;          /* 배치된 일꾼 수 */
    int32_t worker_slots;
    int32_t residents;        /* 사는 주민 수 */
    int32_t resident_slots;
    int32_t working_now;      /* 지금 일하는 중인 일꾼 수 (연기 효과용) */
    int32_t auto_assign;      /* 1 = 자동 배치 켜짐 */
    int32_t door_x;           /* 문 앞 칸 (주민이 오가는 곳) */
    int32_t door_y;
    int32_t productivity;     /* 지형에 따른 생산성 % */
    int32_t flags;            /* 예약 */
} CoreBuildingView;

/* 건물 정의 (정의 파일에서 읽은 값 + 이 월드에서 지을 수 있는지) */
typedef struct CoreBuildingDefView {
    int32_t type;
    int32_t w;
    int32_t h;
    int32_t cost[CORE_RES_COUNT];
    int32_t build_ticks;
    int32_t max_hp;
    int32_t worker_slots;
    int32_t resident_slots;
    int32_t storage;          /* 늘려 주는 저장 한도 */
    int32_t produces;         /* 생산 자원, -1 = 없음 */
    int32_t rate;             /* 일꾼 한 명이 일하는 한 틱에 만드는 양 × 1000 (생산성 100%, 기분 50) */
    int32_t secondary;        /* 두 번째 생산 자원, -1 = 없음 */
    int32_t secondary_rate;
    int32_t work_radius;      /* 지형을 세는 반경 */
    int32_t placement;        /* CorePlacementRule */
    int32_t min_terrain;      /* 배치에 필요한 지형 칸 수 */
    int32_t ideal_terrain;    /* 생산성 100% 가 되는 지형 칸 수 */
    int32_t wage;             /* 일꾼 한 명의 하루 임금 (금화) */
    int32_t required_rank;
    int32_t category;         /* CoreBuildCategory */
    int32_t line;             /* 1 = 끌어서 줄로 놓음 (도로, 성벽) */
    int32_t available;        /* 1 = 이 월드에서 지금 지을 수 있음 (등급) */
    char key[CORE_KEY_BYTES];
} CoreBuildingDefView;

/* 특성 정의 */
typedef struct CoreTraitDefView {
    int32_t index;
    int32_t work_pct;   /* 일 속도 +% */
    int32_t food_pct;   /* 배고파지는 속도 +% */
    int32_t mood;       /* 기분 +값 */
    int32_t bravery;    /* 용기 (마일스톤 4) */
    int32_t magic_pct;  /* 연구 속도 +% (마일스톤 5) */
    char key[CORE_KEY_BYTES];
} CoreTraitDefView;

/* 배치 미리보기 */
typedef struct CorePlacementView {
    int32_t result;          /* CorePlaceResult */
    int32_t door_x;          /* 이 자리에 지으면 생길 문 앞 칸 (-1 = 없음) */
    int32_t door_y;
    int32_t terrain_count;   /* 작업 반경 안의 관련 지형 칸 수 */
    int32_t productivity;    /* 지형에 따른 생산성 % */
    int32_t cost[CORE_RES_COUNT];
} CorePlacementView;

/* 기분 한 줄 */
typedef struct CoreMoodTermView {
    int32_t term;  /* CoreMoodTerm */
    int32_t value; /* 기분에 더해지는 값 */
} CoreMoodTermView;

/* ABI 검사용: C가 아는 구조체 크기. C#이 Marshal.SizeOf 와 비교합니다. */
typedef struct CoreStructSizes {
    int32_t new_game_params;
    int32_t config_view;
    int32_t summary_view;
    int32_t event_view;
    int32_t villager_view;
    int32_t building_view;
    int32_t building_def_view;
    int32_t trait_def_view;
    int32_t placement_view;
    int32_t mood_term_view;
} CoreStructSizes;

/* 불투명 핸들 */
typedef struct CoreWorld CoreWorld;
typedef struct CoreDefs CoreDefs;

/* ---- 버전, ABI ------------------------------------------------------- */
CORE_API int32_t core_abi_version(void);
/* out_size 는 호출한 쪽이 아는 sizeof(CoreStructSizes). 다르면 CORE_ERR_STRUCT_SIZE */
CORE_API int32_t core_get_struct_sizes(CoreStructSizes *out, int32_t out_size);
CORE_API int32_t core_get_config(CoreConfigView *out);
/* 결과 코드의 영어 이름 (로그용). 정적 문자열이므로 해제하지 않습니다. */
CORE_API const char *core_result_name(int32_t code);

/* ---- 정의 (건물, 특성, 이름) ------------------------------------------ */
/* 내장 기본값으로 채운 정의 묶음을 만듭니다 */
CORE_API int32_t core_defs_create(CoreDefs **out_defs);
CORE_API void core_defs_destroy(CoreDefs *defs);
/* 정의 파일 내용(UTF-8)을 읽어 덮어씁니다. 잘못된 곳이 있으면 그 파일 전체를 버리고(기본값 유지)
 * CORE_ERR_FORMAT 과 줄 번호를 돌려줍니다. 경고는 새 월드를 만들 때 이벤트로 나옵니다. */
CORE_API int32_t core_defs_load_text(CoreDefs *defs, int32_t file_id, const char *text, int32_t length,
                                     int32_t *out_line);
/* 위와 같지만 파일에서 읽습니다 (CLI 용). 파일이 없으면 CORE_ERR_FILE_OPEN (기본값 유지) */
CORE_API int32_t core_defs_load_file(CoreDefs *defs, int32_t file_id, const char *path_utf8, int32_t *out_line);

/* 정의를 정의 파일 형식으로 저장합니다 (기본값으로 data 파일을 다시 만들 때) */
CORE_API int32_t core_defs_save_file(const CoreDefs *defs, int32_t file_id, const char *path_utf8);

/* ---- 생성, 해제 ------------------------------------------------------- */
/* defs 가 NULL 이면 내장 기본값. 월드는 defs 를 복사해 두므로 바로 해제해도 됩니다. */
CORE_API int32_t core_create(const CoreNewGameParams *params, const CoreDefs *defs, CoreWorld **out_world);
CORE_API void core_destroy(CoreWorld *world);

/* ---- 시간 ------------------------------------------------------------ */
/* ticks: 1..max_ticks_per_call */
CORE_API int32_t core_tick(CoreWorld *world, int32_t ticks);

/* ---- 읽기 ------------------------------------------------------------ */
CORE_API int32_t core_get_summary(const CoreWorld *world, CoreSummaryView *out);
CORE_API int32_t core_get_map_size(const CoreWorld *world, int32_t *out_width, int32_t *out_height);
/* 지형 격자를 행 우선(y*width + x)으로 복사. capacity >= width*height 여야 함 */
CORE_API int32_t core_copy_terrain(const CoreWorld *world, uint8_t *out, int32_t capacity);
/* 이동 비용 격자 복사. CORE_COST_* 참고. out_rev 는 NULL 가능 */
CORE_API int32_t core_copy_cost_grid(const CoreWorld *world, uint8_t *out, int32_t capacity,
                                     int32_t *out_rev);
/* 도로 격자 복사 (1 = 도로) */
CORE_API int32_t core_copy_roads(const CoreWorld *world, uint8_t *out, int32_t capacity);
/* 모든 주민/건물 뷰 복사. capacity 가 모자라면 CORE_ERR_BUFFER_TOO_SMALL 과 함께 *out_count 에 필요한 개수 */
CORE_API int32_t core_copy_villagers(const CoreWorld *world, CoreVillagerView *out, int32_t capacity,
                                     int32_t *out_count);
CORE_API int32_t core_copy_buildings(const CoreWorld *world, CoreBuildingView *out, int32_t capacity,
                                     int32_t *out_count);
CORE_API int32_t core_get_building_def(const CoreWorld *world, int32_t type, CoreBuildingDefView *out);
/* 특성 정의. index 가 범위 밖이면 CORE_ERR_NOT_FOUND (0 부터 차례로 읽으면 개수를 알 수 있음) */
CORE_API int32_t core_get_trait_def(const CoreWorld *world, int32_t index, CoreTraitDefView *out);
/* 주민 기분의 항목별 값 (툴팁). 항목 수는 CORE_MOOD_TERM_COUNT 이하 */
CORE_API int32_t core_get_mood_terms(const CoreWorld *world, int32_t villager_id, CoreMoodTermView *out,
                                     int32_t capacity, int32_t *out_count);

/* ---- 배치와 명령 ------------------------------------------------------ */
/* (x, y) 는 건물의 왼쪽 위 칸. 함수는 인자가 맞으면 CORE_OK, 결과는 out->result */
CORE_API int32_t core_can_place(const CoreWorld *world, int32_t type, int32_t x, int32_t y,
                                CorePlacementView *out);
/* 비용을 내고 짓기 시작합니다. 규칙상 안 되면 CORE_ERR_REJECTED, *out_reason 에 CorePlaceResult */
CORE_API int32_t core_place_building(CoreWorld *world, int32_t type, int32_t x, int32_t y, int32_t *out_id,
                                     int32_t *out_reason);
/* 도로·성벽을 (x0,y0) 에서 (x1,y1) 까지 가로 → 세로 꺾인 줄로 놓습니다. 놓을 수 있는 칸만 놓고,
 * 자원이 떨어지면 멈춥니다. *out_placed = 놓은 칸 수, *out_reason = 처음 실패한 이유 */
CORE_API int32_t core_place_line(CoreWorld *world, int32_t type, int32_t x0, int32_t y0, int32_t x1, int32_t y1,
                                 int32_t *out_placed, int32_t *out_reason);
/* 건물을 철거하고 비용의 절반을 돌려받습니다 (성채는 철거 불가) */
CORE_API int32_t core_demolish(CoreWorld *world, int32_t building_id);
/* (x, y) 의 도로를 없앱니다 */
CORE_API int32_t core_remove_road(CoreWorld *world, int32_t x, int32_t y);
/* 주민을 건물의 일꾼으로 직접 배치합니다 (자동 배치가 더는 옮기지 않음) */
CORE_API int32_t core_assign_worker(CoreWorld *world, int32_t villager_id, int32_t building_id);
/* 주민을 일터에서 뺍니다 (직접 배치한 것으로 표시되어 자동 배치도 다시 넣지 않음) */
CORE_API int32_t core_unassign_worker(CoreWorld *world, int32_t villager_id);
/* 직접 배치 표시를 지워 자동 배치에 다시 맡깁니다 */
CORE_API int32_t core_release_worker(CoreWorld *world, int32_t villager_id);
CORE_API int32_t core_set_auto_assign(CoreWorld *world, int32_t building_id, int32_t enabled);

/* ---- 보고 (Godot → C) ------------------------------------------------ */
/* 주민이 의도의 목표에 도착함. intent_rev 가 지금 것과 다르면 CORE_IGNORED */
CORE_API int32_t core_villager_arrived(CoreWorld *world, int32_t villager_id, int32_t intent_rev);
/* 목표까지 길이 없음. C 가 다른 할 일을 고르고 알림을 냅니다 */
CORE_API int32_t core_villager_unreachable(CoreWorld *world, int32_t villager_id, int32_t intent_rev);

/* ---- 피드백 이벤트 ---------------------------------------------------- */
/* 이벤트가 있으면 out 에 채우고 1, 없으면 0, 오류면 음수 */
CORE_API int32_t core_poll_event(CoreWorld *world, CoreEventView *out);

/* ---- 저장, 불러오기 ---------------------------------------------------- */
/* 임시 파일에 쓰고 바꿔치기합니다. 기존 파일은 <path>.bak 으로 남깁니다. 경로는 UTF-8 */
CORE_API int32_t core_save(const CoreWorld *world, const char *path_utf8);
/* 성공하면 *out_world 에 새 월드. 실패하면 *out_world 는 NULL, out_error_line 에 문제 줄 번호(없으면 0).
 * defs 가 NULL 이면 내장 기본값 */
CORE_API int32_t core_load(const char *path_utf8, const CoreDefs *defs, CoreWorld **out_world,
                           int32_t *out_error_line);

/* ---- 디버그 ---------------------------------------------------------- */
/* 불변 조건 위반 개수 (0이면 정상), 월드가 NULL 이면 음수 */
CORE_API int32_t core_validate(const CoreWorld *world);
/* 저장되는 상태 전체의 64비트 해시. 저장→불러오기 후 같아야 합니다. */
CORE_API int32_t core_state_hash(const CoreWorld *world, uint64_t *out_hash);

#ifdef __cplusplus
}
#endif

#endif /* CASTLE_CORE_H */
