# Автоматичний політ OnboardAutonomy у Gazebo

## Мета ітерації

До цієї ітерації політ виконував користувач через MAVProxy:

```text
mode GUIDED
arm throttle
takeoff 5
mode LAND
```

Тепер ці дії виконує C++ companion service. MAVProxy залишається GCS-консоллю
для діагностики, але більше не є джерелом flight commands.

## Повний data flow

```text
AutomatedFlightController
        |
        | FlightCommandRequest
        v
CompanionApplication
        |
        | COMMAND_LONG
        v
MAVLink encoder -> UDP -> MAVProxy -> ArduCopter SITL
                                      |
                    COMMAND_ACK + telemetry
                                      |
Gazebo physics <- ArduPilot plugin <-+
```

State machine проходить такі стани:

```text
WAITING
  -> GUIDED
  -> ARMING
  -> TAKEOFF
  -> HOLDING
  -> LANDING
  -> COMPLETE
```

`FAILED` є термінальним станом для rejected command, втрати heartbeat,
timeout або неочікуваного disarm.

## ACK не означає завершення дії

MAVLink `COMMAND_ACK` підтверджує, що команда дійшла і була прийнята. Він не
означає, що фізична дія вже завершилась.

Тому кожен етап має дві перевірки:

| Команда | ACK | Підтвердження стану |
|---|---|---|
| `MAV_CMD_DO_SET_MODE` | `ACCEPTED` | `HEARTBEAT.custom_mode == 4` |
| `MAV_CMD_COMPONENT_ARM_DISARM` | `ACCEPTED` | armed bit у `HEARTBEAT` |
| `MAV_CMD_NAV_TAKEOFF` | `ACCEPTED` | `relative_alt >= 4.75 м` |
| `MAV_CMD_NAV_LAND` | `ACCEPTED` | armed bit очищений |

`4.75 м` є target `5.0 м` мінус tolerance `0.25 м`. Реальна максимальна
висота в перевіреному запуску склала `5.04 м`.

Офіційні джерела:

- [ArduPilot: Get and Set FlightMode](https://ardupilot.org/dev/docs/mavlink-get-set-flightmode.html)
- [ArduPilot: Arming and Disarming](https://ardupilot.org/dev/docs/mavlink-arming-and-disarming.html)
- [ArduPilot: Copter SITL/MAVProxy Tutorial](https://ardupilot.org/dev/docs/copter-sitl-mavproxy-tutorial.html)
- [MAVLink Command Protocol](https://mavlink.io/en/services/command.html)

## Цікаві місця C++

### `std::optional<FlightCommandRequest>`

```cpp
[[nodiscard]] std::optional<FlightCommandRequest> update(...);
```

Один виклик state machine або повертає наступну команду, або `std::nullopt`.
Вона не знає про UDP, serial чи байти MAVLink.

`[[nodiscard]]` примушує caller використати результат. Компілятор знайшов два
місця в тестах, де результат ігнорувався, і тести були уточнені.

### State machine без blocking sleep

`update()` отримує `steady_clock::time_point`. Вона не викликає `sleep()` і
не блокує read loop. Deadline-и зберігаються як дані:

```cpp
acknowledgement_deadline_ = now + std::chrono::seconds(2);
```

Тому OnboardAutonomy продовжує читати heartbeat, position, battery і warnings,
поки очікує ACK або завершення маневру.

### Перевірка переходів

Після accepted ARM команда takeoff не відправляється негайно з ACK handler.
State machine чекає наступний snapshot із `vehicle.armed == true`. ACK handler
лише записує факт прийняття команди; transitions виконуються в одному
послідовному `update()` loop.

## Telemetry extension

`VehicleState` тепер містить:

```text
flight_mode
relative_altitude_m
```

`TelemetryStreamConfigurator` додатково запитує
`GLOBAL_POSITION_INT` з інтервалом `200000 мкс`, тобто `5 Hz`.

Одноразовий startup warning залишається видимим 30 секунд, але блокує arm
тільки 5 секунд після останнього повторення. Активний PreArm повторюється
ArduPilot і продовжує блокувати запуск.

## Safety boundary

Автоматичний політ вимкнений за замовчуванням. Він запускається лише з:

```text
--demo-flight
```

Поєднання `--demo-flight` і `--serial` заборонене до відкриття transport.
Force-arm magic value `21196` не використовується: ARM відправляється з
`param2=0`, тому ArduPilot safety checks залишаються активними.

Автоматичний integration run передає demo flag через:

```text
ONBOARD_AUTONOMY_DEMO_FLIGHT=1
```

Windows launcher `StartOnboardAutonomyGazeboDemo.cmd` тепер запускає:

```text
ONBOARD_AUTONOMY_INTERACTIVE=1
```

Тому політ не починається під час відкриття вікна: користувач явно запускає
цей самий state machine клавішею `[1]`.

## Перевірка

Native tests:

```text
CTest: 1/1 passed
Python unittest: 9/9 passed
```

Живий Gazebo + ArduCopter 4.6.3:

```text
maximum relative altitude: 5.04 m
armed transitions: DISARMED -> ARMED -> DISARMED
custom modes: 0, 4, 9
SET_GUIDED ACK: result=0
ARM ACK: result=0
TAKEOFF ACK: result=0
LAND ACK: result=0
```

`python/inspect_tlog.py` тепер друкує ці flight evidence автоматично.
