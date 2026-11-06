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
#define CORE_ABI_VERSION 5

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
    CORE_BSTATE_RUINED = 2        /* 무너짐: 일도 잠도 못 함. core_repair 로 고침 */
} CoreBuildingState;

/* 건물 상태 플래그 (CoreBuildingView.flags) */
#define CORE_BF_COLD 1 /* 지난밤 난방을 못 한 집 (나무 부족) */

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
    CORE_VS_SEEK_HEALING = 5, /* 아픈 주민이 치료소로 감 */
    CORE_VS_FLEE = 6,         /* 습격 중 성채로 피신 (도착하면 성채 안에 숨음) */
    CORE_VS_GUARD_POST = 7,   /* 습격 중 병영 일꾼이 경비병으로 나섬 (싸움은 Godot 이 함) */
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
#define CORE_VF_SICK 32      /* 병 (일하지 못하고, 치료소에서 약초로 낫거나 며칠 뒤 저절로 나음) */

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
    CORE_MOOD_COLD = 8,      /* 난방 못 한 집, 겨울에 집 없음 */
    CORE_MOOD_SICKNESS = 9,  /* 병 */
    CORE_MOOD_TERM_COUNT = 10
} CoreMoodTerm;

/* 죽음의 원인 */
typedef enum CoreDeathCause {
    CORE_DEATH_STARVATION = 1,
    CORE_DEATH_ILLNESS = 2,
    CORE_DEATH_RAID = 3     /* 습격에 쓰러짐 */
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
    CORE_DATA_ENEMIES = 3,   /* data/enemies.def   : [enemy 키] */
    CORE_DATA_RESEARCH = 4,  /* data/research.def  : [research 키] (파일이 목록 전체를 정함) */
    CORE_DATA_RANKS = 5,     /* data/ranks.def     : [rank 키] (등급 5개의 조건) */
    CORE_DATA_COUNT = 6
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
    CORE_EVT_DEF_WARNING = 19,       /* a = 정의 파일 번호(CoreDataFile), b = 줄 번호 (기본값을 씀) */
    CORE_EVT_WEATHER_CHANGED = 20,   /* a = 날씨(CoreWeather) */
    CORE_EVT_NATURE_FORECAST = 21,   /* a = 내일 시작할 자연 사건(CoreNatureEvent) */
    CORE_EVT_NATURE_STARTED = 22,    /* a = 사건, b = 이어지는 날 수 */
    CORE_EVT_NATURE_ENDED = 23,      /* a = 사건 */
    CORE_EVT_BUILDING_DAMAGED = 24,  /* a = 건물 id, b = 종류, c = x, d = y */
    CORE_EVT_BUILDING_RUINED = 25,   /* a = 건물 id, b = 종류, c = x, d = y */
    CORE_EVT_BUILDING_REPAIRED = 26, /* a = 건물 id, b = 종류, c = x, d = y */
    CORE_EVT_VILLAGER_SICK = 27,     /* a = 주민 id, text = 이름 */
    CORE_EVT_VILLAGER_CURED = 28,    /* a = 주민 id, b = 1 이면 치료소에서, 0 이면 저절로, text = 이름 */
    CORE_EVT_MERCHANT_ARRIVED = 29,  /* a = 머무는 날 수, b = 거래 제안 수 */
    CORE_EVT_MERCHANT_LEFT = 30,
    CORE_EVT_WINTER_WARNING = 31,    /* a = 겨울에 필요한 식량, b = 필요한 목재 (가을에, 모자랄 때) */
    CORE_EVT_HOUSES_COLD = 32,       /* a = 지난밤 난방을 못 한 집 수 */
    CORE_EVT_THREAT_SCHEDULED = 33,  /* a = 위협 종류(CoreThreatType), b = 방향(CoreDirection), c = 도착까지 틱, d = 세기(적 수) */
    CORE_EVT_RAID_STARTED = 34,      /* a = 습격 id, b = 위협 종류, c = 적 수, d = 방향 */
    CORE_EVT_RAID_ENDED = 35,        /* a = 쓰러뜨린 적, b = 전체 적, c = 빼앗긴 금화, d = 쓰러진 주민 */
    CORE_EVT_ENEMY_KILLED = 36,      /* a = 적 id, b = 적 종류, c = 떨어뜨린 마정석 */
    CORE_EVT_LOOT_STOLEN = 37,       /* a = 자원, b = 양, c = 건물 id */
    CORE_EVT_THREAT_NEAR = 38,       /* a = 위협 종류, b = 방향 (도착 반나절 전, 경보) */
    CORE_EVT_RESEARCH_STARTED = 39,  /* a = 연구 번호 */
    CORE_EVT_RESEARCH_DONE = 40,     /* a = 연구 번호, b = 끝낸 연구 수 */
    CORE_EVT_RANK_READY = 41,        /* a = 오를 수 있게 된 등급 (조건을 처음 채웠을 때 한 번) */
    CORE_EVT_RANK_UP = 42,           /* a = 새 등급 (CoreRank) */
    CORE_EVT_VICTORY = 43,           /* a = 날짜 (왕성이 됨. 계속할 수 있음) */
    CORE_EVT_DEFEAT = 44,            /* a = 원인 (CoreLoseReason), b = 날짜 */
    CORE_EVT_TUTORIAL_STEP = 45,     /* a = 목표 (CoreTutorialStep), b = 끝낸 목표 수, c = 전체 */
    CORE_EVT_TUTORIAL_DONE = 46      /* a = 보상 금화, b = 보상 마정석 */
} CoreEventType;

