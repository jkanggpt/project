# Python 없이 같은 시뮬레이터를 직접 호출하는 C 밸런스 도구입니다.
add_executable(castle_balance balance.c)
add_executable(castle_expansion_balance expansion_balance.c)
target_include_directories(castle_expansion_balance PRIVATE ${PROJECT_SOURCE_DIR}/core/src)
target_link_libraries(castle_expansion_balance PRIVATE castle_cli_support castle_frontend castle_warnings)
if(CASTLE_BUILD_TESTS)
  add_test(NAME expansion_bot_closes_region COMMAND castle_expansion_balance --days 120 --seeds 1
    --seed 8 --difficulty 0 --require-protection-by 100 --data "${PROJECT_SOURCE_DIR}/game/data"
    --out "${CMAKE_CURRENT_BINARY_DIR}/fortification_positive")
  set_tests_properties(expansion_bot_closes_region PROPERTIES TIMEOUT 300)
  add_test(NAME expansion_bot_rejects_unprotected COMMAND castle_expansion_balance --days 1 --seeds 1
    --seed 8 --difficulty 0 --require-protection-by 1 --data "${PROJECT_SOURCE_DIR}/game/data"
    --out "${CMAKE_CURRENT_BINARY_DIR}/fortification_negative")
  set_tests_properties(expansion_bot_rejects_unprotected PROPERTIES WILL_FAIL TRUE)
endif()
target_include_directories(castle_balance PRIVATE ${PROJECT_SOURCE_DIR}/core/src)
target_link_libraries(castle_balance PRIVATE castle_cli_support castle_frontend castle_warnings)

if(TARGET raylib)
  add_executable(castle_art pack_art.c)
  target_link_libraries(castle_art PRIVATE castle_frontend raylib castle_warnings)
  add_executable(castle_expansion_art pack_expansion_art.c)
  target_link_libraries(castle_expansion_art PRIVATE castle_frontend raylib castle_warnings)
  if(CASTLE_BUILD_TESTS)
    add_test(NAME native_art_assets COMMAND castle_art --check "${PROJECT_SOURCE_DIR}/game/assets/painted")
    add_test(NAME expansion_art_assets COMMAND castle_expansion_art --check "${PROJECT_SOURCE_DIR}/game/assets/expansion")
  endif()
endif()
