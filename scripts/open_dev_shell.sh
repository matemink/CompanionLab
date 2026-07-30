#!/usr/bin/env bash

set -euo pipefail

printf 'CompanionLab development environment\n\n'
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

printf '\nRunning CompanionLab C++ tests...\n'
build_dir="${COMPANIONLAB_BUILD_DIR:-${HOME}/build/companionlab}"
ctest --test-dir "${build_dir}" --output-on-failure

printf '\nUbuntu shell is ready. Type exit to close it.\n\n'
exec bash
