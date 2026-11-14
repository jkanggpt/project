# 마법성채 키우기

성채를 촌락에서 왕성으로 성장시키는 2D 중세 판타지 경영 게임입니다.
**게임 규칙·화면·입력·이동·전투·메뉴·저장을 C로 구현**하며 그래픽·창·소리는 raylib 5.5를 사용합니다.
Godot, .NET, C#, Python 설치는 필요하지 않습니다.

## Windows 빌드와 실행

준비물: CMake 3.21 이상과 Visual Studio 2022 C/C++ 개발 도구 또는 MinGW-w64 + Ninja.
첫 그래픽 빌드 때 공식 raylib 소스를 내려받습니다. 버전과 SHA-256을 고정했으며 다음부터 빌드 캐시를 사용합니다.

Visual Studio 개발자 터미널:

```powershell
cmake --preset windows-msvc -DCASTLE_WERROR=ON
cmake --build --preset windows-msvc
ctest --preset windows-msvc
./build/windows-msvc/bin/Release/castle_game.exe
```

MinGW/Ninja의 일부 배포판은 한글 프로젝트 경로를 처리하지 못합니다. 이때 PowerShell에서:

```powershell
./tools/build_windows.ps1
```

이 스크립트는 비어 있는 드라이브 문자를 프로젝트에 잠시 연결해 구성·빌드·테스트하고 연결을 해제합니다.
완료 메시지의 폴더에서 `castle_game.exe`를 실행하세요. `data/`, `assets/`도 함께 복사됩니다.
영문 경로에서는 `windows-mingw` 프리셋도 바로 사용할 수 있습니다.

## Linux 빌드

그래픽 빌드에는 C/C++ 빌드 도구, CMake, Ninja, OpenGL·X11 개발 패키지가 필요합니다.
Ubuntu 패키지: `build-essential ninja-build libgl1-mesa-dev libx11-dev libxrandr-dev libxi-dev libxcursor-dev libxinerama-dev`.

```sh
cmake --preset linux-release -DCASTLE_WERROR=ON
cmake --build --preset linux-release
build/linux-release/bin/castle_game
```

그래픽 장치·raylib 없이 C 코어와 이동·전투만 검사:

```sh
cmake --preset linux-debug -DCASTLE_WERROR=ON
cmake --build --preset linux-debug
ctest --preset linux-debug -j8
```

## 조작

| 입력 | 동작 |
| --- | --- |
| WASD / 화살표 | 카메라 이동 |
| 가운데 버튼 끌기 / 휠 | 이동 / 정수 배율 확대 |
| 스페이스 / 1·2·3 | 일시정지 / 배속 |
| 아래쪽 건설 분류·건물 버튼 | 건설 도구 선택 |
| 왼쪽 클릭 / 도로·성벽 드래그 | 건설·선택 / 줄 놓기 |
| 오른쪽 클릭 / X | 도구 취소 / 철거 |
| V / F1 | 주민 목록 / 도움말 |
| Esc | 열린 창 → 도구 → 선택 순으로 닫고 메뉴 |

농장과 벌목장, 주택을 먼저 지으세요. 일터에 도착한 주민만 생산하며 도로는 이동 시간을 줄입니다.
겨울에는 농장이 멈추므로 식량과 난방 목재를 비축하세요.
예고된 습격 방향에 탑·성벽·성문을 마련하고 병영에 경비병을 배치하세요.
마을에서 광산, 요새에서 마탑이 열립니다. 연구와 승급을 거쳐 왕성이 되면 승리합니다.

주민·건물 검사창, 직접 일꾼 배치, 시장·상인, 연구, 승급, 장부, 정렬 가능한 주민 목록,
초반 목표·승패 화면, 이름·시드·난이도, 설정·슬롯 메뉴를 제공합니다.
이름 입력은 완성 문자와 Ctrl+V 한글 붙여넣기를 지원합니다.

## 저장과 배포

- Windows: `%APPDATA%/MagicCastle/saves/`, 설정은 상위 폴더의 `settings.cfg`.
- Linux: `$XDG_DATA_HOME/magic-castle/` 또는 `~/.local/share/magic-castle/`.
- 자동 저장과 슬롯 1~3, 원자적 저장과 `.bak` 복구. 세이브 v5·API ABI 8 유지.
- 습격 중에는 저장·수리를 막으며 자동 저장을 습격 종료 뒤로 미룹니다.
- 자동 검사는 실행 파일 옆 `saves_automated/` 또는 지정한 `--save-dir`를 사용합니다.
- 기존 v5 세이브는 새 저장 폴더로 복사하거나 `--save-dir <기존 폴더>`로 읽을 수 있습니다.
- 실행 파일·`data/`·`assets/`·라이선스를 함께 배포합니다. 별도 코어 DLL은 필요하지 않습니다.

## 검사와 C 도구

아래 실행 파일 경로는 Visual Studio 빌드 기준입니다. 다른 빌드의 bin 폴더도 같은 이름을 사용합니다.

```powershell
# 창 없이 실제 C 이동·전투로 20일 자동 플레이하고 저장 해시 검사
./build/windows-msvc/bin/Release/castle_game.exe --smoke-test --days 20 --seed 42

# 주민 150명 + 적 40명
./build/windows-msvc/bin/Release/castle_game.exe --stress
./build/windows-msvc/bin/Release/castle_game.exe --stress-probe --no-audio

# Python 대신 C로 구현한 밸런스 집계
./build/windows-msvc/bin/Release/castle_balance.exe --seeds 16 --days 100

# C 콘솔판과 장시간 검사
./build/windows-msvc/bin/Release/castle_cli.exe
./build/windows-msvc/bin/Release/castle_cli.exe --soak 400 --seeds 20
```

`castle_game --screenshot <파일.png> --screen title|game|research|rank|market|load --no-audio`로 화면을 촬영합니다.
`tools/screenshots.cmake`, `tools/smoke_test.cmake`로도 실행할 수 있습니다.
각 실행 파일은 `--help`를 제공합니다.

## 구조

| 위치 | 역할 |
| --- | --- |
| core/ | C11 상태·게임 규칙·정의·저장·검증 |
| game/src/session.c | 뷰 동기화·고정 틱·주민 이동 |
| game/src/path.c | 가중치 격자 A* |
| game/src/combat.c | 적·경비병·탑·투사체·결과 보고 |
| game/src/main.c | 한 판의 흐름·자동 저장·자동 검사 |
| game/src/render.c, panels.c, ui.c | 지도·카메라·건설·한국어 UI |
| game/src/assets.c, storage.c | PNG·TTF·OGG·한글 경로·설정 |
| game/data/, game/assets/ | 기존 정의와 CC0/OFL 리소스 |
| cli/ | 콘솔판·공용 C 봇·CSV·간이 전투·소크 |
| tools/balance.c | C 밸런스 결과 집계 |
| tests/ | 코어·CLI·새 실행 계층 검사 |
| docs/ | 구조·전환 범위·발표·QA·AI 기록 |

전환 범위와 간소화는 [NATIVE_PORT.md](docs/NATIVE_PORT.md), 검증 결과는 [PROGRESS.md](PROGRESS.md)를 보세요.
원본 PNG를 유지하고 Python 그림 재생성기는 제거했습니다. 같은 이름의 PNG를 교체해 그림을 바꿀 수 있습니다.
