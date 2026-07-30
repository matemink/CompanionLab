# SITL failure injection: heartbeat loss

## Мета

Перевірити не запуск процесів, а поведінку CompanionLab після реальної
втрати MAVLink link:

```bash
python python/run_sitl_smoke_test.py --scenario heartbeat-loss
```

Сценарій спочатку доводить, що система працювала нормально, і лише
потім створює відмову:

```text
start full SITL stack
→ receive required telemetry
→ verify commands and ACK
→ stop MAVProxy
→ wait for connected=false
→ verify cleanup and tlog
```

## Чому зупиняємо MAVProxy

ArduCopter і CompanionLab залишаються живими, але між ними зникає
маршрутизатор:

```text
ArduCopter -X- MAVProxy -X- CompanionLab
```

Це моделює обрив MAVLink-каналу краще, ніж завершення CompanionLab.
Ми тестуємо реакцію production-коду на відсутність даних, а не
реакцію операційної системи на crash нашого процесу.

## Expected stop і unexpected exit

Звичайний `ProcessSupervisor.assert_running()` вважає раннє завершення
будь-якого процесу помилкою.

Failure injection викликає:

```python
supervisor.stop("MAVProxy")
```

PID спочатку позначається як expected stop, а потім process group
отримує `SIGTERM`. Тому наступна перевірка продовжує вимагати живі
ArduCopter і CompanionLab, але не падає через навмисно завершений
MAVProxy.

## Predicate-based waiting

Очікування JSON узагальнено до:

```python
wait_for_snapshot(
    process,
    supervisor,
    output_log,
    timeout,
    predicate,
    expectation,
)
```

Healthy phase використовує predicate для наявності heartbeat, GPS,
battery і health. Failure phase використовує:

```python
snapshot.get("connected") is False
```

Один timeout/selector loop обслуговує різні сценарії без копіювання
process I/O коду.

## Freshness timeout

`VehicleState` зберігає час останнього autopilot heartbeat.

```cpp
constexpr auto kHeartbeatTimeout = std::chrono::seconds(3);
```

Коли heartbeat стає старішим за три секунди:

- `connected = false`;
- GPS readiness стає false;
- battery readiness стає false;
- system health стає unknown;
- `armable = false`;
- застарілі sensor values не показуються як актуальні.

Тобто система переходить у безпечний unknown state, а не продовжує
вважати останні отримані дані здоровими.

## Artifact evidence

`summary.json` містить два snapshots:

```text
snapshot          стан до ін'єкції
failure_snapshot  стан після heartbeat timeout
```

У перевіреному запуску після відмови:

```text
connected: false
gps_fix_type: null
battery_voltage_v: null
system_health_known: false
armable: false
```

MAVProxy tlog додатково підтвердив, що до розриву були отримані три
interval requests, три accepted ACK і companion heartbeat.

## Перевірка

- Python unit tests: 5 passed.
- Heartbeat-loss SITL scenario: passed.
- Healthy regression scenario: passed.
- Після кожного сценарію не залишилося дочірніх процесів.
