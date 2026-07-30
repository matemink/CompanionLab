#!/usr/bin/env bash

set -euo pipefail

if [[ -t 1 ]]; then
    printf '\033]0;ArduCopter SITL - MAVProxy Flight Console\007'
fi

readonly ardupilot_dir="${ARDUPILOT_DIR:-${HOME}/src/ardupilot-Copter-4.6.3}"
readonly mavproxy="${MAVPROXY:-${HOME}/venv-ardupilot/bin/mavproxy.py}"
readonly arducopter="${ardupilot_dir}/build/sitl/bin/arducopter"
readonly script_dir="$(
    cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd
)"
readonly project_dir="$(cd -- "${script_dir}/.." && pwd)"
readonly companion_defaults="${project_dir}/config/companionlab-gazebo.parm"

if [[ ! -x "${arducopter}" ]]; then
    printf 'ArduCopter SITL is not built: %s\n' "${ardupilot_dir}" >&2
    exit 1
fi

if [[ ! -x "${mavproxy}" ]]; then
    printf 'MAVProxy is not installed: %s\n' "${mavproxy}" >&2
    exit 1
fi

if [[ ! -f "${companion_defaults}" ]]; then
    printf 'CompanionLab SITL defaults not found: %s\n' \
        "${companion_defaults}" >&2
    exit 1
fi

cd "${ardupilot_dir}"

autopilot_pid=''

stop_autopilot() {
    if [[ -z "${autopilot_pid}" ]] ||
        ! kill -0 "${autopilot_pid}" 2>/dev/null; then
        return
    fi

    kill -TERM "${autopilot_pid}" 2>/dev/null || true
    for _ in {1..20}; do
        if ! kill -0 "${autopilot_pid}" 2>/dev/null; then
            wait "${autopilot_pid}" 2>/dev/null || true
            return
        fi
        sleep 0.1
    done

    kill -INT "${autopilot_pid}" 2>/dev/null || true
    for _ in {1..20}; do
        if ! kill -0 "${autopilot_pid}" 2>/dev/null; then
            wait "${autopilot_pid}" 2>/dev/null || true
            return
        fi
        sleep 0.1
    done

    kill -KILL "${autopilot_pid}" 2>/dev/null || true
    wait "${autopilot_pid}" 2>/dev/null || true
}

trap stop_autopilot EXIT INT TERM

"${arducopter}" \
    -S \
    --wipe \
    --model JSON \
    --speedup 1 \
    --slave 0 \
    --defaults "Tools/autotest/default_params/copter.parm,Tools/autotest/default_params/gazebo-iris.parm,${companion_defaults}" \
    --sim-address=127.0.0.1 \
    -I0 &
autopilot_pid=$!

printf '\nMAVProxy flight console\n'
printf 'First flight: mode GUIDED -> arm throttle -> takeoff 5 -> land\n\n'

mavproxy_args=(
    --master=tcp:127.0.0.1:5760
    --sitl=127.0.0.1:5501
    --out=udp:127.0.0.1:14550
    --streamrate=-1
)

if [[ ! -t 0 ]]; then
    mavproxy_args+=(--non-interactive)
fi

"${mavproxy}" "${mavproxy_args[@]}"