typedef enum CoreWarning {
    CORE_WARN_NONE = 0,
    CORE_WARN_EVENTS_DROPPED = 1 /* 이벤트 큐가 가득 차서 오래된 이벤트를 버림. b = 버린 개수 */
} CoreWarning;

/* 날씨 (하루 단위, C 가 정하고 저장함) */
typedef enum CoreWeather {
    CORE_WEATHER_CLEAR = 0, /* 맑음 */
    CORE_WEATHER_RAIN = 1,  /* 비: 농장 +10% */
    CORE_WEATHER_SNOW = 2,  /* 눈: 걷기 느려짐 */
    CORE_WEATHER_STORM = 3, /* 폭풍 (폭풍 사건 날) */
    CORE_WEATHER_COUNT = 4
} CoreWeather;

/* 자연 사건 (처리 함수는 nature.c 의 NATURE_EVENTS 표) */
typedef enum CoreNatureEvent {
    CORE_NATURE_NONE = 0,
    CORE_NATURE_DROUGHT = 1,   /* 가뭄 (여름): 농장 −50% */
    CORE_NATURE_PLAGUE = 2,    /* 역병: 주민이 앓고 옮음 */
    CORE_NATURE_STORM = 3,     /* 폭풍: 건물 피해 */
    CORE_NATURE_COLD_SNAP = 4, /* 혹한 (겨울): 난방 나무가 더 듦 */
    CORE_NATURE_BUMPER = 5,    /* 풍년: 농장 +50% */
    CORE_NATURE_COUNT = 6
} CoreNatureEvent;

/* 장부 항목: 자원이 어디서 들어오고 어디로 나갔는지 (core_get_ledger) */
typedef enum CoreLedgerCategory {
    CORE_LEDGER_PRODUCTION = 0,   /* 생산 */
    CORE_LEDGER_CONSUMPTION = 1,  /* 식사 */
    CORE_LEDGER_WAGES = 2,        /* 임금 */
    CORE_LEDGER_TAXES = 3,        /* 세금 */
    CORE_LEDGER_CONSTRUCTION = 4, /* 건설 */
    CORE_LEDGER_REFUND = 5,       /* 철거 환불 */
    CORE_LEDGER_TRADE = 6,        /* 시장·상인 거래 */
    CORE_LEDGER_HEATING = 7,      /* 난방 */
    CORE_LEDGER_RAID = 8,         /* 약탈·전리품 (마일스톤 4) */
    CORE_LEDGER_OTHER = 9,        /* 저장 한도 초과로 버림 등 */
    CORE_LEDGER_HEALING = 10,     /* 치료에 쓴 약초 */
    CORE_LEDGER_REPAIR = 11,      /* 수리 */
    CORE_LEDGER_TRIBUTE = 12,     /* 승급 공물 */
    CORE_LEDGER_RESEARCH = 13,    /* 연구 비용 */
    CORE_LEDGER_COUNT = 14
} CoreLedgerCategory;

