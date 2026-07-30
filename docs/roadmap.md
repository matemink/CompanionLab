# Roadmap

The roadmap is ordered to keep each milestone demonstrable and testable.

## Project direction

The goal is a production-shaped companion-computer prototype that is
directly relevant to the target Software Engineer vacancy: C++ and
Python, MAVLink, Embedded Linux, ARM deployment, video processing,
hardware interfaces, automated tests, and measurable performance.

The terminal dashboard, scenario selector, one-command launchers,
camera preview, activity animation, readable reports, and other
operator conveniences are intentional engineering deliverables. They
make command flow observable, experiments repeatable, and hardware
failures diagnosable. They are not considered a deviation from the
autonomy work.

The next feature priority is the complete vision-to-guidance vertical
slice. Further interface work should solve a concrete usability or
diagnostic problem rather than delay that slice.

## Immediate portfolio hygiene

This work can proceed alongside the technical milestones.

- [x] Audit ignored/generated files and remove machine-specific or
  sensitive data from the publishable project.
- [x] Create one honest initial Git snapshot; do not manufacture past
  history.
- [ ] Use focused commits for each subsequent feature and test.
- [ ] Publish the repository only after a clean-build and secret scan.
- [ ] Store useful test and benchmark reports as CI artifacts.

## Milestone 1: Telemetry foundation

Status: implemented and verified on Ubuntu 24.04 under WSL2.

- C++20 project and CMake build.
- MAVLink UDP and Linux serial transports.
- Minimum health-message decoder.
- Freshness-aware vehicle state.
- Native tests and Python scenario generator.
- End-to-end Python-to-C++ UDP integration check.
- ARM64 cross-build workflow.

Portfolio evidence: C++, MAVLink integration, architecture, testing, Git.

## Milestone 2: ArduPilot SITL integration

Status: in progress and running locally against ArduCopter 4.6.3.

- [x] Install Ubuntu with WSL2.
- [x] Build and run ArduCopter SITL.
- [x] Route MAVLink telemetry to CompanionLab.
- [x] Let CompanionLab configure its required message rates and verify
  `COMMAND_ACK` responses.
- [x] Start and stop the complete SITL stack from Python.
- [x] Assert healthy telemetry, command acknowledgements, and process
  cleanup in a repeatable smoke test.
- [x] Inject heartbeat loss and assert stale connection handling.
- [x] Inject simulated GPS loss and assert readiness handling.
- [x] Drain a simulated battery and assert the 20% readiness threshold.
- [x] Trigger a real ArduPilot PreArm failure and assert its full
  MAVLink-to-domain path.
- [x] Visualize outbound commands, inbound acknowledgements, and
  telemetry confirmations in a bounded live terminal view.
- [x] Trigger the guarded demo scenario or a manual LAND command from
  the live terminal while keeping serial hardware blocked.
- [x] Replace the fixed demo with a typed C++ Scenario Runner and five
  selectable SITL scenarios.
- [x] Verify local-NED route steps, RTL, and a synthetic MAVLink
  `LANDING_TARGET` integration path.

Portfolio evidence: Python, Embedded Linux, ArduPilot, integration
tests, observability, and developer tooling.

## Milestone 3: Video pipeline

- [x] Pin the official ArduPilot Gazebo plugin and add reproducible
  install and launch scripts.
- [x] Install Gazebo Harmonic and verify WSLg/Ogre2 rendering.
- [x] Run the official Iris world with ArduCopter and CompanionLab.
- [x] Execute and verify an automated GUIDED takeoff, hold, and landing
  from CompanionLab.
- [x] Stream the simulated camera through GStreamer and verify decoded
  frames with a bounded smoke test.
- [x] Receive Raspberry Pi Camera Module 3 Wide frames through
  `rpicam`/libcamera on Raspberry Pi 5.
- [x] Benchmark raw YUV420 frame cadence, estimated drops, process CPU,
  RSS, and sensor metadata with machine-readable reports.
