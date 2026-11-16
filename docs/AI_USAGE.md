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

## 마일스톤 7 — 다듬기와 밸런스 (2026-11-12)

| 파일 / 폴더 | 내용 | 종류 |
| --- | --- | --- |
| `core/src/debug.c`, `core/src/debug.h` | 성능 측정용 큰 마을 (디버그 전용) | 생성 |
| `core/include/castle_core.h`, `core/src/` api, jobs, placement, threat, defs_builtin, difficulty | ABI 7·8: `core_debug_stress`, 정의 뷰의 `count_terrain`; 자동 배치 두 번 돌기; 연구·공물 숫자; 혹독 시작 마정석 | 수정 |
| `game/data/ranks.def`, `game/data/research.def` | 공물·연구 비용 (`--dump-defs`) | 수정 |
| `cli/` main, sim | `--bench`, 봇의 승급 목표 순서 | 수정 |
| `tests/` test_colony, test_progress, test_api, test_list | 종류 먼저 배치, 가을 이주, 큰 마을, 세는 지형 (4개, 총 82개) | 수정 |
| `tools/balance.py` | 시뮬레이터 봇 밸런스 측정 (시드 × 난이도, 등급별 중앙값) | 생성 |
| `game/scripts/debug/StressProbe.cs`, `game/scripts/world/Effects.cs`, `game/scripts/ui/HelpPanel.cs` | 프레임 시간 측정, 반짝임·흙먼지, 도움말 창 | 생성 |
| `game/scripts/` Main, audio/Sfx, core/*, debug/LaunchOptions, debug/SmokeTest, debug/ScreenshotTour, ui/*, world/BuildMode, world/BuildingNode, world/MapView, world/Pathfinder, world/VillagerAgent, world/EnemyAgent, world/WorldSync | `--stress`, 길찾기 예산, 겹쳐 그리기, 일꾼 없는 일터 표시, 소리, 지형 새로 고침, 적 이름·체력, 자동 조종 다듬기 | 수정 |
| `docs/*.md`, `README.md`, `PROGRESS.md`, `CLAUDE.md` | 문서 | 수정 |

## C 실행판 전환 — Codex (OpenAI), 2026-11-14

학생 요청: C 이외의 실행 코드를 가능한 범위에서 C로 재구현하고 Godot를 제거.

| 파일 / 폴더 | 작업 |
| --- | --- |
| game/src/path.c, path.h | C A*·최소 힙·비용 격자·경로 수명 생성 |
| game/src/session.c, session.h | 스냅숏·연속 이동·고정 틱·안전한 월드 교체 생성 |
| game/src/combat.c | 적·경비병·탑·투사체·벽 공격·약탈·보고 생성 |
| game/src/storage.c, storage.h | 한글 경로·명령줄·설정·파일 처리 생성 |
| game/src/main.c, app.h | C 진입점·한 판 흐름·슬롯·자동 저장·자동 검사 생성 |
| game/src/assets.c, assets.h | PNG/TTF/OGG와 한글 글꼴 로더·효과음 생성 |
| game/src/render.c | 타일·건물·주민·전투·날씨·건설·카메라 생성 |
| game/src/panels.c, ui.c, ui.h | 한국어 UI·메뉴·검사창·목록·연구·거래 생성 |
| game/CMakeLists.txt, CMakeLists.txt, CMakePresets.json, core/CMakeLists.txt, cli/CMakeLists.txt | raylib·정적 코어·C 도구·검사 빌드 수정 |
| cli/sim.c, sim.h | 같은 C 봇을 그래픽 자동 검사에서 쓰도록 공용 진입점 추가 |
| tools/balance.c, CMakeLists.txt | Python 대신 C 밸런스 집계 생성 |
| tools/build_windows.ps1, smoke_test.cmake, screenshots.cmake | 빌드·자동 검사·화면 촬영 설정 생성 |
| tests/test_native.c, tests/CMakeLists.txt | C 경로·이동·전투·설정·저장 교체 검사 추가 |
| tests/run_cli_script.cmake | Windows 출력 UTF-8 해석 지정 |
| core/cli/tests의 기존 C·헤더 | 이전 Godot/C#을 가리키던 주석을 C 프런트엔드 설명으로 갱신 |
| game/scripts/**/*.cs, *.uid, scenes/*.tscn, scenes/ui/*.tscn, project.godot, MagicCastle.csproj, MagicCastle.sln, default_bus_layout.tres, ui/theme.tres | C 구현으로 대체해 제거 |
| game/assets/**/*.import, **/.gdignore | Godot 전용 메타데이터 제거 |
| tools/balance.py, make_art.py, smoke_test.sh, screenshots.sh | C 도구·CMake 실행 설정으로 대체하거나 재생성 제외로 제거 |
| .github/workflows/ci.yml, .gitignore, README.md, CLAUDE.md, CREDITS.md, PROGRESS.md | C 빌드·실행·검사·배포·진행 기록 갱신 |
| docs/NATIVE_PORT.md, ARCHITECTURE.md, QA.md, SPEC.md, C_WALKTHROUGH.md, C_REQUIREMENTS.md, DECISIONS.md, AI_USAGE.md | 현재 구조·범위·검사·전환 우선순위 기록 |

