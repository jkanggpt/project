# C 요구사항 대응표

고급프로그래밍 과제의 C 요구사항(`docs/SPEC.md` §1.1)이 **어느 파일의 어느 함수**에서, **어떤 게임 기능**으로 쓰이는지 정리한 표입니다.
마일스톤마다 갱신합니다. (지금: 마일스톤 3 끝)

## 한눈에 보기

| 요구사항 | 대표 위치 | 게임에서 하는 일 |
| --- | --- | --- |
| 구조체 2개 이상, 포함·참조 관계 | `core/src/entity.h` `Villager`, `Building` / `world.h` `struct CoreWorld` | 주민과 건물, 게임 상태 전체 |
| 포인터로 원본 수정, 포인터로 배열·구조체 접근 | `entity.c:entity_link_worker`, `economy.c:eco_change`, `villager_ai.c:vil_set_intent` | 일꾼 배치, 자원·장부, 주민 의도 |
| 사용자 정의 함수 10개 이상 | 코어 약 330개, CLI 약 90개 | 모듈마다 한 가지 역할 |
| 함수 포인터 | `nature.c:NATURE_EVENTS[]`, `villager_ai.c:STATE_TABLE[]`, `jobs.c:JOB_TABLE[]`, `defs.c:SECTION_PARSERS[]`, `save.c:SECTIONS[]`, `cli/commands.c:COMMANDS[]` | 자연 사건, 주민 행동, 직업별 생산, 정의 파일·세이브 해석, CLI 메뉴 |
| 동적 메모리 | `entity.c:entity_add_villager`·`reserve_slot`·`insert_sorted`, `grid.c:grid_alloc`, `events.c:grow` | 늘었다 줄었다 하는 주민·건물 목록, 일꾼·거주민 배열, 지도, 이벤트 큐 |
| 파일 입출력 | `save.c:save_write`/`save_read`, `api.c:core_defs_load_file`, `defs.c:defs_write_file` | 저장 → 껐다 켜기 → 이어하기 (날씨·사건·시장 가격·상인까지), 건물·특성·이름 정의 파일 |
| 오류 처리 | `save.c:save_read`, `defs.c:defs_load_text`, `market.c:market_trade`, `cli/console.c:read_int`, 모든 `mem_*` 호출 | 파일 없음, 망가진 정의 파일, 할 수 없는 거래·수리(이유 코드), 잘못된 입력, 메모리 부족 |

## 1. 구조체 (서로 다른 목적 2개 이상, 포함·참조 관계)

| 구조체 | 파일 | 목적 | 관계 |
| --- | --- | --- | --- |
| `World` (`struct CoreWorld`) | `core/src/world.h` | 게임 상태 전체 | `Clock`, `Rng`, `ResourceBank`, `Ledger`, `Grid`, `EventQueue`, `Defs` 를 **포함**, `Villager **villagers`, `Building **buildings` 포인터 배열을 **소유** |
| `Villager` | `core/src/entity.h` | 주민 한 명 | `Stats stats`, `Needs needs` 를 **포함**, `Building *home`, `Building *workplace` 를 **참조** |
| `Building` | `core/src/entity.h` | 건물 하나 | `Villager **workers`, `Villager **residents` 동적 배열로 주민을 **참조** (양방향: 주민도 건물을 가리킴) |
| `Stats`, `Needs` | `core/src/entity.h` | 능력치 4개, 욕구 5개 | `Villager` 안에 포함 |
| `BuildingDef`, `TraitDef`, `NameEntry`, `Defs` | `core/src/defs.h` | 정의 파일에서 읽은 건물·특성·이름 | `Defs` 가 `TraitDef *traits`, `NameEntry *names` 동적 배열을 **소유** |
| `Ledger` | `core/src/economy.h` | 자원별·항목별 오늘/어제 변화 (HUD 툴팁) | `World` 안에 포함 |
| `Nature` | `core/src/nature.h` | 날씨, 진행 중인 자연 사건, 예보, 추운 집 수 | `World` 안에 포함 |
| `NatureEventDef` | `core/src/nature.c` | 사건 하나의 계절·확률·기간·날씨와 **처리 함수 포인터 3개**(시작, 매일, 끝) | `NATURE_EVENTS[]` 표 |
| `Market`, `Merchant`, `MerchantDeal` | `core/src/market.h` | 시장 가격, 떠돌이 상인과 그 제안들 | `Merchant` 가 `MerchantDeal deals[4]` 를 **포함**, 둘 다 `World` 안에 포함 |
| `Grid` | `core/src/grid.h` | 지도 | 동적 배열 `terrain`, `cost` 를 **포인터로 소유** |
| `ResourceBank` | `core/src/economy.h` | 자원 6종과 저장 한도 | `World` 안에 포함 |
| `Event` | `core/src/events.h` | 피드백 이벤트 하나 | 종류(`type`)로 고르는 **태그 붙은 공용체(union)** 포함 (날짜, 계절, 건물, 주민, 수, 두 값 짝 …) |
| `EventQueue` | `core/src/events.h` | 원형 큐 | `Event *items` 동적 배열을 **참조** |
| `LoadState` | `core/src/save.c` | 불러오는 중인 상태 | 만들고 있는 `World *world` 를 **참조** |
| `SectionDef` | `core/src/save.c` | 섹션 이름 → 해석 함수 | 함수 포인터 필드 |
| `CliContext` | `cli/commands.h` | CLI 의 현재 게임 | `CoreWorld *world` 를 **참조** |
| `CoreSummaryView` 등 뷰 | `core/include/castle_core.h` | Godot 에 건네는 평평한 복사본 | C# `Native.cs` 와 같은 모양 |

