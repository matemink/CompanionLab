#!/usr/bin/env bash

set -euo pipefail

readonly gazebo_source_dir="${ARDUPILOT_GAZEBO_SOURCE_DIR:-${HOME}/src/ardupilot_gazebo}"
readonly gazebo_build_dir="${ARDUPILOT_GAZEBO_BUILD_DIR:-${HOME}/build/ardupilot_gazebo}"
readonly world_file="${COMPANIONLAB_GAZEBO_WORLD:-${gazebo_source_dir}/worlds/iris_runway.sdf}"

if ! command -v gz >/dev/null 2>&1; then
    printf 'Gazebo is not installed. Run scripts/install_gazebo_harmonic.sh first.\n' >&2
    exit 1
fi

if [[ ! -f "${gazebo_build_dir}/libArduPilotPlugin.so" ]]; then
    printf 'ArduPilot Gazebo plugin is not built: %s\n' \
        "${gazebo_build_dir}" >&2
    exit 1
fi

if [[ ! -f "${world_file}" ]]; then
    printf 'Gazebo world is missing: %s\n' "${world_file}" >&2
    exit 1
fi

export GZ_VERSION=harmonic
export GZ_SIM_SYSTEM_PLUGIN_PATH="${gazebo_build_dir}:${GZ_SIM_SYSTEM_PLUGIN_PATH:-}"
export GZ_SIM_RESOURCE_PATH="${gazebo_source_dir}/models:${gazebo_source_dir}/worlds:${GZ_SIM_RESOURCE_PATH:-}"

# WSLg may fall back to CPU rendering even when /dev/dxg is available.
if [[ -e /dev/dxg ]]; then
    export GALLIUM_DRIVER="${GALLIUM_DRIVER:-d3d12}"
    export MESA_D3D12_DEFAULT_ADAPTER_NAME="${MESA_D3D12_DEFAULT_ADAPTER_NAME:-NVIDIA}"
fi

printf 'CompanionLab Gazebo world: %s\n' "${world_file}"
exec gz sim -v4 -r "${world_file}"
