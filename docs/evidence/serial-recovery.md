# Serial recovery evidence

## Scope

The automated Linux acceptance test exercises the same POSIX adapter used for
Pixhawk USB/UART. It does not claim a completed Raspberry Pi/Pixhawk bench run.

## Sequence

1. Create a Linux pseudo-terminal and expose its slave through a stable
   temporary symlink.
2. Open the symlink through `PosixSerialTransport` and receive bytes from the
   first master endpoint.
3. Close the master endpoint and assert that `POLLHUP` is non-fatal.
4. Replace the symlink target with a second pseudo-terminal.
5. Wait past the configured reconnect interval and send bytes through the same
   transport object.
6. Assert that the second master endpoint receives the complete frame.
7. Separately drive all six telemetry requests to `active`, report a lost
   connection, and assert that a reconnect restarts with `SYS_STATUS`.

## Result

The complete C++ suite passed 50 consecutive `ctest --repeat until-fail`
runs on Ubuntu 24.04 under WSL2. The transport object survived descriptor
hangup and reused the stable device path; telemetry configuration discarded
the old session and restarted from request 1 of 6.

Physical unplug/replug acceptance on Raspberry Pi 5 and Pixhawk 6C remains
explicitly open in the roadmap.
