#!/usr/bin/env bash

set -euo pipefail

readonly udp_port="${ONBOARD_AUTONOMY_CAMERA_UDP_PORT:-5600}"
readonly frame_count="${ONBOARD_AUTONOMY_CAMERA_CHECK_FRAMES:-60}"
readonly timeout_seconds="${ONBOARD_AUTONOMY_CAMERA_CHECK_TIMEOUT_SECONDS:-15}"
readonly enable_topic="${ONBOARD_AUTONOMY_CAMERA_ENABLE_TOPIC:-/world/iris_runway/model/iris_with_gimbal/model/gimbal/link/pitch_link/sensor/camera/image/enable_streaming}"

export GZ_VERSION="${GZ_VERSION:-harmonic}"

pipeline_pid=''
camera_enabled=false

set_camera_streaming() {
    local readonly enabled="$1"
    gz topic \
        -t "${enable_topic}" \
        -m gz.msgs.Boolean \
        -p "data: ${enabled}"
}

cleanup() {
    if [[ -n "${pipeline_pid}" ]] &&
        kill -0 "${pipeline_pid}" 2>/dev/null; then
        kill -INT "${pipeline_pid}" 2>/dev/null || true
        wait "${pipeline_pid}" 2>/dev/null || true
    fi

    if [[ "${camera_enabled}" == true ]]; then
        set_camera_streaming false >/dev/null 2>&1 || true
    fi
}

trap cleanup EXIT INT TERM

required_elements=(
    x264enc
    udpsrc
    rtpjitterbuffer
    rtph264depay
    h264parse
    avdec_h264
    videoconvert
    fakesink
)

if ! command -v gst-launch-1.0 >/dev/null 2>&1; then
    printf 'GStreamer CLI is missing. Install gstreamer1.0-tools.\n' >&2
    exit 1
fi

for element in "${required_elements[@]}"; do
    if ! gst-inspect-1.0 "${element}" >/dev/null 2>&1; then
        printf 'GStreamer element is missing: %s\n' "${element}" >&2
        exit 1
    fi
done

printf 'Gazebo camera check\n'
printf '  Input: RTP/H.264 over UDP port %s\n' "${udp_port}"
printf '  Goal: decode %s frames within %s seconds\n' \
    "${frame_count}" "${timeout_seconds}"

timeout --signal=INT "${timeout_seconds}s" \
    gst-launch-1.0 -q -e \
    udpsrc port="${udp_port}" \
        caps="application/x-rtp,media=video,clock-rate=90000,encoding-name=H264,payload=96" \
    ! rtpjitterbuffer latency=50 drop-on-latency=true \
    ! rtph264depay \
    ! h264parse \
    ! avdec_h264 \
    ! videoconvert \
    ! video/x-raw,format=RGB \
    ! fakesink num-buffers="${frame_count}" sync=false &
pipeline_pid=$!

sleep 0.25
set_camera_streaming true
camera_enabled=true

set +e
wait "${pipeline_pid}"
readonly pipeline_status=$?
pipeline_pid=''
set -e

if [[ "${pipeline_status}" -eq 0 ]]; then
    printf 'PASS: decoded %s camera frames.\n' "${frame_count}"
    exit 0
fi

if [[ "${pipeline_status}" -eq 124 ]] ||
    [[ "${pipeline_status}" -eq 130 ]]; then
    printf 'FAIL: no complete camera stream arrived before timeout.\n' >&2
    printf 'Check that Gazebo iris_runway is running and UDP port %s is free.\n' \
        "${udp_port}" >&2
    exit 1
fi

printf 'FAIL: GStreamer pipeline exited with status %s.\n' \
    "${pipeline_status}" >&2
exit "${pipeline_status}"