핵심 관계 (SPEC §6.2): 주민 `Villager` 가 `Stats`, `Needs` 를 **포함**하고 `Building *home`, `Building *workplace` 를 **참조**, 건물이 `Villager **workers`, `Villager **residents` 를 동적 배열로 **참조**합니다.
연결과 해제는 언제나 양쪽을 함께 바꾸는 함수(`entity_link_worker`, `entity_unlink_worker`, `entity_link_resident`, `entity_unlink_resident`)로만 하고,
`validate.c:check_entities` 가 매 틱 "주민이 가리키는 건물의 목록에 그 주민이 있는가"를 양쪽에서 확인합니다. 저장할 때는 포인터 대신 id 를 쓰고, 불러올 때 `save.c:relink` 가 다시 연결합니다.

## 2. 포인터

| 쓰임 | 위치 | 게임 기능 |
| --- | --- | --- |
| 포인터를 받아 원본을 바꾸는 함수 | `rng.c:rng_next(Rng *rng)` — 상태를 한 걸음 진행 | 지형 생성의 모든 무작위 |
| | `economy.c:eco_bank_add(ResourceBank *bank, ...)`, `eco_bank_spend` | 자원 더하기/빼기 (한도 자르기) |
| | `events.c:events_push(EventQueue *queue, ...)`, `events_pop` | 알림 쌓기/꺼내기 |
| | `world.c:world_tick(World *world, ...)` | 시간 진행 |
| | `entity.c:entity_link_worker(Building *building, Villager *villager, ...)` — 두 구조체를 함께 고침 | 일꾼 배치 |
| | `economy.c:eco_change(ResourceBank *bank, Ledger *ledger, ...)` — 자원과 장부를 함께 고침 | 생산, 소비, 임금 |
| | `villager_ai.c:vil_set_intent(Villager *villager, ...)` — 의도와 `intent_rev` 를 바꿈 | 주민 걷기 핸드셰이크 |
| | `placement.c:placement_damage(World *world, Building *building, int32_t damage)` — 체력을 깎고, 0 이면 무너뜨림 | 폭풍 피해 |
| | `market.c:market_trade(..., int32_t *out_gold, int32_t *out_reason)` — 결과와 거절 이유를 출력 매개변수로 | 시장 거래 |
| | `api.c:core_get_map_size(..., int32_t *out_width, int32_t *out_height)` | 출력 매개변수 |
| 포인터로 배열 원소 접근 | `grid.c:stamp_blob`, `place_mana_cluster`, `carve_corridor` — `uint8_t *cell = &grid->terrain[i]; *cell = ...` | 지형 칠하기 |
| | `world.c:world_name_is_valid` — `const unsigned char *s` 로 한 바이트씩 걸으며 UTF-8 검사 | 성채 이름 검사 |
| 포인터로 구조체 접근 | `grid.c:ensure_fair_start` — `const FairRule *rule = &FAIR_RULES[r]; rule->min_count` | 공정한 시작 |
| | `jobs.c:jobs_work` — `villager->workplace` 포인터를 따라가 건물 종류로 작업 함수를 고름 | 생산 |
| 포인터의 포인터 (배열) | `entity.c:insert_sorted(Villager ***array, ...)` — 배열 자체를 다시 할당해 돌려줌 | 일꾼·거주민 목록 |
| | `validate.c:check_grid(Checker *checker, const World *world)` | 불변 조건 검사 |
| 포인터의 포인터 (출력) | `api.c:core_create(..., CoreWorld **out_world)`, `save.c:save_read(..., World **out_world, ...)` | 새 게임, 불러오기 |

