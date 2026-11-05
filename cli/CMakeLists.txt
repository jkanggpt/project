# castle_cli — 공개 API 만 써서 진짜 게임을 돌리는 콘솔 프런트엔드 (선생님 시연, 테스트, 밸런스 시뮬레이터)
add_executable(castle_cli
  main.c
  commands.c
  console.c
  format.c
  sim.c
)
target_link_libraries(castle_cli PRIVATE castle_core_static castle_warnings)
if(MINGW)
  # 선생님 컴퓨터에서 MinGW DLL 없이 실행되도록
  target_link_options(castle_cli PRIVATE -static)
endif()
