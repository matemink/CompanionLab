# OnboardAutonomy

OnboardAutonomy is a C++20 onboard autonomy runtime for ArduPilot-based
UAVs. It combines MAVLink command and telemetry handling, onboard computer
vision, software-in-the-loop verification, and Raspberry Pi 5 deployment.

The long-term demonstration is a precision-landing pipeline:

1. Gazebo or Raspberry Pi Camera Module 3 provides video.
2. A camera adapter delivers typed frames to the companion service.
3. The official AprilTag 3 library detects a landing marker.
4. The companion sends MAVLink `LANDING_TARGET` observations.
5. ArduPilot performs the landing in SITL.

Real flight is not required. The Pixhawk 6C is used on a propeller-free
bench to validate the physical MAVLink link.

## Current milestone

Milestone 1 provides:

- C++20 MAVLink stream decoding using the generated official C library.
- UDP transport for ArduPilot SITL.
- Linux serial transport for a Pixhawk USB or UART link.
- `HEARTBEAT`, `GPS_RAW_INT`, `BATTERY_STATUS`, `SYS_STATUS`, and
  `STATUSTEXT` handling.
- A 1 Hz companion heartbeat using system ID discovery and component
  ID `191`.
- Self-configuration of required telemetry rates through
  `MAV_CMD_SET_MESSAGE_INTERVAL`, sequential `COMMAND_ACK` handling,
  and bounded retries.
- A one-shot `AUTOPILOT_VERSION` request with documented firmware and
  board metadata instead of model-name guessing.
- A pinned full ArduPilot bootloader board catalog with honest
  duplicate-ID alias handling and numeric fallback for unknown IDs.
- A color terminal dashboard for operators and an explicit `--json`
  mode for automation.
- A thread-safe vehicle health model with stale-data handling.
- JSON health snapshots for scripts and integration tests.
- Tests for partial MAVLink frames, missing data, stale heartbeat, and
  PreArm warnings.
- Python MAVLink generators for healthy and failed scenarios.
- Native Linux and ARM64 CI builds.

## Repository layout

```text
include/onboard_autonomy/domain/                Vehicle concepts and rules
include/onboard_autonomy/application/           Use cases and snapshots
include/onboard_autonomy/application/ports/     I/O contracts owned by application
include/onboard_autonomy/adapters/              MAVLink and transport adapters
include/onboard_autonomy/presentation/          Presentation interfaces
src/domain/                                 Protocol-independent vehicle state
src/application/                            Use-case orchestration
src/adapters/mavlink/                       MAVLink protocol adapter
src/adapters/ardupilot/                     ArduPilot metadata adapter
src/adapters/transport/                     UDP and serial implementations
src/presentation/console/                   Console presentation adapter
tests/                                      Dependency-light C++ tests
python/                                     SITL and fault-injection tooling
cmake/toolchains/                           ARM64 cross-compilation
docs/                                       Architecture and hardware setup
third_party/ardupilot/                      Pinned board table and license
.github/workflows/                          Native and ARM64 CI
```

CMake enforces the main dependency boundaries through separate
`onboard_autonomy_domain`, `onboard_autonomy_transport_port`,
`onboard_autonomy_mavlink_adapter`, `onboard_autonomy_transport_adapter`,
`onboard_autonomy_application`, and `onboard_autonomy_console_presentation`
targets.

## Build on Ubuntu or Raspberry Pi OS

Install the toolchain:

```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake git ninja-build python3-venv
```

Configure, build, and test:

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
python3 -m unittest discover -s python/tests -v
```

CMake downloads a pinned revision of the generated MAVLink C headers
into the build directory. MAVLink framing is not reimplemented here.

For a new Ubuntu environment, the repository includes an idempotent
toolchain bootstrap:

```bash
sudo bash scripts/bootstrap_ubuntu.sh
```

## Run with generated UDP telemetry

Start the companion:

```bash
./build/onboard_autonomy --udp-port 14550
```

In another terminal, prepare the Python environment and send a healthy
scenario:

```bash
python3 -m venv .venv
source .venv/bin/activate
python -m pip install -r python/requirements.txt
python python/scenario_runner.py --scenario healthy
```

Try the failure scenarios:

```bash
python python/scenario_runner.py --scenario no-gps
python python/scenario_runner.py --scenario low-battery
python python/scenario_runner.py --scenario prearm
```

## Run with ArduCopter SITL

After installing and building ArduPilot `Copter-4.6.3`, start the
virtual flight controller:

```bash
bash scripts/run_arducopter_sitl.sh
```

In a second Ubuntu terminal, start OnboardAutonomy:

```bash
bash scripts/run_onboard_autonomy_sitl.sh
```

ArduCopter and OnboardAutonomy exchange MAVLink over
`udp://127.0.0.1:14550`. OnboardAutonomy discovers the vehicle system ID
and advertises itself as onboard-computer component `191`. It then
requests health at 1 Hz, GPS at 2 Hz, and battery data at 1 Hz. MAVProxy
does not request a competing stream rate. Stop either process with
`Ctrl+C`.

