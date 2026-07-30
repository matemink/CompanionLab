#!/usr/bin/env bash

set -euo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
binary="${COMPANIONLAB_BINARY:-${script_dir}/companionlab}"
if [[ ! -x "${binary}" ]]; then
    project_binary="${script_dir}/../build/companionlab"
    if [[ -x "${project_binary}" ]]; then
        binary="${project_binary}"
    fi
fi

if [[ ! -x "${binary}" ]]; then
    printf 'CompanionLab binary not found: %s\n' "${binary}" >&2
    exit 1
fi

declare -a candidates=()
declare -A resolved_candidates=()
shopt -s nullglob

if [[ -n "${COMPANIONLAB_SERIAL:-}" ]]; then
    candidates=("${COMPANIONLAB_SERIAL}")
else
    for device in \
        /dev/serial/by-id/* \
        /dev/ttyACM* \
        /dev/ttyUSB* \
        /dev/ttyAMA0; do
        if [[ ! -e "${device}" ]]; then
            continue
        fi
        resolved="$(readlink -f "${device}" 2>/dev/null || true)"
        if [[ -z "${resolved}" ||
              -n "${resolved_candidates[${resolved}]:-}" ]]; then
            continue
        fi
        resolved_candidates["${resolved}"]=1
        candidates+=("${device}")
    done
fi

if (( ${#candidates[@]} == 0 )); then
    printf 'No Pixhawk serial candidate found.\n' >&2
    printf 'Connect USB or configure Pi 5 GPIO UART, then run diagnostics.\n' >&2
    exit 2
fi

if (( ${#candidates[@]} > 1 )); then
    printf 'Multiple serial devices found; refusing to guess:\n' >&2
    printf '  %s\n' "${candidates[@]}" >&2
    printf 'Select one explicitly:\n' >&2
    printf '  COMPANIONLAB_SERIAL=/dev/ttyAMA0 %s\n' "$0" >&2
    exit 3
fi

device="${candidates[0]}"
if [[ ! -r "${device}" || ! -w "${device}" ]]; then
    printf 'Serial device is not readable/writable: %s\n' "${device}" >&2
    printf 'Add the user to dialout, then log in again:\n' >&2
    printf '  sudo usermod -aG dialout "$USER"\n' >&2
    exit 4
fi

baud="${COMPANIONLAB_BAUD:-115200}"
snapshot_ms="${COMPANIONLAB_SNAPSHOT_MS:-1000}"
camera_enabled="${COMPANIONLAB_CAMERA_ENABLED:-1}"
camera_width="${COMPANIONLAB_CAMERA_WIDTH:-640}"
camera_height="${COMPANIONLAB_CAMERA_HEIGHT:-480}"
camera_fps="${COMPANIONLAB_CAMERA_FPS:-30}"
apriltag_enabled="${COMPANIONLAB_APRILTAG_ENABLED:-1}"
preview_enabled="${COMPANIONLAB_CAMERA_PREVIEW_ENABLED:-1}"
preview_port="${COMPANIONLAB_CAMERA_PREVIEW_PORT:-8080}"
log_dir="${COMPANIONLAB_LOG_DIR:-${HOME}/.local/state/companionlab}"
mkdir -p "${log_dir}"
log_file="${log_dir}/telemetry-$(date -u +%Y%m%dT%H%M%SZ).jsonl"

printf 'CompanionLab hardware bench\n'
printf '  Mode:   OBSERVE ONLY\n'
printf '  Link:   %s at %s baud\n' "${device}" "${baud}"
printf '  Log:    %s\n' "${log_file}"
printf '  Safety: interactive scenarios are disabled on serial hardware\n\n'

declare -a camera_arguments=()
if [[ "${camera_enabled}" == "1" ]]; then
    camera_arguments=(
        --camera
        --camera-width "${camera_width}"
        --camera-height "${camera_height}"
        --camera-fps "${camera_fps}"
    )
    if [[ "${apriltag_enabled}" == "1" ]]; then
        camera_arguments+=(--apriltag)
    fi
    if [[ "${preview_enabled}" == "1" ]]; then
        camera_arguments+=(
            --camera-preview
            --camera-preview-port "${preview_port}"
        )
    fi
    printf '  Camera: %sx%s YUV420 at %s FPS\n\n' \
        "${camera_width}" "${camera_height}" "${camera_fps}"
    if [[ "${apriltag_enabled}" == "1" ]]; then
        printf '  Vision: AprilTag tagStandard41h12\n\n'
    fi
    if [[ "${preview_enabled}" == "1" ]]; then
        printf '  Preview: http://companionpi.local:%s/\n\n' \
            "${preview_port}"
    fi
fi

"${binary}" \
    --serial "${device}" \
    --baud "${baud}" \
    --snapshot-ms "${snapshot_ms}" \
    "${camera_arguments[@]}" \
    --json |
    tee -a "${log_file}"
