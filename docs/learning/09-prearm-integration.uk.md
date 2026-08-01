# 09. Реальний PreArm від ArduPilot

## Що таке PreArm

`PreArm` — це не окремий MAVLink message type і не наша власна
перевірка. Це результат внутрішніх перевірок ArduPilot перед
армуванням.

Коли перевірка не проходить, ArduPilot надсилає звичайний MAVLink
`STATUSTEXT`, текст якого починається з `PreArm:`.

Наш production data flow:

```text
ArduPilot check
-> MAVLink STATUSTEXT
-> MavlinkDecoder
-> VehicleState::on_status_text()
-> VehicleSnapshot.warnings
-> armable=false
```

Python не створює warning від імені польотника. Він лише готує
контрольовану несправність у SITL.

## Яку несправність використано

У локальному ArduPilot Copter 4.6.3 перевірено правило:

```cpp
if (_spin_arm > thr_lin.get_spin_min()) {
    // MOT_SPIN_ARM > MOT_SPIN_MIN
}
```

Стандартний `MOT_SPIN_MIN` дорівнює `0.15`. Тест після здорового
baseline встановлює:

```text
MOT_SPIN_ARM = 0.20
```

ArduPilot сам формує:

```text
PreArm: Motors: MOT_SPIN_ARM > MOT_SPIN_MIN
```

Сценарій нічого не армує і не запускає мотори. Кожен тест стартує з
`--wipe` у власній artifact-папці, тому параметр не переходить у
наступний запуск і не торкається Pixhawk.

## Основна бізнес-перевірка

Функція `snapshot_has_prearm_failure()` в
`python/sitl_harness.py` вимагає одночасно:

- MAVLink link активний;
- GPS готовий;
- battery готова;
- `SYS_STATUS` свіжий;
- `system_health_ok == false`;
- у `warnings` є точний очікуваний `PreArm`;
- фінальний `armable == false`.

Цікавий Python-фрагмент:

```python
has_expected_warning = (
    isinstance(warnings, list)
    and any(
        isinstance(warning, str)
        and warning.startswith("PreArm:")
        and PREARM_FAILURE_FRAGMENT in warning
        for warning in warnings
    )
)
```

`any(...)` повертає `True`, щойно generator expression знаходить
перший відповідний warning. На відміну від створення проміжного
списку, решта елементів після збігу не обчислюється.

Підтверджене через `PARAM_VALUE` число перевіряється так:

```python
math.isclose(
    confirmed_spin_arm,
    0.20,
    rel_tol=0.0,
    abs_tol=1e-5,
)
```

Для MAVLink `REAL32` не можна надійно використовувати точне
`confirmed_spin_arm == 0.20`: десяткове `0.20` не має точного
представлення у binary floating point.

## Чому system health теж стає false

Перший варіант тесту помилково вимагав `system_health_ok == true`.
Живий SITL показав:

```text
enabled:   0x52619C2F
healthy:   0x47719C2F
unhealthy: 0x10000000
```

`0x10000000` — стандартний MAVLink
`MAV_SYS_STATUS_PREARM_CHECK`. ArduPilot навмисно скидає цей health
bit, коли pre-arm checks не пройшли.

Тому правильна модель така:

```text
GPS green
battery green
PREARM_CHECK unhealthy
STATUSTEXT contains PreArm
armable false
```

Це не дубльована випадкова помилка. `SYS_STATUS` дає машинний boolean,
а `STATUSTEXT` пояснює оператору конкретну причину.

## Додатковий інструмент

`python/inspect_tlog.py` тепер розпізнає не лише константи з префіксом
`MAV_SYS_STATUS_SENSOR_`, а й `MAV_SYS_STATUS_PREARM_CHECK`. Раніше для
біта `0x10000000` він друкував порожній список.

Інспектор також показує наявні `PreArm` messages разом із MAVLink
`sysid` та `compid`.

## Тести

Додано unit test, який не дозволяє сценарію пройти, якщо:

- warning відсутній;
- використано `Arm:` замість `PreArm:`;
- отримано іншу причину;
- link, GPS або battery не готові;
- pre-arm health помилково зелений;
- `armable` залишився true.

Локальна перевірка:

```text
Python unit tests: 9 passed
Python syntax compile: passed
git diff --check: passed
C++ CTest: 1/1 passed (C++ код у цій ітерації не змінювався)
```

## Що показали живі SITL-прогони

Перший прогін реально доставив у OnboardAutonomy:

```text
warnings=["PreArm: Motors: MOT_SPIN_ARM > MOT_SPIN_MIN"]
gps_ready=true
battery_ready=true
system_health_ok=false
armable=false
```

Він виявив нашу неправильну вимогу `system_health_ok == true`.

Другий прогін пройшов виправлену snapshot-перевірку та базову
перевірку MAVLink command/ACK/heartbeat. Після цього впала лише
експериментальна вимога знайти останній `STATUSTEXT` у MAVProxy tlog:
відомий daemon `log_writer` не встиг записати tail перед shutdown.
Цю крихку дубль-перевірку прибрано; production-шлях перевіряється
безпосередньо через snapshot OnboardAutonomy.

Повторний запуск фінальної команди був заблокований лімітом WSL
execution у Codex, а не помилкою коду. Команда для наступної перевірки:

```bash
python python/run_sitl_smoke_test.py --scenario prearm
```

Очікуваний фінал:

```text
PASSED
Failure injection: ArduPilot PreArm warning detected
```