The interactive command shows the terminal dashboard by default. Add
`--json` when another process needs machine-readable snapshots.

On Windows, `StartOnboardAutonomyDemo.cmd` opens both processes in separate
visible WSL consoles.

Inspect the latest `SYS_STATUS` sensor masks recorded by MAVProxy:

```bash
python python/inspect_tlog.py ~/src/ardupilot-Copter-4.6.3/mav.tlog
```

The complete Windows-to-WSL startup path is documented in
`docs/learning/02-sitl-launch.uk.md`.

The companion prints one JSON snapshot per second. It returns to
`"connected":false` when heartbeat data becomes stale.

Run the automated end-to-end UDP check:

```bash
python python/run_integration_check.py \
    --companion ./build/onboard_autonomy
```

Run the complete ArduCopter SITL stack without opening manual
terminals:

```bash
python python/run_sitl_smoke_test.py
```

Inject a complete MAVLink link loss after initial telemetry:

```bash
python python/run_sitl_smoke_test.py --scenario heartbeat-loss
```

Disable the simulated GPS after reaching a healthy baseline:

```bash
python python/run_sitl_smoke_test.py --scenario gps-loss
```

Drain a small simulated battery through ArduPilot's current integrator:

```bash
python python/run_sitl_smoke_test.py --scenario low-battery
```

Force an invalid motor spin configuration and verify ArduPilot's real
`PreArm` warning:

```bash
python python/run_sitl_smoke_test.py --scenario prearm
```

The smoke test starts ArduCopter, MAVProxy, and OnboardAutonomy; verifies
required telemetry, interval commands, accepted acknowledgements, and
the companion heartbeat; requires a genuinely ready GPS, battery, and
system baseline; stores logs under `artifacts/sitl-smoke/`; then
terminates every child process. The PreArm scenario changes
`MOT_SPIN_ARM` only inside the isolated SITL EEPROM created for that
test; it never arms the vehicle or drives motors.

## Run the Gazebo Iris integration

Install Gazebo Harmonic and build the pinned official ArduPilot plugin:

```bash
bash scripts/install_gazebo_harmonic.sh
```

Then start the three processes in separate terminals:

```bash
bash scripts/run_gazebo_iris.sh
bash scripts/run_arducopter_gazebo.sh
ONBOARD_AUTONOMY_DEMO_FLIGHT=1 bash scripts/run_onboard_autonomy_sitl.sh
```

On Windows, `StartOnboardAutonomyGazeboDemo.cmd` opens all three WSL
terminals. Gazebo owns the 3D world and vehicle physics, ArduCopter SITL
owns the flight-control logic, and OnboardAutonomy uses MAVLink on UDP port
`14550`. Its interactive terminal exposes five guarded SITL scenarios:

- `[1] HOVER`: take off, hold, and land.
- `[2] OUT+RTL`: fly 15 m north, then return and land at home.
- `[3] SQUARE`: fly a 10 m square, then RTL.
- `[4] SEARCH`: fly a 24 m by 12 m search grid, then RTL.
- `[5] PRECISION`: offset from home and precision-land using a synthetic
  `LANDING_TARGET` source. Real Camera/AprilTag pixel observations now
  exist but are not yet converted into pose or connected to this
  SITL-only source.

`[L]` requests an immediate LAND and cancels the active scenario; `[Q]`
exits. Command steps require `COMMAND_ACK`, movement steps are confirmed
from `LOCAL_POSITION_NED`, and landing completes only after `DISARMED`.

The OnboardAutonomy terminal uses a fixed MS-DOS-style control panel.
An ASCII companion computer and flight controller are connected by two
MAVLink wires. The wires briefly show the latest complete frame actually
transmitted or received, using the generated MAVLink message name, and
pulse while traffic is fresh. Semantic event history remains separate.
Current mode, altitude, GPS, battery, warnings, and scenario state remain
visible while Gazebo shows the same vehicle moving in 3D.

The demo uses the official `iris_runway.sdf` world. With Gazebo running,
verify that its gimbal camera produces a decodable RTP/H.264 stream:

```bash
bash scripts/check_gazebo_camera_stream.sh
```

The check enables the camera, decodes 60 frames from UDP port `5600`,
then disables the stream. To inspect the same stream in a WSLg window:

```bash
bash scripts/view_gazebo_camera.sh
```

Run only one receiver at a time. The project does not yet add a landing
marker or computer-vision processing.

