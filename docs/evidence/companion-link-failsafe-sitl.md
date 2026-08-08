# Companion-link failsafe SITL evidence

## Safety claim

If the Raspberry Pi process or its MAVLink link disappears during autonomous
flight, OnboardAutonomy stops producing motion commands and ArduPilot changes
to LAND independently. The recovery action must not depend on a final command
from the failing companion computer.

This claim is scoped to ArduCopter 4.6.3 and the strict policy below. It does
not replace ArduPilot's battery, EKF, radio, or other failsafes.

## Validated policy

| Parameter | Accepted value | Reason |
| --- | --- | --- |
| `FS_GCS_ENABLE` | `5` | ArduCopter Always LAND action |
| `FS_GCS_TIMEOUT` | `2` to `10` seconds | Bounded companion-heartbeat loss timeout |
| `FS_OPTIONS` | GCS continuation bits 1 and 4 clear | Auto or pilot-mode continuation cannot bypass the selected action |
| `SYSID_MYGCS` | Companion heartbeat system id | ArduPilot monitors only the configured MAVLink system heartbeat |

The SITL defaults use a 3-second timeout, `FS_OPTIONS=0`, and system id 1.
OnboardAutonomy reads these values with `PARAM_REQUEST_READ`; it does not write
or silently repair them. A missing, disabled, unsupported, or inconsistent
value blocks autonomous startup before GUIDED mode.

The policy follows the official
[ArduPilot GCS failsafe documentation](https://ardupilot.org/copter/docs/gcs-failsafe.html).
The pinned 4.6.3 implementation checks the heartbeat selected by
`SYSID_MYGCS` and dispatches the configured action in
[ArduCopter events.cpp](https://github.com/ArduPilot/ardupilot/blob/Copter-4.6.3/ArduCopter/events.cpp#L125-L233).

## Reproduce

Build the native binary, then run:

```bash
.venv/bin/python python/link_failsafe_sitl_acceptance.py \
    --companion build/onboard_autonomy
```

The harness starts headless Gazebo, ArduCopter, MAVProxy, and OnboardAutonomy.
A bidirectional UDP relay sits between MAVProxy and the companion. Once the
vehicle is armed in GUIDED and reaches at least 7.5 m, the relay drops traffic
in both directions while a separate read-only telemetry endpoint remains
connected to ArduPilot.

The test accepts only when:

- the pre-cut application snapshot reports the four parameters as accepted;
- the post-cut snapshot reports stale flight-controller heartbeat and failed
  autonomy;
- the companion emitted GUIDED, ARM, and TAKEOFF, but no LAND or RTL;
- ArduPilot reported `GCS Failsafe`, changed to LAND within the configured
  timeout margin, and a separate read-only endpoint observed LAND and disarm.

The tlog proves command provenance and the failover timestamp. Final LAND and
disarm come from the independent monitor rather than MAVProxy's asynchronous
tlog writer, so process shutdown cannot race the final safety assertion.

## Recorded run

Run captured on 2026-08-08:

| Evidence | Result |
| --- | --- |
| Companion heartbeats before cut | 33 |
| Configured `FS_GCS_TIMEOUT` | 3.0 s |
| Measured last-heartbeat to LAND mode | 3.237 s |
| Companion flight commands | SET_GUIDED, ARM, TAKEOFF |
| Companion LAND/RTL after loss | None |
| ArduPilot modes | STABILIZE, GUIDED, LAND |
| ArduPilot status | `GCS Failsafe` |
| Independent monitor | LAND; DISARMED, ARMED, DISARMED |
| Application loss record | `Flight-controller heartbeat was lost during autonomy` |

This separates two responsibilities: OnboardAutonomy cancels its application
behavior when telemetry becomes stale; ArduPilot remains the flight-critical
authority that performs LAND after the companion heartbeat disappears.
