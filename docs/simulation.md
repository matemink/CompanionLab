# Gazebo simulation runbook

Gazebo owns the 3D world, sensors, and vehicle physics. ArduCopter SITL
owns the flight-control loop. OnboardAutonomy sends MAVLink commands,
observes acknowledgements and telemetry, and advances guarded scenarios.

## Prerequisites

Build ArduCopter SITL first by following
[development.md](development.md). Then install Gazebo Harmonic and build
the pinned official ArduPilot Gazebo plugin:

```bash
bash scripts/install_gazebo_harmonic.sh
```

The installer records and verifies the plugin commit rather than
building an arbitrary moving branch.

## Start the simulation

Run the three processes in separate terminals:

```bash
bash scripts/run_gazebo_apriltag.sh
bash scripts/run_arducopter_gazebo.sh
ONBOARD_AUTONOMY_INTERACTIVE=1 \
    bash scripts/run_onboard_autonomy_gazebo_vision.sh
```

On Windows, `StartOnboardAutonomyGazeboDemo.cmd` launches the same stack
in visible WSL/WSLg windows.

| Process | Responsibility |
| --- | --- |
| Gazebo Harmonic | Iris model, world physics, and simulated camera |
| ArduCopter SITL | Stabilization, navigation modes, arming, and landing |
| MAVProxy | MAVLink routing and an optional flight console |
| OnboardAutonomy | Scenario state machine, commands, telemetry confirmation, and operator TUI |

## Interactive scenarios

The operator console accepts five scenario triggers:

| Key | Scenario | Behavior |
| --- | --- | --- |
| `1` | Hover | Take off, hold, and land |
| `2` | Out and RTL | Fly 15 m north, return, and land at home |
| `3` | Square | Fly a 10 m square, then return to launch |
| `4` | Search | Fly a 24 m by 12 m search pattern, then RTL |
| `5` | Precision | Approach and land on the camera-observed AprilTag pad |

`L` requests an immediate LAND and cancels the active scenario. `Q`
exits the runtime.

Command steps wait for `COMMAND_ACK`. Route steps compare
`LOCAL_POSITION_NED` telemetry with their target. RTL and landing finish
only after the vehicle reports `DISARMED`; an accepted command alone is
not treated as completed motion.

The precision scenario moves 3 m north and 1.5 m east after takeoff so the
complete marker remains inside the landing camera field of view. It requires
one second of continuously fresh confirmed observations before requesting
LAND, then streams body-FRD `LANDING_TARGET` messages at 5 Hz. A target older
than 250 ms is not sent to ArduPilot.

Run the non-interactive acceptance flight with compact JSON telemetry:

```bash
ONBOARD_AUTONOMY_SCENARIO=5 \
ONBOARD_AUTONOMY_EXIT_AFTER_SCENARIO=1 \
ONBOARD_AUTONOMY_JSON=1 \
    bash scripts/run_onboard_autonomy_gazebo_vision.sh
```

## Operator console

The terminal presents a bounded MS-DOS-style control panel rather than
an unbounded log stream. It keeps visible:

- the Raspberry Pi 5 and Pixhawk 6C link;
- current mode, arm state, altitude, GPS, battery, and warnings;
- active scenario and step progress;
- the latest complete MAVLink frame sent and received;
- semantic command, acknowledgement, and state-confirmation events.

TX and RX wires pulse only while a fresh complete MAVLink frame is
observed. The animation is presentation state and does not alter protocol
or domain behavior.

## Simulated landing camera

The project world mounts a fixed downward camera under the Iris and places a
`tagStandard41h12` landing pad at home. Gazebo sends `640x480` H.264 over RTP
to UDP port `5601`; `GStreamerCameraSource` decodes it to I420 and publishes
the same `CameraFrame` type used by Camera Module 3.

The simulator calibration is derived from the SDF field of view and stored
in `config/gazebo-landing-camera-640x480.json`. The pad texture, two-metre
detection span, camera geometry, and calibration agreement are guarded by
`python/tests/test_gazebo_apriltag_world.py`.

The camera mount is described independently in
`config/gazebo-landing-camera-extrinsics.json`. It rotates OpenCV camera
optical coordinates `[right, down, forward]` into MAVLink body FRD and adds
the measured 0.16 m camera offset below the simulated body origin.

Open the OnboardAutonomy preview at `http://localhost:8080/`. A complete tag
is not expected while the vehicle rests directly on top of the pad because
the camera is too close to see all four corners. Validate acquisition after
takeoff.

The verified acceptance run and its known close-range limitation are recorded
in [precision-landing-sitl.md](evidence/precision-landing-sitl.md).

## Reference gimbal stream

The official Iris world exposes an RTP/H.264 gimbal-camera stream on UDP
port `5600`. Verify a bounded 60-frame decode:

```bash
bash scripts/check_gazebo_camera_stream.sh
```

Open the stream in a WSLg window:

```bash
bash scripts/view_gazebo_camera.sh
```

Run only one receiver at a time. The check enables the stream, decodes a
bounded frame count, and disables it during cleanup.

## WSLg rendering

The launcher selects the D3D12 Mesa path when `/dev/dxg` is available.
If Gazebo opens without a visible scene or falls back to slow software
rendering, use the verified troubleshooting record in
[learning/11-wslg-gpu-acceleration.uk.md](learning/11-wslg-gpu-acceleration.uk.md).

The rolled-back decorative-airfield experiment is documented separately
in
[learning/14-gazebo-airfield-failed-experiment.uk.md](learning/14-gazebo-airfield-failed-experiment.uk.md)
so an unsuccessful GUI path is not presented as a working feature.

## Safety boundary

Automated scenario motion is enabled only for UDP/SITL. The executable
rejects demo and interactive motion modes when a serial transport is
selected. Gazebo scenarios must not be reused as motor-test procedures on
physical hardware.
