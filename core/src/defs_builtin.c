/*
 * defs_builtin.c — 정의의 내장 기본값
 *
 * game/data/ 의 .def 파일이 없거나 망가졌을 때 쓰는 값입니다. 보통은 파일의 값이 이것을 덮어씁니다.
 * 순서는 CoreBuildingType 과 같아야 합니다.
 *
 * 열 순서: 키, 너비, 높이, 비용{금화,마정석,식량,목재,석재,약초}, 건설 틱, 최대 체력,
 *          일자리, 거주 자리, 저장 한도, 생산 자원, 생산률, 두 번째 자원, 두 번째 생산률,
 *          작업 반경, 배치 규칙, 최소 지형, 이상적 지형, 임금, 필요 등급, 분류, 줄 배치
 */
#include "defs.h"

#define NONE (-1)

const BuildingDef BUILTIN_BUILDINGS[CORE_BLD_COUNT] = {
    {"keep", 3, 3, {0, 0, 0, 0, 0, 0}, 0, 2000, 0, 0, 300, NONE, 0, NONE, 0, 0, CORE_PLACE_ANY, 0, 1, 0, 0,
     CORE_CAT_SPECIAL, 0},
    {"house", 2, 2, {20, 0, 0, 25, 0, 0}, 36, 200, 0, 4, 0, NONE, 0, NONE, 0, 0, CORE_PLACE_ANY, 0, 1, 0, 0,
     CORE_CAT_HOUSING, 0},
    {"farm", 3, 3, {15, 0, 0, 20, 0, 0}, 48, 150, 2, 0, 0, CORE_RES_FOOD, 80, NONE, 0, 0, CORE_PLACE_ANY, 0, 1, 1,
     0, CORE_CAT_RESOURCE, 0},
    {"gatherer", 2, 2, {15, 0, 0, 15, 0, 0}, 36, 120, 2, 0, 0, CORE_RES_HERBS, 30, CORE_RES_FOOD, 35, 3,
     CORE_PLACE_NEAR_FOREST, 6, 16, 1, 0, CORE_CAT_RESOURCE, 0},
    {"lumber", 2, 2, {20, 0, 0, 10, 0, 0}, 36, 150, 2, 0, 0, CORE_RES_WOOD, 90, NONE, 0, 4, CORE_PLACE_NEAR_FOREST,
     8, 24, 1, 0, CORE_CAT_RESOURCE, 0},
    {"quarry", 2, 2, {25, 0, 0, 20, 0, 0}, 48, 200, 2, 0, 0, CORE_RES_STONE, 70, NONE, 0, 3, CORE_PLACE_ADJ_ROCK, 1,
     12, 1, 0, CORE_CAT_RESOURCE, 0},
    {"mana_mine", 2, 2, {60, 0, 0, 30, 30, 0}, 72, 250, 2, 0, 0, CORE_RES_MANA, 12, NONE, 0, 2,
     CORE_PLACE_ON_MANA, 1, 6, 2, 1, CORE_CAT_RESOURCE, 0},
    {"warehouse", 3, 2, {30, 0, 0, 40, 10, 0}, 48, 250, 0, 0, 200, NONE, 0, NONE, 0, 0, CORE_PLACE_ANY, 0, 1, 0, 0,
     CORE_CAT_STORAGE, 0},
    {"market", 3, 2, {50, 0, 0, 40, 20, 0}, 60, 250, 1, 0, 0, NONE, 0, NONE, 0, 0, CORE_PLACE_ANY, 0, 1, 2, 0,
     CORE_CAT_STORAGE, 0},
    {"barracks", 3, 2, {60, 0, 0, 30, 40, 0}, 72, 400, 4, 0, 0, NONE, 0, NONE, 0, 0, CORE_PLACE_ANY, 0, 1, 2, 1,
     CORE_CAT_DEFENSE, 0},
    {"tower", 1, 1, {40, 0, 0, 10, 30, 0}, 48, 300, 0, 0, 0, NONE, 0, NONE, 0, 0, CORE_PLACE_ANY, 0, 1, 0, 1,
     CORE_CAT_DEFENSE, 0},
    {"wall", 1, 1, {0, 0, 0, 0, 4, 0}, 6, 250, 0, 0, 0, NONE, 0, NONE, 0, 0, CORE_PLACE_ANY, 0, 1, 0, 0,
     CORE_CAT_DEFENSE, 1},
    {"gate", 1, 1, {10, 0, 0, 10, 8, 0}, 12, 300, 0, 0, 0, NONE, 0, NONE, 0, 0, CORE_PLACE_ANY, 0, 1, 0, 0,
     CORE_CAT_DEFENSE, 0},
    {"healer", 2, 2, {40, 0, 0, 30, 0, 5}, 60, 200, 1, 0, 0, NONE, 0, NONE, 0, 0, CORE_PLACE_ANY, 0, 1, 2, 0,
     CORE_CAT_SERVICE, 0},
    {"mage_tower", 2, 2, {80, 10, 0, 0, 50, 0}, 96, 300, 2, 0, 0, NONE, 0, NONE, 0, 0, CORE_PLACE_ANY, 0, 1, 3, 2,
     CORE_CAT_SERVICE, 0},
    {"road", 1, 1, {2, 0, 0, 0, 0, 0}, 0, 1, 0, 0, 0, NONE, 0, NONE, 0, 0, CORE_PLACE_ANY, 0, 1, 0, 0, CORE_CAT_ROAD,
     1},
};

