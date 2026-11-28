# 마법성채 키우기 — 최상위 CMake 설정
# C 코어, 그래픽 게임, CLI, 밸런스 도구와 테스트를 빌드합니다.
cmake_minimum_required(VERSION 3.21)
project(MagicCastle VERSION 1.2.4 LANGUAGES C)

option(CASTLE_SANITIZE "AddressSanitizer + UBSan + LeakSanitizer (GCC/Clang 전용)" OFF)
option(CASTLE_WERROR "경고를 오류로 처리 (CI에서 켭니다)" OFF)
option(CASTLE_BUILD_TESTS "단위 테스트 빌드" ON)
option(CASTLE_BUILD_CLI "콘솔 프런트엔드(castle_cli) 빌드" ON)
option(CASTLE_BUILD_GAME "raylib 기반 C 그래픽 게임 빌드" ON)
option(CASTLE_GPU_TESTS "OpenGL 4.3 창·컴퓨트가 있는 환경의 GPU 검사" OFF)
option(CASTLE_BUILD_TOOLS "C 밸런스 도구 빌드" ON)
option(CASTLE_BUILD_SHARED "외부 프로그램용 C 공유 라이브러리 빌드" OFF)

# 지원 환경과 포인터 크기를 64비트로 통일합니다.
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
  message(FATAL_ERROR "castle_core는 64비트로만 빌드합니다. (Visual Studio라면 -A x64 를 쓰세요)")
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# MSVC: C 런타임을 정적으로 링크해서 DLL 하나만 복사하면 되게 합니다.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# 실행 파일과 DLL을 한 폴더에 모읍니다. (Windows는 DLL이 실행 파일 옆에 있어야 함)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# 모든 C 타깃이 공유하는 경고 설정
add_library(castle_warnings INTERFACE)
if(MSVC)
  target_compile_options(castle_warnings INTERFACE /W4 /utf-8 $<$<BOOL:${CASTLE_WERROR}>:/WX>)
  target_compile_definitions(castle_warnings INTERFACE _CRT_SECURE_NO_WARNINGS)
else()
  target_compile_options(castle_warnings INTERFACE
    -Wall -Wextra -Wpedantic -Wshadow -Wstrict-prototypes -Wmissing-prototypes
    -Wformat=2 -Wundef -Wvla
    $<$<BOOL:${CASTLE_WERROR}>:-Werror>)
endif()

# 새니타이저는 정적 코어를 연결한 실행 파일과 테스트에 적용합니다.
add_library(castle_sanitize INTERFACE)
if(CASTLE_SANITIZE)
  if(MSVC)
    message(WARNING "CASTLE_SANITIZE는 GCC/Clang에서만 지원합니다. 무시합니다.")
  else()
    target_compile_options(castle_sanitize INTERFACE
      -fsanitize=address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer)
    target_link_options(castle_sanitize INTERFACE -fsanitize=address,undefined)
  endif()
endif()

include_directories(BEFORE "${PROJECT_SOURCE_DIR}/core/include")
add_subdirectory(core)

if(CASTLE_BUILD_CLI OR CASTLE_BUILD_TOOLS OR CASTLE_BUILD_GAME OR CASTLE_BUILD_TESTS)
  add_subdirectory(cli)
endif()

if(CASTLE_BUILD_TESTS)
  enable_testing()
endif()

include(cmake/Assets.cmake)
add_subdirectory(game)
if(CASTLE_BUILD_TOOLS)
  add_subdirectory(tools)
endif()

if(CASTLE_BUILD_TESTS)
  add_subdirectory(tests)
endif()

# 실행 파일들이 한 작업에서 현재 정의 6파일을 복사합니다.
# 같은 출력 폴더를 공유하는 병렬 타깃 사이의 복사 충돌을 피합니다.
set(CASTLE_DATA_CONSUMERS)
foreach(consumer castle_cli castle_game castle_balance castle_expansion_balance)
  if(TARGET ${consumer})
    list(APPEND CASTLE_DATA_CONSUMERS ${consumer})
  endif()
endforeach()
if(CASTLE_DATA_CONSUMERS)
  add_custom_target(castle_runtime_data ALL)
  foreach(consumer IN LISTS CASTLE_DATA_CONSUMERS)
    add_custom_command(TARGET castle_runtime_data POST_BUILD
      COMMAND ${CMAKE_COMMAND} "-DSOURCE=${PROJECT_SOURCE_DIR}/game/data"
        "-DDESTINATION=$<TARGET_FILE_DIR:${consumer}>/data"
        -P "${PROJECT_SOURCE_DIR}/cmake/CopyData.cmake"
      VERBATIM)
    add_dependencies(${consumer} castle_runtime_data)
  endforeach()
endif()
