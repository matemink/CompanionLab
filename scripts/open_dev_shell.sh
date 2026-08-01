#!/usr/bin/env bash

set -euo pipefail

printf 'OnboardAutonomy development environment\n\n'
printf 'User: '
whoami
printf 'Architecture: '
uname -m
printf 'Ubuntu: '
. /etc/os-release
printf '%s\n' "${PRETTY_NAME}"
printf 'GCC: '
g++ -dumpfullversion -dumpversion
printf 'Ninja: '
ninja --version
cmake --version

printf '\nRunning OnboardAutonomy C++ tests...\n'
build_dir="${ONBOARD_AUTONOMY_BUILD_DIR:-${HOME}/build/onboard_autonomy}"
ctest --test-dir "${build_dir}" --output-on-failure

printf '\nUbuntu shell is ready. Type exit to close it.\n\n'
exec bash
