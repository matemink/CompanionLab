@echo off
setlocal

start "ArduCopter SITL" wsl.exe -d Ubuntu-24.04 --cd "%~dp0" -- bash scripts/run_arducopter_sitl.sh
timeout /t 5 /nobreak >nul
start "CompanionLab" wsl.exe -d Ubuntu-24.04 --cd "%~dp0" -- bash scripts/run_companionlab_sitl.sh
