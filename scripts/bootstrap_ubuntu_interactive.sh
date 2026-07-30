#!/usr/bin/env bash

set -euo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
sudo bash "${script_dir}/bootstrap_ubuntu.sh"

printf '\nCompanionLab Ubuntu toolchain installed successfully.\n'
exec bash
