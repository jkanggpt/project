# 생성형 AI 사용 기록 (AI_USAGE)

수업 규정에 따라 생성형 AI 가 만들었거나 크게 고친 파일을 모두 적습니다.
도구: **Claude Code** (Anthropic), GitHub 저장소에 연결된 클라우드 세션. 사람(학생)은 명세(`docs/SPEC.md`)를 주고, 결과를 Windows 에서 플레이 테스트하고 PR 을 검토·병합합니다.

## 마일스톤 1 — 기초 (2026-11-05)

| 파일 / 폴더 | 내용 | 종류 |
| --- | --- | --- |
| `CMakeLists.txt`, `CMakePresets.json`, `cmake/mingw-w64.cmake`, `core/CMakeLists.txt`, `cli/CMakeLists.txt`, `tests/CMakeLists.txt` | 빌드 설정, 프리셋, MinGW 툴체인 | 생성 |
| `core/include/castle_core.h` | 공개 API | 생성 |
| `core/src/*.c`, `core/src/*.h` (api, clock, economy, events, grid, mem, platform, rng, save, validate, world) | C 게임 코어 | 생성 |
| `cli/*.c`, `cli/*.h` | 콘솔판, 시뮬레이터, 소크 테스트 | 생성 |
| `tests/*` | C 단위 테스트, CLI 스크립트 테스트 | 생성 |
| `game/project.godot`, `game/MagicCastle.csproj`, `game/MagicCastle.sln` | Godot 프로젝트 설정 | 생성 |
| `game/scripts/**/*.cs` | C# 브리지, 시계, 지도, 카메라, HUD, 스모크 테스트, 스크린숏 | 생성 |
| `game/scenes/Main.tscn`, `game/scenes/ui/Hud.tscn`, `game/ui/theme.tres` | 장면, UI 테마 | 생성 |
| `tools/make_art.py` | 지형 아틀라스 생성 스크립트 | 생성 |
| `game/assets/tiles/terrain.png`, `game/assets/buildings/keep.png` | 위 스크립트가 만든 그림 (Kenney CC0 타일 + 절차적 타일) | 생성 |
| `tools/smoke_test.sh`, `tools/screenshots.sh` | 자동 검사 스크립트 | 생성 |
| `.github/workflows/ci.yml` | GitHub Actions CI | 생성 |
| `.gitignore` | | 생성 |
| `README.md`, `docs/ARCHITECTURE.md`, `docs/C_REQUIREMENTS.md`, `docs/C_WALKTHROUGH.md`, `docs/DECISIONS.md`, `docs/QA.md`, `docs/AI_USAGE.md`, `docs/devlog/TEMPLATE.md` | 문서 | 생성 |
| `PROGRESS.md`, `CREDITS.md`, `CLAUDE.md` (Commands 절) | 진행 기록, 출처, 명령 | 수정 |
| `docs/screenshots/m1_*.png` | 헤드리스 스크린숏 (게임 화면) | 생성 |
| Kenney 에셋 폴더의 `.import` 파일, `.gdignore` | Godot 가져오기 메타데이터 | 생성 (Godot 이 만듦) |

학생이 직접 한 것: Kenney 에셋 팩 추가(`game/assets/third_party/kenney/`), 명세 작성.

## 마일스톤 2 — 마을 살림 (2026-11-05)

