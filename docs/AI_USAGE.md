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

