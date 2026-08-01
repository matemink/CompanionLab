# Урок 1. Від байтів MAVLink до стану дрона

## Мета

Зрозуміти повний шлях одного `HEARTBEAT`:

```text
Pixhawk або SITL
    -> UDP/serial transport
    -> масив байтів
    -> MAVLink frame parser
    -> HEARTBEAT observation
    -> VehicleState
    -> connected=true у JSON
```

Після уроку ти маєш уміти пояснити, чому відкритий COM/serial port ще не
означає підключення до flight controller і чому один виклик `read()` не
дорівнює одному MAVLink message.

## 1. Transport схожий на InputStream, але не зовсім

Файл:
`include/onboard_autonomy/application/ports/Transport.hpp`

```cpp
virtual std::size_t read(std::span<std::uint8_t> destination) = 0;
```

Android-аналогом приблизно є:

```kotlin
inputStream.read(buffer)
```

Але `std::span` не володіє пам'яттю. Це лише пара:

```text
address + length
```

У Kotlin `ByteArray` є об'єктом, життям якого керує GC. У C++ масив
належить тому scope, де він створений. `span` безпечний лише поки живе
цей масив.

У `CompanionApplication::Impl` власником пам'яті є:

```cpp
std::array<std::uint8_t, 4096> buffer{};
```

Він є полем application-об'єкта і автоматично знищується разом із ним.
Окремий `delete` не потрібний.

## 2. `unique_ptr` означає одного власника

У `main` конкретний transport створюється фабрикою:

```cpp
std::unique_ptr<Transport> transport;
```

Це не аналог nullable Java reference. `unique_ptr` формалізує ownership:
саме цей об'єкт відповідає за знищення transport. Коли `unique_ptr`
виходить зі scope, викликається destructor transport.

Для UDP destructor закриває socket. Для serial destructor закриває file
descriptor. Це базовий приклад RAII:

```text
успішно отримали ресурс -> прив'язали його до lifetime об'єкта
```

RAII у C++ виконує ту роль, яку в Java часто виконують
`try-with-resources`, `use {}` або `finally`.

## 3. `read()` повертає фрагмент потоку

Serial є потоком байтів без меж повідомлень. Один виклик `read()` може
повернути:

- половину MAVLink frame;
- рівно один frame;
- кілька frame одночасно;
- нуль байтів через timeout.

Тому не можна робити так:

```text
read() -> припустити, що отримали одне готове повідомлення
```

UDP зберігає межі datagram, але одна datagram теж може містити довільну
кількість MAVLink bytes. Decoder не повинен залежати від transport.

## 4. Decoder володіє незавершеним frame

Файли:

- `include/onboard_autonomy/adapters/mavlink/MavlinkDecoder.hpp`
- `src/adapters/mavlink/MavlinkDecoder.cpp`

Decoder зберігає між викликами:

```cpp
mavlink_message_t receive_message_{};
mavlink_status_t receive_status_{};
```

Якщо сьогоднішній `read()` приніс перші 12 байтів, parser запам'ятовує
їхній стан. Наступний `read()` продовжує той самий frame.

Ми не пишемо MAVLink framing самостійно. Функція
`mavlink_frame_char_buffer` походить з офіційних generated headers.
Наш код лише керує lifetime parser state і реагує на
`MAVLINK_FRAMING_OK`.

Цю властивість фіксує тест:

```text
tests/MavlinkDecoderTests.cpp
    -> partial_heartbeat_is_reassembled()
```

Тест навмисно розрізає `HEARTBEAT` навпіл. Після першої половини
`connected` залишається `false`; після другої стає `true`.

## 5. Protocol object не є domain state

Після успішного framing decoder отримує `mavlink_message_t`. Це
transport/protocol representation, а не стан дрона.

Для `HEARTBEAT` виконується:

```cpp
mavlink_msg_heartbeat_decode(&message, &heartbeat);
state_.on_heartbeat(...);
```

