@echo off
setlocal

start "Gazebo AprilTag World" wsl.exe -d Ubuntu-24.04 --cd "%~dp0" -- bash scripts/run_gazebo_apriltag.sh
timeout /t 4 /nobreak >nul
start "ArduCopter Gazebo SITL" wsl.exe -d Ubuntu-24.04 --cd "%~dp0" -- bash scripts/run_arducopter_gazebo.sh
timeout /t 4 /nobreak >nul
start "OnboardAutonomy Vision" wsl.exe -d Ubuntu-24.04 --cd "%~dp0" -- env ONBOARD_AUTONOMY_INTERACTIVE=1 bash scripts/run_onboard_autonomy_gazebo_vision.sh
