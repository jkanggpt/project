@echo off
chcp 65001 >nul
cd /d "%~dp0"
castle_gpu_lab.exe --check-natural
if errorlevel 1 goto finish
castle_gpu_lab.exe --check-shadow-cut
:finish
pause
