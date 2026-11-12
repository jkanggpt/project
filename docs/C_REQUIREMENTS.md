# C 요구사항 대응표

고급프로그래밍 과제의 C 요구사항(`docs/SPEC.md` §1.1)이 **어느 파일의 어느 함수**에서, **어떤 게임 기능**으로 쓰이는지 정리한 표입니다.
마일스톤마다 갱신합니다. (지금: 마일스톤 7 끝)

## 한눈에 보기

| 요구사항 | 대표 위치 | 게임에서 하는 일 |
| --- | --- | --- |
| 구조체 2개 이상, 포함·참조 관계 | `core/src/entity.h` `Villager`, `Building` / `world.h` `struct CoreWorld` / `threat.h` `Raid`, `Enemy` / `defs.h` `ResearchDef`, `RankDef` | 주민과 건물, 게임 상태 전체, 습격과 적, 연구와 등급 |
| 포인터로 원본 수정, 포인터로 배열·구조체 접근 | `entity.c:entity_link_worker`, `economy.c:eco_change`, `villager_ai.c:vil_set_intent`, `threat.c:reserve_enemies`, `research.c:scale` | 일꾼 배치, 자원·장부, 주민 의도, 습격의 적 목록, 연구 효과 |
| 사용자 정의 함수 10개 이상 | 코어 약 480개, CLI 약 140개 | 모듈마다 한 가지 역할 |
| 함수 포인터 | `research.c:RESEARCH_EFFECTS[]`, `tutorial.c:TUTORIAL_CHECKS[]`, `threat.c:THREAT_KINDS[]`, `nature.c:NATURE_EVENTS[]`, `villager_ai.c:STATE_TABLE[]`, `jobs.c:JOB_TABLE[]`, `defs.c:SECTION_PARSERS[]`, `save.c:SECTIONS[]`, `cli/commands.c:COMMANDS[]` | 연구 효과, 튜토리얼 목표 확인, 위협 종류별 세기·습격 구성, 자연 사건, 주민 행동, 직업별 생산, 정의 파일·세이브 해석, CLI 메뉴 |
| 동적 메모리 | `entity.c:entity_add_villager`·`reserve_slot`·`insert_sorted`, `threat.c:reserve_enemies`, `defs.c:add_research`, `grid.c:grid_alloc`, `events.c:grow` | 늘었다 줄었다 하는 주민·건물 목록, 일꾼·거주민 배열, 습격마다 수가 다른 적 목록, 정의 파일이 정하는 연구 목록, 지도, 이벤트 큐 |
| 파일 입출력 | `save.c:save_write`/`save_read`, `api.c:core_defs_load_file`, `defs.c:defs_write_file` | 저장 → 껐다 켜기 → 이어하기 (날씨·사건·시장 가격·상인·위협 예고·연구·등급·승패까지), 슬롯 셋 + 자동 저장과 목록 요약(`core_peek_save`), 건물·특성·이름·적·연구·등급 정의 파일 |
| 오류 처리 | `save.c:save_read`, `defs.c:defs_load_text`, `market.c:market_trade`, `threat.c:threat_report_*`, `research.c:research_start`, `rank.c:rank_up`, `cli/console.c:read_int`, 모든 `mem_*` 호출 | 파일 없음, 망가진 세이브(→ 예비 파일 `.bak` 제안)·다른 버전의 세이브, 망가진 정의 파일, 할 수 없는 거래·수리·연구·승급(이유 코드), 늦게 온 습격 보고, 습격 중 저장, 패배 뒤 진행, 잘못된 입력, 메모리 부족 |

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
| `ThreatState`, `Threat` | `core/src/threat.h` | 예고된 위협들(종류, 방향, 도착 틱, 세기)과 진행 중인 습격 | `ThreatState` 가 `Threat threats[4]` 와 `Raid raid` 를 **포함**, `World` 안에 포함 |
| `Raid`, `Enemy` | `core/src/threat.h` | 습격 하나와 그 적들 (처치·달아남·약탈 집계) | `Raid` 가 `Enemy *enemies` **동적 배열을 소유** (습격마다 적 수가 다름) |
| `EnemyDef` | `core/src/defs.h` | 적 정의 (체력, 피해, 공격 사이, 속도, 거리, 행동, 훔치는 자원·양, 마정석) | `Defs` 안에 `EnemyDef enemies[6]` 로 포함, `data/enemies.def` 에서 읽음 |
| `ThreatKind` | `core/src/threat.c` | 위협 종류 하나의 도착 시각·첫 날과 **함수 포인터 2개**(세기 계산, 습격 구성) | `THREAT_KINDS[]` 표 |
| `ResearchDef` | `core/src/defs.h` | 연구 하나 (키, 갈래, 앞선 연구 번호, 필요 등급, 비용, 점수, **효과 번호와 값**) | `Defs` 가 `ResearchDef *research` **동적 배열을 소유** (`data/research.def`) |
| `RankDef` | `core/src/defs.h` | 등급 하나의 조건(인구·기분·방어·연구 수)과 공물·저장 보너스 | `Defs` 안에 `RankDef ranks[5]` 로 포함 (`data/ranks.def`) |
| `ResearchState` | `core/src/research.h` | 끝낸 연구(비트), 진행 중인 연구와 점수, 효과가 바꾼 규칙 값(시장 차이·난방·치료·결계) | `World` 안에 포함 |
| `RunStats` | `core/src/world.h` | 한 판의 기록 (최다 인구, 처치, 죽음, 떠남, 지은 건물, 승리한 날) | `World` 안에 포함, 승리·패배 화면 |
| `DifficultyPreset` | `core/src/difficulty.h` | 난이도 하나의 값 (시작 주민·자원, 기분, 습격 세기·간격, 자연 사건, 공물 %) | `PRESETS[3]` 표, `difficulty_preset` 가 포인터로 돌려줌 |
| `Battle`, `Snapshot` | `cli/battle.c` | CLI 숫자 전투의 진행 상태, 한 판 동안 쓰는 뷰 복사본 | `Snapshot` 이 건물·주민·적 뷰 **동적 배열을 소유** |
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
| | `placement.c:placement_damage(World *world, Building *building, int32_t damage)` — 체력을 깎고, 0 이면 무너뜨림 | 폭풍 피해, 습격 피해 |
| | `market.c:market_trade(..., int32_t *out_gold, int32_t *out_reason)` — 결과와 거절 이유를 출력 매개변수로 | 시장 거래 |
| | `threat.c:reserve_enemies(Raid *raid, int32_t needed)` — 구조체 안의 배열 포인터와 용량을 함께 바꿈 | 습격의 적 목록 |
| | `threat.c:threat_report_loot(..., int32_t *out_amount)` — 실제로 빼앗긴 양을 출력 매개변수로 | 약탈 |
| | `research.c:scale(int32_t *field, int32_t value)` — 건물 정의의 필드 하나를 가리키는 포인터를 받아 % 만큼 키움 (`&bdef(world, CORE_BLD_FARM)->rate`) | 연구 효과 (농장 생산 +25% …) |
| | `rank.c:rank_up(World *world, int32_t *out_reason)`, `research.c:research_start(..., int32_t *out_reason)` — 못 하면 이유를 출력 매개변수로 | 승급, 연구 시작 |
| | `cli/battle.c:battle_tick(Battle *battle, CoreWorld *world)` — CLI 쪽 전투 상태를 한 판 진행 | CLI 숫자 전투 |
| | `api.c:core_get_map_size(..., int32_t *out_width, int32_t *out_height)` | 출력 매개변수 |
| 포인터로 배열 원소 접근 | `grid.c:stamp_blob`, `place_mana_cluster`, `carve_corridor` — `uint8_t *cell = &grid->terrain[i]; *cell = ...` | 지형 칠하기 |
| | `world.c:world_name_is_valid` — `const unsigned char *s` 로 한 바이트씩 걸으며 UTF-8 검사 | 성채 이름 검사 |
| 포인터로 구조체 접근 | `grid.c:ensure_fair_start` — `const FairRule *rule = &FAIR_RULES[r]; rule->min_count` | 공정한 시작 |
| | `threat.c:start_raid` — `THREAT_KINDS[threat->type].compose(world, raid, threat->strength)`: 표의 구조체 원소에서 함수 포인터를 꺼내 부름 | 습격 구성 |
| | `threat.c:add_enemies` — `Enemy *enemy = &raid->enemies[raid->enemy_count]; enemy->type = ...` | 적 하나 채우기 |
| | `research.c:research_reapply` — `const ResearchDef *def = &world->defs.research[i]; RESEARCH_EFFECTS[def->effect](world, def->value)` | 끝낸 연구의 효과 다시 적용 |
| | `difficulty.c:difficulty_preset` — 표 원소의 주소(`const DifficultyPreset *`)를 돌려주고, 부르는 쪽이 `preset->raid_strength_pct` 로 읽음 | 난이도 |
| | `jobs.c:jobs_work` — `villager->workplace` 포인터를 따라가 건물 종류로 작업 함수를 고름 | 생산 |
| 포인터의 포인터 (배열) | `entity.c:insert_sorted(Villager ***array, ...)` — 배열 자체를 다시 할당해 돌려줌 | 일꾼·거주민 목록 |
| | `validate.c:check_grid(Checker *checker, const World *world)` | 불변 조건 검사 |
| 포인터의 포인터 (출력) | `api.c:core_create(..., CoreWorld **out_world)`, `save.c:save_read(..., World **out_world, ...)` | 새 게임, 불러오기 |