- [x] Measure sensor-to-application frame latency inside the C++ runtime
  receiver using per-frame `FrameWallClock` metadata.
- [x] Run Camera Module 3 and Pixhawk telemetry concurrently at 30 FPS
  without blocking the application loop.
- [ ] Reconnect after camera or stream loss.

Portfolio evidence: GStreamer, video streaming, profiling, ARM.

Experiment note: a project-owned decorative airfield was implemented,
tested, and rolled back because the Gazebo GUI displayed only its
background instead of the scene entities. The server-side world,
camera, physics, and automated flight continued to run, but the
user-visible result did not satisfy the acceptance criterion. See
`docs/learning/14-gazebo-airfield-failed-experiment.uk.md`.

## Milestone 4: Vision and precision landing

- [x] Integrate the official AprilTag 3 detector, typed pixel-space
  observations, processing metrics, and a generated-marker unit test.
- [x] Validate a physical `tagStandard41h12` marker through the Camera
  Module 3 pipeline at 301/301 detected frames over a 10-second window.
- [ ] Calibrate Camera Module 3 Wide and store reproducible camera
  intrinsics and distortion coefficients.
- [ ] Estimate the AprilTag 3D pose and expose position, orientation,
  confidence, and observation freshness through the domain model.
- [ ] Transform camera coordinates into the MAVLink/body coordinate
  frame and validate axes, signs, units, and timestamps with tests.
- [ ] Filter noisy measurements without hiding stale or lost targets.
- [ ] Replace the synthetic SITL target provider with real AprilTag
  `LANDING_TARGET` observations.
- [ ] Validate the complete precision-landing sequence in Gazebo.
- [ ] Test lost-target, reacquisition, outlier, and noisy-observation
  behavior.

Portfolio evidence: computer vision, guidance integration, algorithms.

## Milestone 5: Raspberry Pi deployment

- [x] Add a reproducible ARM64 Release package candidate.
- [x] Add read-only hardware diagnostics, deterministic Pixhawk serial
  discovery, and JSONL telemetry logging.
- [x] Provision Raspberry Pi OS Lite 64-bit and verify headless
  public-key SSH access.
- [x] Deploy and run the cross-built ARM64 package on Raspberry Pi 5.
- [x] Connect Pixhawk 6C over USB and capture real MAVLink telemetry.
- [x] Read ArduPilot `BATT_ARM_VOLT` and verify battery readiness
  against a real pre-arm failure.
- [x] Expose the six acknowledged telemetry-rate requests and verify
  documented `AUTOPILOT_VERSION` metadata on the real Pixhawk 6C.
- [x] Resolve every pinned ArduPilot bootloader board ID through the
  full official table while preserving ambiguous aliases.
- [x] Visualize actual complete TX/RX MAVLink frames with bounded
  freshness and a tested live activity pulse.
- [ ] Build natively on Raspberry Pi 5 as a toolchain comparison.
- [x] Connect and identify Camera Module 3 Wide (Sony IMX708).
- [x] Deploy the C++ camera receiver and verify 30.013 FPS, zero
  processing drops, and approximately 10 ms latency on Raspberry Pi 5.
- [x] Add a read-only browser camera preview from the same Y plane used
  by AprilTag, with target ID and corner overlay.
- [ ] Add a systemd service and JSONL log rotation.
- [ ] Profile and optimize the ARM release build.
- [ ] Validate graceful restart and device reconnection.

Portfolio evidence: Embedded Linux, ARM Cortex, target deployment.

## Milestone 6: Hardware interfaces and delivery

- [ ] Move from USB to the documented Pixhawk TELEM/UART connection.
- [ ] Add one documented I2C or GPIO peripheral if useful.
- [ ] Add a reproducible deployment image or Yocto proof of concept.
- [ ] Record a short architecture and demonstration video.
- [ ] Publish diagrams, test evidence, performance numbers, and
  limitations.

Portfolio evidence: UART, I2C/GPIO, deployment, technical communication.
