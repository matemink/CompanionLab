# Scenario Engine і п'ять SITL-демо

## Навіщо був потрібен рефакторинг

Старий `AutomatedFlightController` одночасно описував один конкретний маршрут
і виконував його. Послідовність існувала лише як переходи великого `switch`:

```text
GUIDED -> ARM -> TAKEOFF -> HOLD -> LAND
```

Щоб додати другий маршрут, довелося б копіювати стани або додавати ще більше
умов у той самий клас. Тепер розділено дві відповідальності:

```text
ScenarioDefinition = що треба виконати
ScenarioRunner     = як безпечно виконувати кроки
```

## Каталог

| Key | Scenario | Реальна послідовність |
|---|---|---|
| 1 | HOVER CHECK | GUIDED, ARM, TAKEOFF 5 m, HOLD 5 s, LAND |
| 2 | OUT & RTL | TAKEOFF 5 m, NORTH 15 m, HOLD, RTL |
| 3 | SQUARE PATROL | чотири сторони 10 m, RTL |
| 4 | SEARCH GRID | шість NED-відрізків 24 x 12 m, RTL |
| 5 | PRECISION LANDING | offset 8/4 m, synthetic LANDING_TARGET, LAND |

Усі рухові клавіші досі доступні лише в explicit UDP/SITL interactive mode.
Serial/Pixhawk запуск блокується старою safety policy.

## Typed steps

Кроки не зберігаються як рядки:

```cpp
using ScenarioStep = std::variant<
    SetGuidedStep,
    ArmStep,
    TakeoffStep,
    HoldStep,
    MoveLocalStep,
    ReturnToLaunchStep,
    LandStep,
    PrecisionLandStep
>;
```

`std::variant` означає: значення містить рівно один тип із закритого списку.
Це схоже на sealed hierarchy, але без heap allocation і virtual dispatch.

Опис сценарію читається як дані:

```cpp
{
    .id = ScenarioId::out_and_rtl,
    .name = "OUT & RTL",
    .steps = {
        SetGuidedStep{},
        ArmStep{},
        TakeoffStep{5.0},
        MoveLocalStep{15.0, 0.0, 0.0, "NORTH 15M"},
        HoldStep{3s},
        ReturnToLaunchStep{},
    },
}
```

Додати або прибрати крок тепер можна в `Scenario.cpp`, не переписуючи
алгоритм виконання.

## Як runner переходить далі

Runner не вважає успішним сам факт запису bytes у UDP:

- `GUIDED` завершується після accepted `COMMAND_ACK` і `HEARTBEAT.mode = 4`;
- `ARM` завершується після ACK і `HEARTBEAT.armed = true`;
- `TAKEOFF` завершується після ACK і досягнення altitude;
- `MOVE_LOCAL` завершується після потрапляння в радіус 0.75 m;
- `RTL`, `LAND` і precision land завершуються після `DISARMED`.

Це різниця між **command accepted** і **physical action completed**.

## Локальні координати

Route step надсилає:

```text
SET_POSITION_TARGET_LOCAL_NED
frame = MAV_FRAME_LOCAL_OFFSET_NED
type_mask = 3576
```

`north/east/down` задаються відносно позиції на початку кроку. Runner зберігає
абсолютну target position і порівнює її з `LOCAL_POSITION_NED`.

NED має вісь `Z` вниз, тому висота 5 m приблизно відповідає `down = -5 m`.

## Precision landing без вигадки

Computer vision ще не реалізовано. Тому сценарій 5 не називає координати
результатом AprilTag detector.

У SITL target вважається нерухомим у точці старту. Runner бере поточну
`LOCAL_POSITION_NED`, `ATTITUDE`, переводить вектор до home з NED у body FRD
і надсилає `LANDING_TARGET` з `position_valid = 1` на частоті 5 Hz.

ArduPilot запускається з:

```text
PLND_ENABLED 1
PLND_TYPE 1
PLND_STRICT 0
```

Це реально перевіряє MAVLink integration та precision-land backend.
`PLND_STRICT=0` означає: якщо синтетична ціль зникла вже біля землі,
ArduPilot завершує звичайну посадку замість запуску retry.
Synthetic source пізніше замінить adapter, який отримає body-FRD offset від
OpenCV/AprilTag. Сам `PrecisionLandStep` змінювати не потрібно.