## 3. 함수 (10개 이상, 각자 한 가지 역할)

코어 약 480개, CLI 약 140개. 예:

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
| `threat.c` | `threat_start_game`, `threat_tick`, `schedule_next`, `start_raid`, `end_raid`, `end_if_done`, `reserve_enemies`, `add_enemies`, `compose_bandits`, `threat_report_building_damage`, `threat_report_loot`, `threat_is_guard` |
| `research.c` | `research_start`, `research_work`, `research_reapply`, `research_state_of`, `research_mages`, `complete`, `scale`, `effect_farm_yield` … `effect_ward` |
| `rank.c` | `rank_defense`, `rank_fill_view`, `rank_is_ready`, `rank_up`, `rank_storage_bonus`, `rank_lose`, `rank_hourly` |
| `debug.c` | `debug_stress`, `build_ring`, `build_free` (성능 측정용 큰 마을, 디버그 전용, 마일스톤 7) |
| `jobs.c` (배치) | `jobs_auto_assign` (두 번 돎: 아무도 일하지 않는 종류 먼저, 그다음 지은 순서), `fill_up_to`, `best_candidate` |
| `tutorial.c` | `tutorial_hourly`, `tutorial_note_assign`, `built_farm`, `built_house`, `assigned_worker`, `built_wall`, `survived_raid` |
| `cli/battle.c` | `battle_tick`, `snapshot_take`, `defense_power`, `pick_wall`, `defend`, `attack`, `loot` |
| `defs.c` | `defs_load_text`, `defs_write_file`, `parse_sections`, `parse_names`, `apply_field`, `begin_building`, `begin_trait` |
| `cli/console.c` | `console_init`, `console_read_line`, `read_int`, `read_text` |

