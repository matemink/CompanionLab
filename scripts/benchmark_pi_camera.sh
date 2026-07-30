#!/usr/bin/env bash

set -euo pipefail

readonly width="${COMPANIONLAB_CAMERA_WIDTH:-1280}"
readonly height="${COMPANIONLAB_CAMERA_HEIGHT:-720}"
readonly target_fps="${COMPANIONLAB_CAMERA_FPS:-30}"
readonly frame_count="${COMPANIONLAB_CAMERA_FRAMES:-300}"
readonly state_root="${COMPANIONLAB_CAMERA_STATE_DIR:-${HOME}/.local/state/companionlab/camera}"

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
analyzer="${COMPANIONLAB_CAMERA_ANALYZER:-${script_dir}/camera_benchmark.py}"
if [[ ! -f "${analyzer}" ]]; then
    analyzer="${script_dir}/../python/camera_benchmark.py"
fi

require_positive_integer() {
    local readonly name="$1"
    local readonly value="$2"
    if [[ ! "${value}" =~ ^[1-9][0-9]*$ ]]; then
        printf '%s must be a positive integer, got: %s\n' \
            "${name}" "${value}" >&2
        exit 2
    fi
}

require_positive_integer COMPANIONLAB_CAMERA_WIDTH "${width}"
require_positive_integer COMPANIONLAB_CAMERA_HEIGHT "${height}"
require_positive_integer COMPANIONLAB_CAMERA_FPS "${target_fps}"
require_positive_integer COMPANIONLAB_CAMERA_FRAMES "${frame_count}"

if ! command -v rpicam-hello >/dev/null 2>&1 ||
   ! command -v rpicam-vid >/dev/null 2>&1; then
    printf 'rpicam-apps are required for the camera benchmark.\n' >&2
    exit 3
fi
if ! command -v python3 >/dev/null 2>&1; then
    printf 'python3 is required for benchmark report generation.\n' >&2
    exit 3
fi
if [[ ! -f "${analyzer}" ]]; then
    printf 'Camera benchmark analyzer not found: %s\n' "${analyzer}" >&2
    exit 3
fi

camera_listing="$(rpicam-hello --list-cameras 2>&1 || true)"
camera_model="$(
    awk '$1 == "0" && $2 == ":" { print $3; exit }' \
        <<<"${camera_listing}"
)"
if [[ -z "${camera_model}" ]]; then
    printf 'No Raspberry Pi camera was detected.\n' >&2
    printf '%s\n' "${camera_listing}" >&2
    exit 4
fi

run_id="$(date -u +%Y%m%dT%H%M%SZ)-$$"
run_dir="${state_root}/${run_id}"
mkdir -p "${run_dir}"

pts_file="${run_dir}/frames.pts"
metadata_file="${run_dir}/metadata.json"
samples_file="${run_dir}/process-samples.tsv"
capture_log="${run_dir}/rpicam.log"
camera_listing_file="${run_dir}/camera-list.txt"
report_json="${run_dir}/report.json"
report_markdown="${run_dir}/report.md"

printf '%s\n' "${camera_listing}" >"${camera_listing_file}"
printf 'elapsed_ms\tcpu_ticks\trss_kib\n' >"${samples_file}"

camera_pid=''
cleanup() {
    if [[ -n "${camera_pid}" ]] &&
       kill -0 "${camera_pid}" 2>/dev/null; then
        kill -INT "${camera_pid}" 2>/dev/null || true
        wait "${camera_pid}" 2>/dev/null || true
    fi
}
trap cleanup EXIT INT TERM

printf 'CompanionLab Camera Module 3 benchmark\n'
printf '  Camera: %s\n' "${camera_model}"
printf '  Stream: %sx%s YUV420 at %s FPS\n' \
    "${width}" "${height}" "${target_fps}"
printf '  Frames: %s\n' "${frame_count}"
printf '  Output: %s\n\n' "${run_dir}"

start_ns="$(date +%s%N)"
rpicam-vid \
    --nopreview \
    --frames "${frame_count}" \
    --framerate "${target_fps}" \
    --width "${width}" \
    --height "${height}" \
    --codec yuv420 \
    --output /dev/null \
    --save-pts "${pts_file}" \
    --metadata "${metadata_file}" \
    --metadata-format json \
    --autofocus-mode continuous \
    >"${capture_log}" 2>&1 &
camera_pid=$!

while kill -0 "${camera_pid}" 2>/dev/null; do
    now_ns="$(date +%s%N)"
    elapsed_ms="$(((now_ns - start_ns) / 1000000))"
    cpu_ticks="$(
        awk '{ print $14 + $15 }' \
            "/proc/${camera_pid}/stat" 2>/dev/null || true
    )"
    rss_kib="$(
        awk '/^VmRSS:/ { print $2 }' \
            "/proc/${camera_pid}/status" 2>/dev/null || true
    )"
    if [[ -n "${cpu_ticks}" && -n "${rss_kib}" ]]; then
        printf '%s\t%s\t%s\n' \
            "${elapsed_ms}" "${cpu_ticks}" "${rss_kib}" \
            >>"${samples_file}"
    fi
    sleep 0.1
done

set +e
wait "${camera_pid}"
capture_status=$?
camera_pid=''
set -e

clock_ticks="$(getconf CLK_TCK)"
set +e
python3 "${analyzer}" \
    --pts "${pts_file}" \
    --metadata "${metadata_file}" \
    --samples "${samples_file}" \
    --camera-model "${camera_model}" \
    --width "${width}" \
    --height "${height}" \
    --target-fps "${target_fps}" \
    --requested-frames "${frame_count}" \
    --capture-status "${capture_status}" \
    --clock-ticks "${clock_ticks}" \
    --report-json "${report_json}" \
    --report-markdown "${report_markdown}"
analysis_status=$?
set -e

printf '\nArtifacts:\n'
printf '  %s\n' "${report_markdown}" "${report_json}" "${capture_log}"
exit "${analysis_status}"