/* 거래·수리 명령을 거절한 이유 (out_reason) */
typedef enum CoreDenyReason {
    CORE_DENY_NONE = 0,
    CORE_DENY_NO_MARKET = 1,          /* 다 지은 시장이 없음 */
    CORE_DENY_NO_TRADER = 2,          /* 시장에 일꾼이 없음 */
    CORE_DENY_NOT_TRADABLE = 3,       /* 시장에서 다루지 않는 자원 (금화, 마정석) */
    CORE_DENY_NO_GOLD = 4,            /* 금화가 모자람 */
    CORE_DENY_NO_GOODS = 5,           /* 팔 물건이 모자람 */
    CORE_DENY_STORAGE_FULL = 6,       /* 저장 한도에 자리가 없음 */
    CORE_DENY_NO_MERCHANT = 7,        /* 상인이 없음 */
    CORE_DENY_DEAL_TAKEN = 8,         /* 이미 한 거래 */
    CORE_DENY_NOT_DAMAGED = 9,        /* 고칠 곳이 없음 */
    CORE_DENY_UNDER_CONSTRUCTION = 10,/* 아직 짓는 중 */
    CORE_DENY_BAD_AMOUNT = 11,        /* 수량이 범위 밖 */
    CORE_DENY_NO_RESOURCES = 12,      /* 수리 비용이 모자람 */
    CORE_DENY_RAID = 13,              /* 습격 중 (싸우는 동안에는 고칠 수 없음) */
    CORE_DENY_MAX_RANK = 14,          /* 이미 가장 높은 등급 */
    CORE_DENY_REQUIREMENTS = 15,      /* 승급 조건(인구·기분·방어·연구)이 모자람 (core_get_rank 로 무엇이) */
    CORE_DENY_NO_MANA = 16,           /* 마정석이 모자람 */
    CORE_DENY_RESEARCH_BUSY = 17,     /* 이미 다른 연구를 하는 중 */
    CORE_DENY_RESEARCH_DONE = 18,     /* 이미 끝낸 연구 */
    CORE_DENY_RESEARCH_LOCKED = 19,   /* 앞선 연구나 등급이 모자람 */
    CORE_DENY_GAME_OVER = 20,         /* 게임이 끝남 (패배) */
    CORE_DENY_COUNT = 21
} CoreDenyReason;

/* 위협 종류 (습격 구성 함수는 threat.c 의 THREAT_KINDS 표) */
typedef enum CoreThreatType {
    CORE_THREAT_NONE = 0,
    CORE_THREAT_BANDITS = 1,   /* 도적: 작고, 저장소에서 금화를 훔쳐 달아남 */
    CORE_THREAT_MONSTERS = 2,  /* 마물: 밤에 옴, 쓰러뜨리면 마정석 */
    CORE_THREAT_ARMY = 3,      /* 적대 영주 군대: 크고, 오래 전에 예고, 건물을 부숨 */
    CORE_THREAT_COUNT = 4
} CoreThreatType;

typedef enum CoreDirection {
    CORE_DIR_NORTH = 0,
    CORE_DIR_EAST = 1,
    CORE_DIR_SOUTH = 2,
    CORE_DIR_WEST = 3,
    CORE_DIR_COUNT = 4
} CoreDirection;

/* 적의 행동 (Godot 의 CombatDirector 가 이 값으로 목표를 고름) */
typedef enum CoreEnemyBehavior {
    CORE_BEHAVIOR_THIEF = 0,   /* 가장 가까운 저장소(성채·창고)로 가서 훔치고 달아남 */
    CORE_BEHAVIOR_RAIDER = 1,  /* 가까운 건물을 부숨 (성채 쪽으로) */
    CORE_BEHAVIOR_MONSTER = 2, /* 밖에 있는 주민과 경비병, 그다음 집을 노림 */
    CORE_BEHAVIOR_COUNT = 3
} CoreEnemyBehavior;

/* 적 상태 (CoreEnemyView.state) */
typedef enum CoreEnemyState {
    CORE_ENEMY_ALIVE = 0,
    CORE_ENEMY_KILLED = 1,
    CORE_ENEMY_ESCAPED = 2     /* 훔치고 지도 밖으로 달아났거나 습격이 끝나 물러남 */
} CoreEnemyState;

#define CORE_MAX_THREATS 4        /* 예고된 위협은 최대 4개 */

/* 등급: 촌락 → 마을 → 요새 → 성 → 왕성. 왕성이 되면 승리 (계속할 수 있음) */
typedef enum CoreRank {
    CORE_RANK_HAMLET = 0,   /* 촌락 (시작) */
    CORE_RANK_VILLAGE = 1,  /* 마을 */
    CORE_RANK_FORTRESS = 2, /* 요새 */
    CORE_RANK_CASTLE = 3,   /* 성 */
    CORE_RANK_ROYAL = 4,    /* 왕성 (승리) */
    CORE_RANK_COUNT = 5
} CoreRank;