## 4. 함수 포인터 (선언, 실제 함수에 연결, 실행할 동작 고르기)

| 표 | 위치 | 고르는 기준 | 게임 기능 |
| --- | --- | --- | --- |
| `ResearchEffectFn` / `RESEARCH_EFFECTS[]` | `core/src/research.c` | 연구 정의의 효과 번호(`effect = farm_yield` …) → `effect_farm_yield`(농장 생산 %), `effect_extract_yield`, `effect_storage`, `effect_market`(시장 차이 −), `effect_tower_damage`(감시탑 피해 +), `effect_wall_hp`, `effect_guard_damage`, `effect_keep_damage`, `effect_mana_yield`, `effect_healing`(병 확률 −%), `effect_heating`(난방 −), `effect_ward`(습격 피해 −%). 연구가 끝나거나 불러올 때 `research_reapply` 가 원래 정의에서 시작해 끝낸 연구를 번호 순으로 다시 부름 | 연구 나무 |
| `TutorialCheckFn` / `TUTORIAL_CHECKS[]` | `core/src/tutorial.c` | 튜토리얼 목표 번호 → 확인 함수(`built_farm`, `built_house`, `assigned_worker`, `built_wall`, `survived_raid`). 매시간 아직 못 한 목표의 함수만 부름 | 처음 목표 목록 |
| `StrengthFn`·`ComposeFn` / `THREAT_KINDS[]` | `core/src/threat.c` | 위협 종류(도적, 마물, 적대 영주 군대) → 세기 함수(`bandit_strength`: 날짜와 금화, `monster_strength`, `army_strength`)와 구성 함수(`compose_bandits`: 다섯에 하나는 두목, `compose_monsters`: 스무 날 뒤 오우거, `compose_army`: 셋에 하나는 궁수) | 습격 |
| `NatureHandler` / `NATURE_EVENTS[]` | `core/src/nature.c` | 자연 사건 번호(가뭄, 역병, 폭풍, 혹한, 풍년) → `on_start`, `on_day`, `on_end` 함수 (역병은 `plague_start`·`plague_day`, 폭풍은 `storm_start`, 효과만 있는 사건은 NULL) | 계절의 압박 |
| `StateHandler` / `STATE_TABLE[]` | `core/src/villager_ai.c` | 주민 상태 번호(쉬기, 잠, 출근, 일, 식사, 떠남 …) → 그 상태에서 할 일을 정하는 함수 | 주민 행동 |
| `JobWorkFn` / `JOB_TABLE[]` | `core/src/jobs.c` | 건물 종류 → `work_farm`(계절을 따짐), `work_gatherer`(약초 + 식량 조금), `work_extract`(벌목장·채석장·광산: 주변 지형 칸 수를 따짐) | 생산 |
| `SectionParser` / `SECTION_PARSERS[]` | `core/src/defs.c` | 정의 파일의 `[building …]`, `[trait …]`, `[enemy …]`, `[research …]`, `[rank …]` → 시작 함수(`begin_building` … `begin_research`, `begin_rank`)와 필드 함수(`field_building` … `field_research`, `field_rank`) 짝 | 건물·특성·적·연구·등급 정의 읽기 |
| `SectionParser` / `SECTIONS[]` | `core/src/save.c` | 세이브 파일의 `[섹션]` 이름 → `parse_meta`, `parse_clock`, `parse_rng`, `parse_resources`, `parse_ledger`, `parse_map`, `parse_building`, `parse_villager`, `parse_nature`, `parse_market`, `parse_merchant`, `parse_threat`, `parse_research`, `parse_progress` | 불러오기 |
| `CliCommandFn` / `COMMANDS[]` | `cli/commands.c` | 메뉴 번호 → `cmd_status`, `cmd_map`, `cmd_save` … | CLI 메뉴 |
| `EventFormatter` / `EVENT_FORMATTERS[]` | `cli/format.c` | 이벤트 종류 번호 → 문장 만드는 함수 (위협·습격 이벤트 6개는 `format_threat`) | CLI 알림 문장 |
| `BotDailyFn` / `BOTS[]` | `cli/sim.c` | `--bot` 이름 → 봇 함수 | 밸런스 시뮬레이터 |
| `ValidateReportFn` | `core/src/validate.h` | 호출한 쪽이 넘긴 콜백 | 테스트에서 위반 내용 출력 |


