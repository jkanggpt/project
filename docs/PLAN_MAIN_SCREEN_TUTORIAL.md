# 메인 화면·튜토리얼 병행 작업 계획

두 기능을 **동시에** 만듭니다. **메인 화면은 Codex, 튜토리얼은 Claude**가 맡습니다.
두 작업은 서로의 진행 중인 코드를 보지 못하므로, 이 문서가 둘 사이의 약속입니다. 담당·약속을 바꿔야 하면 먼저 사용자에게 알리고 이 문서를 고치는 PR로 바꿉니다.

- 기준 브랜치: `codex/structure-cleanup` (저장소의 기본 브랜치)
- 준비 단계(Phase 0) PR: CI 복구 + 아래 연결점. 이 PR이 병합된 뒤의 기본 브랜치에서 두 작업을 시작합니다.

---

## 1. 정한 것

| 항목 | 결정 |
|---|---|
| 담당 | 메인 화면 = Codex, 튜토리얼 = Claude |
| 메인 화면 그림 | Codex가 새 로고·제목 그림을 아트 파이프라인으로 만든다. 어려우면 기존 그림 + 글자 로고로 대신한다 |
| 튜토리얼 형태 | 메인 화면의 "튜토리얼" 단추로 시작하는 **별도 튜토리얼 게임** (고정 지도, 평화 난이도). 일반 게임은 지금의 "처음 목표" 5개를 그대로 둔다 |
| 세이브 | 튜토리얼 진행을 저장하므로 스키마 13 → 14. 이전 저장은 읽지 않는다 (지금 정책과 같음). 튜토리얼 작업만 바꾼다 |
| 공개 API | 튜토리얼 API 추가로 ABI 14 → 15. 튜토리얼 작업만 바꾼다 |

## 2. 브랜치와 병합

- **Codex**: `codex/main-screen` 브랜치 (Phase 0 병합 뒤의 `codex/structure-cleanup`에서 만듦).
- **Claude**: `claude/wizardly-allen-7ou35f` 브랜치 (Claude 세션이 푸시할 수 있는 유일한 브랜치).
- 둘 다 기본 브랜치(`codex/structure-cleanup`)에 직접 커밋·푸시하지 않습니다. 각자 PR을 열고 **사용자가 병합**합니다.
- 먼저 끝난 쪽이 먼저 병합됩니다. 나중 쪽은 최신 기본 브랜치를 자기 브랜치에 합치고(충돌 해결·전체 검사) PR을 갱신합니다.
- 상대 브랜치를 가끔 `git fetch` 해서 겹치는 곳을 일찍 찾습니다. 겹치면 고치기 전에 사용자에게 알립니다.
- Codex는 그림 생성 전에 먼저 디자인·아트 작업을 시작해도 됩니다 (Phase 0와 겹치는 파일이 없음). 코드는 Phase 0가 병합된 기본 브랜치에서 시작합니다.

## 3. 파일 담당 (가장 중요)

