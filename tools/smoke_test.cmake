# cmake -DGAME=<castle_game> -DDAYS=20 -P tools/smoke_test.cmake
if(NOT DEFINED GAME)
  message(FATAL_ERROR "-DGAME=castle_game의 절대 경로를 지정하세요")
endif()
if(NOT DEFINED DAYS)
  set(DAYS 20)
endif()
execute_process(COMMAND "${GAME}" --smoke-test --days "${DAYS}" --seed 42
  RESULT_VARIABLE result TIMEOUT 600 ENCODING UTF-8)
if(NOT result EQUAL 0)
  message(FATAL_ERROR "C 게임 스모크 테스트 실패: ${result}")
endif()
