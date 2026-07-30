#!/usr/bin/env bash

set -euo pipefail

COMPANIONLAB_BUILD_DIR="${COMPANIONLAB_BUILD_DIR:-${HOME}/build/companionlab}"
COMPANION="${COMPANIONLAB_BUILD_DIR}/companionlab"

if [[ ! -x "${COMPANION}" ]]; then
    printf 'CompanionLab is not built: %s\n' "${COMPANION}" >&2
    exit 1
fi

arguments=(
    --udp-bind 127.0.0.1
    --udp-port 14550
    --snapshot-ms 1000
)

if [[ "${COMPANIONLAB_DEMO_FLIGHT:-0}" == "1" ]]; then
    arguments+=(--demo-flight)
fi

if [[ -n "${COMPANIONLAB_SCENARIO:-}" ]]; then
    arguments+=(--scenario "${COMPANIONLAB_SCENARIO}")
fi

if [[ "${COMPANIONLAB_EXIT_AFTER_SCENARIO:-0}" == "1" ]]; then
    arguments+=(--exit-after-scenario)
fi

if [[ "${COMPANIONLAB_INTERACTIVE:-0}" == "1" ]]; then
    arguments+=(--interactive)
fi

exec "${COMPANION}" "${arguments[@]}"