/* 연구 갈래 */
typedef enum CoreResearchBranch {
    CORE_BRANCH_ECONOMY = 0, /* 경제 */
    CORE_BRANCH_DEFENSE = 1, /* 방어 */
    CORE_BRANCH_MAGIC = 2,   /* 마법 */
    CORE_BRANCH_COUNT = 3
} CoreResearchBranch;

/* 연구 효과 (research.c 의 효과 함수 표 RESEARCH_EFFECTS 의 번호, 정의 파일의 effect 키) */
typedef enum CoreResearchEffect {
    CORE_EFFECT_FARM_YIELD = 0,    /* 농장 생산 +value% */
    CORE_EFFECT_EXTRACT_YIELD = 1, /* 벌목장·채석장 생산 +value% */
    CORE_EFFECT_STORAGE = 2,       /* 성채·창고 저장 한도 +value% */
    CORE_EFFECT_MARKET = 3,        /* 시장의 사고팔 때 차이 −value%p */
    CORE_EFFECT_TOWER_DAMAGE = 4,  /* 감시탑 화살 피해 +value */
    CORE_EFFECT_WALL_HP = 5,       /* 성벽·성문 최대 체력 +value% */
    CORE_EFFECT_GUARD_DAMAGE = 6,  /* 경비병 피해 +value */
    CORE_EFFECT_KEEP_DAMAGE = 7,   /* 성채 망루 피해 +value */
    CORE_EFFECT_MANA_YIELD = 8,    /* 마정석 광산 생산 +value% */
    CORE_EFFECT_HEALING = 9,       /* 병을 앓는 시간 −value% */
    CORE_EFFECT_HEATING = 10,      /* 집 한 채의 하룻밤 난방 목재 −value */
    CORE_EFFECT_WARD = 11,         /* 습격 때 건물이 받는 피해 −value% */
    CORE_EFFECT_COUNT = 12
} CoreResearchEffect;

/* 연구 하나의 상태 */
typedef enum CoreResearchState {
    CORE_RSTATE_LOCKED = 0,    /* 앞선 연구나 등급이 모자람 */
    CORE_RSTATE_AVAILABLE = 1, /* 시작할 수 있음 (비용을 내면) */
    CORE_RSTATE_ACTIVE = 2,    /* 마탑에서 연구 중 */
    CORE_RSTATE_DONE = 3       /* 끝남 (효과가 적용됨) */
} CoreResearchState;

/* 게임 결과 */
typedef enum CoreOutcome {
    CORE_OUTCOME_NONE = 0, /* 진행 중 */
    CORE_OUTCOME_WON = 1,  /* 왕성이 됨 (계속할 수 있음) */
    CORE_OUTCOME_LOST = 2  /* 패배 (시간이 더는 흐르지 않음) */
} CoreOutcome;

typedef enum CoreLoseReason {
    CORE_LOSE_NONE = 0,
    CORE_LOSE_KEEP = 1,       /* 성채가 무너짐 */
    CORE_LOSE_POPULATION = 2  /* 주민이 한 명도 남지 않음 */
} CoreLoseReason;

/* 튜토리얼 목표 (처음 10분쯤의 체크리스트) */
typedef enum CoreTutorialStep {
    CORE_TUTORIAL_FARM = 0,   /* 농장을 짓기 */
    CORE_TUTORIAL_HOUSE = 1,  /* 주택을 짓기 */
    CORE_TUTORIAL_ASSIGN = 2, /* 주민을 일터에 직접 배치하기 */
    CORE_TUTORIAL_WALL = 3,   /* 성벽을 짓기 */
    CORE_TUTORIAL_RAID = 4,   /* 첫 도적 습격을 버티기 */
    CORE_TUTORIAL_COUNT = 5
} CoreTutorialStep;

#define CORE_MAX_RESEARCH 32      /* 연구 정의는 최대 32개 */

