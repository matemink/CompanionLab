# Повторюваний інтерактивний Gazebo demo flow

## Мета

Один Windows-ярлик запускає повну симуляцію: Gazebo, ArduCopter SITL,
інтерактивний OnboardAutonomy та browser camera preview. Після посадки
клавіша `S` запускає той самий guarded autonomy scenario повторно без
перезапуску процесів.

## Чому недостатньо було перейменувати `L`

Попередня команда `L` одразу надсилала `MAV_CMD_NAV_LAND`. Команда `S`
не є одним MAVLink message: вона має повернути дві state machine у
початковий стан:

- `FlightStartupController`: readiness, GUIDED, ARM і TAKEOFF;
- `AutonomyRuntime`: vision guidance, LAND і підтвердження disarm.

Методи `restart()` очищають system ID, attempts, ACK deadlines, LAND
timers, vision flags і попередній failure result. Завдяки цьому другий
політ не успадковує стан першого.

## Де приймається рішення

`main.cpp` виконує тільки presentation mapping:

```text
's'/'S' -> request_autonomy_start()
'q'/'Q' -> shutdown
```

`CompanionApplication` дозволяє restart лише коли:

- motion-команди дозволені explicit SITL policy;
- автономний scenario сконфігурований;
- flight controller підключений;
- vehicle disarmed;
- попередні startup та autonomy state machines завершені або failed.

Відхилений запит стає видимим `START` event у TUI, але не надсилає
MAVLink-команду.

## Чому Gazebo server і GUI розділені

Комбінований WSLg process міг показувати GUI, але блокувати rendering
sensor: HTTP preview працював, однак `/api/frame` повертав `204`.
Headless server окремо стабільно декодував контрольні 3/3 H.264 frames.

Тому one-click launcher запускає:

1. `gz sim -s` як simulation, physics і rendering-sensor server;
2. `gz sim -g` як окремий WSLg visualization client;
3. ArduCopter SITL;
4. OnboardAutonomy з `--autonomous --interactive`;
5. browser preview лише після першого `200 OK` від `/api/frame`.

GUI тепер не є умовою роботи onboard camera pipeline. Якщо preview не
отримає живий кадр за 30 секунд, launcher показує bounded error замість
нескінченного очікування.

## Перевірка

- C++ build: clean;
- C++ tests: passed;
- headless Gazebo camera check: 3/3 frames;
- desktop shortcut: запустив окремі server і GUI processes;
- camera preview: `640x480`, `HTTP 200`, frame sequence оновлюється;
- runtime arguments: `--sitl --autonomous --interactive`.

Цей demo flow керує лише SITL. Serial hardware як і раніше не приймає
motion-команди.
