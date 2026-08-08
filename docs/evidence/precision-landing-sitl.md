# Precision landing SITL evidence

Verified on 2026-08-08 with ArduCopter 4.6.3, Gazebo Sim 8.14.0, and the
project-owned `apriltag_landing` world.

## Acceptance path

```text
Gazebo landing camera
  -> RTP/H.264
  -> GStreamer I420 frame
  -> AprilTag pose in camera-optical coordinates
  -> freshness-aware confirmed track
  -> configured camera-to-body-FRD transform
  -> MAVLink LANDING_TARGET at 5 Hz
  -> ArduCopter precision-land backend
  -> touchdown and DISARMED
```

The production runtime waited for readiness, entered GUIDED, armed, took off
to 8.04 m, and handed control from `FlightStartupController` to
`AutonomyRuntime`. It acquired tag ID 0 directly from current camera state,
requested LAND only after the target warmup, and exited with code zero after
telemetry-confirmed automatic disarm. No numbered scenario, fixed route, or
Python-issued flight command participated in the run.

## Protocol evidence

The MAVProxy tlog was inspected with `python/inspect_tlog.py`:

| Check | Observed |
| --- | --- |
| Arm state | `DISARMED -> ARMED -> DISARMED` |
| Modes | `STABILIZE (0), GUIDED (4), LAND (9)` |
| Flight command ACKs | GUIDED, ARM, TAKEOFF, and LAND accepted |
| `LANDING_TARGET` count | 78 |
| MAVLink frame | 12, `MAV_FRAME_BODY_FRD` |
| `position_valid` | 1 |
| Maximum relative altitude | `8.04 m` |
| First target F/R/D | `-0.013 / 0.003 / 7.622 m` |
| Last target F/R/D | `0.000 / -0.001 / 1.562 m` |
| Final local N/E/D | `0.000 / 0.000 / -0.200 m` |
| Final horizontal error | `0.000 m` |

## Target loss behavior

The live run exercised the expected close-range target loss. OnboardAutonomy
stopped sending observations older than 250 ms immediately; because LAND was
already accepted, ArduPilot continued the ordinary final descent. Unit tests
separately verify interrupted warmup, reacquisition, smoothing, expiry,
confidence rejection, corrected-bit rejection, and protection against
switching between tag IDs. A production-runtime test also verifies the
five-second fallback LAND when vision is unavailable before LAND starts.

The two-metre marker no longer fits fully in the 640x480 image below roughly
2 m. With the simulation profile's `PLND_STRICT=0`, ArduPilot completed the
remaining vertical descent as a normal landing. This is an explicit v1.0
simulation limitation, not evidence that the physical camera geometry has
been validated.

## Remaining hardware gate

Before real-aircraft guidance is enabled, repeat the metric scale check with
the printed marker at measured distances, record the physical camera mount
extrinsics, and review the ArduPilot precision-landing parameters. Serial
hardware motion remains blocked by the application safety policy.

## Reproduction

The complete run, cleanup, final JSON assertion, and independent tlog checks
are automated by:

```bash
source ~/venv-ardupilot/bin/activate
python python/autonomy_sitl_acceptance.py
```

The recorded acceptance summary was:

```text
PASSED
Path: readiness -> GUIDED -> ARM -> TAKEOFF -> vision LAND
Evidence: 78 LANDING_TARGET, 8.04 m max, 0.000 m error
```
