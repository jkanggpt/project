# castle_core — 게임 규칙과 상태를 가진 C11 라이브러리
set(CASTLE_CORE_SOURCES
  src/api.c
  src/clock.c
  src/defs.c
  src/defs_builtin.c
  src/economy.c
  src/entity.c
  src/events.c
  src/grid.c
  src/jobs.c
  src/market.c
  src/mem.c
  src/nature.c
  src/placement.c
  src/platform.c
  src/population.c
  src/rng.c
  src/save.c
  src/text.c
  src/validate.c
  src/villager_ai.c
  src/world.c
)

# 플랫폼별 폴더 이름: Godot의 CoreBridge가 res://bin/<이 이름>/ 에서 라이브러리를 찾습니다.
if(WIN32)
  set(CASTLE_GODOT_PLATFORM windows-x64)
elseif(APPLE)
  set(CASTLE_GODOT_PLATFORM macos)
else()
  set(CASTLE_GODOT_PLATFORM linux-x64)
endif()

# 1) Godot이 불러 쓰는 공유 라이브러리 (castle_core.dll / libcastle_core.so)
add_library(castle_core SHARED ${CASTLE_CORE_SOURCES})
target_include_directories(castle_core PUBLIC include PRIVATE src)
target_compile_definitions(castle_core PRIVATE CASTLE_CORE_BUILD)
target_link_libraries(castle_core PRIVATE castle_warnings)
set_target_properties(castle_core PROPERTIES C_VISIBILITY_PRESET hidden)
if(WIN32)
  # MinGW도 MSVC와 같은 이름(castle_core.dll)을 쓰게 합니다.
  set_target_properties(castle_core PROPERTIES PREFIX "")
endif()
if(MINGW)
  # libgcc DLL 없이도 로드되도록 정적으로 링크합니다.
  target_link_options(castle_core PRIVATE -static-libgcc)
endif()

if(CASTLE_COPY_TO_GODOT)
  set(CASTLE_GODOT_BIN_DIR ${PROJECT_SOURCE_DIR}/game/bin/${CASTLE_GODOT_PLATFORM})
  add_custom_command(TARGET castle_core POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CASTLE_GODOT_BIN_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:castle_core> ${CASTLE_GODOT_BIN_DIR}/
    COMMENT "castle_core를 ${CASTLE_GODOT_BIN_DIR} 에 복사합니다 (게임이 실행 중이면 실패합니다)")
endif()

# 2) CLI가 링크하는 정적 라이브러리 (공개 API만 사용)
add_library(castle_core_static STATIC ${CASTLE_CORE_SOURCES})
target_include_directories(castle_core_static PUBLIC include PRIVATE src)
target_compile_definitions(castle_core_static PUBLIC CASTLE_CORE_STATIC)
target_link_libraries(castle_core_static PRIVATE castle_warnings PUBLIC castle_sanitize)

# 3) 테스트용 정적 라이브러리: 내부 헤더를 열어 주고, 메모리 할당 실패 주입을 켭니다.
if(CASTLE_BUILD_TESTS)
  add_library(castle_core_testing STATIC ${CASTLE_CORE_SOURCES})
  target_include_directories(castle_core_testing PUBLIC include src)
  target_compile_definitions(castle_core_testing PUBLIC CASTLE_CORE_STATIC CASTLE_TESTING)
  target_link_libraries(castle_core_testing PRIVATE castle_warnings PUBLIC castle_sanitize)
endif()
