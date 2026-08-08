#!/usr/bin/env bash

set -euo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
project_dir="$(cd -- "${script_dir}/.." && pwd)"
build_dir="${ONBOARD_AUTONOMY_ARM64_BUILD_DIR:-${project_dir}/build-arm64-gcc12-c-cxx-release}"
artifact_dir="${project_dir}/artifacts"
stage_dir="${artifact_dir}/onboard_autonomy-pi5"
archive="${artifact_dir}/onboard_autonomy-pi5-arm64.tar.gz"
cross_compiler="${ONBOARD_AUTONOMY_ARM64_CXX:-aarch64-linux-gnu-g++-12}"
cross_c_compiler="${ONBOARD_AUTONOMY_ARM64_CC:-aarch64-linux-gnu-gcc-12}"
maximum_glibc="${ONBOARD_AUTONOMY_MAX_GLIBC:-GLIBC_2.41}"
maximum_glibcxx="${ONBOARD_AUTONOMY_MAX_GLIBCXX:-GLIBCXX_3.4.30}"

if ! command -v "${cross_compiler}" >/dev/null 2>&1; then
    printf 'ARM64 cross-compiler is missing: %s\n' \
        "${cross_compiler}" >&2
    printf 'Run: bash scripts/install_arm64_cross_compiler.sh\n' >&2
    exit 1
fi
if ! command -v "${cross_c_compiler}" >/dev/null 2>&1; then
    printf 'ARM64 C cross-compiler is missing: %s\n' \
        "${cross_c_compiler}" >&2
    printf 'Run: bash scripts/install_arm64_cross_compiler.sh\n' >&2
    exit 1
fi

cmake \
    -S "${project_dir}" \
    -B "${build_dir}" \
    -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE="${project_dir}/cmake/toolchains/aarch64-linux-gnu.cmake" \
    -DCMAKE_C_COMPILER="${cross_c_compiler}" \
    -DCMAKE_CXX_COMPILER="${cross_compiler}" \
    -DBUILD_TESTING=OFF
cmake --build "${build_dir}" --parallel

case "${stage_dir}" in
    "${project_dir}"/artifacts/onboard_autonomy-pi5) ;;
    *)
        printf 'Refusing to replace unexpected stage path: %s\n' \
            "${stage_dir}" >&2
        exit 2
        ;;
esac

cmake -E remove_directory "${stage_dir}"
cmake -E make_directory "${stage_dir}/bin"
cmake --install "${build_dir}" --prefix "${stage_dir}"

install -m 0755 \
    "${project_dir}/python/calibrate_camera.py" \
    "${project_dir}/python/camera_benchmark.py" \
    "${project_dir}/python/rotate_jsonl_logs.py" \
    "${project_dir}/python/runtime_profile.py" \
    "${project_dir}/scripts/benchmark_pi_camera.sh" \
    "${project_dir}/scripts/capture_camera_calibration.sh" \
    "${project_dir}/scripts/configure_pi5_uart.sh" \
    "${project_dir}/scripts/diagnose_pi_hardware.sh" \
    "${project_dir}/scripts/install_onboard_autonomy_service.sh" \
    "${project_dir}/scripts/profile_onboard_autonomy_pi.sh" \
    "${project_dir}/scripts/run_onboard_autonomy_pi.sh" \
    "${stage_dir}/bin/"
install -d "${stage_dir}/share/onboard_autonomy/systemd"
install -m 0644 \
    "${project_dir}/deployment/systemd/onboard-autonomy@.service" \
    "${project_dir}/deployment/systemd/onboard-autonomy.env.example" \
    "${stage_dir}/share/onboard_autonomy/systemd/"
install -m 0644 \
    "${project_dir}/docs/raspberry-pi-5-bench.md" \
    "${stage_dir}/BENCH.md"
install -m 0644 \
    "${project_dir}/python/requirements.txt" \
    "${stage_dir}/requirements.txt"
install -m 0644 \
    "${project_dir}/LICENSE" \
    "${stage_dir}/LICENSE"

binary="${stage_dir}/bin/onboard_autonomy"
required_glibc="$(
    strings "${binary}" |
        grep '^GLIBC_[0-9]' |
        sort -V |
        tail -n 1
)"
required_glibcxx="$(
    strings "${binary}" |
        grep '^GLIBCXX_[0-9]' |
        sort -V |
        tail -n 1
)"

version_is_at_most() {
    [[ "$(
        printf '%s\n%s\n' "$1" "$2" |
            sort -V |
            tail -n 1
    )" == "$2" ]]
}

if ! version_is_at_most "${required_glibc}" "${maximum_glibc}"; then
    printf 'ABI gate failed: %s exceeds %s\n' \
        "${required_glibc}" "${maximum_glibc}" >&2
    exit 3
fi
if ! version_is_at_most "${required_glibcxx}" "${maximum_glibcxx}"; then
    printf 'ABI gate failed: %s exceeds %s\n' \
        "${required_glibcxx}" "${maximum_glibcxx}" >&2
    exit 3
fi

tar \
    -C "${artifact_dir}" \
    -czf "${archive}" \
    onboard_autonomy-pi5

printf '\nARM64 release package created:\n'
printf '  %s\n' "${archive}"
file "${binary}"
printf 'ABI gate: %s <= %s, %s <= %s\n' \
    "${required_glibc}" "${maximum_glibc}" \
    "${required_glibcxx}" "${maximum_glibcxx}"
sha256sum "${archive}"
