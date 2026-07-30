#!/usr/bin/env python3

import sys

from pymavlink import mavutil


def main() -> int:
    names = sys.argv[1:]
    if not names:
        print("Usage: read_sitl_parameters.py PARAM [PARAM...]")
        return 1

    vehicle = mavutil.mavlink_connection("udpin:127.0.0.1:14550")
    if vehicle.wait_heartbeat(timeout=10) is None:
        print("No vehicle heartbeat received.")
        return 2

    for name in names:
        vehicle.mav.param_request_read_send(
            vehicle.target_system,
            vehicle.target_component,
            name.encode("ascii"),
            -1,
        )
        response = vehicle.recv_match(
            type="PARAM_VALUE",
            condition=f'PARAM_VALUE.param_id=="{name}"',
            blocking=True,
            timeout=5,
        )
        if response is None:
            print(f"{name}=<no response>")
        else:
            print(f"{name}={response.param_value}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
