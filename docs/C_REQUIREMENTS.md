# C 요구사항 대응표

고급프로그래밍 과제의 C 요구사항(`docs/SPEC.md` §1.1)이 **어느 파일의 어느 함수**에서, **어떤 게임 기능**으로 쓰이는지 정리한 표입니다.
마일스톤마다 갱신합니다. (지금: 마일스톤 1 끝)

## 한눈에 보기

| 요구사항 | 대표 위치 | 게임에서 하는 일 |
| --- | --- | --- |
| 구조체 2개 이상, 포함·참조 관계 | `core/src/world.h` `struct CoreWorld` | 게임 상태 전체 |
| 포인터로 원본 수정, 포인터로 배열·구조체 접근 | `rng.c:rng_next`, `economy.c:eco_bank_add`, `grid.c:stamp_blob` | 난수, 자원, 지형 생성 |
| 사용자 정의 함수 10개 이상 | 코어·CLI 합쳐 약 170개 | 모듈마다 한 가지 역할 |
| 함수 포인터 | `save.c:SECTIONS[]`, `cli/commands.c:COMMANDS[]` | 세이브 섹션 해석, CLI 메뉴 |
| 동적 메모리 | `grid.c:grid_alloc`, `events.c:grow`, `save.c:read_line` | 지도 격자, 이벤트 큐, 세이브 줄 읽기 |
| 파일 입출력 | `save.c:save_write`, `save.c:save_read` | 저장 → 껐다 켜기 → 이어하기 |
| 오류 처리 | `save.c:save_read`, `cli/console.c:read_int`, 모든 `mem_*` 호출 | 파일 없음, 잘못된 입력, 메모리 부족 |

## 1. 구조체 (서로 다른 목적 2개 이상, 포함·참조 관계)

| 구조체 | 파일 | 목적 | 관계 |
| --- | --- | --- | --- |
| `World` (`struct CoreWorld`) | `core/src/world.h` | 게임 상태 전체 | `Clock`, `Rng`, `ResourceBank`, `Grid`, `EventQueue` 를 **포함** |
| `Grid` | `core/src/grid.h` | 지도 | 동적 배열 `terrain`, `cost` 를 **포인터로 소유** |
| `ResourceBank` | `core/src/economy.h` | 자원 6종과 저장 한도 | `World` 안에 포함 |
| `Event` | `core/src/events.h` | 피드백 이벤트 하나 | 종류(`type`)로 고르는 **태그 붙은 공용체(union)** 포함 |
| `EventQueue` | `core/src/events.h` | 원형 큐 | `Event *items` 동적 배열을 **참조** |
| `LoadState` | `core/src/save.c` | 불러오는 중인 상태 | 만들고 있는 `World *world` 를 **참조** |
| `SectionDef` | `core/src/save.c` | 섹션 이름 → 해석 함수 | 함수 포인터 필드 |
| `CliContext` | `cli/commands.h` | CLI 의 현재 게임 | `CoreWorld *world` 를 **참조** |
| `CoreSummaryView` 등 뷰 | `core/include/castle_core.h` | Godot 에 건네는 평평한 복사본 | C# `Native.cs` 와 같은 모양 |

마일스톤 2 에서 더해질 핵심 관계 (SPEC §6.2): 주민 `Villager` 가 `Stats`, `Needs` 를 **포함**하고 `Building *home`, `Building *workplace` 를 **참조**, 건물이 `Villager **workers`, `Villager **residents` 를 동적 배열로 **참조**.

## 2. 포인터