#define CORE_MARKET_LOT 10        /* 시장에서 한 번에 사고파는 양 */
#define CORE_MARKET_MAX_LOTS 10   /* core_trade 한 번에 최대 묶음 수 */
#define CORE_MERCHANT_MAX_DEALS 4

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
    int32_t weather;          /* CoreWeather */
    int32_t nature_event;     /* 진행 중인 자연 사건 (CoreNatureEvent, 0 = 없음) */
    int32_t nature_days_left; /* 그 사건이 남은 날 (오늘 포함) */
    int32_t forecast_event;   /* 내일 시작할 사건 (0 = 없음) */
    int32_t merchant_days_left; /* 떠돌이 상인이 머무는 남은 날 (0 = 없음) */
    int32_t market_open;      /* 1 = 시장에서 거래할 수 있음 */
    int32_t sick_count;       /* 아픈 주민 수 */
    int32_t cold_houses;      /* 지난밤 난방을 못 한 집 수 */
    int32_t heating_per_house;/* 지금 집 한 채가 하룻밤에 쓰는 목재 */
    int32_t farm_percent;     /* 지금 농장 생산 % (계절 × 날씨 × 사건) */
    int32_t winter_food_need; /* 가을·겨울: 봄까지 버티는 데 필요한 식량 (다른 계절은 0) */
    int32_t winter_wood_need; /* 가을·겨울: 봄까지 필요한 난방 목재 */
    int32_t threat_type;      /* 다음 위협 (CoreThreatType, 0 = 없음) */
    int32_t threat_direction; /* CoreDirection */
    int32_t threat_ticks_left;/* 도착까지 남은 틱 */
    int32_t threat_strength;  /* 예상 적 수 */
    int32_t raid_id;          /* 진행 중인 습격 (0 = 없음). 습격 중에는 저장할 수 없음 */
    int32_t raid_type;
    int32_t raid_enemies_left;/* 아직 남은 적 */
    int32_t guards;           /* 지금 경비병(병영 일꾼) 수 */
    int32_t defense;          /* 방어 점수 (승급 조건: 다 지은 감시탑·병영·성문 + 경비병) */
    int32_t rank_ready;       /* 1 = 지금 승급할 수 있음 (조건과 공물 모두) */
    int32_t research_active;  /* 연구 중인 번호 (-1 = 없음) */
    int32_t research_progress;/* 모은 연구 점수 */
    int32_t research_points;  /* 그 연구에 필요한 점수 */
    int32_t research_mages;   /* 지금 마탑에서 일하는 마법사 수 */
    int32_t research_done;    /* 끝낸 연구 수 */
    int32_t outcome;          /* CoreOutcome */
    int32_t lose_reason;      /* CoreLoseReason */
    int32_t tutorial_done;    /* 끝낸 튜토리얼 목표 비트 (1 << CoreTutorialStep) */
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
    int32_t attack;           /* 감시탑: 화살 한 발의 피해, 병영: 경비병 한 명의 피해 (0 = 싸우지 않음) */
    int32_t attack_range;     /* 공격 거리 (칸) */
    int32_t attack_cooldown;  /* 공격 사이 시간 (게임 속 분) */
    int32_t defense;          /* 방어 점수 (승급 조건) */
    char key[CORE_KEY_BYTES];
} CoreBuildingDefView;

/* 적 정의 (data/enemies.def) */
typedef struct CoreEnemyDefView {
    int32_t type;            /* 적 종류 번호 (0 부터) */
    int32_t hp;
    int32_t damage;          /* 한 번 공격의 피해 */
    int32_t cooldown;        /* 공격 사이 시간 (게임 속 분) */
    int32_t speed;           /* 보통 풀밭에서 한 틱에 걷는 거리 × 1000 */
    int32_t range;           /* 공격 거리 (칸, 1 = 붙어서) */
    int32_t behavior;        /* CoreEnemyBehavior */
    int32_t loot_resource;   /* 훔치는 자원 (-1 = 없음) */
    int32_t loot_amount;     /* 한 명이 훔치는 양 */
    int32_t mana_reward;     /* 쓰러뜨리면 떨어뜨리는 마정석 */
    char key[CORE_KEY_BYTES];
} CoreEnemyDefView;

/* 예고된 위협 하나 */
typedef struct CoreThreatView {
    int32_t type;            /* CoreThreatType */
    int32_t direction;       /* CoreDirection */
    int32_t eta_tick;        /* 도착하는 틱 */
    int32_t ticks_left;
    int32_t strength;        /* 예상 적 수 */
} CoreThreatView;