| 경로 | 담당 | 메모 |
|---|---|---|
| `game/src/ui/main_menu.{h,c}`, 새 `game/src/ui/menu_*.c` | 메인 화면 | 상태는 `GameMenuState`에 더함 |
| `game/src/ui/panels.c`의 `title()`, `new_game()`, `game_render_interface`의 `PANEL_TITLE`·`PANEL_NEW` case | 메인 화면 | main_menu.c로 옮겨도 됨 |
| `game/src/ui/panels.c`의 `hud()`, `build_bar()`, `inspector()`의 "성채 안내/처음 목표", `PANEL_HELP` case | 튜토리얼 | 강조할 단추 위치 기록 호출을 더함 |
| `game/src/ui/panels.c`의 나머지 (저장·설정·연구·거래·주민·장부 …) | 둘 다 고치지 않음 | 필요하면 사용자와 상의 |
| `game/src/ui/widgets.c`, `game/src/ui.h` (새 위젯) | 메인 화면 | 튜토리얼은 자기 파일 안에 그리기 함수를 둠 |
| `game/src/ui/tutorial.{h,c}`, 새 `game/src/ui/tutorial_*.c` | 튜토리얼 | 상태는 `GameTutorialState`에 더함 |
| `game/src/main.c`: 시작 순서(제목 배경 세계), `enter_game`의 전환 연출 | 메인 화면 | |
| `game/src/main.c`: `screenshot_screen`의 `"tutorial"` 분기 | 튜토리얼 | |
| `game/src/render/world.c`: 게임 밖(`!in_game`)일 때의 배경 그리기(안개·카메라) | 메인 화면 | |
| `game/src/render/world.c`: `game_world_input`의 입력 제한 | 튜토리얼 | |
| `game/src/app.h` | 둘 다 고치지 않음 | Phase 0에서 `menu`·`tutorial` 필드를 넣어 둠 |
| `game/CMakeLists.txt` | 새 소스: 메인 화면은 `add_executable(castle_game …)` **바로 다음 줄**에 `target_sources(castle_game PRIVATE …)`, 튜토리얼은 `if(CASTLE_BUILD_TESTS)` **바로 앞 줄**에 `target_sources(…)` | 같은 줄을 고치지 않기 위함. `if(CASTLE_BUILD_TESTS)` 블록은 튜토리얼 담당 |
| `core/**`, `core/include/**`, `core/CMakeLists.txt` | 튜토리얼 | 메인 화면은 지금 공개 API로 충분 (`core_create`, `core_debug_stress`, `core_copy_terrain`, `core_get_map_size`, `core_get_difficulty`, `core_peek_save`) |
| `core/src/persistence/**`, `docs/SAVE_FORMAT.md` | 튜토리얼 | |
| `cli/**` | 튜토리얼 | 콘솔 상태 보기에 튜토리얼 문구 |
| `tests/**` | 튜토리얼 | 메인 화면은 스크린숏으로 확인 (테스트가 꼭 필요하면 사용자와 상의) |
| `art/**`, `game/assets/**`, `tools/art/**`, `tools/capture_scenes.ps1`, `tools/screenshots.cmake`, `CREDITS.md`, `docs/ASSETS.md`, `docs/audit/**` | 메인 화면 | 튜토리얼은 새 그림을 만들지 않음 (`ui/portraits.png` 등 기존 그림만) |
| `.github/workflows/ci.yml` | 둘 다 고치지 않음 | |
| 새 문서 `docs/MAIN_SCREEN.md` / `docs/TUTORIAL.md` | 메인 화면 / 튜토리얼 | 각자 만든 것·검사 결과를 여기에 |
| `PROGRESS.md`, `README.md`, `docs/GAME_STATE.md`, `docs/PLAYER_GUIDE.md`, `docs/SPEC.md` | 둘 다 고치지 않음 | 마지막 통합 단계에서 한 번에 고침 (6절) |

## 4. 약속 (연결점)

Phase 0에서 만든 것입니다. 이름·인자·반환값은 바꾸지 않습니다.

```c
/* game/src/ui/tutorial.h — 메인 화면의 "튜토리얼" 단추가 부르는 유일한 함수 */
int game_tutorial_start(struct GameApp *app);   /* 1: 튜토리얼 게임으로 들어감, 0: 실패(알림 표시됨) */
void game_tutorial_update(struct GameApp *app, float delta);   /* 게임 중 매 프레임 (main.c) */
void game_tutorial_draw(struct GameApp *app);   /* HUD 위·창 아래 (panels.c) */

/* game/src/ui/main_menu.h */
void game_main_menu_update(struct GameApp *app, float delta);  /* 게임 밖 매 프레임 (main.c) */
```

- 지금의 `game_tutorial_start`는 자리표시입니다: "튜토리얼 영지", 시드 42, 평화로 새 게임을 시작할 뿐입니다. 그래도 단추는 지금부터 끝까지 동작합니다.
- **배경 세계**: 메인 화면이 `a->session`의 세계를 배경으로 쓴다면, 새 게임·불러오기·`game_tutorial_start`가 그 세계를 바꿔 끼운다는 점을 지켜 주세요. 배경용으로 넣은 것(큰 마을 등)이 실제 게임에 남으면 안 되고, 제목 화면으로 돌아오면 배경 세계를 다시 만듭니다. 배경 세계는 자동 저장되지 않아야 합니다 (`game_app_save`는 `in_game == 0`이면 저장하지 않음).
- 자동 검사 길은 계속 동작해야 합니다: `--screenshot … --screen title`, `--screen tutorial`, `--screen game`(CI), `--smoke-test`.
- 새 그림은 `art/art_manifest.json`·`game/assets/assets_manifest.json`에 등록하고 `assets_regenerate`·`assets_reproducible` 테스트를 통과해야 합니다. 생성 요청 전문은 `art/recipes/provenance.json`에, 권리는 `CREDITS.md`에 적습니다.
- 감사 도구의 금지 이름(`v`+세 자리 숫자, `patch`+숫자, `saves_v`+숫자, `legacy`)을 새 파일 이름·코드에 쓰지 않습니다.

## 5. 기능 명세

### 5.1 메인 화면 (Codex)

지금: 검은 바탕의 상자 하나, 단추 7개, 개발자용 부제("C로 다스리는 중세 판타지 성채").

