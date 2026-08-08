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

The scenario took off to 7.93 m, moved 3 m north and 1.5 m east, acquired
tag ID 0, requested LAND only after the target warmup, and exited with code
zero after automatic disarm.

## Protocol evidence

The MAVProxy tlog was inspected with `python/inspect_tlog.py`:

| Check | Observed |
| --- | --- |
| Arm state | `DISARMED -> ARMED -> DISARMED` |
| Modes | `STABILIZE (0), GUIDED (4), LAND (9)` |
| Flight command ACKs | GUIDED, ARM, TAKEOFF, and LAND accepted |
| `LANDING_TARGET` count | 72 |
| MAVLink frame | 12, `MAV_FRAME_BODY_FRD` |
| `position_valid` | 1 |
| First target F/R/D | `-3.429 / 0.023 / 7.653 m` |
| Final local N/E/D | `-0.378 / -0.254 / -0.200 m` |
| Final horizontal error | `0.456 m` |

## Target loss behavior

The live run exercised both loss and reacquisition. OnboardAutonomy stopped
sending observations older than 250 ms and resumed only after the tracker
confirmed the tag again. Unit tests separately verify interrupted warmup,
reacquisition, smoothing, expiry, confidence rejection, corrected-bit
rejection, and protection against switching between tag IDs.

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
