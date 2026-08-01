@echo off
setlocal

start "ArduCopter SITL" wsl.exe -d Ubuntu-24.04 --cd "%~dp0" -- bash scripts/run_arducopter_sitl.sh
timeout /t 5 /nobreak >nul
start "OnboardAutonomy" wsl.exe -d Ubuntu-24.04 --cd "%~dp0" -- bash scripts/run_onboard_autonomy_sitl.sh