/* 키, 일 속도 %, 배고픔 %, 기분, 용기, 연구 % */
const TraitDef BUILTIN_TRAITS[] = {
    {"diligent", 25, 0, 0, 0, 0},        /* 근면함 */
    {"lazy", -25, 0, 3, 0, 0},           /* 게으름 */
    {"glutton", 0, 50, 2, 0, 0},         /* 대식가 */
    {"coward", 0, 0, 0, -1, 0},          /* 겁쟁이 */
    {"brave", 0, 0, 0, 1, 0},            /* 용감함 */
    {"mana_affinity", 0, 0, 0, 0, 50},   /* 마력 친화 */
    {"optimist", 0, 0, 8, 0, 0},         /* 낙천가 */
    {"grumbler", 0, 0, -8, 0, 0},        /* 투덜이 */
    {"strong", 10, 20, 0, 0, 0},         /* 장사 */
    {"frugal", 0, -25, -2, 0, 0},        /* 검소함 */
};
const int32_t BUILTIN_TRAIT_COUNT = (int32_t)(sizeof(BUILTIN_TRAITS) / sizeof(BUILTIN_TRAITS[0]));

/* 중세 판타지 느낌의 이름 150개 (game/data/names.txt 와 같은 목록) */
const char *const BUILTIN_NAMES[] = {
    "에드윈", "로웬", "브리엘", "카이렌", "엘다", "마렌", "토르빈", "세이라", "한스", "이델", "로잘린", "베른",
    "아일라", "가렛", "미라벨", "오스릭", "펠린", "다리안", "케이트린", "울프릭", "레아", "시그문", "노라", "알베른",
    "이브렌", "콘라드", "멜리사", "바르톨", "에이린", "루카", "헬가", "오드릭", "실비아", "브란", "카밀라", "테오",
    "길다", "레오릭", "아나벨", "하콘", "엘로이즈", "드레드", "마일라", "요한", "세레나", "고드윈", "피오나", "랄프",
    "이졸데", "에릭", "로제타", "윌머", "클라라", "베오른", "니엘", "아르덴", "소피아", "케인", "마그다", "티볼트",
    "엘사", "로데릭", "이레네", "할바드", "루시엔", "빌헬름", "안젤라", "토마스", "에스텔", "그림", "레나", "오웬",
    "실라", "바리안", "에다", "모르간", "잉그리드", "케일", "로렌", "프레야", "알데릭", "미아", "디트리히", "엘레나",
    "우르술라", "라스", "카리나", "벤", "아델", "시몬", "헤일", "브리짓", "로날드", "에피", "가브리엘", "이사",
    "닐스", "마틸다", "오토", "샤를린", "라그나", "엘린", "필립", "도린", "아서", "리베카", "크리스", "에밀",
    "베아트리체", "하랄드", "올가", "레몽", "이리스", "구스타프", "앨리스", "레너드", "마리안", "펠릭스", "오로라",
    "콜린", "헤르타", "로베르", "세실", "아론", "에바", "로키", "자넷", "델핀", "카스파르", "릴리", "보리스", "유나",
    "에반", "로라", "막스", "엘리스", "스벤", "아멜리", "도미닉", "케이", "릴리안", "바스", "레아나", "토비아스",
    "에르나", "프란츠", "노엘", "일마", "잭", "안나", "오를라", "필",
};
const int32_t BUILTIN_NAME_COUNT = (int32_t)(sizeof(BUILTIN_NAMES) / sizeof(BUILTIN_NAMES[0]));
