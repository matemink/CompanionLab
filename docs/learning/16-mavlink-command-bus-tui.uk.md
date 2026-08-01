# MS-DOS TUI для MAVLink command bus

> Подальша ітерація розділила семантичну історію `LinkEvent` і фактичну
> TX/RX активність стрілок. Актуальна реалізація описана в
> [25-live-mavlink-link-activity.uk.md](25-live-mavlink-link-activity.uk.md).

## Мета ітерації

Показати одночасно дві частини одного польоту:

```text
Gazebo:
  фізичне положення і рух моделі

OnboardAutonomy:
  команда -> ACK -> телеметричне підтвердження
```

Це не debug dump усіх MAVLink message. Heartbeat і position приходять багато
разів на секунду та швидко зробили б екран нечитабельним. TUI показує останні
значущі транзакції й окремо поточний агрегований стан.

## Архітектурна межа

Новий data flow:

```text
MAVLink decoder / command sender
              |
              v
CompanionApplication
  -> LinkEvent ring buffer
  -> AppSnapshot
              |
              v
ConsoleView
  -> fixed 80-column ASCII screen
```

Presentation не декодує MAVLink, не визначає успішність команди й не керує
дроном. Вона отримує семантичну модель:

```cpp
struct LinkEvent {
    std::uint64_t sequence;
    std::chrono::milliseconds elapsed;
    LinkEventDirection direction;
    LinkEventStatus status;
    std::string label;
    std::string detail;
};
```

Завдяки цьому ту саму модель пізніше можна відобразити у web UI або зберегти
в structured log без зміни flight logic.

## Що потрапляє в журнал

Outbound:

```text
SET_INTERVAL
SET_MODE GUIDED
ARM
TAKEOFF 5.0 M
LAND
```

Inbound:

```text
flight-controller HEARTBEAT
COMMAND_ACK
MODE GUIDED
ARMED
MODE LAND
DISARMED
heartbeat loss
```

Companion heartbeat не додається щосекунди, бо це службовий шум. Його
поточний стан залишається в `AppSnapshot`.

## ACK і виконана дія не є одним фактом

Для кожного маневру можна побачити дві окремі події:

```text
+00:03.100 TX > SET_MODE       GUIDED | ATTEMPT 1
+00:03.130 < RX ACK SET_MODE   ACCEPTED | SYS 1
+00:03.250 < RX HEARTBEAT      MODE GUIDED
```

`COMMAND_ACK ACCEPTED` означає, що ArduPilot прийняв команду. Наступний
`HEARTBEAT MODE GUIDED` означає, що режим уже змінився. Так само ARM має
ACK і окремий armed bit, а LAND завершується лише після `DISARMED`.

Це важлива бізнес-логіка, а не декорація UI.

## Bounded history

Application використовує:

```cpp
std::deque<LinkEvent> link_events_;
```

Після додавання дев'ятої події найстаріша видаляється:

```cpp
if (link_events_.size() > kMaximumLinkEvents) {
    link_events_.pop_front();
}
```

Отже пам'ять не росте протягом багатогодинної роботи companion service.
Snapshot копіює максимум вісім невеликих подій, а TUI показує останні сім.

## State confirmation через snapshot diff

`observe_vehicle()` зберігає попередні:

```text
connected
flight_mode
armed
```

Подія створюється лише при переході:

```text
STABILIZE -> GUIDED
DISARMED -> ARMED
GUIDED -> LAND
ARMED -> DISARMED
```

Це схоже на `distinctUntilChanged`: значення може приходити 1-5 разів на
секунду, але UI отримує лише зміну стану.

## Console lifecycle і RAII

`ConsoleSession` у `main.cpp` ховає курсор при старті TUI та повертає його в
деструкторі:

```cpp
~ConsoleSession() {
    std::cout << "\x1b[?25h\x1b[0m\n";
}
```

Навіть exception або нормальний `Ctrl+C` не залишає термінал із невидимим
курсором. Екран очищається один раз, після чого кожен refresh використовує
ANSI `HOME`, а renderer завжди повертає однакову кількість рядків.

JSON mode не запускає `ConsoleSession` і залишається machine-readable.

## Тести

Application test перевіряє:

```text
RX HEARTBEAT реально виникає після decoder input
TX SET_INTERVAL реально виникає після transport write
accepted COMMAND_ACK стає RX success event
історія не перевищує 8 подій
```

Presentation test перевіряє:

```text
обидва endpoints присутні
TX > і < RX присутні
SET_MODE і ACK SET_MODE присутні
```

Результати:

```text
CMake build: PASS
CTest: 1/1 PASS
Python unittest: 9/9 PASS
git diff --check: PASS
```

## Жива перевірка

Після перезапуску лише OnboardAutonomy, без зупинки Gazebo та ArduCopter:

```text
commands:
  SET_GUIDED
  ARM
  TAKEOFF altitude=5.0m
  LAND

acknowledgements:
  SET_GUIDED result=0
  ARM result=0
  TAKEOFF result=0
  LAND result=0

telemetry:
  maximum relative altitude: 5.04m
  armed: DISARMED -> ARMED -> DISARMED
  modes: STABILIZE -> GUIDED -> LAND
```

Поточний endpoint у симуляції є ArduPilot SITL, а не фізичний Pixhawk. Після
переходу на Raspberry Pi + Pixhawk зміниться transport із UDP на serial, але
`COMMAND_LONG`, `COMMAND_ACK`, `LinkEvent` і TUI залишаться тими самими.