| 쓰임 | 위치 | 게임 기능 |
| --- | --- | --- |
| 포인터를 받아 원본을 바꾸는 함수 | `rng.c:rng_next(Rng *rng)` — 상태를 한 걸음 진행 | 지형 생성의 모든 무작위 |
| | `economy.c:eco_bank_add(ResourceBank *bank, ...)`, `eco_bank_spend` | 자원 더하기/빼기 (한도 자르기) |
| | `events.c:events_push(EventQueue *queue, ...)`, `events_pop` | 알림 쌓기/꺼내기 |
| | `world.c:world_tick(World *world, ...)` | 시간 진행 |
| | `api.c:core_get_map_size(..., int32_t *out_width, int32_t *out_height)` | 출력 매개변수 |
| 포인터로 배열 원소 접근 | `grid.c:stamp_blob`, `place_mana_cluster`, `carve_corridor` — `uint8_t *cell = &grid->terrain[i]; *cell = ...` | 지형 칠하기 |
| | `world.c:world_name_is_valid` — `const unsigned char *s` 로 한 바이트씩 걸으며 UTF-8 검사 | 성채 이름 검사 |
| 포인터로 구조체 접근 | `grid.c:ensure_fair_start` — `const FairRule *rule = &FAIR_RULES[r]; rule->min_count` | 공정한 시작 |
| | `validate.c:check_grid(Checker *checker, const World *world)` | 불변 조건 검사 |
| 포인터의 포인터 (출력) | `api.c:core_create(..., CoreWorld **out_world)`, `save.c:save_read(..., World **out_world, ...)` | 새 게임, 불러오기 |

## 3. 함수 (10개 이상, 각자 한 가지 역할)

코어 약 110개, CLI 약 50개. 예:

| 모듈 | 함수 |
| --- | --- |
| `world.c` | `world_alloc`, `world_destroy`, `world_create`, `world_tick`, `world_step`, `world_set_name`, `world_name_is_valid`, `world_state_hash` |
| `grid.c` | `grid_alloc`, `grid_free`, `grid_generate`, `grid_rebuild_costs`, `grid_flood_reachable`, `grid_count_in_radius`, `fractal_noise`, `paint_top_percent`, `ensure_connectivity`, `ensure_fair_start` |
| `save.c` | `save_write`, `save_read`, `read_line`, `parse_meta`, `parse_map`, `parse_i32`, `check_header`, `begin_section` |
| `economy.c` | `eco_bank_init`, `eco_bank_add`, `eco_bank_can_afford`, `eco_bank_spend` |
| `cli/console.c` | `console_init`, `console_read_line`, `read_int`, `read_text` |

## 4. 함수 포인터 (선언, 실제 함수에 연결, 실행할 동작 고르기)

| 표 | 위치 | 고르는 기준 | 게임 기능 |
| --- | --- | --- | --- |
| `SectionParser` / `SECTIONS[]` | `core/src/save.c` | 세이브 파일의 `[섹션]` 이름 → `parse_meta`, `parse_clock`, `parse_rng`, `parse_resources`, `parse_map` | 불러오기 |
| `CliCommandFn` / `COMMANDS[]` | `cli/commands.c` | 메뉴 번호 → `cmd_status`, `cmd_map`, `cmd_save` … | CLI 메뉴 |
| `EventFormatter` / `EVENT_FORMATTERS[]` | `cli/format.c` | 이벤트 종류 번호 → 문장 만드는 함수 | CLI 알림 문장 |
| `BotDailyFn` / `BOTS[]` | `cli/sim.c` | `--bot` 이름 → 봇 함수 | 밸런스 시뮬레이터 |
| `ValidateReportFn` | `core/src/validate.h` | 호출한 쪽이 넘긴 콜백 | 테스트에서 위반 내용 출력 |

마일스톤 2~5 에서 더해질 것: 주민 상태 표 `STATE_TABLE[state]`, 직업별 작업 `JOB_TABLE[job]`, 자연 사건 처리기, 연구 효과 함수, 정의 파일 섹션 해석기.

## 5. 동적 메모리 (크기를 미리 모르는 데이터, 모두 해제)

| 데이터 | 할당 | 해제 | 왜 동적인가 |
| --- | --- | --- | --- |
| 월드 | `world.c:world_alloc` (`mem_calloc`) | `world_destroy` | 게임마다 하나, 수명이 프로그램과 다름 |
| 지형·비용 격자 | `grid.c:grid_alloc` (`mem_calloc` × 2) | `grid_free` | 지도 크기가 월드를 만들 때(또는 세이브에서) 정해짐 |
| 이벤트 큐 | `events.c:events_init`, **`grow` 에서 `mem_realloc` 으로 두 배씩** | `events_free` | 한 틱에 몇 개 생길지 모름 |
| 세이브 한 줄 버퍼 | `save.c:read_line` 에서 **`mem_realloc` 으로 키움** | `save_read` 끝 | 줄 길이를 모름 (지도 너비만큼 길어짐) |
| 노이즈 값, BFS 대기열, 방문 표시 | `grid.c:grid_generate`, `grid_flood_reachable`, `ensure_connectivity` | 같은 함수 안에서 | 지도 크기에 비례 |
| 임시 경로 문자열 | `save.c:path_with_suffix` | `save_write` 끝 | 경로 길이를 모름 |
| CLI 지도 버퍼 | `cli/commands.c:cmd_map` (`malloc`) | 같은 함수 끝 | 지도 크기를 실행 중에 앎 |

