@echo off
chcp 65001 >nul
cd /d "%~dp0"
rem 실제 게임 렌더러로 조명·재료·구름·물·등불을 검수합니다. 저장 슬롯에 기록하지 않습니다.
castle_game.exe --lighting-review %*
if errorlevel 1 (
  echo 조명 검수 장면을 열지 못했습니다. assets와 data 폴더, OpenGL 4.3 이상 그래픽 드라이버를 확인하세요.
  pause
)
