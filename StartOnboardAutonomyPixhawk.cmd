@echo off
setlocal

if exist "%~dp0OnboardAutonomyLocal.cmd" call "%~dp0OnboardAutonomyLocal.cmd"
if exist "%~dp0CompanionLabLocal.cmd" call "%~dp0CompanionLabLocal.cmd"

if not defined ONBOARD_AUTONOMY_PI_HOST if defined COMPANIONLAB_PI_HOST set "ONBOARD_AUTONOMY_PI_HOST=%COMPANIONLAB_PI_HOST%"
if not defined ONBOARD_AUTONOMY_PI_USER if defined COMPANIONLAB_PI_USER set "ONBOARD_AUTONOMY_PI_USER=%COMPANIONLAB_PI_USER%"
if not defined ONBOARD_AUTONOMY_SSH_KEY if defined COMPANIONLAB_SSH_KEY set "ONBOARD_AUTONOMY_SSH_KEY=%COMPANIONLAB_SSH_KEY%"
if not defined ONBOARD_AUTONOMY_REMOTE_ROOT if defined COMPANIONLAB_REMOTE_ROOT set "ONBOARD_AUTONOMY_REMOTE_ROOT=%COMPANIONLAB_REMOTE_ROOT%"
if not defined ONBOARD_AUTONOMY_SERIAL if defined COMPANIONLAB_SERIAL set "ONBOARD_AUTONOMY_SERIAL=%COMPANIONLAB_SERIAL%"

if not defined ONBOARD_AUTONOMY_PI_HOST set "ONBOARD_AUTONOMY_PI_HOST=companionpi.local"
if not defined ONBOARD_AUTONOMY_PI_USER set "ONBOARD_AUTONOMY_PI_USER=companion"
if not defined ONBOARD_AUTONOMY_SSH_KEY set "ONBOARD_AUTONOMY_SSH_KEY=%USERPROFILE%\.ssh\onboard_autonomy_ed25519"
if not defined ONBOARD_AUTONOMY_REMOTE_ROOT set "ONBOARD_AUTONOMY_REMOTE_ROOT=/home/%ONBOARD_AUTONOMY_PI_USER%/onboard_autonomy-pi5"

start "OnboardAutonomy - Raspberry Pi 5 and Pixhawk 6C" ssh.exe ^
  -t ^
  -i "%ONBOARD_AUTONOMY_SSH_KEY%" ^
  -o StrictHostKeyChecking=accept-new ^
  %ONBOARD_AUTONOMY_PI_USER%@%ONBOARD_AUTONOMY_PI_HOST% ^
  "if [ -x '%ONBOARD_AUTONOMY_REMOTE_ROOT%/bin/run_onboard_autonomy_pi.sh' ]; then env ONBOARD_AUTONOMY_SERIAL='%ONBOARD_AUTONOMY_SERIAL%' '%ONBOARD_AUTONOMY_REMOTE_ROOT%/bin/run_onboard_autonomy_pi.sh'; else env COMPANIONLAB_SERIAL='%ONBOARD_AUTONOMY_SERIAL%' '%ONBOARD_AUTONOMY_REMOTE_ROOT%/bin/run_companionlab_pi.sh'; fi"

timeout /t 3 /nobreak >nul
start "" "http://%ONBOARD_AUTONOMY_PI_HOST%:8080/"
