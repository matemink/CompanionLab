# SITL failure injection: GPS loss

## Мета

Довести не просто отримання `GPS_RAW_INT`, а правильну реакцію
CompanionLab на реальне вимкнення simulated GPS у вже готовому
ArduCopter:

```bash
python python/run_sitl_smoke_test.py --scenario gps-loss
```

Production C++ у цій ітерації не змінювався. Python керує тестовим
середовищем і перевіряє наскрізний результат:

```text
healthy ArduCopter
→ SIM_GPS_DISABLE = 1
→ GPS backend втрачає fix
→ GPS_RAW_INT змінюється
→ MavlinkDecoder
→ VehicleState
→ JSON snapshot
→ integration assertion
```

## Healthy baseline

Спочатку harness вважав початковий стан достатнім, якщо повідомлення
GPS, battery і system health просто були отримані. Перший запуск
`gps-loss` формально пройшов, але artifact показав:

```text
before: gps_fix_type=0, gps_ready=false
after:  gps_fix_type=0, gps_ready=false
```

Це false positive: відмова не може бути доведена, якщо система не була
здоровою до ін'єкції.

Тому додано окремий predicate:

```python
def snapshot_is_ready(snapshot: dict[str, object]) -> bool:
    return (
        snapshot_has_required_telemetry(snapshot)
        and snapshot.get("gps_ready") is True
        and snapshot.get("battery_ready") is True
        and snapshot.get("system_health_ok") is True
        and snapshot.get("armable") is True
    )
```

Різниця принципова:

```text
telemetry present  = ми отримали значення
system ready       = значення пройшли domain rules
```

Failure test завжди має спочатку довести другий стан.

## MAVLink side-channel

ArduCopter SITL відкриває окремі serial endpoints:

```text
SERIAL0, TCP 5760 → MAVProxy → CompanionLab
SERIAL1, TCP 5762 → Python fault injector
```

Artifact `mav.parm` підтверджує `SERIAL1_PROTOCOL=2`, тобто MAVLink2.
Fault injector не треба підмішувати в production UDP link або додавати
другий MAVProxy output.

Fault injector є окремим MAVLink-вузлом:

```text
system ID:    250
component ID: 190
```

Він чекає heartbeat саме автопілота. Heartbeat CompanionLab
відкидається через `MAV_AUTOPILOT_INVALID`, тому параметр не може бути
помилково адресований component `191`.

Після цього `set_sitl_parameter()`:

1. надсилає `PARAM_SET`;
2. очікує `PARAM_VALUE` з тим самим іменем;
3. повертає підтверджене ArduPilot значення;
4. падає по timeout, якщо підтвердження немає.

Успішний UDP `send` тут, як і для `COMMAND_LONG`, не означає, що
одержувач застосував запит. Бізнес-доказом є відповідь протоколу.

## Чому `SIM_GPS_DISABLE`

Параметр узято не з припущення. Він визначений у локальному source
ArduPilot 4.6.3:

```text
libraries/SITL/SITL.cpp

SIM_GPS_DISABLE
0 = GPS enabled
1 = GPS disabled
```

Після `PARAM_VALUE=1` harness очікує:

```text
connected=true
gps_ready=false
gps_fix_type < 3 або null
battery_ready=true
system_health_known=true
armable=false
```

Так predicate доводить локальну GPS-відмову, а не загальну втрату
MAVLink link.

## Ізоляція EEPROM

Другий корисний баг з'явився після першої ін'єкції:
`SIM_GPS_DISABLE=1` зберігся у SITL EEPROM. Наступний test run стартував
уже зі зламаним GPS.

Виправлення:

```text
окремий artifact directory стає working directory ArduCopter
і
ArduCopter запускається з --wipe
```

`--wipe` підтверджений у
`libraries/AP_HAL_SITL/SITL_cmdline.cpp` ArduPilot 4.6.3 як команда
очищення EEPROM.

Тепер кожен test run:

- не залежить від попереднього сценарію;
- не змінює state наступного сценарію;
- зберігає власні SITL-файли разом з artifact;
- починається з відомих `copter.parm` defaults.

Це аналог test fixture isolation: порядок запуску тестів не повинен
впливати на результат.

## Завершення процесів

Раніше process group одразу отримувала `SIGTERM`. MAVProxy завершувався,
але іноді не встигав пройти штатний signal handler.

Тепер escalation послідовна:

```text
SIGINT
→ timeout
→ SIGTERM
→ timeout
→ SIGKILL
```

Нормальний шлях використовує `SIGINT`, тому ArduCopter, MAVProxy і
CompanionLab отримують можливість штатно завершити cleanup. Сильніші
сигнали залишаються страховкою від зависання.

Є відомий residual у локальному MAVProxy 1.8.74: після unloading усіх
modules він іноді друкує `_enter_buffered_busy` traceback свого daemon
`log_writer` під час завершення Python interpreter. Додатковий drain
window і зміна порядку shutdown не усунули його, тому workaround не
залишено в harness.

Це не приховано як зелений результат:

- tlog встигає записатися й проходить protocol assertions;
- MAVProxy process завершується;
- фонових процесів не залишається;
- production CompanionLab не падає;
- traceback лишається дефектом зовнішнього test tool.

## Фактичний результат

Healthy snapshot:

```text
connected=true
gps_fix_type=6
satellites_visible=10
gps_ready=true
battery_ready=true
system_health_ok=true
armable=true
```

Після `SIM_GPS_DISABLE=1`:

```text
connected=true
gps_fix_type=1
satellites_visible=3
gps_ready=false
battery_ready=true
system_health_ok=true
armable=false
```

Пройшли:

- 7 Python unit tests;
- усі C++ tests;
- healthy SITL regression;
- heartbeat-loss SITL regression;
- gps-loss SITL scenario;
- перевірка трьох interval requests і трьох accepted ACK;
- cleanup без залишених процесів;
- зафіксований residual shutdown traceback MAVProxy 1.8.74.

## Що далі

Battery failure реалізовано наступною ітерацією:
[08-low-battery.uk.md](08-low-battery.uk.md). Наступна відмова —
реальний `PreArm` status від ArduPilot.