- 모든 할당은 `core/src/mem.c` 의 `mem_malloc`/`mem_calloc`/`mem_realloc`/`mem_free` 를 거칩니다. 안에서는 그냥 `malloc`/`calloc`/`realloc`/`free` 를 부릅니다.
- `realloc` 은 **임시 포인터에 받아서** 실패해도 원래 배열을 잃지 않습니다 (`events.c:grow`, `save.c:read_line`).
- 새는 메모리 0: Linux CI 에서 AddressSanitizer + LeakSanitizer 로 모든 테스트와 400일 × 20시드 소크 테스트를 돌립니다.

## 6. 파일 입출력 (저장 → 프로그램 종료 → 다시 실행 → 불러오기)

| 위치 | 하는 일 |
| --- | --- |
| `save.c:save_write` | 텍스트 세이브를 `<경로>.tmp` 에 쓰고, 예전 파일을 `<경로>.bak` 으로 복사한 뒤, 한 번에 바꿔치기 |
| `save.c:save_read` | 한 줄씩 읽어 섹션별 해석 함수로 보내고, 값의 범위와 불변 조건을 모두 검사 |
| `platform.c:platform_fopen_utf8` | Windows 에서 한글 경로를 `_wfopen` 으로 엶 |
| `platform.c:platform_replace_file` | Windows 는 `MoveFileExW(REPLACE_EXISTING)`, 그 밖은 `rename` |

게임 기능: CLI 메뉴 "저장하기"/"불러오기", `castle_cli --load <파일>`, Godot 스모크 테스트의 저장→불러오기 해시 비교. 세이브 메뉴 UI 는 마일스톤 6.
확인하는 테스트: `save_roundtrip_same_hash`, `save_keeps_backup`, `save_korean_path`(Windows CI 포함), `cli_menu_script`.

## 7. 오류 처리

| 경우 | 위치 | 처리 | 테스트 |
| --- | --- | --- | --- |
| 파일 열기 실패 | `save.c:save_read`, `save_write` | `CORE_ERR_FILE_OPEN` 반환, CLI 는 "파일을 열 수 없습니다" 출력 | `load_missing_file`, `save_refuses_invalid_world`, `cli_menu_script` |
| 망가진 세이브 | `save.c` 의 모든 해석 함수 | `CORE_ERR_FORMAT`/`CORE_ERR_VERSION` + 문제 줄 번호, 반쯤 읽은 월드는 모두 해제 | `load_truncated_files`, `load_garbled_files`, `load_rejects_bad_values`, `load_wrong_version` |
| 잘못된 사용자 입력 → 다시 묻기 | `cli/console.c:read_int`, `read_text` | 숫자가 아니거나 범위 밖이면 이유를 말하고 다시 물음 | `cli_menu_script` ("abc", "99", "-1", "5" 입력) |
| 잘못된 명령줄 인자 | `cli/main.c:parse_options` | 사용법 출력 후 종료 코드 2 | `cli_bad_arguments` |
| 할당이 `NULL` | 모든 `mem_*` 호출 지점 | `CORE_ERR_OUT_OF_MEMORY` 반환, 이미 할당한 것은 해제, 기존 상태 유지 | `world_out_of_memory_is_clean`, `load_out_of_memory_is_clean` (k 번째 할당을 일부러 실패시킴) |
| 잘못된 API 인자 | `api.c` 모든 함수 | `CORE_ERR_NULL_ARG`/`CORE_ERR_INVALID_ARG`, 절대 죽지 않음 | `api_rejects_null_and_bad_args` |
| DLL 없음, 버전 다름 | `game/scripts/core/CoreBridge.cs` | 화면에 한국어 안내, 게임을 시작하지 않음 | Godot 스모크 테스트 (DLL 을 치우면 종료 코드 2) |
