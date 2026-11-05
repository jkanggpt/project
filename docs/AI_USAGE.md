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
