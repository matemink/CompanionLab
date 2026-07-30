#!/usr/bin/env bash

set -euo pipefail

readonly ardupilot_tag="Copter-4.6.3"
readonly ardupilot_commit="92b0cd788ec29406f26c6f9c31d5ceedbd1cc538"
readonly source_root="${HOME}/src"
readonly ardupilot_dir="${source_root}/ardupilot-${ardupilot_tag}"

mkdir -p "${source_root}"

if [[ ! -d "${ardupilot_dir}/.git" ]]; then
    git clone \
        --branch "${ardupilot_tag}" \
        --depth 1 \
        --recurse-submodules \
        --shallow-submodules \
        https://github.com/ArduPilot/ardupilot.git \
        "${ardupilot_dir}"
fi

git -C "${ardupilot_dir}" submodule update \
    --init \
    --recursive \
    --depth 1

actual_commit="$(git -C "${ardupilot_dir}" rev-parse HEAD)"
if [[ "${actual_commit}" != "${ardupilot_commit}" ]]; then
    printf 'Unexpected ArduPilot commit: %s\n' "${actual_commit}" >&2
    exit 1
fi

cd "${ardupilot_dir}"
Tools/environment_install/install-prereqs-ubuntu.sh -y

printf '\nArduPilot SITL prerequisites installed successfully.\n'
printf 'Pinned source: %s at %s\n' "${ardupilot_tag}" "${ardupilot_commit}"
exec bash
