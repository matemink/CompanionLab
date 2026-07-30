@echo off
setlocal

if not defined COMPANIONLAB_PI_HOST set "COMPANIONLAB_PI_HOST=companionpi.local"
if not defined COMPANIONLAB_PI_USER set "COMPANIONLAB_PI_USER=companion"
if not defined COMPANIONLAB_SSH_KEY set "COMPANIONLAB_SSH_KEY=%USERPROFILE%\.ssh\companionlab_ed25519"

if exist "%~dp0CompanionLabLocal.cmd" call "%~dp0CompanionLabLocal.cmd"

if not defined COMPANIONLAB_REMOTE_ROOT set "COMPANIONLAB_REMOTE_ROOT=/home/%COMPANIONLAB_PI_USER%/companionlab-pi5"

start "CompanionLab - Raspberry Pi 5 and Pixhawk 6C" ssh.exe ^
  -t ^
  -i "%COMPANIONLAB_SSH_KEY%" ^
  -o StrictHostKeyChecking=accept-new ^
  %COMPANIONLAB_PI_USER%@%COMPANIONLAB_PI_HOST% ^
  "env COMPANIONLAB_SERIAL=%COMPANIONLAB_SERIAL% %COMPANIONLAB_REMOTE_ROOT%/bin/run_companionlab_pi.sh"

timeout /t 3 /nobreak >nul
start "" "http://%COMPANIONLAB_PI_HOST%:8080/"
