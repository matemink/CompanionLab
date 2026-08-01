# C++ camera runtime: від сенсора до OnboardAutonomy

## Ціль ітерації

Попередній benchmark довів, що Camera Module 3 стабільно створює
`30 FPS`. Але кадри тоді забирав `rpicam-vid`, а наш C++ application їх
не бачив.

Тепер шлях такий:

```text
IMX708
  -> libcamera / PiSP
  -> rpicam-vid
  -> raw YUV420 pipe
  -> RpicamCameraSource
  -> CameraMonitor
  -> AppSnapshot
  -> console / JSON
```

## Нові файли та їхня роль

### `application/ports/CameraSource.hpp`

Це port, тобто контракт, яким application описує свою потребу:

```cpp
virtual std::optional<CameraFrame> take_latest_frame() = 0;
virtual CameraSourceStatus status() const = 0;
```

Application не знає про `fork`, file descriptor або `rpicam-vid`.
Майбутній GStreamer чи synthetic source зможе реалізувати той самий
інтерфейс.

`CameraFrame` містить:

- `sequence`;
- розмір і YUV420 bytes;
- `captured_at` із camera metadata;
- `received_at`, записаний C++ adapter після читання повного кадру.

### `adapters/camera/RpicamCameraSource.cpp`

На Pi поки немає development headers `libcamera` і GStreamer. Замість
встановлення великого SDK adapter використовує вже перевірений
`rpicam-vid`.

`fork()` створює дочірній процес, `execvp()` замінює його програмою
`rpicam-vid`. Два Unix pipes розділяють дані:

```text
stdout pipe    -> fixed-size YUV420 frames
metadata pipe  -> FrameWallClock for each frame
```

Фоновий `std::jthread` читає камеру. `std::stop_token` дає cooperative
cancellation, а destructor надсилає дочірньому процесу `SIGINT`. Це
перевірено апаратно: після завершення OnboardAutonomy процес
`rpicam-vid` не залишається.

Queue навмисно має місткість один кадр. Computer vision потрібен
найсвіжіший кадр, а не черга застарілих зображень. Якщо application не
встигає, старий кадр замінюється і counter це показує.

### `application/CameraMonitor.cpp`

Monitor не працює з Linux. Він отримує typed frames і рахує:

```text
consumed FPS
sequence gaps
latest / average / maximum latency
latest frame age
```

Така логіка тестується через `FakeCameraSource` без Raspberry Pi.

### `application/AppSnapshot.cpp`

Старі vehicle JSON-поля залишилися на верхньому рівні, тому Python
integration harness не зламався. Нові дані додані окремим об'єктом:

```json
"camera": {
  "phase": "streaming",
  "measured_fps": 30.013,
  "latest_latency_ms": 10.424,
  "dropped_before_processing": 0
}
```

## Який timestamp використано

Metadata IMX708 містить обидва значення:

```text
SensorTimestamp
FrameWallClock
```

`SensorTimestamp` належить до Linux `CLOCK_BOOTTIME`.
`FrameWallClock` відповідає тому самому моменту кадру, але виражений у
`CLOCK_REALTIME`. `received_at` у C++ теж береться через
`std::chrono::system_clock`, тому віднімати потрібно саме
`FrameWallClock`.

```text
latency = received_at - FrameWallClock
```

Ця метрика охоплює шлях від camera timestamp до повного YUV-кадру в
нашому процесі. Вона ще не включає майбутню AprilTag обробку.

## Баг, який знайшла камера

Перший інтегрований запуск показав приблизно `11 FPS` і багато
перезаписаних кадрів, хоча незалежний benchmark давав рівні `30 FPS`.

Причина була у serial transport:

```cpp
options.c_cc[VMIN] = 0;
options.c_cc[VTIME] = 2;
```

`VTIME` вимірюється не в мілісекундах, а в десятих частках секунди.
Значення `2` дозволяло `read()` блокувати application loop до `200 ms`.
Після переходу на `VTIME = 0` timing знову контролює main loop.

Це хороший приклад системної інтеграції: камера не була зламана, вона
зробила видимим старий blocking behavior іншого adapter.

## Результат на реальному hardware

Одночасно працювали Raspberry Pi 5, Camera Module 3 Wide і Pixhawk 6C
через USB:

```text
Resolution:                 640x480 YUV420
Target FPS:                 30
Measured FPS:               30.013
Dropped before processing:  0
Average latency:            9.8-10.2 ms
Observed maximum latency:   11.595 ms
MAVLink telemetry:          active
Camera process cleanup:     PASS
```

C++ unit tests перевіряють parsing timestamp, FPS, latency, sequence
gaps, JSON compatibility і console output. ARM64 package також пройшов
ABI gate.

Package SHA-256:

```text
8781e84457c7004d68841f32c8c55e2ba3b0176d9c1f910882c4b2ef39d0cb12
```

## Що далі

Receiver уже передає реальні YUV bytes, тому наступний логічний модуль
не transport і не ще один benchmark. Це image processing:

1. Конвертація або пряме використання Y plane.
2. Детектор AprilTag на одному кадрі.
3. Вимірювання окремої vision latency.
4. Typed `TargetObservation`, який ще не надсилає flight commands.

Reconnect після фізичного зникнення камери залишається окремою
незакритою задачею.
