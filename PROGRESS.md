# PROGRESS

## 현재 상태 (2026-11-05)

**마일스톤 1 (Foundations) 완료.** 브랜치 `claude/wizardly-allen-7ou35f` (세션이 지정한 이름. SPEC 의 `m1-foundations` 대신 사용, `docs/DECISIONS.md` 참고). PR 을 열고 학생들의 Windows 플레이 테스트를 기다리는 중.

### 된 것
- **빌드**: CMake + 프리셋(`linux-debug`, `linux-clang-debug`, `linux-release`, `mingw-release`, `windows-msvc`, `windows-mingw`), MinGW 툴체인 파일. 경고 0 (GCC/Clang `-Wall -Wextra -Wpedantic` 외, MSVC `/W4`, CI 에서 `-Werror`/`/WX`).
- **C 코어** (`core/`): 월드 생성·해제, 시드 지형 생성(96×96, 공정한 시작, 연결성 보장), 틱(시계·계절·밤낮 이벤트), 자원 은행, 피드백 이벤트 큐(태그 공용체, realloc 성장), 텍스트 세이브(원자적 쓰기 + `.bak`, 엄격한 검증), 불변 조건 검사기, 상태 해시, UTF-8/Windows 플랫폼 층, 공개 API 16개.
- **테스트**: CTest 33개 (단위 29 + CLI 4). 할당 실패 주입, 세이브 퍼징(잘림·뒤섞임·잘못된 값·버전), 한글 경로, 결정성. GCC·Clang 새니타이저(ASan/UBSan/LSan) 통과.
- **CLI** (`cli/`): 함수 포인터 명령 표 메뉴, `read_int` 재입력, 상태·지도·시간·저장·불러오기, `--simulate`(CSV), `--soak`(매 틱 검사). 400일 × 20시드 소크 통과(위반 0, 누수 0).
- **Godot** (`game/`): 자동 로드 `Core`(DLL 해석, ABI·구조체 크기 검사, 모든 호출 감싸기)와 `GameClock`(고정 틱, 일시정지/1×/2×/3×), `WorldSync`, 지형 TileMapLayer, 성채 임시 그림, 낮밤, 카메라(WASD, 가장자리, 가운데 끌기, 정수 확대, 경계), 위쪽 HUD(날짜·시각·배속·자원, 툴팁), 알림, 시작 실패 화면, Galmuri 글꼴, 정수 UI 배율.
- **자동 검사**: `tools/smoke_test.sh` (헤드리스 20일, 매 프레임 검사, 저장→불러오기 해시, 엔진 오류·경고 0), `tools/screenshots.sh` (Xvfb + llvmpipe OpenGL 로 잘 동작).
- **CI** (`.github/workflows/ci.yml`): Linux GCC·Clang 새니타이저 + 짧은 소크, 400일 × 20시드 소크, MinGW 교차, Windows MSVC(테스트 + 한글 경로 + DLL 아티팩트), C# Debug/ExportRelease, Godot 스모크 테스트. **모두 초록** (run 2).
- **문서**: README, ARCHITECTURE, C_REQUIREMENTS, C_WALKTHROUGH(질문 15개), DECISIONS, QA, AI_USAGE, devlog 템플릿, CREDITS, CLAUDE.md Commands.

### 확인하는 방법
`CLAUDE.md` 의 Commands 절 참고. 요약: `ctest --preset linux-debug`, `tools/smoke_test.sh 20`, `tools/screenshots.sh`.

### 알려진 문제 / 빈 곳
- 물 가장자리(물가) 그림이 없어 호수 경계가 반듯함 → M7 그림 작업.
- 겨울 눈 덮개, 날씨 파티클 없음 → M3.
- 제목 화면 없음: 실행하면 바로 무작위 시드의 새 게임(성채 이름 "바람언덕", 보통)이 시작됨 → M6.
- 창 폭이 1280 보다 좁으면 UI 배율이 1배가 되어 글씨가 작음. 설정에서 고를 수 있게 할 예정 → M6.
- 음악 없음 (CC0 곡을 찾으면 추가).
- 클라우드 VM 메모: Clang 새니타이저에는 `libclang-rt-18-dev` 가 필요해서 이번 세션에서 `apt-get install` 로 설치함. 환경 설정 스크립트에 넣으면 좋음. Xvfb 스크린숏 실행이 한 번 멈춘 적이 있어(재현 안 됨) 스크립트에 `timeout` 을 넣음.
- 빠진 에셋: 없음 (Kenney 팩과 Galmuri 로 충분). 물·바위 지대·광맥은 Kenney 팔레트로 만든 절차적 타일.

## 다음 할 일 — 마일스톤 2 (Colony loop)

학생들이 M1 PR 을 병합한 뒤 `main` 에서 시작합니다. 순서 제안:

1. **정의 파일**: `defs` 모듈과 `game/data/buildings.def`, `traits.def`, `names.txt`(한국어 이름 약 150개). 섹션 해석 함수 표. 파일 위치 결정(`docs/DECISIONS.md` "다음 마일스톤에서 정할 것").
2. **엔티티**: `entity` 모듈 — `Villager`(Stats, Needs 포함, `Building *home/workplace` 참조), `Building`(`Villager **workers/residents` 동적 배열), 포인터 배열 풀(`realloc`), id 카운터(저장), 양방향 연결 끊고 해제, 뒤에서부터 지우기.
3. **배치**: 점유 격자, `core_can_place`(오류 코드 + 생산성 미리보기), 건설 명령(비용 선불, 건설 틱), 철거(50% 환불), 성채를 건물로 미리 배치.
4. **주민 핸드셰이크**: 상태 표 `STATE_TABLE[state]`, 의도 + `intentRev`, `core_villager_arrived/unreachable`, 뷰 복사(주민·건물).
5. **경제**: 직업 표 `JOB_TABLE[job]`, 생산(WORKING 일 때만), 소비, 임금, 기분, 이주와 떠남, 도로(비용 격자 수정 → `cost_grid_rev`).
6. **Godot**: `WorldSync` 에서 id 로 주민·건물 노드 맞추기, `AStarGrid2D`(비용 격자 rev 바뀔 때만 다시), `VillagerAgent`, `BuildMode`(유령 그림, 끌어서 도로), 건물·주민 검사창, 아래쪽 건설 막대.
7. **세이브 확장**(주민·건물·id), 테스트(양방향 연결, 배치 규칙, 생산·소비·임금, 오래된 intentRev), 시뮬레이터 "reasonable" 봇, 스모크 테스트 오토파일럿(건물 짓기).
