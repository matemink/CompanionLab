# OnboardAutonomy

[![CI](https://github.com/matemink/OnboardAutonomy/actions/workflows/ci.yml/badge.svg)](https://github.com/matemink/OnboardAutonomy/actions/workflows/ci.yml)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-00599C.svg)](https://isocpp.org/)
[![Platform](https://img.shields.io/badge/Platform-Linux%20x86__64%20%7C%20ARM64-FCC624.svg)](https://www.raspberrypi.com/)

OnboardAutonomy is a C++20 onboard autonomy runtime for ArduPilot-based
UAVs. It runs against ArduCopter SITL or a physical Pixhawk, handles
MAVLink command and telemetry flows, processes onboard camera frames,
and exposes vehicle state through an operator console and JSON snapshots.

The project is building toward vision-guided precision landing on a
Raspberry Pi 5 and Pixhawk 6C. Development remains reproducible without
real flight: motion is verified in SITL and Gazebo, while hardware work
is performed on a propeller-free bench.

## System overview

```mermaid
flowchart LR
    Camera["Camera Module 3"]

    subgraph Pi["Raspberry Pi 5"]
        Runtime["OnboardAutonomy"]
    end

    subgraph FC["Pixhawk 6C"]
        Firmware["ArduPilot firmware"]
    end

    Camera --> Runtime
    Runtime --> Firmware
    Runtime --> SITL["ArduPilot SITL"]
    SITL <--> Gazebo["Gazebo Harmonic"]

    classDef hardware fill:#FFF3C4,stroke:#B7791F,color:#3D2C00,stroke-width:2px
    classDef software fill:#DCEBFF,stroke:#2563EB,color:#0F2A52,stroke-width:2px
    class Camera hardware
    class Runtime,Firmware,SITL,Gazebo software
    style Pi fill:#FFF8DE,stroke:#B7791F,stroke-width:2px,color:#3D2C00
    style FC fill:#FFF8DE,stroke:#B7791F,stroke-width:2px,color:#3D2C00
```

Amber containers and nodes represent physical hardware; blue nodes
represent software.

ArduPilot remains responsible for stabilization and flight control.
OnboardAutonomy owns companion-computer concerns: telemetry, health,
scenario orchestration, vision, diagnostics, and future landing-target
guidance. The diagram shows the target deployment: camera frames enter
OnboardAutonomy on Raspberry Pi 5, which sends guidance through either
ArduPilot firmware on a physical Pixhawk or ArduPilot SITL. In simulation,
the same runtime can run on the Ubuntu/WSL development host instead of the
Pi.

## Capabilities

- MAVLink 2 decoding and encoding through pinned generated C headers.
- UDP transport for SITL and Linux serial transport for Pixhawk USB/UART.
- Freshness-aware GPS, battery, system-health, PreArm, and link state.
- Sequential telemetry-rate configuration with `COMMAND_ACK`, timeouts,
  and bounded retries.
- ArduPilot firmware and board metadata without model-name guessing.
- Five guarded SITL scenarios with command acknowledgements and telemetry
  confirmation: hover, out-and-RTL, square, search, and precision landing.
- Raspberry Pi Camera Module 3 and Gazebo RTP/H.264 ingestion into the same
  YUV420 pipeline, with performance metrics, AprilTag detection, and a
  read-only browser preview.
- Native Linux tests, Python integration tests, fault injection, and an
  ARM64 cross-build quality gate in GitHub Actions.

## Verified environments

| Environment | Evidence |
| --- | --- |
| Ubuntu 24.04 / WSL2 | Native C++ build, unit tests, ArduCopter SITL, and fault injection |
| Gazebo Harmonic | Automated takeoff, route scenarios, RTL, landing, and H.264 camera stream |
| Raspberry Pi 5 | ARM64 runtime, Camera Module 3 Wide, and concurrent camera/MAVLink processing |
| Pixhawk 6C | Real USB MAVLink telemetry, health state, metadata, and acknowledged stream requests |

The measured Raspberry Pi camera path sustained `30.013 FPS` with zero
processing drops and approximately `10 ms` sensor-to-application latency
on the documented bench setup.

## Quick start

Ubuntu 24.04 or Raspberry Pi OS 64-bit is recommended.

```bash
sudo bash scripts/bootstrap_ubuntu.sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ctest --test-dir build --output-on-failure
python3 -m unittest discover -s python/tests -v
```

Run the service with generated healthy MAVLink telemetry:

```bash
./build/onboard_autonomy --udp-port 14550
```

In a second terminal:

```bash
python3 -m venv .venv
source .venv/bin/activate
python -m pip install -r python/requirements.txt
python python/scenario_runner.py --scenario healthy
```

Continue with the [development and SITL](docs/development.md),
[Gazebo simulation](docs/simulation.md), or
[Raspberry Pi/Pixhawk bench](docs/raspberry-pi-5-bench.md) runbook.

## Architecture

The code follows dependency inversion without introducing a framework:

```text
domain <- application <- adapters / presentation
```

Application-owned ports isolate MAVLink transport, camera capture,
target detection, and camera preview. CMake targets enforce the main
build-time boundaries, while test fakes exercise application behavior
without SITL, camera hardware, or a serial device.

| Path | Responsibility |
| --- | --- |
| `include/onboard_autonomy/domain/` | Vehicle concepts and readiness rules |
| `include/onboard_autonomy/application/` | Use cases and I/O ports |
| `src/adapters/` | MAVLink, transport, camera, preview, and AprilTag adapters |
| `src/presentation/` | Operator console |
| `tests/` | Dependency-light C++ tests |
| `python/` | Integration harnesses, fault injection, and camera tooling |
| `scripts/` | Reproducible development, simulation, and deployment commands |
| `docs/` | Architecture, runbooks, roadmap, and learning notes |

See [docs/architecture.md](docs/architecture.md) for the complete runtime
and build-time dependency diagrams.

## Project status

The telemetry, command, simulation, ARM deployment, camera ingestion,
calibrated AprilTag pose, and target-tracking stages are implemented. The
project-owned Gazebo world streams a downward camera over RTP/H.264 through
the same application camera port used by the rest of the vision pipeline.

The simulated precision scenario now completes the full vertical slice:
AprilTag pose, confirmed fresh track, camera-optical to body-FRD transform,
5 Hz MAVLink `LANDING_TARGET`, ArduPilot LAND, touchdown, and DISARMED. The
verified run finished 0.456 m from the marker center. Physical printed-target
scale validation remains required before enabling this guidance path on a
real aircraft. See [docs/roadmap.md](docs/roadmap.md).

## Safety

Normal startup is observation-only. Automated motion requires an
explicit SITL-only flag or interactive trigger. The executable rejects
motion scenarios when a serial transport is selected. Physical bench
work is performed with propellers removed, and autonomous behavior is
validated in simulation before hardware-in-the-loop testing.

## Documentation

- [Architecture](docs/architecture.md)
- [Development and SITL runbook](docs/development.md)
- [Gazebo simulation runbook](docs/simulation.md)
- [Raspberry Pi 5 and Pixhawk 6C bench](docs/raspberry-pi-5-bench.md)
- [Camera calibration workflow](docs/learning/29-camera-calibration.uk.md)
- [AprilTag target tracking](docs/learning/30-apriltag-target-tracking.uk.md)
- [Roadmap](docs/roadmap.md)