## 5. 동적 메모리 (크기를 미리 모르는 데이터, 모두 해제)

| 데이터 | 할당 | 해제 | 왜 동적인가 |
| --- | --- | --- | --- |
| 월드 | `world.c:world_alloc` (`mem_calloc`) | `world_destroy` | 게임마다 하나, 수명이 프로그램과 다름 |
| 주민·건물 하나하나 | `entity.c:entity_add_villager`, `entity_add_building` (`mem_calloc`) | `entity_remove_villager`, `entity_remove_building`, `entity_free_all` | 이주·떠남·건설·철거로 수가 계속 바뀜 |
| 주민·건물 포인터 배열 | `entity.c:reserve_slot` (**`mem_realloc` 으로 두 배씩**) | `entity_free_all` | 몇 명, 몇 채가 될지 모름 |
| 건물의 일꾼·거주민 배열 | `entity.c:insert_sorted` (`mem_realloc`) | `entity_remove_building` | 건물마다 칸 수가 다름 (정의 파일이 정함) |
| 습격의 적 목록 | `threat.c:reserve_enemies` (**`mem_realloc` 으로 두 배씩, 임시 포인터**) | `threat.c:threat_free` (`world_destroy`) | 위협마다 적 수가 다름 (날짜·금화·난이도로 정해짐, 최대 64). 습격이 끝나도 배열은 다음 습격에 다시 씀 |
| 특성·이름 목록 | `defs.c:grow_array` (`mem_realloc`) | `defs_free` | 정의 파일에 몇 줄이 있을지 모름 |
| 연구 목록 | `defs.c:add_research` → `grow_array` (`mem_realloc`, 최대 32개) | `defs_free` | 연구가 몇 개인지 `data/research.def` 가 정함 (파일이 목록 전체를 바꿈). 월드마다 자기 복사본(`defs_copy`) |
| 정의 파일 내용 | `api.c:core_defs_load_file` (`mem_malloc`, 상한만큼) | 같은 함수 끝 | 파일 크기를 모름 |
| 지형·비용 격자 | `grid.c:grid_alloc` (`mem_calloc` × 2) | `grid_free` | 지도 크기가 월드를 만들 때(또는 세이브에서) 정해짐 |
| 이벤트 큐 | `events.c:events_init`, **`grow` 에서 `mem_realloc` 으로 두 배씩** | `events_free` | 한 틱에 몇 개 생길지 모름 |
| 세이브 한 줄 버퍼 | `save.c:read_line` 에서 **`mem_realloc` 으로 키움** | `save_read` 끝 | 줄 길이를 모름 (지도 너비만큼 길어짐) |
| 노이즈 값, BFS 대기열, 방문 표시 | `grid.c:grid_generate`, `grid_flood_reachable`, `ensure_connectivity` | 같은 함수 안에서 | 지도 크기에 비례 |
| 임시 경로 문자열 | `save.c:path_with_suffix` | `save_write` 끝 | 경로 길이를 모름 |
| CLI 지도 버퍼 | `cli/commands.c:cmd_map` (`malloc`) | 같은 함수 끝 | 지도 크기를 실행 중에 앎 |
| CLI 숫자 전투의 뷰 복사본 | `cli/battle.c:snapshot_take` (`malloc` × 3) | `snapshot_free` | 건물·주민·적 수를 실행 중에 앎 |
| CLI 걷기 흉내의 거리표·힙 | `cli/walker.c:refresh_cost` (`malloc`) | `walker_free` | 지도 크기에 비례 |
| 소크 테스트의 주민 명령 버퍼 | `cli/sim.c` (`malloc`, 주민 수만큼) | 같은 함수 끝 | 주민 수를 실행 중에 앎 |

