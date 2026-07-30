# Мінімальна архітектура CompanionLab

## Проблема до рефакторингу

Папки `core`, `mavlink`, `transport` і `ui` вже існували, але CMake
збирав їх в одну бібліотеку `companionlab_core`. Папка не створює
архітектурної межі: будь-який код міг залежати від будь-якого іншого.

`main.cpp` одночасно:

- читав transport;
- запускав MAVLink decoder;
- планував heartbeat;
- керував telemetry configurator;
- надсилав frames;
- готував дані для UI.

Це ще не був god object, але наступна camera або guidance feature
швидко зробила б його таким.

## Цільова залежність

```text
domain
  ↑
mavlink adapter
  ↑
application → transport port
  ↑
presentation
  ↑
executable composition root
```

Фактичні CMake targets:

```text
companionlab_domain
companionlab_transport_port
companionlab_transport_adapter
companionlab_mavlink_adapter
companionlab_application
companionlab_console_presentation
companionlab
```

`target_link_libraries()` тепер не лише додає бібліотеки linker-у, а
документує дозволений напрямок залежностей.

## CompanionApplication

`CompanionApplication` став application layer. Один виклик:

```cpp
application.poll(now);
```

виконує один прохід event loop:

```text
Transport::read
→ MavlinkDecoder::ingest
→ VehicleState
→ heartbeat timer
→ TelemetryStreamConfigurator
→ Transport::write
```

`main.cpp` залишив собі коректні обов'язки composition root:

- розібрати command-line options;
- створити UDP або serial adapter;
- з'єднати application і presentation;
- обробити process signals;
- керувати життєвим циклом процесу.

## Pimpl

У public header є лише:

```cpp
class Impl;
std::unique_ptr<Impl> impl_;
```

Повний `Impl` визначений у `.cpp`. Це Pimpl, або pointer to
implementation.

Бізнес-причина тут не в приховуванні коду. Public API
`CompanionApplication.hpp` не включає decoder, encoder або telemetry
configurator. Заміна MAVLink-реалізації не змусить усіх клієнтів
перекомпілюватися через зміну private members.

Destructor оголошено в header, але визначено в `.cpp`:

```cpp
CompanionApplication::~CompanionApplication() = default;
```

У цій точці `Impl` уже є complete type, тому `std::unique_ptr` знає,
який destructor викликати.

## AppSnapshot

Раніше `ConsoleView` напряму приймав
`mavlink::TelemetrySetupSnapshot`. Це прив'язувало presentation до
protocol adapter.

Тепер UI отримує:

```cpp
struct AppSnapshot {
    domain::VehicleSnapshot vehicle;
    bool companion_heartbeat_active;
    TelemetryStatus telemetry;
};
```

`CompanionApplication` перетворює внутрішній MAVLink-стан на
нейтральний application model. UI більше не включає MAVLink headers.

## Fake transport test

`CompanionApplicationTests.cpp` використовує `FakeTransport`, який
реалізує той самий абстрактний `Transport`, але зберігає байти в
пам'яті.

Тест проходить повний сценарій без UDP:

```text
autopilot HEARTBEAT
→ companion HEARTBEAT
→ first COMMAND_LONG
→ three COMMAND_ACK
→ Telemetry ACTIVE
```

Це перевіряє wiring між модулями. Детальні правила decoder,
configurator і domain залишаються у вузьких unit tests.

## Що навмисно не додано

Рефакторинг мінімальний:

- немає global event bus;
- немає interface для кожного класу;
- немає dependency-injection framework;
- немає repository або use case без реальної потреби;
- немає C++20 modules.

Новий abstraction з'явився лише там, де вже існувала реальна межа:
transport, application orchestration та presentation model.

## Вирівнювання назви domain

Після першого рефакторингу CMake target уже називався
`companionlab_domain`, але папка та namespace історично залишалися
`core`. Це створювало три назви для однієї ролі.

Виконано повний rename:

```text
src/core
→ src/domain

include/companionlab/core
→ include/companionlab/domain

companionlab::core
→ companionlab::domain
```

Compatibility alias не залишався: кодова база ще невелика, зовнішнього
public API немає, тому підтримувати дві назви означало б створити
технічний борг без користі.

Тепер усі рівні узгоджені:

```text
folder:       domain
namespace:    companionlab::domain
CMake target: companionlab_domain
```

Після rename пройшли C++ tests, Python tests, UDP integration check і
heartbeat-loss SITL regression.

## Фінальне вирівнювання папок

Попередній рефакторинг створив правильні CMake-межі, але назви папок
ще не повністю показували ролі компонентів. `mavlink`, `transport` і
`ui` виглядали як три рівноправні верхньорівневі шари, хоча це не так.

Фінальна структура:

```text
src/
├── domain/
├── application/
├── adapters/
│   ├── mavlink/
│   └── transport/
├── presentation/
│   └── console/
└── main.cpp

include/companionlab/
├── domain/
├── application/
│   └── ports/
├── adapters/
│   ├── mavlink/
│   └── transport/
└── presentation/
    └── console/
```