1. **살아 있는 배경**: 시작 때 만드는 세계(시드 42)에 작은 마을을 세우고 (예: `core_debug_stress`를 작은 수로), 안개 없이, 카메라가 천천히 움직이며, 낮밤 색이 바뀌고, 주민이 걸어 다님. 프레임 부담을 작게 (주민 수 제한, 창이 비활성이면 멈춤).
2. **로고와 제목 그림**: "마법성채 키우기" 로고(새 그림 또는 Galmuri 글꼴의 금색 글자 + 그림자), 영주관 문장.
3. **메뉴**: 이어하기(가장 최근 저장의 요약 카드: 이름·날짜·등급·인구, `core_peek_save`), 새 게임, **튜토리얼**(`game_tutorial_start`), 불러오기, 설정, 도움말, 제작진, 종료. 마우스 올림 효과, 클릭 소리, ↑/↓·Enter, 구석에 버전 1.2.4. 개발자용 부제는 지움.
4. **새 게임 창**: 시드의 작은 지도 미리보기 (`core_create` + `core_copy_terrain`, 다 쓰면 해제), 난이도 카드와 설명(`core_get_difficulty`), 지도 크기 문구를 실제 값으로 (`core_get_map_size`, 지금은 "192×192"로 잘못 나옴).
5. **제작진 창**: `CREDITS.md`의 내용 (그림·소리·글꼴·이름 출처).
6. **화면 전환**: 메뉴 ↔ 게임 사이 짧은 어두워짐/밝아짐.

**끝난 것으로 보는 기준**: Windows·Linux CI 통과, `--screen title`을 네 해상도(800×600, 1024×768, 1440×900, 1920×1080)에서 확인, 메인 화면 → 새 게임/튜토리얼/이어하기/불러오기 → 제목으로 돌아오기를 반복해도 메모리가 늘지 않음, 새 그림의 재생성 검사 통과, `docs/MAIN_SCREEN.md`.

### 5.2 튜토리얼 (Claude)

지금: 오른쪽의 "처음 목표" 5개와 고정 안내 문단, F1 도움말뿐. `core/src/common/tutorial.c`는 1단계(Godot판)의 쓰이지 않는 코드이고, 실제 목표 확인은 `core/src/world/content.c`의 `expansion_goals`.

1. **코어 단계 표** (`core/src/common/tutorial.c`를 새로 씀): 단계마다 제목, 설명, 목표(UI 요소·건물 종류·지도 칸), 완료 확인 함수, 정보 단계인지(시간 멈춤). 함수 포인터 표로 둡니다. 공개 API: 현재 단계 보기, 정보 단계 넘기기, 건너뛰기 (ABI 15). 세계에 튜토리얼 상태를 두고 저장 (스키마 14).
2. **튜토리얼 게임**: `game_tutorial_start`가 미리 고른 지도(시드 확정)·평화 난이도로 시작. 방어 단계 전에는 습격이 오지 않음.
3. **단계 (약 12개, 초안)**: 카메라 이동 → 일시정지·배속 → 텃밭(건설 막대 탭 → 카드 → 표시한 자리) → 막사 → 우물 → 벌목지 → 주민을 일터에 직접 배치 → 연결 도로 10칸 → 자원·장부 보기 → 영지 창의 승급 조건 → 초소·목책과 예고된 작은 습격 → 촌락 승급, 끝으로 연구·거래 안내.
4. **화면**: 안내 창(기존 초상화, "다음"·"건너뛰기", 진행 "3/12"), 목표 단추·카드의 깜빡이는 강조, 지을 자리의 지도 표시. 정보 단계에서는 시간을 멈추고, 행동 단계는 해내면 저절로 넘어감.
5. **콘솔**: 상태 보기에 현재 단계 문구 (같은 코어 상태).
6. **검사**: 단계별 코어 테스트, 공개 API만으로 튜토리얼을 끝까지 진행하는 테스트, 저장 왕복, `--screen tutorial`, 튜토리얼 스모크.

**끝난 것으로 보는 기준**: Linux gcc·clang ASan 테스트와 Windows CI 통과, 튜토리얼을 처음부터 끝까지 진행 가능(자동 테스트), 중간 저장 → 불러오기 후 같은 단계, `docs/TUTORIAL.md`, `docs/SAVE_FORMAT.md`(스키마 14).

## 6. 통합 (두 PR이 모두 병합된 뒤, Claude)

1. 기본 브랜치에서 전체 빌드·테스트 (Linux gcc·clang ASan, 릴리스 그래픽, Windows CI).
2. 흐름 확인: 메인 화면 → 튜토리얼 → 중간 저장 → 제목 → 이어하기 → 튜토리얼 끝 → 제목.
3. 네 해상도 스크린숏, 20일·60일 스모크.
4. 문서 한 번에 갱신: `PROGRESS.md`, `README.md`, `docs/GAME_STATE.md`, `docs/PLAYER_GUIDE.md`, `docs/SPEC.md`.