- 모든 할당은 `core/src/mem.c` 의 `mem_malloc`/`mem_calloc`/`mem_realloc`/`mem_free` 를 거칩니다. 안에서는 그냥 `malloc`/`calloc`/`realloc`/`free` 를 부릅니다.
- `realloc` 은 **임시 포인터에 받아서** 실패해도 원래 배열을 잃지 않습니다 (`events.c:grow`, `save.c:read_line`, `threat.c:reserve_enemies`).
- 새는 메모리 0: Linux CI 에서 AddressSanitizer + LeakSanitizer 로 모든 테스트와 400일 × 20시드 소크 테스트를 돌립니다.

## 6. 파일 입출력 (저장 → 프로그램 종료 → 다시 실행 → 불러오기)

| 위치 | 하는 일 |
| --- | --- |
| `save.c:save_write` | 텍스트 세이브를 `<경로>.tmp` 에 쓰고, 예전 파일을 `<경로>.bak` 으로 복사한 뒤, 한 번에 바꿔치기 |
| `save.c:save_read` | 한 줄씩 읽어 섹션별 해석 함수로 보내고, 값의 범위와 불변 조건을 모두 검사. 주민·건물은 id 로 읽어 `relink` 로 포인터를 다시 연결 |
| `save.c:save_peek_version` | 첫 줄만 읽어 세이브 형식 번호를 알아냄 (다른 버전의 세이브라고 알려 주려고) |
| `api.c:core_peek_save` | 슬롯 목록용 요약: `save_read` 로 **파일 전체를 실제로 불러와 검사**한 뒤 이름·날짜·등급·주민·난이도·승패·틱만 `CoreSaveInfoView` 에 남기고 월드를 바로 해제. 실패하면 결과 코드와 줄 번호 |
| `api.c:core_defs_load_file` | `game/data/buildings.def`, `traits.def`, `names.txt`, `enemies.def`, `research.def`, `ranks.def` 를 읽음. 틀린 줄이 있으면 **그 파일 전체를 기본값으로** 두고 파일·줄 번호로 경고 |
| `defs.c:defs_write_file` | 내장 기본값을 정의 파일로 씀 (`castle_cli --dump-defs game/data`) |
| `platform.c:platform_fopen_utf8` | Windows 에서 한글 경로를 `_wfopen` 으로 엶 |
| `platform.c:platform_replace_file` | Windows 는 `MoveFileExW(REPLACE_EXISTING)`, 그 밖은 `rename` |