## 3. 함수 (10개 이상, 각자 한 가지 역할)

코어 약 330개, CLI 약 90개. 예:

| 모듈 | 함수 |
| --- | --- |
| `world.c` | `world_alloc`, `world_destroy`, `world_create`, `world_tick`, `world_step`, `world_set_name`, `world_name_is_valid`, `world_state_hash` |
| `grid.c` | `grid_alloc`, `grid_free`, `grid_generate`, `grid_rebuild_costs`, `grid_flood_reachable`, `grid_count_in_radius`, `fractal_noise`, `paint_top_percent`, `ensure_connectivity`, `ensure_fair_start` |
| `save.c` | `save_write`, `save_read`, `read_line`, `parse_meta`, `parse_map`, `parse_i32`, `check_header`, `begin_section` |
| `economy.c` | `eco_bank_init`, `eco_bank_add`, `eco_bank_can_afford`, `eco_change`, `eco_pay`, `eco_roll_ledger` |
| `entity.c` | `entity_add_villager`, `entity_add_building`, `entity_find_building`, `entity_link_worker`, `entity_unlink_worker`, `entity_remove_building` |
| `placement.c` | `placement_check`, `placement_place`, `placement_line`, `placement_demolish`, `check_paths`, `ring_is_connected` |
| `villager_ai.c` | `vil_tick`, `vil_arrived`, `vil_unreachable`, `vil_mood_terms`, `update_needs`, `fall_back` |
| `jobs.c` | `jobs_auto_assign`, `jobs_assign_housing`, `jobs_assign_manual`, `jobs_work`, `jobs_pay_wages_and_taxes`, `jobs_relevant_stat` |
| `population.c` | `population_create`, `population_spawn_initial`, `population_daily`, `population_begin_leaving`, `population_kill` |
| `nature.c` | `nature_daily`, `nature_farm_percent`, `nature_heating_per_house`, `nature_winter_needs`, `nature_make_sick`, `nature_cure`, `plague_start`, `storm_start` |
| `market.c` | `market_trade`, `market_is_open`, `market_buy_price`, `market_sell_price`, `market_daily`, `merchant_accept`, `make_deals` |
| `defs.c` | `defs_load_text`, `defs_write_file`, `parse_sections`, `parse_names`, `apply_field`, `begin_building`, `begin_trait` |
| `cli/console.c` | `console_init`, `console_read_line`, `read_int`, `read_text` |

## 4. 함수 포인터 (선언, 실제 함수에 연결, 실행할 동작 고르기)

| 표 | 위치 | 고르는 기준 | 게임 기능 |
| --- | --- | --- | --- |
| `NatureHandler` / `NATURE_EVENTS[]` | `core/src/nature.c` | 자연 사건 번호(가뭄, 역병, 폭풍, 혹한, 풍년) → `on_start`, `on_day`, `on_end` 함수 (역병은 `plague_start`·`plague_day`, 폭풍은 `storm_start`, 효과만 있는 사건은 NULL) | 계절의 압박 |
| `StateHandler` / `STATE_TABLE[]` | `core/src/villager_ai.c` | 주민 상태 번호(쉬기, 잠, 출근, 일, 식사, 떠남 …) → 그 상태에서 할 일을 정하는 함수 | 주민 행동 |
| `JobWorkFn` / `JOB_TABLE[]` | `core/src/jobs.c` | 건물 종류 → `work_farm`(계절을 따짐), `work_gatherer`(약초 + 식량 조금), `work_extract`(벌목장·채석장·광산: 주변 지형 칸 수를 따짐) | 생산 |
| `SectionParser` / `SECTION_PARSERS[]` | `core/src/defs.c` | 정의 파일의 `[building …]`, `[trait …]` → 시작 함수(`begin_building`, `begin_trait`)와 필드 함수(`field_building`, `field_trait`) 짝 | 건물·특성 정의 읽기 |
| `SectionParser` / `SECTIONS[]` | `core/src/save.c` | 세이브 파일의 `[섹션]` 이름 → `parse_meta`, `parse_clock`, `parse_rng`, `parse_resources`, `parse_ledger`, `parse_map`, `parse_building`, `parse_villager`, `parse_nature`, `parse_market`, `parse_merchant` | 불러오기 |
| `CliCommandFn` / `COMMANDS[]` | `cli/commands.c` | 메뉴 번호 → `cmd_status`, `cmd_map`, `cmd_save` … | CLI 메뉴 |
| `EventFormatter` / `EVENT_FORMATTERS[]` | `cli/format.c` | 이벤트 종류 번호 → 문장 만드는 함수 | CLI 알림 문장 |
| `BotDailyFn` / `BOTS[]` | `cli/sim.c` | `--bot` 이름 → 봇 함수 | 밸런스 시뮬레이터 |
| `ValidateReportFn` | `core/src/validate.h` | 호출한 쪽이 넘긴 콜백 | 테스트에서 위반 내용 출력 |

