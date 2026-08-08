# Camera recovery evidence

## Safety claim

A camera process exit or stalled byte stream must not require restarting the
onboard application. The source becomes visibly unavailable, retries in the
adapter layer, and publishes new complete frames when the producer returns.

## Reproduce

```bash
.venv/bin/python python/camera_recovery_acceptance.py \
    --companion build/onboard_autonomy
```

The harness starts the project Gazebo camera and one OnboardAutonomy process.
After the process consumes at least ten frames, the harness stops Gazebo,
waits for a two-second frame-progress timeout and `reconnecting`, then starts
Gazebo and enables the camera again. OnboardAutonomy is never restarted.

## Recorded run

Run captured on 2026-08-08:

| Evidence | Result |
| --- | --- |
| Initial state | STREAMING, 11 frames, 0 restarts |
| Producer outage | RECONNECTING after 2000 ms |
| Recovered state | STREAMING, 22 frames, 6 restarts |
| False sequence gaps | 0 |
| Companion process restart | None |

The six retries are expected: the adapter continues bounded attempts while
the Gazebo process and camera sensor are being reconstructed. The first full
I420 frame clears the error and returns the same source to `streaming`.
