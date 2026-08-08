# ArduPilot companion-link failsafe

## Яку проблему закрито

Зупинити C++ runtime після втрати heartbeat недостатньо. Якщо Raspberry Pi
завис або знеструмлений, він уже не здатний надіслати `LAND`. Тому посадка має
бути попередньо налаштована всередині ArduPilot.

OnboardAutonomy тепер читає чотири параметри:

- `FS_GCS_ENABLE=5` вибирає Always LAND;
- `FS_GCS_TIMEOUT` задає час без heartbeat;
- `FS_OPTIONS` не повинен дозволяти обхід GCS failsafe;
- `SYSID_MYGCS` має збігатися із system id heartbeat-а companion component.

## Як розділено код

`MavlinkDecoder` перетворює wire message `PARAM_VALUE` на generic
`ParameterValue`. Він не знає, які значення безпечні.

`CompanionLinkFailsafe` є application policy. Він накопичує чотири значення,
типізує action та bitmask і повертає один із станів:

```cpp
enum class CompanionLinkFailsafePhase {
    waiting_for_vehicle,
    reading_parameters,
    accepted,
    rejected,
};
```

Це аналог sealed state, але C++ `enum class` не несе payload. Payload лежить у
`CompanionLinkFailsafeSnapshot`: detail, timeout, action, options і system id.

`FlightStartupController` перевіряє `accepted()` до першого `SET_MODE`. Тобто
gate стоїть до GUIDED, ARM і TAKEOFF, а не десь після запуску сценарію.

## Що доводить integration test

Python harness ставить UDP relay між MAVProxy та C++ process. Після злету relay
припиняє forwarding, але Gazebo та ArduPilot продовжують працювати. Окремий
read-only monitor бачить, як ArduPilot через 3.237 секунди сам переходить у
LAND і роззброюється.

Tlog додатково перевіряється негативно: після TAKEOFF companion не надсилав ні
LAND, ні RTL. Саме ця негативна перевірка доводить правильну межу
відповідальності, а не просто успішну посадку.