## Run with Pixhawk 6C on Raspberry Pi 5

Create the ARM64 deployment candidate on the Ubuntu development host:

```bash
bash scripts/package_pi5_release.sh
```

After transferring and extracting the archive on the Pi:

```bash
bin/diagnose_pi_hardware.sh
bin/run_onboard_autonomy_pi.sh
bin/benchmark_pi_camera.sh
```

The launcher selects a single `/dev/serial/by-id`, `/dev/ttyACM`, or
`/dev/ttyUSB` candidate, refuses ambiguous hardware, writes JSONL
telemetry under `~/.local/state/onboard_autonomy`, and never enables the
interactive flight scenarios. It also starts the C++ Camera Module
receiver by default at `640x480 YUV420 @ 30 FPS`. Every JSON snapshot
includes camera phase, consumed FPS, dropped frames, frame age, and
sensor-to-application latency. Set `ONBOARD_AUTONOMY_CAMERA_ENABLED=0` to
run telemetry only.

The launcher also enables the read-only grayscale camera preview:

```text
http://companionpi.local:8080/
```

It uses the same Y plane consumed by AprilTag and overlays confirmed
target corners, center, and ID. No second camera process is started.
`StartOnboardAutonomyPixhawk.cmd` starts the Pi runtime and opens the
preview automatically. Set `ONBOARD_AUTONOMY_CAMERA_PREVIEW_ENABLED=0` to
disable it.

The Windows launcher defaults to `companion@companionpi.local` and lets
the Pi launcher auto-detect one serial device. Machine-specific values
belong in the ignored `OnboardAutonomyLocal.cmd` file:

```bat
set "ONBOARD_AUTONOMY_PI_HOST=companionpi.local"
set "ONBOARD_AUTONOMY_PI_USER=companion"
set "ONBOARD_AUTONOMY_SSH_KEY=%USERPROFILE%\.ssh\onboard_autonomy_ed25519"
set "ONBOARD_AUTONOMY_REMOTE_ROOT=/home/companion/onboard_autonomy-pi5"
set "ONBOARD_AUTONOMY_SERIAL=/dev/serial/by-id/<your-pixhawk-device>"
```

During the rename transition, the launcher also reads an existing ignored
`CompanionLabLocal.cmd` and maps its legacy variables automatically.

A cross-built binary still has to pass the runtime-library check on the
target Raspberry Pi OS; a native Pi build is the compatibility fallback.

The camera benchmark is bounded and hardware-read-only. Its default
profile captures 300 `1280x720 YUV420` frames, measures cadence and
estimated gaps from PTS, samples process CPU/RSS, and writes JSON plus
Markdown reports under `~/.local/state/onboard_autonomy/camera`.

The bounded benchmark validates the camera independently. The regular
launcher validates the integrated runtime where Pixhawk serial traffic
and camera frames are handled concurrently.

Prepare a Camera Module 3 calibration dataset with the printable
`assets/calibration/checkerboard-9x6-25mm-a4.svg` target:

```bash
python3 -m venv .venv
.venv/bin/python -m pip install -r python/requirements.txt
COMPANIONLAB_PYTHON=.venv/bin/python \
    bash scripts/capture_camera_calibration.sh
```

In the extracted ARM64 package, use its root-level `requirements.txt`
instead of `python/requirements.txt`.

The capture uses the runtime's `rpicam-vid` pipeline, `640x480`
resolution, and fixed `manual/default` hyperfocal lens policy. Keeping
the lens position fixed prevents camera intrinsics from changing between
calibration and AprilTag pose estimation. The Python/OpenCV analyzer
accepts only complete checkerboard views, calculates pinhole intrinsics
and Brown-Conrady distortion, records input hashes and per-view
reprojection errors, and returns failure when the quality gate is not
met. No sample intrinsics are committed as if they belonged to the
physical camera.

See [docs/raspberry-pi-5-bench.md](docs/raspberry-pi-5-bench.md) before
connecting hardware.

## Safety boundary

Normal OnboardAutonomy startup remains read-only. Automated motion requires
the explicit `--demo-flight` flag or a keyboard trigger enabled by
`--interactive`. The application rejects both modes when a serial
transport is selected, and interactive input additionally requires a
live terminal. All initial hardware work must be performed with
propellers removed. Autonomous motion is developed and tested in
SITL/Gazebo before any hardware-in-the-loop work.

## Roadmap

See [docs/roadmap.md](docs/roadmap.md) for the staged path from telemetry
foundation to simulated precision landing.

## Learning track

This repository is also a guided transition from senior Android
engineering to C++, Embedded Linux, and UAV systems. Ukrainian learning
notes start at
[docs/learning/README.uk.md](docs/learning/README.uk.md).