외부 raylib 소스는 공식 아카이브를 검증해 사용하며 AI가 만든 코드로 표시하지 않습니다.
스크린숏은 새 C 실행판이 렌더링한 화면입니다. 사용자 기존 AGENTS.md와 저장 파일은 변경하지 않습니다.

최종 검증에서 한국어 C 문자열을 C11 `u8` 리터럴로 명시하고, 구형 Windows SDK 헤더의 C5105 경고를 해당 include에만 한정 처리했습니다.
`docs/screenshots/native_title.png`, `native_game.png`는 배포본이 직접 렌더링한 화면입니다. README와 CMake 배포 규칙에 연결했습니다.
배포본·검사 로그·ZIP은 artifacts/에만 두고 Git에 넣지 않습니다. GitHub 인증 취소 후 학생 지시에 따라 로컬 커밋으로 마무리합니다.

## 2026-11-16 — 새 픽셀아트와 높이 표현

학생 요청: 첨부 주민·병영 화풍을 참고해 모든 그림을 새로 제작하고, 판정 칸을 유지하면서 주민 1.5타일·건물과 나무의 높이 및 가림을 적용.
내장 image_gen 도구를 사용했습니다. 생성 프롬프트 6개 전문은 art/PROMPTS.md에 보관합니다. 첨부 참고 그림 자체는 저장소·배포본에 넣지 않았습니다.

| 파일 | 생성·수정 내용 |
| --- | --- |
| art/generated/villagers.png, enemies.png | 주민 8직업·적 6종, 각각 걷기 4프레임 생성 |
| art/generated/buildings.png | 성채·주택·농장·채집·벌목·채석·광산·창고·시장·병영·탑·벽·문·치료소·마탑·공사장 생성 |
| art/generated/props.png, ground.png, icons.png | 계절 수목·돌·광맥·풀, 지면 5종·눈, 자원·화살·포장길 생성 |
| game/assets/painted/*.png (29개), LICENSE.txt | C 도구로 원본 자르기·알파 정리·크기와 발밑 정렬, 새 그림의 CC0 헌정 기록 |
| tools/pack_art.c, tools/CMakeLists.txt | Python 없이 PNG 패킹·크기/알파 검사하는 C 도구 생성·빌드 등록 |
| game/src/sprite_layout.c, sprite_layout.h | 점유와 별개인 비율 유지 건물 영역·1.5타일 주민·발밑 비교·직업 매핑 생성 |
| game/src/render.c | 지면/소품 분리, 수목·건물·주민·적 통합 정렬, 걷기, 큰 건설 미리보기·공사장·화살, 좌표 해시·점유 마스크 |
| game/src/assets.c, assets.h | 새 PNG 로딩·해제, 누락 리소스 오류 처리 |
| game/src/app.h, main.c | 정렬·점유 마스크 버퍼 수명 관리, 촬영 마을 주민의 3시간 실제 이동 |
| game/CMakeLists.txt, tests/test_native.c | 화면과 분리된 높이·지면 기준점·가림 순서·직업 검사 |
| art/README.md, PROMPTS.md, README.md, CREDITS.md, CLAUDE.md | 제작 경로·프롬프트·라이선스·재패킹·현재 그림 설명 |
| docs/ARCHITECTURE.md, DECISIONS.md, QA.md, C_WALKTHROUGH.md, SPEC.md, AI_USAGE.md, PROGRESS.md | 구현·판정 유지·검증·한계 기록 |
| docs/screenshots/native_game.png, native_title.png | 새 C 실행판에서 직접 촬영한 현재 그림으로 갱신 |

이미지 생성 외에 C 코드와 문서는 Codex가 작성했습니다. 글꼴·효과음은 기존 외부 에셋이며 새로 생성하지 않았습니다.
