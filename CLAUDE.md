# 마법성채 키우기 — C 실행판 개발 안내

먼저 docs/SPEC.md, docs/NATIVE_PORT.md, PROGRESS.md를 읽습니다.
현재 게임은 C11 + raylib이며 Godot/.NET/Python을 사용하지 않습니다.
main에 푸시하거나 PR을 병합하지 않습니다. 한국어 UI·문서·주석, 영어 식별자를 사용합니다.

## Commands

저장소 루트에서 실행합니다.

- Windows MSVC: `cmake --preset windows-msvc -DCASTLE_WERROR=ON`, `cmake --build --preset windows-msvc`, `ctest --preset windows-msvc`
- Windows MinGW (한글 경로 포함): `./tools/build_windows.ps1`
- Linux C 검사: `cmake --preset linux-debug -DCASTLE_WERROR=ON`, `cmake --build --preset linux-debug`, `ctest --preset linux-debug -j8`
- Linux 그래픽: `cmake --preset linux-release -DCASTLE_WERROR=ON`, `cmake --build --preset linux-release`
- 자동 플레이: `<bin>/castle_game --smoke-test --days 20 --seed 42`
- 긴 자동 플레이: `<bin>/castle_game --smoke-test --days 60 --seed 42`
- 성능: `<bin>/castle_game --stress` 또는 `--stress-probe --no-audio`
- 화면: `<bin>/castle_game --screenshot artifacts/title.png --screen title --no-audio`
- 전체 촬영: `cmake -DGAME=<실행파일 절대경로> -DOUT=<출력폴더> -P tools/screenshots.cmake`
- 소크: `<bin>/castle_cli --soak 400 --seeds 20`
- 밸런스: `<bin>/castle_balance --data game/data --seeds 16 --days 100`
- 배포: `cmake --install <빌드폴더> --prefix <배포폴더>`

CMake 설정은 CASTLE_BUILD_GAME=OFF로 외부 그래픽 의존성 없이 빌드할 수 있습니다.
MSVC 실행 파일은 bin/Release, 단일 구성 MinGW/Linux는 bin에 있습니다.
최신 결과와 한계는 PROGRESS.md에 남깁니다.
