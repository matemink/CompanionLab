#!/usr/bin/env bash

set -euo pipefail

sudo apt-get update
sudo apt-get install -y \
    g++-aarch64-linux-gnu \
    g++-12-aarch64-linux-gnu \
    ninja-build

printf '\nARM64 cross-compiler installed:\n'
aarch64-linux-gnu-g++-12 --version | head -n 1
