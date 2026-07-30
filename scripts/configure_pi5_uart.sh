#!/usr/bin/env bash

set -euo pipefail

config_file="/boot/firmware/config.txt"
backup_file="${config_file}.companionlab.bak"
overlay="dtoverlay=uart0-pi5"

model="$(tr -d '\0' </proc/device-tree/model)"
if [[ "${model}" != *"Raspberry Pi 5"* ]]; then
    printf 'Expected Raspberry Pi 5, detected: %s\n' "${model}" >&2
    exit 1
fi

if [[ ! -r "${config_file}" ]]; then
    printf 'Boot configuration is not readable: %s\n' "${config_file}" >&2
    exit 1
fi

if grep -Fxq "${overlay}" "${config_file}"; then
    printf 'Pi 5 UART0 overlay is already configured.\n'
    exit 0
fi

if ! grep -Fxq '[pi5]' "${config_file}"; then
    printf 'Missing [pi5] section in %s\n' "${config_file}" >&2
    exit 1
fi

sudo_command=()
if (( EUID != 0 )); then
    sudo_command=(sudo)
fi

if ! "${sudo_command[@]}" test -e "${backup_file}"; then
    "${sudo_command[@]}" cp -a "${config_file}" "${backup_file}"
fi

temporary_file="$(mktemp)"
trap 'rm -f "${temporary_file}"' EXIT

awk -v overlay="${overlay}" '
    /^\[pi5\]$/ {
        print
        print "# CompanionLab flight-controller UART on GPIO14/GPIO15"
        print overlay
        next
    }
    { print }
' "${config_file}" >"${temporary_file}"

"${sudo_command[@]}" install \
    -o root \
    -g root \
    -m 0644 \
    "${temporary_file}" \
    "${config_file}"

if ! grep -Fxq "${overlay}" "${config_file}"; then
    printf 'UART overlay verification failed.\n' >&2
    exit 1
fi

printf 'Configured %s in the [pi5] section.\n' "${overlay}"
printf 'Backup: %s\n' "${backup_file}"
printf 'Reboot Raspberry Pi before using /dev/ttyAMA0.\n'
