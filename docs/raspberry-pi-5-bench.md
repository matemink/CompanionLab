# Raspberry Pi 5 and Pixhawk 6C bench setup

This procedure validates ARM64 Linux deployment and physical MAVLink
communication without running motors. OnboardAutonomy may request telemetry
message rates, but the serial hardware mode cannot start ARM, TAKEOFF,
LAND, route, or precision-landing scenarios.

## Safety

1. Remove every propeller.
2. Do not connect the flight battery for the first USB test.
3. Do not run arming or motor-test commands.
4. Keep the Pixhawk on a stable, non-conductive surface.
5. Use USB first. Do not wire TELEM/UART until the USB path is verified.

Milestone 1 reads telemetry only.

## Verify the operating system

Run on the Raspberry Pi:

```bash
cat /etc/os-release
uname -m
```

The expected architecture is `aarch64`. If it reports `armv7l`, install
a 64-bit Raspberry Pi OS before the ARM deployment milestone.

## Verify Camera Module 3

On current Raspberry Pi OS images:

```bash
rpicam-hello --list-cameras
```

The output should list an IMX708 camera. Do not troubleshoot the camera
and Pixhawk simultaneously; validate one device at a time.

Run the bounded raw-camera benchmark from the deployed package:

```bash
bin/benchmark_pi_camera.sh
```

The default profile captures 300 `1280x720 YUV420` frames at 30 FPS.
It writes PTS, per-frame metadata, process samples, the raw `rpicam` log,
and JSON/Markdown reports under:

```text
~/.local/state/onboard_autonomy/camera/<run-id>/
```

This baseline measures cadence, estimated frame gaps, CPU, RSS, and
sensor metadata independently from OnboardAutonomy.

The deployed runtime receiver can then be tested together with Pixhawk:

```bash
bin/run_onboard_autonomy_pi.sh
```

The launcher enables `640x480 YUV420 @ 30 FPS` camera reception by
default. The `camera` object in each JSON line reports measured FPS,
processing drops, frame age, and `FrameWallClock` to application
latency. Disable it for a telemetry-only run with:

```bash
ONBOARD_AUTONOMY_CAMERA_ENABLED=0 bin/run_onboard_autonomy_pi.sh
```

## Install the Milestone 1 toolchain

```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake git ninja-build python3-venv
```

Camera and vision packages are added in a later milestone:

```bash
sudo apt-get install -y gstreamer1.0-tools libopencv-dev
```

## Connect Pixhawk by USB

Connect the Pixhawk 6C USB-C port to a USB host port on the Raspberry Pi.
Then inspect serial devices:

```bash
ls -l /dev/ttyACM* /dev/ttyUSB* 2>/dev/null
```

ArduPilot USB commonly appears as `/dev/ttyACM0`. Grant the current user
serial access:

```bash
sudo usermod -aG dialout "$USER"
```

Log out and back in after changing group membership.

## Build and run

### Packaged ARM64 candidate

On the Ubuntu development host:

```bash
bash scripts/package_pi5_release.sh
```

Transfer `artifacts/onboard_autonomy-pi5-arm64.tar.gz` to the Pi, then:

```bash
tar -xzf onboard_autonomy-pi5-arm64.tar.gz
cd onboard_autonomy-pi5
bin/diagnose_pi_hardware.sh
bin/run_onboard_autonomy_pi.sh
```

The diagnostic checks architecture, `dialout`, Camera Module 3, serial
candidates, ARM64 ELF format, and runtime libraries. The launcher:

- prefers stable `/dev/serial/by-id` names;
- accepts exactly one serial candidate and refuses to guess otherwise;
- defaults to 115200 baud;
- runs `--serial --json --camera` by default;
- stores JSONL under `~/.local/state/onboard_autonomy`.

Override an ambiguous serial device explicitly:

```bash
ONBOARD_AUTONOMY_SERIAL=/dev/serial/by-id/usb-... \
    bin/run_onboard_autonomy_pi.sh
```

The cross-built binary is a deployment candidate, not an ABI promise.
Ubuntu and Raspberry Pi OS can ship different glibc versions. If the
diagnostic reports missing runtime support, perform the native build
below instead of copying random libraries.

### Native Raspberry Pi build

From the OnboardAutonomy source directory on the Pi:

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
ONBOARD_AUTONOMY_BINARY=./build/onboard_autonomy \
    scripts/run_onboard_autonomy_pi.sh
```

Expected behavior:

- JSON is printed once per second and written to a timestamped JSONL log.
- `connected` changes to `true` after an ArduPilot heartbeat.
- GPS and battery remain not ready if the corresponding messages are
  absent.
- Disconnecting USB changes `connected` to `false` after the freshness
  timeout or terminates with a serial error. Automatic device reopening
  is a later milestone.
- `camera.phase` changes from `starting` to `streaming`.
- On the verified Pi 5 and IMX708 Wide setup, the runtime sustained
  `30.013 FPS` with zero processing drops and approximately `10 ms`
sensor-to-application latency.

It also enables a 10 FPS grayscale diagnostic preview:

```text
http://companionpi.local:8080/
```

The page shows the exact Y plane consumed by the AprilTag detector and
overlays only confirmed targets. It is unauthenticated HTTP intended for
the local trusted bench network. Disable it with:

```bash
ONBOARD_AUTONOMY_CAMERA_PREVIEW_ENABLED=0 \
  bin/run_onboard_autonomy_pi.sh
```

## Windows launcher

`StartOnboardAutonomyPixhawk.cmd` opens the Raspberry Pi runtime over SSH
and then opens the local camera-preview page. Machine-specific values
belong in the ignored `OnboardAutonomyLocal.cmd` file at the repository
root:

```bat
set "ONBOARD_AUTONOMY_PI_HOST=companionpi.local"
set "ONBOARD_AUTONOMY_PI_USER=companion"
set "ONBOARD_AUTONOMY_SSH_KEY=%USERPROFILE%\.ssh\onboard_autonomy_ed25519"
set "ONBOARD_AUTONOMY_REMOTE_ROOT=/home/companion/onboard_autonomy-pi5"
set "ONBOARD_AUTONOMY_SERIAL=/dev/serial/by-id/<your-pixhawk-device>"
```

The launcher lets the Pi auto-detect one serial device when
`ONBOARD_AUTONOMY_SERIAL` is unset. It also reads an existing ignored
`CompanionLabLocal.cmd` and maps its legacy variables during the rename
transition; this compatibility path is not part of the public runtime
configuration.

## UART milestone

The Pixhawk TELEM port uses serial signaling and requires correct voltage,
ground, TX/RX crossing, port configuration, and connector pinout. Do not
guess the wiring. We will use the Pixhawk 6C and Raspberry Pi 5 hardware
documentation before enabling this stage.
