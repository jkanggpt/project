# Python 없이 같은 시뮬레이터를 직접 호출하는 C 밸런스 도구입니다.
add_executable(castle_balance balance.c)
target_include_directories(castle_balance PRIVATE ${PROJECT_SOURCE_DIR}/core/src)
target_link_libraries(castle_balance PRIVATE castle_cli_support castle_frontend castle_warnings)
