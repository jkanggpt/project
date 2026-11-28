@echo off
chcp 65001 >nul
cd /d "%~dp0"
castle_gpu_lab.exe --scene shadow-cut --pixel-scale 3 --azimuth 225 --elevation 40 %*
if errorlevel 1 pause
