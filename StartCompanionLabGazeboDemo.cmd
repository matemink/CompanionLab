@echo off
setlocal

start "Gazebo Iris World" wsl.exe -d Ubuntu-24.04 --cd "%~dp0" -- bash scripts/run_gazebo_iris.sh
timeout /t 4 /nobreak >nul
start "ArduCopter Gazebo SITL" wsl.exe -d Ubuntu-24.04 --cd "%~dp0" -- bash scripts/run_arducopter_gazebo.sh
timeout /t 4 /nobreak >nul
start "CompanionLab" wsl.exe -d Ubuntu-24.04 --cd "%~dp0" -- env COMPANIONLAB_INTERACTIVE=1 bash scripts/run_companionlab_sitl.sh
