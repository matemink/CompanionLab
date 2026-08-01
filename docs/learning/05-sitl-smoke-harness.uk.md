# Автоматичний SITL smoke test

## Навіщо він потрібен

Ручний launcher з двома терміналами корисний для спостереження, але
не є тестом. Людина не повинна щоразу дивитися на dashboard і
вирішувати, чи “ніби працює”.

Новий запуск:

```bash
python python/run_sitl_smoke_test.py
```

автоматично виконує:

```text
ArduCopter
→ MAVProxy
→ OnboardAutonomy
→ telemetry snapshot
→ tlog assertions
→ process cleanup
```

У цьому сценарії немає arm або команд руху.

## Розділення Python-коду

`run_sitl_smoke_test.py` є тонким CLI:

- читає arguments;
- визначає paths;
- створює унікальну artifact directory;
- друкує короткий результат.

`sitl_harness.py` містить test orchestration:

- `ProcessSupervisor`;
- запуск трьох процесів;
- очікування telemetry snapshot;
- аналіз MAVProxy tlog;
- assertions;
- cleanup.

Так CLI не змішується з process-management бізнес-логікою.

## Process groups

Кожен процес запускається з:

```python
start_new_session=True
```

У Linux це створює окрему process group. Завершення виконується через:

```python
os.killpg(process.pid, signal.SIGTERM)
```

Це важливо для MAVProxy або shell-процесів, які можуть мати власних
дітей. `process.terminate()` завершує лише один PID і може залишити
дочірній ArduCopter.

Якщо процес не завершується за три секунди, harness надсилає
`SIGKILL`. Cleanup знаходиться у `finally`, тому виконується і після
успішного тесту, і після exception.

## Очікування без sleep-driven логіки

OnboardAutonomy запускається з `--json`. Harness використовує
`selectors`, читає stdout у міру появи рядків і чекає snapshot, де:

```text
connected = true
gps_fix_type != null
battery_voltage_v != null
system_health_known = true
```

Є загальний timeout. Фіксований `sleep(20)` був би або повільним, або
нестабільним на іншому комп'ютері.

## Protocol evidence

Самого JSON недостатньо: він доводить наявність телеметрії, але не
доводить, що OnboardAutonomy налаштував її сам.

Після завершення MAVProxy harness читає окремий `mav.tlog` і вимагає:

```text
COMMAND_LONG  message 1   interval 1000000
COMMAND_LONG  message 24  interval 500000
COMMAND_LONG  message 147 interval 1000000
3 або більше COMMAND_ACK: ACCEPTED
1 або більше HEARTBEAT від component 191
```

Таким чином тест перевіряє і observable result, і потрібний MAVLink
protocol flow.

## Logs і artifacts

Кожен запуск створює:

```text
artifacts/sitl-smoke/<timestamp-pid>/
├── arducopter.log
├── mavproxy.log
├── mav.tlog
├── companion.stderr.log
├── companion.snapshots.jsonl
├── summary.json
└── mavproxy-state/
```

`artifacts/` додано до `.gitignore`. При помилці CLI друкує хвости
log-файлів, а повні дані залишаються для аналізу.

## Виявлена проблема з TCP TIME_WAIT

Перша реалізація перевіряла TCP-порт через спробу `bind()`. Одразу
після завершення SITL порт `5760` міг перебувати у `TIME_WAIT`, хоча
активного listener уже не було.

Перевірку змінено на `connect_ex()`:

```text
connection accepted → порт зайнятий listener-ом
connection refused  → активного listener немає
```

UDP-порт і далі перевіряється через `bind()`, бо UDP не має TCP
connection states.

## Unit tests

`test_sitl_harness.py` не запускає важкі процеси. Він перевіряє pure
rules:

- які поля утворюють повний telemetry snapshot;
- які interval requests обов'язкові;
- мінімальну кількість ACK;
- наявність companion heartbeat.

Повний process lifecycle перевіряється самим smoke test.

## Фактичний результат

- Python unit tests: 4 passed.
- Повний smoke test: passed.
- Interval requests: 3.
- Accepted ACK: 3.
- Companion heartbeat у tlog: 4.
- Тест успішно запущено двічі поспіль.
- Після кожного запуску не залишилося дочірніх процесів.
