@echo off
chcp 65001 >nul
cd /d "%~dp0"
castle_game.exe --gpu-transitions 3 --no-audio --width 800 --height 600 > graphics-check.log 2>&1
if errorlevel 1 (
  echo 그래픽 전환 검사에 실패했습니다. graphics-check.log를 확인하세요.
) else (
  echo 새 게임·튜토리얼·이어하기·불러오기와 제목 화면 반복 검사를 통과했습니다.
)
pause