세이브 v5 는 `[threat]` 섹션에 예고된 위협과 습격 번호를, `[research]` 에 끝낸 연구(번호가 아니라 **키**로: 정의 파일 순서가 바뀌어도 맞게)와 진행 중인 연구를, `[progress]` 에 등급 알림·승패·튜토리얼·한 판의 기록을 씁니다. `[research]` 는 건물보다 먼저 와야 합니다(연구가 성벽 최대 체력 등을 바꾸므로). **습격 중에는 저장하지 않습니다** (`core_save` 가 `CORE_ERR_REJECTED`): 싸움의 실시간 상태(적의 위치)는 Godot 의 것이라 C 가 저장할 수 없기 때문입니다.
게임 기능 (마일스톤 6): Godot 의 세이브 슬롯 셋(`user://saves/slot1~3.sav`)과 자동 저장(`autosave.sav`: 하루가 시작될 때, 습격 중이면 끝난 뒤, 제목 화면으로 가거나 끝낼 때), 제목 화면의 "이어하기"(가장 최근 세이브), 불러오기 창의 요약 목록. 망가진 슬롯은 예비 파일(`.bak`, 그 전 저장)을 제안하고, 다른 버전의 세이브는 형식 번호를 보여 주며 막습니다. CLI 메뉴 "불러오기"도 요약을 먼저 보여 주고 망가졌으면 `.bak` 을 묻습니다. 파일을 쓰고 읽고 검사하는 일은 모두 C 가 하고, Godot 은 슬롯 번호를 경로로 바꿀 뿐입니다.
확인하는 테스트: `save_peek_summarizes_slot`, `save_peek_broken_offers_backup`(잘린 세이브·`.bak`·다른 버전), `save_roundtrip_same_hash`, `save_roundtrip_colony`(주민·건물·연결까지), `save_roundtrip_world_pressure`(사건·예보·상인·가격·병까지), `threat_save_roundtrip`(위협 예고까지), `research_effects_and_save`(연구 효과와 진행까지), `save_keeps_backup`, `save_korean_path`(Windows CI 포함), `defs_builtin_roundtrip`, `cli_menu_script`(두 번째 실행에서 잘린 세이브 → `.bak` 로 불러오기), Godot 스모크 테스트(슬롯 저장 → 제목 화면 → 이어하기 해시 비교, 잘린 슬롯 → 예비 파일, 자동 저장 횟수).

## 7. 오류 처리