| 파일 / 폴더 | 내용 | 종류 |
| --- | --- | --- |
| `core/include/castle_core.h` | ABI 2: 주민·건물·정의 뷰, 배치·명령·보고 함수 | 수정 |
| `core/src/` defs, defs_builtin, entity, placement, villager_ai, jobs, population, text (`.c`/`.h`) | 정의 파일, 주민·건물, 배치·건설·철거, 주민 상태 기계, 직업·임금, 이주 | 생성 |
| `core/src/` api, world, grid, economy, events, save, validate | 장부, 도로·점유 격자, 세이브 v2, 새 불변 조건 | 수정 |
| `cli/walker.c`, `cli/walker.h` | CLI 의 주민 걷기 흉내 | 생성 |
| `cli/` commands, format, sim, main | 건설·철거·목록 명령, reasonable 봇, `--dump-defs`, `--data`, `--events` | 수정 |
| `tests/test_colony.c`, `tests/` 나머지 | 마을 테스트 15개, 기존 테스트 갱신 | 생성 / 수정 |
| `game/data/buildings.def`, `traits.def`, `names.txt` | 정의 파일 (`castle_cli --dump-defs` 로 만듦) | 생성 |
| `game/scripts/world/` Art, BuildMode, BuildingNode, FloatingText, Pathfinder, VillagerAgent | 건물·주민 노드, 길찾기, 건설 도구 | 생성 |
| `game/scripts/ui/` BuildBar, Inspector, VillagerList | 건설 막대, 검사창, 주민 목록 | 생성 |
| `game/scripts/` Main, core/*, ui/Hud, ui/Toasts, ui/Strings, world/MapView, world/WorldSync, debug/* | 연결, ABI 2 바인딩, 자동 조종 스모크 테스트, 스크린숏 | 수정 |
| `game/scenes/Main.tscn` | 도로 층, 건물·주민 층, 건설 도구 | 수정 |
| `tools/make_art.py`, `game/assets/buildings/*.png`, `game/assets/units/villagers.png`, `game/assets/ui/*.png`, `game/assets/tiles/roads.png` (+ `.import`) | Kenney CC0 타일을 잘라 만든 건물·주민·아이콘 그림 | 생성 |
| `docs/*.md`, `PROGRESS.md`, `CREDITS.md`, `CLAUDE.md`, `.github/workflows/ci.yml` | 문서, CI 시간 제한 | 수정 |

## 마일스톤 3 — 계절의 압박 (2026-11-05)

| 파일 / 폴더 | 내용 | 종류 |
| --- | --- | --- |
| `core/src/nature.c`, `core/src/nature.h`, `core/src/market.c`, `core/src/market.h` | 날씨·자연 사건·난방·병, 시장·떠돌이 상인 | 생성 |
| `core/include/castle_core.h` | ABI 3: 날씨·사건·장부·거절 이유 열거형, 장부·시장·상인 뷰, 거래·수리 명령 | 수정 |
| `core/src/` api, world, events, economy, entity, jobs, placement, population, save, validate, villager_ai, defs_builtin | 사건·시장 연결, 피해·수리, 병 행동, 세이브 v3, 불변 조건 | 수정 |
| `cli/` commands, format, sim | 장부·시장·상인·수리 메뉴, 사건 문장, 봇의 거래·수리, 소크 명령 | 수정 |
| `tests/test_pressure.c`, `tests/test_town.c`, `tests/test_town.h`, 나머지 테스트 | 계절·사건·수리·병·시장·상인·장부·저장 테스트 9개, 공통 도우미 분리 | 생성 / 수정 |
| `game/data/buildings.def` | 시장·치료소 등급 0 | 수정 (`--dump-defs`) |
| `game/scripts/ui/MarketPanel.cs`, `MerchantPanel.cs`, `game/scripts/world/WeatherFx.cs` | 시장 창, 상인 창, 비·눈 입자 | 생성 |
| `game/scripts/` core/Native, core/CoreBridge, ui/Hud, ui/Inspector, ui/Strings, world/MapView, world/BuildingNode, world/WorldSync, Main, debug/* | ABI 3, 위쪽 막대 상태, 장부 툴팁, 수리, 계절 색·눈, 피해 표시, 자동 조종·스크린숏 | 수정 |
| `tools/make_art.py`, `game/assets/tiles/snow.png` (+ `.import`) | 눈 덮개 타일 (절차적으로 그림) | 생성 |
| `docs/*.md`, `README.md`, `PROGRESS.md`, `CREDITS.md`, `CLAUDE.md` | 문서 | 수정 |

## 마일스톤 4 — 위협 (2026-11-05)

| 파일 / 폴더 | 내용 | 종류 |
| --- | --- | --- |
| `core/src/threat.c`, `core/src/threat.h` | 위협 예고·습격 구성 표(`THREAT_KINDS`)·적 동적 배열·보고 적용·시간 초과 | 생성 |
| `core/include/castle_core.h` | ABI 4: 위협·방향·적 행동·적 상태 열거형, 위협·습격·적 뷰, 적 정의 뷰, 습격 보고 함수, 건물 공격 수치, `CORE_DENY_RAID` | 수정 |
| `core/src/` api, defs, defs_builtin, events, placement, save, validate, villager_ai, world | 적 정의 파일, 습격 이벤트, 피신·경비 상태, 습격 중 수리 금지, 세이브 v4, 불변 조건 | 수정 |
| `cli/battle.c`, `cli/battle.h` | 숫자 전투 (같은 보고 API) | 생성 |
| `cli/` commands, format, main, sim | 위협·습격 상태와 문장, 봇의 감시탑·병영, 소크의 습격 보고 | 수정 |
| `tests/test_threat.c`, 나머지 테스트 | 위협 예고, 습격 시작(피신·경비), 보고 적용, 시간 초과, 무너진 성벽, 적 정의, 저장, 습격 중 수리, 피신한 주민의 식사 (9개) | 생성 / 수정 |
| `game/data/enemies.def`, `game/data/buildings.def` | 적 6종, 건물 공격 수치 | 생성 / 수정 (`--dump-defs`) |
| `game/scripts/world/EnemyAgent.cs`, `CombatDirector.cs`, `game/scripts/audio/Sfx.cs`, `game/default_bus_layout.tres` | 적 행동, 전투 지휘(감시탑·경비병·화살), 효과음, 오디오 버스 | 생성 |
| `game/scripts/` core/Native, core/CoreBridge, world/Pathfinder, world/VillagerAgent, world/WorldSync, world/Art, world/CameraRig, ui/Hud, ui/Inspector, ui/Strings, Main, debug/* | ABI 4, 적용 길찾기, 피신·경비 모습, 위협 카운트다운·습격 띠, 화면 흔들림, 자동 조종의 방어·습격 진행, 습격 스크린숏 | 수정 |
| `tools/make_art.py`, `game/assets/units/enemies.png`, `game/assets/units/arrow.png`, `game/assets/ui/bubble_alarm.png` (+ `.import`) | 적 그림(Kenney 타일), 화살·경보 말풍선(직접 그림) | 생성 |
| `docs/*.md`, `README.md`, `PROGRESS.md`, `CREDITS.md`, `CLAUDE.md` | 문서 | 수정 |

## 마일스톤 5 — 진행 (2026-11-06)

| 파일 / 폴더 | 내용 | 종류 |
| --- | --- | --- |
| `core/src/research.c`, `research.h`, `rank.c`, `rank.h`, `tutorial.c`, `tutorial.h`, `difficulty.c`, `difficulty.h` | 연구(효과 함수 표 `RESEARCH_EFFECTS`), 등급·승급·승패, 튜토리얼 목표(확인 함수 표 `TUTORIAL_CHECKS`), 난이도 프리셋 표 | 생성 |
| `core/include/castle_core.h` | ABI 5: 등급·연구·승패·튜토리얼 열거형, 연구·등급·기록·난이도 뷰, 연구 시작·승급 명령, 새 이벤트·거절 이유·장부 항목 | 수정 |
| `core/src/` api, defs, defs_builtin, economy, events, jobs, market, nature, placement, population, save, threat, validate, villager_ai, world | 연구·등급 정의 파일, 마탑 직업, 연구 효과가 닿는 곳(시장 차이·난방·병·결계), 성채 함락, 한 판의 기록, 세이브 v5, 불변 조건 | 수정 |
| `cli/` commands, format, main, sim | 연구·승급 메뉴, 상태의 등급·목표·결과, 봇의 승급 목표·연구, 소크의 패배 처리 | 수정 |
| `tests/test_progress.c`, `tests/test_town.c`·`.h`, 나머지 테스트 | 연구·효과·저장, 승급 규칙, 승리, 성채 함락·인구 0 패배, 튜토리얼, 난이도, 정의 파일 (9개), 기존 테스트를 패배 규칙에 맞게 | 생성 / 수정 |
| `game/data/research.def`, `game/data/ranks.def`, `game/data/buildings.def` | 연구 12개, 등급 5개, 방어 점수·등급 잠금·밸런스 | 생성 / 수정 (`--dump-defs`) |
| `game/scripts/ui/ResearchPanel.cs`, `RankPanel.cs`, `TutorialPanel.cs`, `OutcomePanel.cs` | 연구 창, 등급 창, 처음 목표 목록, 승리·패배 화면 | 생성 |
| `game/scripts/` core/Native, core/CoreBridge, ui/Hud, ui/Inspector, ui/Strings, ui/Toasts, world/CombatDirector, world/EnemyAgent, Main, debug/LaunchOptions, debug/SmokeTest, debug/ScreenshotTour | ABI 5, 등급·연구 단추, 결과 화면과 새 게임, `--difficulty`, 연구 뒤 전투 수치 다시 읽기, 같은 알림 합치기, 자동 조종의 승급·연구, 새 스크린숏 | 수정 |
| `docs/*.md`, `README.md`, `PROGRESS.md`, `CLAUDE.md` | 문서 | 수정 |

## 마일스톤 6 — 저장과 메뉴 (2026-11-06)

| 파일 / 폴더 | 내용 | 종류 |
| --- | --- | --- |
| `core/include/castle_core.h`, `core/src/api.c`, `core/src/save.c`, `core/src/save.h` | ABI 6: 세이브 요약 뷰와 `core_peek_save`, `core_check_castle_name`, `save_peek_version`, `CORE_SAVE_VERSION` | 수정 |
| `cli/commands.c`, `cli/format.c`, `cli/format.h` | 불러오기 전에 요약을 보여 주고 망가졌으면 예비 파일 제안, 새 게임 이름 검사 | 수정 |
| `tests/test_save.c`, `tests/test_api.c`, `tests/test_list.h`, `tests/cli_menu_input.txt`, `tests/run_cli_script.cmake` | 요약·망가진 세이브와 예비 파일·다른 버전·이름 검사 테스트 3개, CLI 스크립트의 잘린 세이브 실행 | 수정 |
| `game/scripts/core/GameSettings.cs`, `game/scripts/core/SaveSlots.cs` | 설정 파일, 세이브 슬롯 경로·요약·이어하기 | 생성 |
| `game/scripts/ui/MenuLayer.cs`, `MenuParts.cs`, `TitleScreen.cs`, `PauseMenu.cs`, `NewGameDialog.cs`, `SlotDialog.cs`, `SettingsPanel.cs` | 제목 화면, 일시정지 메뉴, 새 게임 창, 저장·불러오기 창, 설정 창, 확인 창 | 생성 |
| `game/scripts/` Main, core/Native, core/CoreBridge, ui/Hud, ui/Strings, world/CameraRig, debug/SmokeTest, debug/ScreenshotTour | 한 판의 흐름(제목 → 게임 → 저장·불러오기), 자동 저장, ABI 6, 메뉴 단추·자동 저장 표시, 메뉴 중 카메라 입력 끄기, 저장 흐름 검사, 메뉴 스크린숏 | 수정 |
| `docs/*.md`, `README.md`, `PROGRESS.md`, `CLAUDE.md` | 문서 | 수정 |
