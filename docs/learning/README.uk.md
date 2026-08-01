# OnboardAutonomy як навчальний проєкт

## Для кого цей маршрут

Цей маршрут розрахований не на початківця у програмуванні, а на senior
Android-інженера з досвідом Kotlin/Java, Clean Architecture,
багатопотоковості, API та великих production-застосунків.

Тому ми не витрачаємо час на пояснення змінних, циклів, класів чи SOLID.
Натомість детально розбираємо те, що суттєво відрізняється від Android:

- ownership і lifetime замість garbage collector;
- value semantics, references, pointers, copy і move;
- компіляція, linking, headers, CMake і toolchain;
- Linux file descriptors, serial, UDP і permissions;
- обробка нескінченного потоку байтів та partial messages;
- поведінка системи за відсутніх або застарілих даних;
- ARM64 deployment і обмеження embedded-систем;
- MAVLink, ArduPilot, SITL, сенсори та flight-controller semantics;
- GStreamer, camera pipeline і вимірювання latency;
- алгоритми оцінки положення та precision landing.

## Як ми працюємо

Кожен етап проходить однаково:

1. **Контекст.** Яку реальну проблему дрона вирішуємо.
2. **Місток з Android.** Що вже знайоме з Kotlin/Java.
3. **Нова концепція.** Що в C++/Linux працює інакше та чому.
4. **Невеликий шматок коду.** Розбираємо один data flow, а не весь
   репозиторій.
5. **Експеримент.** Змінюємо одну умову і прогнозуємо результат до
   запуску.
6. **Тест.** Фіксуємо очікувану поведінку автоматизованим тестом.
7. **Перевірка розуміння.** Ти пояснюєш рішення своїми словами так, як
   пояснював би його іншому інженеру під час design review.

Після кожного завершеного шматка коду створюємо або оновлюємо окремий
`NN-topic.uk.md` у цій папці. Репорт містить ключові методи, новий C++
синтаксис, бізнес-логіку, тести та результат живої перевірки. У чаті
залишається лише короткий підсумок і посилання на цей файл.

Я не буду приховувати складність за згенерованим кодом. Перед великими
змінами пояснюватиму дизайн, після змін показуватиму ключові місця,
результат тестів і компроміси. Код, який ти не можеш пояснити, ще не
вважаємо завершеним навчальним результатом.

## Навчальний маршрут

### 1. MAVLink data path

Від сирих байтів до осмисленого стану апарата:

```text
UDP або serial -> byte buffer -> MAVLink decoder -> VehicleState -> JSON
```

Нові теми: RAII, `std::span`, `std::optional`, partial frames,
`steady_clock`, stale data, CMake.

Матеріал: [01-mavlink-data-path.uk.md](01-mavlink-data-path.uk.md)

### 2. Linux build і process model

Збираємо C++ у WSL, розбираємо compiler, linker, CMake targets,
dependencies, exit codes, signals і stdout/stderr.

Практичний результат: локальна збірка та зелені C++ tests.

### 3. ArduPilot SITL

Запускаємо віртуальний ArduCopter, маршрутизуємо MAVLink через UDP і
спостерігаємо реальні `HEARTBEAT`, GPS, battery та `STATUSTEXT`.

Практичний результат: companion service працює з повним автопілотом, а
не лише з нашими штучними повідомленнями.

### 4. MAVLink command/ACK

Перед failure injection додаємо першу двосторонню команду:
OnboardAutonomy сам налаштовує частоти телеметрії через
`MAV_CMD_SET_MESSAGE_INTERVAL` і перевіряє `COMMAND_ACK`.

Матеріал: [03-command-ack.uk.md](03-command-ack.uk.md)

### 5. Мінімальна архітектура

Розділяємо domain, MAVLink, transport, application і presentation
окремими CMake targets. Виносимо orchestration з `main.cpp`, а UI
переводимо на нейтральний `AppSnapshot`.

Матеріал: [04-minimal-architecture.uk.md](04-minimal-architecture.uk.md)

### 6. Автоматичний SITL smoke test

Python сам запускає ArduCopter, MAVProxy і OnboardAutonomy, перевіряє
телеметрію та ACK, зберігає логи й завершує всю process group.

Матеріал: [05-sitl-smoke-harness.uk.md](05-sitl-smoke-harness.uk.md)

### 7. Failure injection і integration tests

Python запускає сценарії втрати heartbeat, GPS, battery telemetry та
PreArm errors. C++ залишається production-кодом, Python виконує роль
test orchestration.

Практичний результат: повторювані SITL-тести замість ручного
"здається, працює".

Перший сценарій: [06-heartbeat-loss.uk.md](06-heartbeat-loss.uk.md)

Другий сценарій:
[07-gps-loss.uk.md](07-gps-loss.uk.md)

Третій сценарій:
[08-low-battery.uk.md](08-low-battery.uk.md)

Четвертий сценарій:
[09-prearm-integration.uk.md](09-prearm-integration.uk.md)

### 8. Gazebo як зовнішній physics simulator

Додаємо офіційний ArduPilot Gazebo plugin і розділяємо JSON simulation
interface та MAVLink companion interface.

Матеріал:
[10-gazebo-foundation.uk.md](10-gazebo-foundation.uk.md)

