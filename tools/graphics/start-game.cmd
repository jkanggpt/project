@echo off
chcp 65001 >nul
cd /d "%~dp0"
castle_game.exe %*
if errorlevel 1 (
  echo 게임을 실행하지 못했습니다. assets와 data 폴더, OpenGL 4.3 이상 그래픽 드라이버를 확인하세요.
  pause
)
