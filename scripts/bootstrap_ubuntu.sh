#!/usr/bin/env bash

set -euo pipefail

apt-get update
apt-get install -y \
    build-essential \
    ca-certificates \
    cmake \
    git \
    ninja-build \
    pkg-config \
    python3 \
    python3-pip \
    python3-venv
