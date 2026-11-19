# 단위 테스트: castle_tests <테스트이름> 으로 하나씩 실행하고, CTest 가 test_list.h 의 이름을 모두 등록합니다.
add_executable(castle_tests
  test_main.c
  test_basics.c
  test_grid.c
  test_world.c
  test_save.c
  test_api.c
  test_colony.c
  test_town.c
  test_pressure.c
  test_threat.c
  test_progress.c
  test_expansion.c
  test_v111.c
)
target_link_libraries(castle_tests PRIVATE castle_core_testing castle_warnings)

add_executable(castle_native_tests test_native.c)
target_include_directories(castle_native_tests PRIVATE ${PROJECT_SOURCE_DIR}/core/src)
target_link_libraries(castle_native_tests PRIVATE castle_frontend castle_cli_support castle_warnings)
add_test(NAME native_runtime COMMAND castle_native_tests ${PROJECT_SOURCE_DIR}/game/data)
set_tests_properties(native_runtime PROPERTIES WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

file(STRINGS test_list.h CASTLE_TEST_LINES REGEX "^TEST_ENTRY\\(")
foreach(line IN LISTS CASTLE_TEST_LINES)
  string(REGEX REPLACE "^TEST_ENTRY\\(([a-z0-9_]+)\\).*" "\\1" test_name "${line}")
  add_test(NAME ${test_name} COMMAND castle_tests ${test_name})
  set_tests_properties(${test_name} PROPERTIES WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
endforeach()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS test_list.h)

# CLI 테스트 (CLI 를 빌드할 때만)
if(TARGET castle_cli)
  # 잘못된 입력에 다시 묻는지, 저장/불러오기 메뉴가 동작하는지 입력 파일로 확인합니다
  add_test(NAME cli_menu_script
    COMMAND ${CMAKE_COMMAND}
      -DCLI=$<TARGET_FILE:castle_cli>
      -DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/cli_menu_input.txt
      -DDATA=${PROJECT_SOURCE_DIR}/game/data
      -P ${CMAKE_CURRENT_SOURCE_DIR}/run_cli_script.cmake
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
  add_test(NAME cli_simulate COMMAND castle_cli --simulate 20 --seed 7 --bot reasonable --out sim_test.csv
    --data ${PROJECT_SOURCE_DIR}/game/data)
  add_test(NAME cli_soak_short COMMAND castle_cli --soak 12 --seeds 3 --data ${PROJECT_SOURCE_DIR}/game/data)
  add_test(NAME cli_bad_arguments COMMAND castle_cli --soak abc)
  set_tests_properties(cli_bad_arguments PROPERTIES WILL_FAIL TRUE)
  set_tests_properties(cli_simulate cli_soak_short cli_bad_arguments PROPERTIES
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
endif()
