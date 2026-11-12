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
    "0부터 20 사이의 숫자를 입력하세요"
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
    "연구는 마탑에서 일하는 마법사가"
    "윤작"
    "다음 등급: 마을"
    "아직 승급할 수 없습니다"
    "저장: 성공했습니다"
    "'cli_test.sav' 에서 불러왔습니다"
    "불러오기 실패: 파일을 열 수 없습니다"
    "안녕히 가세요")
  string(FIND "${output}" "${expected}" position)
  if(position EQUAL -1)
    message(FATAL_ERROR "expected text not found: ${expected}\n--- output ---\n${output}\n${errors}")
  endif()
endforeach()

# 두 번째 실행: 저장 도중 꺼진 것처럼 본 파일의 뒤쪽 절반을 잘라 내면, 불러오기가 예비 파일(.bak)을 제안해야 합니다
file(READ cli_test.sav whole)
string(LENGTH "${whole}" whole_length)
math(EXPR half "${whole_length} / 2")
string(SUBSTRING "${whole}" 0 ${half} broken)
file(WRITE cli_test.sav "${broken}")
file(WRITE cli_backup_input.txt "13\ncli_test.sav\n1\n0\n")
execute_process(
  COMMAND ${CLI} --data ${DATA}
  INPUT_FILE cli_backup_input.txt
  OUTPUT_VARIABLE output
  ERROR_VARIABLE errors
  RESULT_VARIABLE result)
if(NOT result EQUAL 0)
  message(FATAL_ERROR "castle_cli (backup run) exited with ${result}\n${output}\n${errors}")
endif()
foreach(expected IN ITEMS
    "불러오기 실패: 세이브 파일이 손상되었습니다"
    "문제가 있는 줄"
    "예비 파일(.bak, 그 전에 저장한 것)은 불러올 수 있습니다"
    "바람언덕 — 1년"
    "'cli_test.sav.bak' 에서 불러왔습니다"
    "=== 바람언덕 ===")
  string(FIND "${output}" "${expected}" position)
  if(position EQUAL -1)
    message(FATAL_ERROR "expected text not found (backup run): ${expected}\n--- output ---\n${output}\n${errors}")
  endif()
endforeach()
file(REMOVE cli_test.sav cli_test.sav.bak cli_backup_input.txt)
message(STATUS "CLI menu script OK")
