# Interactive scenario triggers

## Мета ітерації

Перетворити command-bus TUI з пасивного монітора на контрольовану SITL
консоль:

```text
[1] RUN 5M DEMO
[L] LAND NOW
[Q] QUIT
```

Клавіша `[1]` запускає не одну MAVLink-команду, а існуючий state machine:

```text
GUIDED
  -> ARM
  -> TAKEOFF 5m
  -> HOLD 5s
  -> LAND
  -> wait for DISARMED
```

`[L]` є окремою manual command. Вона скасовує майбутні кроки automation і
відправляє `MAV_CMD_NAV_LAND`.

## Чому не raw command prompt

Можна було дозволити вводити:

```text
command=400 param1=1 param2=21196
```

Для user-facing utility це поганий API:

- користувач має знати numeric MAVLink constants;
- легко випадково використати force-arm magic value;
- немає whitelist і зрозумілої safety policy;
- важко пояснити, який результат вважається завершенням команди.

Тому TUI віддає лише named use case-и з відомою семантикою. Новий сценарій
додається в application як state machine, а не як текстовий macro-файл із
довільними байтами.

## Два рівні команд

Scenario:

```cpp
application.trigger_demo_flight(now);
```

Він запускає `AutomatedFlightController`, який має retries, ACK deadlines і
telemetry verification.

Manual command:

```cpp
application.request_land(now);
```

Вона призначена для негайного переходу в LAND. Перед відправленням automation
отримує:

```cpp
automated_flight_.cancel("Manual LAND requested");
```

Тому попередній state machine не надішле TAKEOFF або іншу прострочену команду
після manual override.

## Restartable state machine

`AutomatedFlightController::start()` дозволяє запуск лише зі станів:

```text
DISABLED
COMPLETED
FAILED
```

Повторне натискання `[1]` під час активного сценарію повертає `false` і додає
у журнал:

```text
SCENARIO ALREADY RUNNING
```

Після завершеної посадки `[1]` може запустити сценарій знову без restart
процесу.

`cancel()` очищає:

```text
vehicle system ID
attempt counter
awaiting ACK
accepted-command flag
previous failure result
```

## Non-blocking terminal input

`ConsoleInput` є presentation adapter над Linux `termios` і `fcntl`.

Звичайний terminal працює в canonical mode: `read()` повертає рядок лише після
Enter. Для hotkeys потрібно отримувати один символ відразу:

```cpp
raw.c_lflag &= ~(ICANON | ECHO);
raw.c_cc[VMIN] = 0;
raw.c_cc[VTIME] = 0;
```

`O_NONBLOCK` гарантує, що відсутність клавіші не зупиняє main loop:

```cpp
fcntl(STDIN_FILENO, F_SETFL, original_flags_ | O_NONBLOCK);
```

Це критично: під час очікування keyboard input application продовжує читати
MAVLink, оновлювати ACK deadlines і надсилати companion heartbeat.

Destructor повертає original `termios` та `fcntl` flags. Отже після `[Q]`,
`Ctrl+C` або exception shell знову має echo й нормальне введення.

## Composition root

`main.cpp` виконує тільки mapping:

```text
'1'     -> trigger_demo_flight()
'l'/'L' -> request_land()
'q'/'Q' -> shutdown
```

Він не кодує MAVLink і не змінює flight phase напряму.

## Safety boundary

Motion API потребує:

```cpp
CompanionApplicationOptions::motion_commands_allowed = true;
```

CLI встановлює permission лише для:

```text
--demo-flight
--interactive + live TTY
```

Обидва режими відхиляються, якщо задано `--serial`. Тобто цей build не дає
натисканням `[1]` запустити фізичний Pixhawk.

Навіть усередині application команда без permission або heartbeat не
ігнорується мовчки, а створює failure event:

```text
BLOCKED BY MOTION SAFETY POLICY
NOT SENT | NO FLIGHT CONTROLLER
```

## Launcher

`StartCompanionLabGazeboDemo.cmd` більше не запускає політ автоматично. Він
передає:

```text
COMPANIONLAB_INTERACTIVE=1
```

У `scripts/run_companionlab_sitl.sh` це перетворюється на:

```text
--interactive
```

## Перевірка

```text
CMake configure/build: PASS, warnings: 0
CTest: 1/1 PASS
interactive process: running with --interactive
TTY guard: PASS
```

Native tests підтверджують:

```text
disabled state machine запускається explicit trigger-ом
другий trigger під час active scenario відхиляється
cancel зупиняє retries
LAND без permission блокується
LAND без heartbeat блокується
connected LAND створює COMMAND_LONG і TX event
TUI показує [1] та [L]
```

Сам state machine і його живий Gazebo flight уже перевірені в ітераціях 13
та 16. Після запуску interactive build сценарій виконується користувацькою
клавішею `[1]`, а не прихованим auto-start.
