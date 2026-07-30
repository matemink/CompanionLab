#!/usr/bin/env python3

import time

from pymavlink import mavutil


def is_armed(heartbeat: object) -> bool:
    return bool(
        heartbeat.base_mode & mavutil.mavlink.MAV_MODE_FLAG_SAFETY_ARMED
    )


def main() -> int:
    vehicle = mavutil.mavlink_connection("udpin:127.0.0.1:14550")
    heartbeat = vehicle.wait_heartbeat(timeout=10)
    if heartbeat is None:
        print("No vehicle heartbeat received.")
        return 1

    if not is_armed(heartbeat):
        print("Vehicle is already disarmed.")
        return 0

    vehicle.mav.command_long_send(
        vehicle.target_system,
        vehicle.target_component,
        mavutil.mavlink.MAV_CMD_NAV_LAND,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
    )
    print("LAND requested; waiting for touchdown.")

    touchdown_since = None
    deadline = time.monotonic() + 60
    while time.monotonic() < deadline:
        message = vehicle.recv_match(
            type=["HEARTBEAT", "GLOBAL_POSITION_INT"],
            blocking=True,
            timeout=1,
        )
        if message is None:
            continue

        if message.get_type() == "HEARTBEAT":
            if not is_armed(message):
                print("Vehicle landed and disarmed.")
                return 0
            continue

        altitude_m = message.relative_alt / 1000.0
        if altitude_m <= 0.15:
            touchdown_since = touchdown_since or time.monotonic()
        else:
            touchdown_since = None

        if touchdown_since is not None and time.monotonic() - touchdown_since >= 3:
            vehicle.mav.command_long_send(
                vehicle.target_system,
                vehicle.target_component,
                mavutil.mavlink.MAV_CMD_COMPONENT_ARM_DISARM,
                0,
                0,
                21196,
                0,
                0,
                0,
                0,
                0,
            )
            print("Touchdown confirmed; SITL force-DISARM requested.")
            touchdown_since = time.monotonic() + 3600

    print("Vehicle did not reach a confirmed disarmed state.")
    return 2


if __name__ == "__main__":
    raise SystemExit(main())