/* 진행 중인 습격 */
typedef struct CoreRaidView {
    int32_t id;              /* 0 = 습격 없음 */
    int32_t type;            /* CoreThreatType */
    int32_t direction;
    int32_t spawn_x;         /* 적이 나타나는 칸 (지도 가장자리) */
    int32_t spawn_y;
    int32_t enemy_count;
    int32_t alive;
    int32_t started_tick;
    int32_t stolen;          /* 지금까지 빼앗긴 양 (금화 등 합계) */
} CoreRaidView;

/* 습격의 적 한 명 (위치와 체력은 Godot 이 가짐) */
typedef struct CoreEnemyView {
    int32_t id;
    int32_t type;            /* 적 종류 (core_get_enemy_def) */
    int32_t state;           /* CoreEnemyState */
} CoreEnemyView;

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

/* 장부: 자원 × 항목 (오늘 쌓는 중인 것과 어제 하루) */
typedef struct CoreLedgerView {
    int32_t today[CORE_RES_COUNT][CORE_LEDGER_COUNT];
    int32_t yesterday[CORE_RES_COUNT][CORE_LEDGER_COUNT];
} CoreLedgerView;

/* 시장 가격 (한 묶음 = CORE_MARKET_LOT 개의 값, 금화) */
typedef struct CoreMarketView {
    int32_t open;                        /* 1 = 거래할 수 있음 */
    int32_t closed_reason;               /* open 이 0 이면 CoreDenyReason */
    int32_t buy_price[CORE_RES_COUNT];   /* 한 묶음 살 때 (0 = 다루지 않음) */
    int32_t sell_price[CORE_RES_COUNT];  /* 한 묶음 팔 때 */
    int32_t base_price[CORE_RES_COUNT];  /* 기준 가격 */
    int32_t trend[CORE_RES_COUNT];       /* 지금 가격 ÷ 기준 가격 × 100 */
} CoreMarketView;

/* 떠돌이 상인의 거래 제안 하나: give 를 내고 get 을 받음 */
typedef struct CoreDealView {
    int32_t give_resource;
    int32_t give_amount;
    int32_t get_resource;
    int32_t get_amount;
    int32_t taken; /* 1 = 이미 함 */
} CoreDealView;

typedef struct CoreMerchantView {
    int32_t days_left;  /* 0 = 상인 없음 */
    int32_t deal_count;
    CoreDealView deals[CORE_MERCHANT_MAX_DEALS];
} CoreMerchantView;

/* 연구 하나 (core_get_research). 효과는 연구가 끝나는 순간 적용되고, 건물 정의(공격·체력·생산률·저장)에 반영됩니다 */
typedef struct CoreResearchView {
    int32_t index;
    int32_t branch;          /* CoreResearchBranch */
    int32_t requires;        /* 먼저 끝내야 하는 연구 번호 (-1 = 없음) */
    int32_t required_rank;   /* 시작하려면 필요한 등급 */
    int32_t cost_gold;       /* 시작할 때 내는 금화 */
    int32_t cost_mana;       /* 시작할 때 내는 마정석 */
    int32_t points;          /* 마탑에서 모아야 하는 연구 점수 */
    int32_t effect;          /* CoreResearchEffect */
    int32_t value;           /* 효과의 크기 */
    int32_t state;           /* CoreResearchState */
    int32_t progress;        /* 모은 점수 (연구 중일 때) */
    char key[CORE_KEY_BYTES];
} CoreResearchView;

/* 승급: 지금 등급과 다음 등급의 조건 (core_get_rank) */
typedef struct CoreRankView {
    int32_t rank;            /* 지금 등급 (CoreRank) */
    int32_t next_rank;       /* 다음 등급 (-1 = 이미 왕성) */
    int32_t need_population;
    int32_t have_population;
    int32_t need_mood;       /* 평균 기분 */
    int32_t have_mood;
    int32_t need_defense;    /* 방어 점수 */
    int32_t have_defense;
    int32_t need_research;   /* 끝낸 연구 수 */
    int32_t have_research;
    int32_t tribute_gold;    /* 오를 때 바치는 금화 */
    int32_t tribute_mana;    /* 오를 때 바치는 마정석 */
    int32_t ready;           /* 1 = 조건을 모두 채웠고 공물을 낼 수 있음 */
    int32_t unlocks;         /* 다음 등급에서 풀리는 건물 비트 (1 << CoreBuildingType) */
    int32_t storage_bonus;   /* 다음 등급에서 늘어나는 저장 한도 */
    int32_t research_unlocks;/* 다음 등급에서 시작할 수 있게 되는 연구 수 */
} CoreRankView;