Такий поділ тобі знайомий з Android:

```text
network DTO -> mapper -> domain model
```

`mavlink_heartbeat_t` є DTO. `VehicleState` є domain model. Завдяки
цьому readiness logic можна тестувати без serial port, Pixhawk або
MAVLink parser.

## 6. Connection визначається heartbeat, а не USB

Файл:
`src/domain/VehicleState.cpp`

Відкритий `/dev/ttyACM0` доводить лише, що Linux бачить serial device.
Це може бути не Pixhawk або Pixhawk може не передавати MAVLink.

Тому connection має дві різні події:

```text
transport opened != flight controller connected
```

Flight controller вважається підключеним лише після валідного
`HEARTBEAT`. Якщо новий heartbeat не приходить три секунди, connection
стає stale.

## 7. Чому `steady_clock`, а не системний час

Freshness перевіряється через:

```cpp
std::chrono::steady_clock
```

Android-аналогом є `SystemClock.elapsedRealtime()`, а не
`System.currentTimeMillis()`.

Системний час може стрибнути після NTP correction або ручної зміни
годинника. Monotonic clock не використовується для календарних дат, але
правильно вимірює timeout:

```text
now - lastHeartbeat > 3 seconds
```

## 8. Missing, failed і stale є різними станами

Для telemetry небезпечно трактувати відсутність даних як успіх.

Приклад:

```text
GPS fix = 1       -> дані є, GPS не готовий
GPS message none  -> стан GPS невідомий
GPS message old   -> дані застарілі
```

У поточному JSON усі три випадки не проходять readiness, але надалі UI
може показувати їх різними кольорами та поясненнями.

`std::optional<T>` використовується там, де значення може бути
відсутнім. Це близько до Kotlin nullable type `T?`, але без GC і з value
semantics.

## 9. Thread safety

`VehicleState` захищений:

```cpp
std::mutex
std::scoped_lock
```

Зараз decoder і snapshot працюють в одному thread, але camera pipeline
пізніше працюватиме паралельно. Lock закладає безпечну межу вже зараз.

На відміну від coroutine, mutex не призупиняє coroutine і не перемикає
dispatcher. Він блокує OS thread. Тому critical section має бути
коротким і не повинен містити serial I/O, network I/O чи важку обробку
кадру.

## 10. Перша практична перевірка

Після завершення WSL setup ми виконаємо:

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

Потім у двох terminal:

```bash
./build/onboard_autonomy --udp-port 14550
```

```bash
python python/scenario_runner.py --scenario healthy
```

До запуску треба спрогнозувати:

1. Який JSON буде до першого heartbeat?
2. Коли `connected` стане `true`?
3. Через скільки секунд після завершення Python-сценарію воно знову
   стане `false`?
4. Чому `no-gps` не блокує отримання heartbeat, але блокує `armable`?

## Маленька самостійна зміна

Після першої зеленої збірки зробимо її через TDD:

1. Додамо тест, що батарея `19%` не ready.
2. Переконаємося, що тест проходить із поточним threshold `20%`.
3. Узгодимо, чи `20%` має проходити.
4. Винесемо threshold з hardcoded constant у configuration.
5. Перевіримо healthy і low-battery scenarios повторно.

Ця зміна маленька, але зачіпає API design, tests, configuration та
доменне рішення. Саме такий формат краще навчає, ніж переписування
великого файлу за інструкцією.

## Питання рівня співбесіди

Після уроку ти маєш відповісти своїми словами:

1. Чому partial MAVLink frame не можна відкидати?
2. Хто володіє byte buffer, transport і parser state?
3. Чим `std::span` відрізняється від `std::vector`?
4. Навіщо transport і decoder розділені?
5. Чому serial port open не означає MAVLink connection?
6. Чому timeout вимірюється через monotonic clock?
7. Чому missing telemetry не можна вважати healthy?
