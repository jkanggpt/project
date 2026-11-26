# 빌드와 검증

64비트 C11 컴파일러와 CMake3.21 이상을 준비합니다. Windows는 Visual Studio2019 이상 C++ 데스크톱 구성요소 또는 MinGW-w64/Ninja를 사용합니다. 게임 실행에는 추가 개발 도구가 필요하지 않습니다. raylib5.5 소스는 third_party/raylib-5.5.tar.gz에 포함하며 CMake가 SHA256을 검증합니다.

## MSVC

리포지터리 루트에서 실행합니다.

~~~powershell
cmake --preset windows-msvc -DCASTLE_WERROR=ON -DCASTLE_BUILD_SHARED=ON
cmake --build --preset windows-msvc --parallel 4
ctest --preset windows-msvc --parallel 4
cmake --install build/windows-msvc --config Release --prefix build/release/MagicCastle --component Runtime
build/release/MagicCastle/castle_game.exe
~~~

빈 경로를 직접 지정할 수도 있습니다. 한글과 공백 경로는 따옴표로 감쌉니다.

~~~powershell
cmake -S . -B "build/독립 빌드" -G "Visual Studio 16 2019" -A x64 -DCASTLE_WERROR=ON
cmake --build "build/독립 빌드" --config Release --parallel 4
ctest --test-dir "build/독립 빌드" -C Release --output-on-failure --parallel 4
~~~

## MinGW와 Linux

Windows MinGW는 windows-mingw 프리셋을 사용합니다. 한글 경로에서 도구 제약이 있으면 tools/build_windows.ps1의 임시 드라이브 매핑을 사용합니다. Linux는 linux-debug 또는 linux-clang-debug 프리셋이 코어·공용 실행기·콘솔 회귀를, linux-release가 그래픽판을 포함합니다. 그래픽 빌드는 운영체제의 X11/OpenGL/오디오 개발 패키지가 필요합니다. 이 작업의 실제 플랫폼 확인 범위는 CLEANUP_AUDIT.md를 확인하세요.

## 아트 재생성

현재 실행 파일 경로를 사용합니다. 원본을 덮어쓰지 않고 별도 작업/발행 폴더로 검증합니다.

~~~powershell
build/windows-msvc/bin/Release/castle_art_pipeline.exe --sources art/sources --work build/art-work --output build/art-regenerated
build/windows-msvc/bin/Release/castle_assets_check.exe build/art-regenerated
~~~

CTest의 assets_regenerate와 assets_reproducible도 같은 흐름입니다. 런타임 복사·설치는 단일 매니페스트 파일만 사용합니다. 원화·이전 문서·검사 출력은 설치하지 않습니다.

## 실행 검사

~~~powershell
build/windows-msvc/bin/Release/castle_cli.exe --simulate 60 --seed 7 --bot reasonable --out build/reports/simulation.csv --data game/data
build/windows-msvc/bin/Release/castle_cli.exe --soak 400 --seeds 20 --data game/data
build/windows-msvc/bin/Release/castle_expansion_balance.exe --days 200 --seeds 48 --difficulty 1 --data game/data --out build/reports/balance
build/windows-msvc/bin/Release/castle_game.exe --help
~~~

콘솔은 같은 GameSession을 통해 실제 이동·전투를 진행합니다. 검사는 사용자 saves와 분리된 경로를 사용합니다. 게임이나 검사 프로세스가 해당 실행 파일을 사용하는 동안 다시 링크하지 않습니다.

콘솔만 필요하면 CASTLE_BUILD_GAME=OFF, CASTLE_BUILD_TOOLS=OFF로 구성할 수 있습니다. 공용 castle_runtime_data 타깃이 현재 정의 6파일을 필요한 실행 폴더에 순서대로 복사합니다. 콘솔의 기본 경로도 실행 파일 옆 data이며 작업 디렉터리에 의존하지 않습니다.

정리 원장과 설치 파일 대조는 tools/audit_project.ps1을 사용합니다. 성능 비교는 tools/compare_performance.ps1 -Before <기준exe> -After <현재exe> -Output <build/reports/실행ID>로 수행합니다. 예산 미달의 passed=0과 실제 실행 오류를 구별하며, 전후 표본과 동일 실행 파일 대조를 보존합니다.