`Transport` розділено на дві різні речі:

```text
application/ports/Transport.hpp
    абстрактний контракт read/write/description

adapters/transport/TransportFactory.hpp
    створення конкретного UDP або serial adapter
```

Це dependency inversion у практичному вигляді. Application визначає,
який I/O-контракт їй потрібний, але не знає, хто його реалізує.
`main.cpp` як composition root обирає adapter і передає його в
`CompanionApplication`.

MAVLink також лежить в `adapters`, бо він перетворює зовнішній протокол
на domain observations і назад. Console лежить у
`presentation/console`, бо це один конкретний спосіб показати
`AppSnapshot`; майбутній web UI не буде змішаний із ним.

Папку `data` навмисно не додано. В Android-проєкті вона часто об'єднує
network, database, DTO та repository implementations. Тут немає
database чи repository, а конкретні зовнішні інтеграції вже точно
названі `adapters/mavlink` і `adapters/transport`. Загальна `data`
лише приховала б їхні ролі.

## Як розвиваємо проєкт далі

### Правило залежностей

```text
presentation → application → domain
                         ↘ protocol adapters
                         ↘ transport port

transport adapter → transport port
```

Стрілка означає “може залежати від”. Зворотні залежності заборонені:

- `domain` не включає MAVLink, Linux, camera або UI headers;
- `mavlink` не малює UI і не відкриває serial/UDP;
- `transport` не розбирає MAVLink і не знає стан дрона;
- `presentation` не викликає encoder або transport;
- `main.cpp` не реалізує бізнес-правила.

### Нова вхідна телеметрія

Наприклад, для `ATTITUDE` або `GLOBAL_POSITION_INT`:

```text
MavlinkDecoder
→ domain observation
→ VehicleState
→ AppSnapshot
→ Console або JSON
```

MAVLink units і sentinel values перетворюються в decoder. Domain
отримує нормалізовані типи й не знає номерів MAVLink messages.

### Нова команда

Команди більше не додаються безпосередньо в `main.cpp`:

```text
application use case
→ safety policy
→ command lifecycle
→ MavlinkEncoder
→ Transport
→ COMMAND_ACK
→ application state
```

Для arm, flight mode або movement command обов'язково з'явиться
окремий `SafetyGate`. Він перевірятиме connection freshness, режим
роботи, джерело команди, timeout і дозволені межі. Encoder лише
серіалізує вже дозволену команду.

### Camera і computer vision

Майбутній напрямок:

```text
Camera port
← libcamera або Gazebo adapter
→ VideoPipeline
→ TargetDetector
→ Guidance
→ SafetyGate
→ command lifecycle
```

OpenCV не потрапляє в domain vehicle state. Результат detector
перетворюється на невеликий domain type: target position, timestamp і
confidence.

### Новий transport

USB serial і TELEM/UART використовують один `Transport` port. Якщо
знадобиться інший канал, додається adapter, а application code не
змінюється:

```text
UdpTransport
PosixSerialTransport
майбутній TcpTransport
        ↓
     Transport
        ↓
CompanionApplication
```

### Новий UI або логування

Console, JSON, файл або майбутній web dashboard читають
`AppSnapshot`. Вони не отримують доступу до decoder, configurator чи
mutable `VehicleState`.

```text
CompanionApplication::snapshot()
→ ConsoleView
→ JsonView
→ StructuredLogger
```

### Коли створюємо новий модуль

Окремий CMake target додається, якщо одночасно виконуються хоча б дві
умови:

- компонент має окрему зовнішню dependency;
- його можна тестувати ізольовано;
- він має чіткий напрямок залежностей;
- реалізацію реально можна замінити;
- компонент має власний lifecycle або thread.

Одна допоміжна функція чи один DTO не стають окремим модулем.

### Перевірка кожної наступної ітерації

Після нового шматка функціональності перевіряємо:

1. Domain залишився незалежним від framework і hardware.
2. Бізнес-рішення знаходиться в application/domain, не в adapter.
3. I/O виконується лише через port.
4. UI читає snapshot і не змінює внутрішній стан.
5. Новий сценарій має вузький unit test.
6. Wiring має integration test із fake або SITL.
7. CMake target graph не отримав циклічної залежності.

## Перевірка

- CMake створив п'ять окремих production libraries і незалежний
  `companionlab_transport_port`.
- `companionlab` успішно linked із нових targets.
- `companionlab_tests` включає application test із fake transport.
- усі C++ tests пройшли.
- усі Python unit tests пройшли.
- Python UDP integration check пройшов без зміни JSON contract.
- У живому ArduCopter SITL зафіксовано три
  `MAV_CMD_SET_MESSAGE_INTERVAL`, три `COMMAND_ACK: ACCEPTED`, companion
  heartbeat і коректне виявлення втрати зв'язку.
- SITL harness завершив ArduCopter, MAVProxy і CompanionLab без
  залишених фонових процесів.
