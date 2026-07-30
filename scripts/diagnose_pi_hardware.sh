#!/usr/bin/env bash

set -u

pass_count=0
warning_count=0

pass() {
    printf '[PASS] %s\n' "$1"
    pass_count=$((pass_count + 1))
}

warn() {
    printf '[WARN] %s\n' "$1"
    warning_count=$((warning_count + 1))
}

printf 'CompanionLab Raspberry Pi 5 bench diagnostics\n'
printf '==============================================\n\n'

architecture="$(uname -m)"
if [[ "${architecture}" == "aarch64" ]]; then
    pass "64-bit ARM operating system (${architecture})"
else
    warn "Expected aarch64, detected ${architecture}"
fi

if [[ -r /etc/os-release ]]; then
    os_name="$(
        . /etc/os-release
        printf '%s' "${PRETTY_NAME:-unknown Linux}"
    )"
    printf '[INFO] OS: %s\n' "${os_name}"
else
    warn 'Unable to read /etc/os-release'
fi

if id -nG | tr ' ' '\n' | grep -qx dialout; then
    pass 'Current user belongs to the dialout group'
else
    warn 'Current user is not in dialout'
    printf '       Fix: sudo usermod -aG dialout "$USER", then log in again.\n'
fi

serial_devices=()
declare -A resolved_serial_devices=()
shopt -s nullglob

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
          -n "${resolved_serial_devices[${resolved}]:-}" ]]; then
        continue
    fi
    resolved_serial_devices["${resolved}"]=1
    serial_devices+=("${device}")
done

if (( ${#serial_devices[@]} == 0 )); then
    warn 'No Pixhawk serial link detected'
    printf '       Expected USB serial or Pi 5 GPIO UART /dev/ttyAMA0.\n'
else
    pass "Detected ${#serial_devices[@]} serial candidate(s)"
    for device in "${serial_devices[@]}"; do
        resolved="$(readlink -f "${device}" 2>/dev/null || true)"
        permissions='not readable/writable'
        if [[ -r "${device}" && -w "${device}" ]]; then
            permissions='readable/writable'
        fi
        printf '       %s -> %s (%s)\n' \
            "${device}" "${resolved}" "${permissions}"
    done
fi

if command -v rpicam-hello >/dev/null 2>&1; then
    camera_output="$(rpicam-hello --list-cameras 2>&1 || true)"
    if grep -q 'imx708' <<<"${camera_output}"; then
        pass 'Raspberry Pi Camera Module 3 detected (IMX708)'
    else
        warn 'rpicam-hello is installed, but IMX708 was not detected'
        printf '%s\n' "${camera_output}" | sed 's/^/       /'
    fi
else
    warn 'rpicam-hello is not installed'
fi

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
binary="${COMPANIONLAB_BINARY:-${script_dir}/companionlab}"
if [[ ! -x "${binary}" ]]; then
    project_binary="${script_dir}/../build/companionlab"
    if [[ -x "${project_binary}" ]]; then
        binary="${project_binary}"
    fi
fi

if [[ -x "${binary}" ]]; then
    binary_description="$(file -b "${binary}" 2>/dev/null || true)"
    if grep -q 'ARM aarch64' <<<"${binary_description}"; then
        pass 'CompanionLab binary is ARM64'
    else
        warn "Unexpected CompanionLab binary: ${binary_description}"
    fi

    if [[ "${architecture}" != "aarch64" ]]; then
        warn 'Runtime-library check skipped on non-ARM64 host'
    else
        missing_libraries="$(
            ldd "${binary}" 2>/dev/null |
                awk '/not found/ { print $1 }'
        )"
        if [[ -z "${missing_libraries}" ]]; then
            pass 'CompanionLab runtime libraries are available'
        else
            warn "Missing runtime libraries: ${missing_libraries}"
        fi
    fi
else
    warn "CompanionLab binary not found at ${binary}"
fi

printf '\nSummary: %d passed, %d warning(s)\n' \
    "${pass_count}" "${warning_count}"
printf 'This diagnostic does not arm the controller or send flight commands.\n'
