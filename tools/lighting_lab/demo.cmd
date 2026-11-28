@echo off
chcp 65001 >nul
cd /d "%~dp0"
castle_gpu_lab.exe --scene natural-slice --pixel-scale 3 %*
if errorlevel 1 (
  echo 실행하지 못했습니다. OpenGL 4.3 이상을 지원하는 그래픽 드라이버를 확인하세요.
  pause
)
