# castle_core — 게임 규칙과 상태를 가진 C11 라이브러리
set(CASTLE_CORE_SOURCES
  src/common/api.c
  src/common/clock.c
  src/common/definitions.c
  src/common/debug.c
  src/common/test_tools.c
  src/common/builtin_definitions.c
  src/common/difficulty.c
  src/economy/bank.c
  src/buildings/catalog.c
  src/world/content.c
  src/buildings/commands.c
  src/people/routines.c
  src/people/work.c
  src/services/capacity.c
  src/economy/trade_rules.c
  src/people/identity.c
  src/people/consumption.c
  src/combat/defense.c
  src/persistence/runtime_state.c
  src/buildings/serials.c
  src/buildings/facilities.c
  src/people/schedule.c
  src/services/water.c
  src/world/clay.c
  src/services/healing.c
  src/people/departures.c
  src/people/hunting.c
  src/common/interface_rules.c
  src/ecology/species.c
  src/services/lighthouse.c
  src/people/immigration.c
  src/economy/harvest.c
  src/services/medical.c
  src/world/generator.c
  src/world/resource_budget.c
  src/economy/wandering_merchant.c
  src/services/forecast.c
  src/services/fishing_boats.c
  src/buildings/requirements.c
    src/combat/raids.c
  src/people/wellbeing.c
  src/ecology/population.c
  src/common/debug_town.c
  src/buildings/storage_network.c
  src/economy/production.c
  src/buildings/progression.c
  src/economy/prices.c
  src/ecology/livestock.c
  src/persistence/content_save.c
  src/common/entities.c
  src/common/events.c
  src/world/grid.c
  src/people/assignment.c
  src/economy/market.c
  src/common/memory.c
  src/world/weather.c
  src/buildings/placement.c
  src/buildings/rank.c
  src/services/research.c
  src/common/platform.c
  src/people/population.c
  src/common/rng.c
  src/persistence/save.c
  src/common/text.c
  src/combat/threat.c
  src/common/tutorial.c
  src/common/validate.c
  src/people/intents.c
  src/world/state.c
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
