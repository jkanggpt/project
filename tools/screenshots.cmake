# cmake -DGAME=<castle_game> -DOUT=<폴더> -P tools/screenshots.cmake
if(NOT DEFINED GAME OR NOT DEFINED OUT)
  message(FATAL_ERROR "GAME 실행 파일과 OUT 출력 폴더를 지정하세요")
endif()
file(MAKE_DIRECTORY "${OUT}")
foreach(screen title game villagers research rank market load)
  execute_process(COMMAND "${GAME}" --screenshot "${OUT}/${screen}.png" --screen "${screen}" --no-audio
    RESULT_VARIABLE result TIMEOUT 60 ENCODING UTF-8)
  if(NOT result EQUAL 0)
    message(FATAL_ERROR "C 게임 스크린숏 실패: ${screen} (${result})")
  endif()
endforeach()
