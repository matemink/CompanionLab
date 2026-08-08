@echo off
setlocal

title OnboardAutonomy Demo Launcher

start "Gazebo Simulation Server" /min wsl.exe -d Ubuntu-24.04 --cd "%~dp0" -- env ONBOARD_AUTONOMY_GAZEBO_HEADLESS=1 bash scripts/run_gazebo_apriltag.sh
timeout /t 3 /nobreak >nul
start "Gazebo AprilTag World" wsl.exe -d Ubuntu-24.04 --cd "%~dp0" -- bash scripts/run_gazebo_gui.sh
timeout /t 4 /nobreak >nul
start "ArduCopter Gazebo SITL" wsl.exe -d Ubuntu-24.04 --cd "%~dp0" -- bash scripts/run_arducopter_gazebo.sh
timeout /t 4 /nobreak >nul
start "OnboardAutonomy Console" wsl.exe -d Ubuntu-24.04 --cd "%~dp0" -- env ONBOARD_AUTONOMY_AUTONOMOUS=1 ONBOARD_AUTONOMY_INTERACTIVE=1 bash scripts/run_onboard_autonomy_gazebo_vision.sh

echo Waiting for the first camera frame...
powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "$deadline = (Get-Date).AddSeconds(30); while ((Get-Date) -lt $deadline) { try { $response = Invoke-WebRequest -UseBasicParsing -Uri 'http://localhost:8080/api/frame' -TimeoutSec 1; if ($response.StatusCode -eq 200) { Start-Process 'http://localhost:8080/'; exit 0 } } catch {}; Start-Sleep -Milliseconds 500 }; exit 1"

if errorlevel 1 (
    echo Camera preview did not become available at http://localhost:8080/
    echo Check the OnboardAutonomy Console window for details.
    pause
)
