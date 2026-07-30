#!/usr/bin/env bash

set -euo pipefail

readonly udp_port="${COMPANIONLAB_CAMERA_UDP_PORT:-5600}"
readonly enable_topic="${COMPANIONLAB_CAMERA_ENABLE_TOPIC:-/world/iris_runway/model/iris_with_gimbal/model/gimbal/link/pitch_link/sensor/camera/image/enable_streaming}"

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

if ! command -v gst-launch-1.0 >/dev/null 2>&1; then
    printf 'GStreamer CLI is missing. Install gstreamer1.0-tools.\n' >&2
    exit 1
fi

printf 'Opening Gazebo camera stream from UDP port %s.\n' "${udp_port}"
printf 'Close the video window or press Ctrl+C to stop.\n'

gst-launch-1.0 -q -e \
    udpsrc port="${udp_port}" \
        caps="application/x-rtp,media=video,clock-rate=90000,encoding-name=H264,payload=96" \
    ! rtpjitterbuffer latency=50 drop-on-latency=true \
    ! rtph264depay \
    ! h264parse \
    ! avdec_h264 \
    ! videoconvert \
    ! autovideosink sync=false &
pipeline_pid=$!

sleep 0.25
set_camera_streaming true
camera_enabled=true

set +e
wait "${pipeline_pid}"
readonly pipeline_status=$?
pipeline_pid=''
set -e

exit "${pipeline_status}"