마일스톤 4~5 에서 더해질 것: 연구 효과 함수, 습격 구성·결과 처리.

## 5. 동적 메모리 (크기를 미리 모르는 데이터, 모두 해제)

| 데이터 | 할당 | 해제 | 왜 동적인가 |
| --- | --- | --- | --- |
| 월드 | `world.c:world_alloc` (`mem_calloc`) | `world_destroy` | 게임마다 하나, 수명이 프로그램과 다름 |
| 주민·건물 하나하나 | `entity.c:entity_add_villager`, `entity_add_building` (`mem_calloc`) | `entity_remove_villager`, `entity_remove_building`, `entity_free_all` | 이주·떠남·건설·철거로 수가 계속 바뀜 |
| 주민·건물 포인터 배열 | `entity.c:reserve_slot` (**`mem_realloc` 으로 두 배씩**) | `entity_free_all` | 몇 명, 몇 채가 될지 모름 |
| 건물의 일꾼·거주민 배열 | `entity.c:insert_sorted` (`mem_realloc`) | `entity_remove_building` | 건물마다 칸 수가 다름 (정의 파일이 정함) |
| 특성·이름 목록 | `defs.c:grow_array` (`mem_realloc`) | `defs_free` | 정의 파일에 몇 줄이 있을지 모름 |
| 정의 파일 내용 | `api.c:core_defs_load_file` (`mem_malloc`, 상한만큼) | 같은 함수 끝 | 파일 크기를 모름 |
| 지형·비용 격자 | `grid.c:grid_alloc` (`mem_calloc` × 2) | `grid_free` | 지도 크기가 월드를 만들 때(또는 세이브에서) 정해짐 |
| 이벤트 큐 | `events.c:events_init`, **`grow` 에서 `mem_realloc` 으로 두 배씩** | `events_free` | 한 틱에 몇 개 생길지 모름 |
| 세이브 한 줄 버퍼 | `save.c:read_line` 에서 **`mem_realloc` 으로 키움** | `save_read` 끝 | 줄 길이를 모름 (지도 너비만큼 길어짐) |
| 노이즈 값, BFS 대기열, 방문 표시 | `grid.c:grid_generate`, `grid_flood_reachable`, `ensure_connectivity` | 같은 함수 안에서 | 지도 크기에 비례 |
| 임시 경로 문자열 | `save.c:path_with_suffix` | `save_write` 끝 | 경로 길이를 모름 |
| CLI 지도 버퍼 | `cli/commands.c:cmd_map` (`malloc`) | 같은 함수 끝 | 지도 크기를 실행 중에 앎 |
| CLI 걷기 흉내의 거리표·힙 | `cli/walker.c:refresh_cost` (`malloc`) | `walker_free` | 지도 크기에 비례 |
| 소크 테스트의 주민 명령 버퍼 | `cli/sim.c` (`malloc`, 주민 수만큼) | 같은 함수 끝 | 주민 수를 실행 중에 앎 |

- 모든 할당은 `core/src/mem.c` 의 `mem_malloc`/`mem_calloc`/`mem_realloc`/`mem_free` 를 거칩니다. 안에서는 그냥 `malloc`/`calloc`/`realloc`/`free` 를 부릅니다.
- `realloc` 은 **임시 포인터에 받아서** 실패해도 원래 배열을 잃지 않습니다 (`events.c:grow`, `save.c:read_line`).
- 새는 메모리 0: Linux CI 에서 AddressSanitizer + LeakSanitizer 로 모든 테스트와 400일 × 20시드 소크 테스트를 돌립니다.

## 6. 파일 입출력 (저장 → 프로그램 종료 → 다시 실행 → 불러오기)