| 경우 | 위치 | 처리 | 테스트 |
| --- | --- | --- | --- |
| 파일 열기 실패 | `save.c:save_read`, `save_write` | `CORE_ERR_FILE_OPEN` 반환, CLI 는 "파일을 열 수 없습니다" 출력 | `load_missing_file`, `save_refuses_invalid_world`, `cli_menu_script` |
| 망가진 세이브 | `save.c` 의 모든 해석 함수 | `CORE_ERR_FORMAT`/`CORE_ERR_VERSION` + 문제 줄 번호, 반쯤 읽은 월드는 모두 해제 | `load_truncated_files`, `load_garbled_files`, `load_rejects_bad_values`, `load_wrong_version` |
| 망가진 세이브를 고르면 | `api.c:core_peek_save`, `cli/commands.c:cmd_load`, Godot `SlotDialog`·`Main.ContinueLatest` | 목록에 "망가진 세이브 (N번째 줄)", 예비 파일이 멀쩡하면 "예비 파일로 불러올까요?", 다른 버전이면 "형식 4, 이 게임은 5" 로 막음. 실패하면 하던 게임은 그대로 | `save_peek_broken_offers_backup`, `cli_menu_script`, Godot 스모크 테스트 |
| 쓸 수 없는 성채 이름 | `api.c:core_check_castle_name` (`world_name_is_valid`) | 새 게임 창이 입력할 때마다 검사해 시작 단추를 끄고 이유를 보여 줌, CLI 는 다시 물음 | `castle_name_check` |
| 망가진 정의 파일 | `defs.c:defs_load_text` | 그 파일은 기본값으로, 나머지 파일은 그대로. 경고(파일·줄)를 Godot 알림과 CLI 에 보여 줌 | `defs_errors_keep_defaults` |
| 지을 수 없는 자리, 돈 부족, 길을 막는 건물 | `placement.c:placement_check` | 이유 코드(`CORE_PLACE_*`)를 돌려주고 아무것도 바꾸지 않음. Godot 은 미리보기를 빨갛게 하고 이유를 보여 줌 | `placement_rules`, `placement_roads_and_paths` |
| 늦게 온 걷기 보고 | `villager_ai.c:vil_arrived` | 주민의 의도가 그사이 바뀌었으면 `CORE_IGNORED` 로 무시 | `villager_handshake_stale_rev` |
| 할 수 없는 거래·수리 | `market.c:market_trade`, `merchant_accept`, `placement.c:placement_repair` | `CORE_ERR_REJECTED` 와 이유(`CORE_DENY_*`: 시장 없음, 금화 부족, 창고 가득, 습격 중 …). 미리 다 계산해 보고 **전부 할 수 있을 때만** 바꿈 | `market_trade_rules`, `merchant_visits_and_deals`, `storm_damage_ruin_and_repair`, `no_repair_during_raid` |
| 늦게 온 습격 보고, 규칙에 맞지 않는 보고 | `threat.c:threat_report_*` | 끝난 습격의 보고는 `CORE_IGNORED`, 없는 적·건물은 `CORE_ERR_NOT_FOUND`, 저장소가 아닌 곳의 약탈·두 번째 약탈은 거절. Godot 이 멈춰도 하루 뒤 C 가 스스로 습격을 끝냄 | `raid_reports_apply`, `raid_times_out` |
| 할 수 없는 연구·승급 | `research.c:research_start`, `rank.c:rank_up` | `CORE_ERR_REJECTED` + 이유(`CORE_DENY_RESEARCH_LOCKED`·`RESEARCH_BUSY`·`RESEARCH_DONE`·`NO_GOLD`·`NO_MANA`, `REQUIREMENTS`·`MAX_RANK`·`RAID`). 비용을 확인한 뒤에만 냄 | `research_flow`, `rank_up_rules` |
| 패배한 뒤의 진행·명령 | `world.c:world_tick`, `research_start`, `rank_up` | `core_tick` 은 `CORE_ERR_REJECTED` (시간이 흐르지 않음), 명령은 `CORE_DENY_GAME_OVER`. 진 월드도 저장·불러오기는 됨 | `keep_falls_defeat`, `population_zero_defeat`, 소크 테스트 |
| 망가진 연구·등급 파일 | `defs.c` | 없는 효과 키·갈래, 앞에 없는 `requires` 키, 범위 밖 숫자 → 그 파일만 기본값. 세이브의 모르는 연구 키는 건너뜀 | `research_and_rank_files` |
| 습격 중 저장 | `api.c:core_save` | `CORE_ERR_REJECTED`, 파일을 건드리지 않음 | `raid_start_flee_and_guards`, Godot 스모크 테스트 |
| 잘못된 사용자 입력 → 다시 묻기 | `cli/console.c:read_int`, `read_text` | 숫자가 아니거나 범위 밖이면 이유를 말하고 다시 물음 | `cli_menu_script` ("abc", "99", "-1", "5" 입력) |
| 잘못된 명령줄 인자 | `cli/main.c:parse_options` | 사용법 출력 후 종료 코드 2 | `cli_bad_arguments` |
| 할당이 `NULL` | 모든 `mem_*` 호출 지점 | `CORE_ERR_OUT_OF_MEMORY` 반환, 이미 할당한 것은 해제, 기존 상태 유지 | `world_out_of_memory_is_clean`, `load_out_of_memory_is_clean` (k 번째 할당을 일부러 실패시킴) |
| 성능 측정 마을의 잘못된 요청 | `api.c:core_debug_stress`, `debug.c` | 주민 0~300·적 0~64 밖이면 `CORE_ERR_INVALID_ARG`, 이미 습격 중이면 `CORE_ERR_REJECTED`, 진 월드도 거절. 공짜 건물도 장부("기타")를 거쳐 불변 조건을 지킴 | `debug_stress_town` |
| 잘못된 API 인자 | `api.c` 모든 함수 | `CORE_ERR_NULL_ARG`/`CORE_ERR_INVALID_ARG`, 절대 죽지 않음 | `api_rejects_null_and_bad_args` |
| DLL 없음, 버전 다름 | `game/scripts/core/CoreBridge.cs` | 화면에 한국어 안내, 게임을 시작하지 않음 | Godot 스모크 테스트 (DLL 을 치우면 종료 코드 2) |