/* 한 판의 기록 (승리·패배 화면) */
typedef struct CoreRunStatsView {
    int32_t days;            /* 지난 날 수 */
    int32_t peak_population; /* 가장 많았던 주민 수 */
    int32_t raids_survived;  /* 끝난 습격 수 */
    int32_t enemies_killed;  /* 쓰러뜨린 적 */
    int32_t villagers_died;  /* 죽은 주민 */
    int32_t villagers_left;  /* 떠난 주민 */
    int32_t buildings_built; /* 다 지은 건물 (도로·성벽 칸 제외) */
    int32_t research_done;
    int32_t rank;
    int32_t outcome;         /* CoreOutcome */
    int32_t lose_reason;     /* CoreLoseReason */
    int32_t victory_day;     /* 왕성이 된 날 (0 = 아직) */
} CoreRunStatsView;

/* 난이도 하나의 설정 (새 게임 화면의 설명용, core_get_difficulty) */
typedef struct CoreDifficultyView {
    int32_t difficulty;      /* CoreDifficulty */
    int32_t start_villagers;
    int32_t start_resources[CORE_RES_COUNT];
    int32_t base_mood;       /* 기분의 기본값 */
    int32_t raid_strength_pct; /* 습격 세기 % */
    int32_t raid_interval_pct; /* 습격 사이 간격 % (클수록 드묾) */
    int32_t nature_pct;      /* 자연 사건의 확률·피해 % */
    int32_t tribute_pct;     /* 승급 공물 % */
} CoreDifficultyView;

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
    int32_t ledger_view;
    int32_t market_view;
    int32_t merchant_view;
    int32_t enemy_def_view;
    int32_t threat_view;
    int32_t raid_view;
    int32_t enemy_view;
    int32_t research_view;
    int32_t rank_view;
    int32_t run_stats_view;
    int32_t difficulty_view;
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
/* ticks: 1..max_ticks_per_call. 패배한 뒤에는 CORE_ERR_REJECTED (시간이 더는 흐르지 않음) */
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
CORE_API int32_t core_get_ledger(const CoreWorld *world, CoreLedgerView *out);
/* 적 정의. type 이 범위 밖이면 CORE_ERR_NOT_FOUND (0 부터 차례로 읽으면 개수를 알 수 있음) */
CORE_API int32_t core_get_enemy_def(const CoreWorld *world, int32_t type, CoreEnemyDefView *out);
/* 예고된 위협들 (도착이 빠른 순) */
CORE_API int32_t core_copy_threats(const CoreWorld *world, CoreThreatView *out, int32_t capacity, int32_t *out_count);
/* 진행 중인 습격 (없으면 out->id = 0) */
CORE_API int32_t core_get_raid(const CoreWorld *world, CoreRaidView *out);
/* 진행 중인 습격의 적 전체. 버퍼가 작으면 CORE_ERR_BUFFER_TOO_SMALL 과 필요한 개수 */
CORE_API int32_t core_copy_enemies(const CoreWorld *world, CoreEnemyView *out, int32_t capacity, int32_t *out_count);
CORE_API int32_t core_get_market(const CoreWorld *world, CoreMarketView *out);
CORE_API int32_t core_get_merchant(const CoreWorld *world, CoreMerchantView *out);
/* 건물 수리 비용 (자원 6개). 고칠 곳이 없으면 모두 0 */
/* 연구 하나. index 가 범위 밖이면 CORE_ERR_NOT_FOUND (0 부터 차례로 읽으면 개수를 알 수 있음) */
CORE_API int32_t core_get_research(const CoreWorld *world, int32_t index, CoreResearchView *out);
/* 지금 등급과 다음 등급의 조건 */
CORE_API int32_t core_get_rank(const CoreWorld *world, CoreRankView *out);
/* 한 판의 기록 (승리·패배 화면) */
CORE_API int32_t core_get_run_stats(const CoreWorld *world, CoreRunStatsView *out);
/* 난이도 설정 (월드가 없어도 읽을 수 있음). 범위 밖이면 CORE_ERR_INVALID_ARG */
CORE_API int32_t core_get_difficulty(int32_t difficulty, CoreDifficultyView *out);
CORE_API int32_t core_get_repair_cost(const CoreWorld *world, int32_t building_id, int32_t *out_cost,
                                      int32_t count);

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
/* 시장 거래. lots > 0 이면 사고, < 0 이면 팝니다 (한 묶음 = CORE_MARKET_LOT 개, 1..CORE_MARKET_MAX_LOTS 묶음).
 * 전부 할 수 있을 때만 하고, 안 되면 CORE_ERR_REJECTED 와 *out_reason(CoreDenyReason).
 * *out_gold = 금화 변화 (산 경우 음수) */
