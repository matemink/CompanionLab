# SITL failure injection: low battery

## Мета

Перевірити production threshold OnboardAutonomy на реальній послідовності
MAVLink battery telemetry:

```bash
python python/run_sitl_smoke_test.py --scenario low-battery
```

Очікуваний перехід:

```text
battery_remaining >= 20%
→ battery_ready=true
→ ArduPilot інтегрує simulated current
→ battery_remaining < 20%
→ battery_ready=false
→ armable=false
```

Production C++ знову не змінювався. Новий тест доводить поведінку
наявного `VehicleState`.

## Чому не просто voltage

`SIM_BATT_VOLTAGE=10.5` не є достатнім доказом low battery. Однакова
pack voltage має різний зміст для 3S, 4S або 6S LiPo.

OnboardAutonomy поки не знає chemistry та cell count, тому його
підтверджене правило використовує переданий ArduPilot відсоток:

```cpp
battery_remaining_pct >= 20
```

ArduPilot формує цей відсоток не з одного voltage measurement.

## Формула ArduPilot

Локальний source ArduPilot 4.6.3 показує:

```text
AP_BattMonitor_Backend::capacity_remaining_pct()

remaining =
    100 * (pack_capacity - consumed_mAh) / pack_capacity
```

`consumed_mAh` оновлюється в
`AP_BattMonitor_Backend::update_consumed()` інтегруванням current у
часі.

Для analog battery monitor current обчислюється так:

```text
current =
    (sensor_voltage - BATT_AMP_OFFSET) * BATT_AMP_PERVLT
```

SITL у disarmed стані дає `sensor_voltage=0`, а defaults містять:

```text
BATT_AMP_OFFSET=0
BATT_AMP_PERVLT=17
BATT_CAPACITY=3300 mAh
```

Тому початковий current дорівнює нулю.

## Ін'єкція

Після healthy baseline Python test client через окремий
`SERIAL1/TCP 5762` MAVLink2 link встановлює:

```text
BATT_CAPACITY=20
BATT_AMP_OFFSET=-1
```

Отримуємо:

```text
current = (0 - (-1)) * 17 = 17 A
```

`20 mAh` обрано не навмання. ArduPilot вважає capacity `<=10 mAh`
невалідною для percentage calculation. Значення `20` валідне, але
достатньо мале, щоб тест завершився за кілька секунд.

Обидва `PARAM_SET` обов'язково підтверджуються відповідними
`PARAM_VALUE`. Без protocol confirmation fault injection вважається
невдалою.

Мотори не запускаються, vehicle залишається disarmed. Струм створюється
математично через model calibration offset лише всередині SITL.

## Integration predicate

Тест не задовольняється лише `battery_ready=false`. Він вимагає:

```text
connected=true
gps_ready=true
battery_remaining_pct < 20
battery_ready=false
system_health_known=true
armable=false
```

Так ми відрізняємо локальне виснаження батареї від втрати MAVLink,
GPS або всієї telemetry.

Перевірка `remaining < 20`, а не `<=20`, відповідає production rule:
рівно `20%` ще проходить, `19%` уже блокує readiness.

## Python predicate

```python
remaining_is_low = (
    isinstance(remaining, (int, float))
    and not isinstance(remaining, bool)
    and 0 <= remaining < 20
)
```

Окрема перевірка `bool` потрібна через особливість Python:

```python
isinstance(True, int) is True
```

Без неї JSON `true` теоретично міг би бути помилково прийнятий за
число `1`.

## Фактичний результат

Healthy snapshot:

```text
battery_voltage_v=12.6
battery_current_a=0.0
battery_remaining_pct=100
battery_ready=true
gps_ready=true
armable=true
```

Після ін'єкції snapshots показали:

```text
100% → 85% → 62% → 38% → 14%
0 A  → 17 A
```

Failure snapshot:

```text
connected=true
gps_fix_type=6
gps_ready=true
battery_voltage_v=12.6
battery_current_a=17.0
battery_remaining_pct=14
battery_ready=false
system_health_ok=true
armable=false
```

## Перевірка

- 8 Python unit tests пройшли.
- Усі C++ tests пройшли.
- Low-battery SITL scenario пройшов.
- GPS-loss regression пройшов.
- Heartbeat-loss regression пройшов.
- Зафіксовано три interval requests і три accepted ACK.
- EEPROM кожного сценарію ізольований через artifact directory і
  `--wipe`.

Відомий shutdown traceback MAVProxy 1.8.74 залишається описаним у
[07-gps-loss.uk.md](07-gps-loss.uk.md); він не залишає живих процесів
і не впливає на tlog assertions.

## Що далі

Останній failure scenario цього етапу — `PreArm`. На відміну від
synthetic `STATUSTEXT` unit test, integration test має змусити саме
ArduPilot сформувати реальне повідомлення `PreArm: ...`, після чого
перевірити warning та `armable=false`.