GPU-діагностика:
[11-wslg-gpu-acceleration.uk.md](11-wslg-gpu-acceleration.uk.md)

Ручний політ:
[12-first-gazebo-flight.uk.md](12-first-gazebo-flight.uk.md)

Автоматичний політ із C++ companion:
[13-automated-gazebo-flight.uk.md](13-automated-gazebo-flight.uk.md)

Невдалий експеримент із декоративним аеродромом і його rollback:
[14-gazebo-airfield-failed-experiment.uk.md](14-gazebo-airfield-failed-experiment.uk.md)

Перший перевірений RTP/H.264 потік із симульованої камери через GStreamer:
[15-gstreamer-simulated-camera.uk.md](15-gstreamer-simulated-camera.uk.md)

MS-DOS TUI для команд, ACK і телеметричних підтверджень:
[16-mavlink-command-bus-tui.uk.md](16-mavlink-command-bus-tui.uk.md)

Безпечні keyboard triggers для сценарію та manual LAND:
[17-interactive-scenario-triggers.uk.md](17-interactive-scenario-triggers.uk.md)

Мінімальна ASCII-панель і семантичні кольори:
[18-minimal-ascii-control-panel.uk.md](18-minimal-ascii-control-panel.uk.md)

Typed Scenario Engine, локальні маршрути, RTL і п'ять SITL-демо:
[19-scenario-engine-five-demos.uk.md](19-scenario-engine-five-demos.uk.md)

ARM64 packaging, безпечний Pixhawk USB bench і Raspberry Pi diagnostics:
[20-raspberry-pi-bringup-package.uk.md](20-raspberry-pi-bringup-package.uk.md)

Raspberry Pi OS Lite, headless SSH, ARM64 deployment і перший реальний
bench:
[21-raspberry-pi-first-boot.uk.md](21-raspberry-pi-first-boot.uk.md)

Перший Pixhawk 6C USB bench, реальний MAVLink і виправлення battery
readiness через `BATT_ARM_VOLT`:
[22-first-pixhawk-usb-bench.uk.md](22-first-pixhawk-usb-bench.uk.md)

Двонаправлений telemetry handshake, шість прийнятих потоків і
документований запит firmware/hardware metadata:
[23-autopilot-version-handshake.uk.md](23-autopilot-version-handshake.uk.md)

Повний pinned ArduPilot board catalog, aliases і data-driven hardware
mapper:
[24-ardupilot-board-type-catalog.uk.md](24-ardupilot-board-type-catalog.uk.md)

Фактичні TX/RX MAVLink-пакети, freshness і миготіння стрілок:
[25-live-mavlink-link-activity.uk.md](25-live-mavlink-link-activity.uk.md)

Перший hardware capture Camera Module 3, PTS, dropped frames і
CPU/RAM benchmark:
[26-camera-module-3-hardware-benchmark.uk.md](26-camera-module-3-hardware-benchmark.uk.md)

C++ `CameraSource`, `rpicam` process adapter, `FrameWallClock`, реальна
latency і одночасна робота з Pixhawk:
[27-cpp-camera-runtime.uk.md](27-cpp-camera-runtime.uk.md)

AprilTag як visual fiducial, application port, `std::span`, HTTP adapter
і live grayscale preview з target overlay:
[28-apriltag-live-preview.uk.md](28-apriltag-live-preview.uk.md)

Camera intrinsics, distortion, printable checkerboard, OpenCV quality
gate і reproducible calibration JSON:
[29-camera-calibration.uk.md](29-camera-calibration.uk.md)

### 9. Raspberry Pi 5 і Pixhawk 6C

Збираємо ARM64 binary на Pi, працюємо з `/dev/ttyACM0`, Linux groups,
USB serial, reconnect і structured logs.

Практичний результат: реальний Embedded Linux deployment без запуску
моторів.

### 10. Camera Module 3 і GStreamer

Розбираємо libcamera, формати кадрів, pipeline, buffering, backpressure,
frame rate, latency та відновлення після втрати камери.

Практичний результат: стабільний відеопотік на Raspberry Pi.

### 11. Computer vision

OpenCV знаходить AprilTag, оцінює pose та confidence. Окремо розбираємо
camera intrinsics, coordinate frames і фільтрацію шуму.

Практичний результат: виміряне положення landing target.

### 12. Precision landing

Перетворюємо результат vision у MAVLink `LANDING_TARGET`, перевіряємо
поведінку в Gazebo і тестуємо втрату target.

Практичний результат: автономна посадка лише в симуляторі.

### 13. Production engineering

Додаємо systemd, ARM profiling, GitHub Actions, release artifacts,
технічний README, architecture diagram і коротке demo video.

Практичний результат: проєкт, який можна відтворити, продемонструвати й
технічно обґрунтувати.

## Що вже готово, а що ще ні

У репозиторії вже є перевірений у WSL2 код першого data path,
Python-сценарії, unit tests, UDP integration check, ARM64 CI
configuration та hardware notes. Це ще не означає, що перший етап
вивчено.

Етап вважаємо завершеним лише коли:

- C++ збирається локально;
- тести проходять;
- healthy і failure scenarios дають очікуваний результат;
- ти можеш пояснити ownership, partial frame і freshness timeout;
- ти самостійно робиш одну маленьку зміну через тест.