| 위치 | 하는 일 |
| --- | --- |
| `save.c:save_write` | 텍스트 세이브를 `<경로>.tmp` 에 쓰고, 예전 파일을 `<경로>.bak` 으로 복사한 뒤, 한 번에 바꿔치기 |
| `save.c:save_read` | 한 줄씩 읽어 섹션별 해석 함수로 보내고, 값의 범위와 불변 조건을 모두 검사. 주민·건물은 id 로 읽어 `relink` 로 포인터를 다시 연결 |
| `api.c:core_defs_load_file` | `game/data/buildings.def`, `traits.def`, `names.txt` 를 읽음. 틀린 줄이 있으면 **그 파일 전체를 기본값으로** 두고 파일·줄 번호로 경고 |
| `defs.c:defs_write_file` | 내장 기본값을 정의 파일로 씀 (`castle_cli --dump-defs game/data`) |
| `platform.c:platform_fopen_utf8` | Windows 에서 한글 경로를 `_wfopen` 으로 엶 |
| `platform.c:platform_replace_file` | Windows 는 `MoveFileExW(REPLACE_EXISTING)`, 그 밖은 `rename` |

게임 기능: CLI 메뉴 "저장하기"/"불러오기", `castle_cli --load <파일>`, Godot 스모크 테스트의 저장→불러오기 해시 비교. 세이브 메뉴 UI 는 마일스톤 6.
확인하는 테스트: `save_roundtrip_same_hash`, `save_roundtrip_colony`(주민·건물·연결까지), `save_roundtrip_world_pressure`(사건·예보·상인·가격·병까지), `save_keeps_backup`, `save_korean_path`(Windows CI 포함), `defs_builtin_roundtrip`, `cli_menu_script`.

## 7. 오류 처리

| 경우 | 위치 | 처리 | 테스트 |
| --- | --- | --- | --- |
| 파일 열기 실패 | `save.c:save_read`, `save_write` | `CORE_ERR_FILE_OPEN` 반환, CLI 는 "파일을 열 수 없습니다" 출력 | `load_missing_file`, `save_refuses_invalid_world`, `cli_menu_script` |
| 망가진 세이브 | `save.c` 의 모든 해석 함수 | `CORE_ERR_FORMAT`/`CORE_ERR_VERSION` + 문제 줄 번호, 반쯤 읽은 월드는 모두 해제 | `load_truncated_files`, `load_garbled_files`, `load_rejects_bad_values`, `load_wrong_version` |
| 망가진 정의 파일 | `defs.c:defs_load_text` | 그 파일은 기본값으로, 나머지 파일은 그대로. 경고(파일·줄)를 Godot 알림과 CLI 에 보여 줌 | `defs_errors_keep_defaults` |
| 지을 수 없는 자리, 돈 부족, 길을 막는 건물 | `placement.c:placement_check` | 이유 코드(`CORE_PLACE_*`)를 돌려주고 아무것도 바꾸지 않음. Godot 은 미리보기를 빨갛게 하고 이유를 보여 줌 | `placement_rules`, `placement_roads_and_paths` |
| 늦게 온 걷기 보고 | `villager_ai.c:vil_arrived` | 주민의 의도가 그사이 바뀌었으면 `CORE_IGNORED` 로 무시 | `villager_handshake_stale_rev` |
| 할 수 없는 거래·수리 | `market.c:market_trade`, `merchant_accept`, `placement.c:placement_repair` | `CORE_ERR_REJECTED` 와 이유(`CORE_DENY_*`: 시장 없음, 금화 부족, 창고 가득 …). 미리 다 계산해 보고 **전부 할 수 있을 때만** 바꿈 | `market_trade_rules`, `merchant_visits_and_deals`, `storm_damage_ruin_and_repair` |
| 잘못된 사용자 입력 → 다시 묻기 | `cli/console.c:read_int`, `read_text` | 숫자가 아니거나 범위 밖이면 이유를 말하고 다시 물음 | `cli_menu_script` ("abc", "99", "-1", "5" 입력) |
| 잘못된 명령줄 인자 | `cli/main.c:parse_options` | 사용법 출력 후 종료 코드 2 | `cli_bad_arguments` |
| 할당이 `NULL` | 모든 `mem_*` 호출 지점 | `CORE_ERR_OUT_OF_MEMORY` 반환, 이미 할당한 것은 해제, 기존 상태 유지 | `world_out_of_memory_is_clean`, `load_out_of_memory_is_clean` (k 번째 할당을 일부러 실패시킴) |
| 잘못된 API 인자 | `api.c` 모든 함수 | `CORE_ERR_NULL_ARG`/`CORE_ERR_INVALID_ARG`, 절대 죽지 않음 | `api_rejects_null_and_bad_args` |
| DLL 없음, 버전 다름 | `game/scripts/core/CoreBridge.cs` | 화면에 한국어 안내, 게임을 시작하지 않음 | Godot 스모크 테스트 (DLL 을 치우면 종료 코드 2) |
