#!/usr/bin/env bash

set -euo pipefail

readonly gazebo_source_dir="${ARDUPILOT_GAZEBO_SOURCE_DIR:-${HOME}/src/ardupilot_gazebo}"
readonly gazebo_build_dir="${ARDUPILOT_GAZEBO_BUILD_DIR:-${HOME}/build/ardupilot_gazebo}"
readonly script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
readonly project_dir="$(cd -- "${script_dir}/.." && pwd)"

if ! command -v gz >/dev/null 2>&1; then
    printf 'Gazebo is not installed. Run scripts/install_gazebo_harmonic.sh first.\n' >&2
    exit 1
fi

export GZ_VERSION=harmonic
export GZ_SIM_SYSTEM_PLUGIN_PATH="${gazebo_build_dir}:${GZ_SIM_SYSTEM_PLUGIN_PATH:-}"
export GZ_SIM_RESOURCE_PATH="${project_dir}/simulation/models:${project_dir}/simulation/worlds:${gazebo_source_dir}/models:${gazebo_source_dir}/worlds:${GZ_SIM_RESOURCE_PATH:-}"

if [[ -e /dev/dxg ]]; then
    export GALLIUM_DRIVER="${GALLIUM_DRIVER:-d3d12}"
    export MESA_D3D12_DEFAULT_ADAPTER_NAME="${MESA_D3D12_DEFAULT_ADAPTER_NAME:-NVIDIA}"
fi

printf 'Connecting Gazebo GUI to the running simulation server\n'
exec gz sim -g -v4
