# castle_cli 에 입력 파일을 넣어 대화형 메뉴를 끝까지 돌리고, 출력에 기대한 문장이 있는지 확인합니다.
# 사용: cmake -DCLI=<castle_cli> -DINPUT=<입력 파일> -P run_cli_script.cmake
file(REMOVE cli_test.sav cli_test.sav.bak)
execute_process(
  COMMAND ${CLI} --data ${DATA}
  INPUT_FILE ${INPUT}
  OUTPUT_VARIABLE output
  ERROR_VARIABLE errors
  RESULT_VARIABLE result)
if(NOT result EQUAL 0)
  message(FATAL_ERROR "castle_cli exited with ${result}\n${output}\n${errors}")
endif()
foreach(expected IN ITEMS
    "숫자를 입력하세요"
    "0부터 18 사이의 숫자를 입력하세요"
    "먼저 새 게임을 시작하거나 불러오세요"
    "0부터 2147483647 사이의 숫자를 입력하세요"
    "0부터 2 사이의 숫자를 입력하세요"
    "새 게임을 시작했습니다"
    "=== 바람언덕 ==="
    "밤이 되었습니다"
    "2일째가 시작되었습니다"
    "주택(45, 45)을 짓기 시작했습니다"
    "건물: K 성채"
    "번호  이름"
    "번호  종류"
    "어제 하루 장부"
    "지금은 거래할 수 없음"
    "다 지은 시장이 없습니다"
    "지금은 떠돌이 상인이 없습니다"
    "고칠 수 없습니다: 고칠 곳이 없습니다"
    "저장: 성공했습니다"
    "'cli_test.sav' 에서 불러왔습니다"
    "불러오기 실패: 파일을 열 수 없습니다"
    "안녕히 가세요")
  string(FIND "${output}" "${expected}" position)
  if(position EQUAL -1)
    message(FATAL_ERROR "expected text not found: ${expected}\n--- output ---\n${output}\n${errors}")
  endif()
endforeach()
file(REMOVE cli_test.sav cli_test.sav.bak)
message(STATUS "CLI menu script OK")
