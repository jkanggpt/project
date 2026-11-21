# castle_core — 게임 규칙과 상태를 가진 C11 라이브러리
set(CASTLE_CORE_SOURCES
  src/api.c
  src/clock.c
  src/defs.c
  src/debug.c
  src/defs_builtin.c
  src/difficulty.c
  src/economy.c
  src/expansion_catalog.c
  src/expansion_world.c
  src/expansion_commands.c
  src/expansion_people.c
  src/expansion_v121.c
  src/expansion_v122.c
    src/expansion_raids.c
  src/expansion_wellbeing.c
  src/expansion_ecology.c
  src/expansion_debug.c
  src/expansion_hubs.c
  src/expansion_economy.c
  src/expansion_progress.c
  src/expansion_prices.c
  src/expansion_animals.c
  src/expansion_save.c
  src/entity.c
  src/events.c
  src/grid.c
  src/jobs.c
  src/market.c
  src/mem.c
  src/nature.c
  src/placement.c
  src/rank.c
  src/research.c
  src/platform.c
  src/population.c
  src/rng.c
  src/save.c
  src/text.c
  src/threat.c
  src/tutorial.c
  src/validate.c
  src/villager_ai.c
  src/world.c
)

# 공유 라이브러리는 선택 사항이며 C 게임은 정적 코어를 직접 연결합니다.
if(CASTLE_BUILD_SHARED)
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

endif()

# 게임과 CLI가 링크하는 정적 라이브러리 (공개 API만 사용)
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