## Нові telemetry streams

До health telemetry додано:

```text
LOCAL_POSITION_NED at 10 Hz
ATTITUDE at 10 Hz
```

Перший потрібний для route completion, обидва разом - для перетворення
synthetic landing target у `MAV_FRAME_BODY_FRD`.

## Цікава помилка з тестів

Після фінального `DISARMED` runner переходив у `completed`, але цикл immediate
transitions запускав ще одну ітерацію та помилково викликав `fail()`.

Виправлення - terminal states повертають результат до перевірки executable
step:

```cpp
if (phase_ == ScenarioRunnerPhase::completed ||
    phase_ == ScenarioRunnerPhase::failed) {
    return actions;
}
```

Це типовий state-machine invariant: terminal state не повинен мати outgoing
transition.

## Автоматичні перевірки

- каталог містить рівно п'ять typed scenarios;
- HOVER завершується тільки після `DISARMED`;
- OUT & RTL чекає локальну позицію;
- precision step прогріває `LANDING_TARGET` перед `LAND`;
- missing ACK повторюється тричі й завершує сценарій помилкою;
- encoded MAVLink frames декодуються назад і перевіряються по полях;
- Python tests перевіряють старі health/failure сценарії.

Офіційна документація:

- [ArduPilot Copter commands in Guided mode](https://ardupilot.org/dev/docs/copter-commands-in-guided-mode.html)
- [ArduPilot RTL mode](https://ardupilot.org/copter/docs/rtl-mode.html)
- [ArduPilot MAVLink precision landing](https://ardupilot.org/dev/docs/mavlink-precision-landing.html)

## Перевірка всіх п'яти сценаріїв у SITL + Gazebo

Кожен сценарій запускався окремим процесом із
`--scenario N --exit-after-scenario`. Успіх означав не лише прийняті
команди, а `exit code 0` і фінальний `HEARTBEAT.armed=false`.

| № | Сценарій | Фактичний результат |
|---|---|---|
| 1 | HOVER CHECK | Зліт до 5 м, hold, LAND, DISARMED |
| 2 | OUT & RTL | North max 15.09 м, RTL mode 6, DISARMED |
| 3 | SQUARE PATROL | North max 10.03 м, East max 9.99 м, RTL, DISARMED |
| 4 | SEARCH GRID | North max 23 м, East range приблизно 13 м, RTL, DISARMED |
| 5 | PRECISION LANDING | Зліт 7.94 м, LANDING_TARGET, LAND mode 9, DISARMED |

Сценарій 5 спочатку двічі чесно завершився помилкою: апарат торкнувся
землі, але не роззброївся за 90 секунд. Tlog показав:

```text
PrecLand: Target Found
PrecLand: Target Lost
PrecLand: Failsafe Measures
```

Причиною був `PLND_STRICT=1` із Gazebo defaults. Після припинення
синтетичного target біля землі прошивка запускала retry. У OnboardAutonomy
SITL profile зафіксовано `PLND_STRICT=0`; після цього той самий сценарій
завершився штатним auto-disarm і `exit code 0`.

Фінальне відхилення synthetic target у простій Gazebo-моделі було близько
2.7 м від home. Це підтверджує MAVLink pipeline, але не точність майбутньої
camera/AprilTag системи. Для реальної precision landing потрібні камера,
калібрування, marker detector і окремі accuracy-тести.

## Recovery після невдалого SITL-тесту

`scripts/recover_sitl_vehicle.py` спочатку надсилає `LAND`, чекає
підтвердженої висоти до 0.15 м протягом трьох секунд і лише тоді виконує
SITL force-disarm. Цей скрипт не входить у flight scenario та не
використовується для реального Pixhawk.

`scripts/read_sitl_parameters.py` читає активні ArduPilot parameters через
MAVLink. Саме ним підтверджено:

```text
PLND_STRICT=0
DISARM_DELAY=10
PLND_ENABLED=1
PLND_TYPE=1
```

Фоновий launcher також автоматично додає MAVProxy
`--non-interactive`, якщо stdin не є TTY. У ручній консолі поведінка не
змінюється.