CORE_API int32_t core_trade(CoreWorld *world, int32_t resource, int32_t lots, int32_t *out_gold,
                            int32_t *out_reason);
/* 떠돌이 상인의 거래 제안 index 를 받아들입니다 */
CORE_API int32_t core_merchant_accept(CoreWorld *world, int32_t index, int32_t *out_reason);
/* 부서지거나 무너진 건물을 고칩니다 (비용: core_get_repair_cost) */
CORE_API int32_t core_repair(CoreWorld *world, int32_t building_id, int32_t *out_reason);
/* 연구를 시작합니다: 비용(금화·마정석)을 내고, 마탑의 마법사가 점수를 모읍니다. 한 번에 하나만.
 * 안 되면 CORE_ERR_REJECTED 와 *out_reason (CORE_DENY_RESEARCH_*, NO_GOLD, NO_MANA) */
CORE_API int32_t core_start_research(CoreWorld *world, int32_t index, int32_t *out_reason);
/* 다음 등급으로 오릅니다: 조건(인구·기분·방어·연구)을 채우고 공물(금화·마정석)을 냅니다.
 * 안 되면 CORE_ERR_REJECTED 와 *out_reason (CORE_DENY_REQUIREMENTS, NO_GOLD, NO_MANA, MAX_RANK, RAID) */
CORE_API int32_t core_rank_up(CoreWorld *world, int32_t *out_reason);

/* ---- 보고 (Godot → C) ------------------------------------------------ */
/* 주민이 의도의 목표에 도착함. intent_rev 가 지금 것과 다르면 CORE_IGNORED */
CORE_API int32_t core_villager_arrived(CoreWorld *world, int32_t villager_id, int32_t intent_rev);
/* 목표까지 길이 없음. C 가 다른 할 일을 고르고 알림을 냅니다 */
CORE_API int32_t core_villager_unreachable(CoreWorld *world, int32_t villager_id, int32_t intent_rev);

/* 습격 보고 (Godot 의 전투 결과 → C 가 규칙대로 적용). 습격이 이미 끝났으면 CORE_IGNORED */
/* 적이 건물에 준 피해 (성채는 체력 1 아래로 내려가지 않음 — 패배는 마일스톤 5) */
CORE_API int32_t core_report_building_damage(CoreWorld *world, int32_t raid_id, int32_t building_id, int32_t damage);
/* 적이 주민(경비병)에게 준 피해. 건강이 0 이 되면 쓰러짐 */
CORE_API int32_t core_report_villager_damage(CoreWorld *world, int32_t raid_id, int32_t villager_id, int32_t damage);
/* 적을 쓰러뜨림 (마물이면 마정석을 얻음) */
CORE_API int32_t core_report_enemy_killed(CoreWorld *world, int32_t raid_id, int32_t enemy_id);
/* 적이 저장소(성채·창고)에 닿아 훔침. *out_amount = 실제로 빼앗긴 양 */
CORE_API int32_t core_report_loot(CoreWorld *world, int32_t raid_id, int32_t enemy_id, int32_t building_id,
                                  int32_t *out_amount);
/* 적이 지도 밖으로 달아남 */
CORE_API int32_t core_report_enemy_escaped(CoreWorld *world, int32_t raid_id, int32_t enemy_id);
/* 습격이 끝남 (남은 적은 물러난 것으로 봄). Godot 이 알리지 않으면 하루 뒤 C 가 스스로 끝냄 */
CORE_API int32_t core_report_raid_ended(CoreWorld *world, int32_t raid_id);

/* ---- 피드백 이벤트 ---------------------------------------------------- */
/* 이벤트가 있으면 out 에 채우고 1, 없으면 0, 오류면 음수 */
CORE_API int32_t core_poll_event(CoreWorld *world, CoreEventView *out);

/* ---- 저장, 불러오기 ---------------------------------------------------- */
/* 임시 파일에 쓰고 바꿔치기합니다. 기존 파일은 <path>.bak 으로 남깁니다. 경로는 UTF-8.
 * 습격 중에는 CORE_ERR_REJECTED (습격이 끝난 뒤 저장) */
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
