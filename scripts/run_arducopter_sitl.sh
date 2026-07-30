#!/usr/bin/env bash

set -euo pipefail

ARDUPILOT_DIR="${ARDUPILOT_DIR:-${HOME}/src/ardupilot-Copter-4.6.3}"
MAVPROXY="${MAVPROXY:-${HOME}/venv-ardupilot/bin/mavproxy.py}"

if [[ ! -x "${ARDUPILOT_DIR}/build/sitl/bin/arducopter" ]]; then
    printf 'ArduCopter SITL is not built: %s\n' "${ARDUPILOT_DIR}" >&2
    exit 1
fi

if [[ ! -x "${MAVPROXY}" ]]; then
    printf 'MAVProxy is not installed: %s\n' "${MAVPROXY}" >&2
    exit 1
fi

cd "${ARDUPILOT_DIR}"

autopilot_pid=''

stop_autopilot() {
    if [[ -n "${autopilot_pid}" ]]; then
        kill "${autopilot_pid}" 2>/dev/null || true
        wait "${autopilot_pid}" 2>/dev/null || true
    fi
}

trap stop_autopilot EXIT INT TERM

build/sitl/bin/arducopter \
    -S \
    --wipe \
    --model + \
    --speedup 1 \
    --slave 0 \
    --defaults Tools/autotest/default_params/copter.parm \
    --sim-address=127.0.0.1 \
    -I0 &
autopilot_pid=$!

"${MAVPROXY}" \
    --master=tcp:127.0.0.1:5760 \
    --sitl=127.0.0.1:5501 \
    --out=udp:127.0.0.1:14550 \
    --streamrate=-1 \
    --non-interactive
