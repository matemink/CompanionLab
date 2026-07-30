# AprilTag і live camera preview

## Що таке AprilTag

AprilTag є **fiducial marker**, тобто штучною візуальною міткою з
відомою геометрією.

Він схожий на QR-код зовні, але вирішує іншу задачу:

- QR переважно переносить payload: текст, URL або інші байти;
- AprilTag дає detector-у стабільний `ID`, центр і чотири кути;
- після калібрування камери та задання фізичного розміру мітки за
  кутами можна оцінити її 3D pose відносно камери.

У CompanionLab використовується рекомендована upstream family
`tagStandard41h12`. На цьому етапі ми визначаємо `ID` та геометрію у
пікселях. 3D pose ще не обчислюється, бо для коректного результату
потрібні camera intrinsics і реальний розмір надрукованої мітки.

## Навіщо потрібен preview

Без preview значення `targets: []` неоднозначне:

- камера може дивитися не туди;
- мітка може бути обрізана краєм кадру;
- зображення може бути розмитим;
- мітка може бути надто малою;
- detector може справді не розпізнавати коректний кадр.

Preview прибирає перші чотири варіанти без логів і здогадок. Він також
малює контур, центр та `ID` тільки для підтвердженої детекції.

## Потік даних

```text
Camera Module 3
  -> rpicam-vid
  -> один YUV420 CameraFrame
  -> AprilTag detector читає Y plane
  -> CameraPreviewSink отримує той самий Y plane + targets
  -> HTTP /api/frame
  -> JavaScript Canvas у Windows browser
```

Другий camera process не запускається. Це важливо: два незалежні
процеси не повинні конкурувати за один Camera Module.

Preview навмисно grayscale. Перша площина YUV420, `Y`, уже містить
яскравість кожного пікселя і саме її читає AprilTag detector. Передача
лише Y:

- не потребує JPEG/H.264 encoder;
- показує оператору саме detector input;
- передає `640 * 480 = 307200` байтів замість повного YUV420 кадру;
- не додає lossy compression artifacts.

## Нові архітектурні частини

### `CameraPreviewSink`

Файл:

```text
include/companionlab/application/ports/CameraPreviewSink.hpp
```

Це application-owned port. Core знає лише контракт:

```cpp
virtual void publish(
    const CameraFrame& frame,
    std::span<const TargetObservation> targets
) = 0;
```

`std::span` тут є non-owning view на послідовність targets. Це ближче
до Kotlin-параметра `List<T>`, який метод лише читає, але без allocation
і без копіювання vector. `span` не володіє елементами, тому sink має
скопіювати дані, якщо вони потрібні після повернення з `publish()`.

Application layer не включає `httplib.h`, socket API або HTML.

### `HttpCameraPreviewServer`

Файли:

```text
include/companionlab/adapters/preview/HttpCameraPreviewServer.hpp
src/adapters/preview/HttpCameraPreviewServer.cpp
```

Adapter реалізує port через pinned `cpp-httplib`. Він:

- зберігає останній preview frame;
- обмежує publication до 10 FPS;
- захищає shared frame через `std::mutex`;
- віддає HTML на `/`;
- віддає raw Y bytes і target headers на `/api/frame`;
- запускає blocking HTTP server у `std::jthread`;
- викликає `server.stop()` у destructor.

`std::jthread` є RAII-thread: під час знищення object він join-ить
worker. Це C++-еквівалент ownership правила: server не може пережити
adapter, який ним володіє.

Mutex потрібен, бо `publish()` викликається application loop, а HTTP
request обробляється іншим thread. Без lock браузер міг би отримати
width від нового кадру та bytes від попереднього.

### Browser presentation

Файл:

```text
assets/camera-preview/index.html
```

JavaScript раз на 100 ms викликає `/api/frame`, перетворює кожен Y byte
на однакові `R`, `G`, `B` значення і малює `ImageData` у `canvas`.
Координати AprilTag уже задані у пікселях цього самого кадру, тому
overlay не потребує додаткового mapper.

## Запуск

На Windows:

```text
StartCompanionLabPixhawk.cmd
```

Launcher відкриває SSH console з CompanionLab, а через три секунди:

```text
http://companionpi.local:8080/
```

Окремо повторно відкрити preview можна через:

```text
OpenCompanionLabCamera.cmd
```

Pi launcher вмикає preview за замовчуванням. Override:

```bash
COMPANIONLAB_CAMERA_PREVIEW_ENABLED=0 \
  bin/run_companionlab_pi.sh
```

або:

```bash
COMPANIONLAB_CAMERA_PREVIEW_PORT=8090 \
  bin/run_companionlab_pi.sh
```

## Виміряний результат

На Raspberry Pi 5 з Camera Module 3 Wide, Pixhawk 6C та відкритим HTTP
preview:

```text
Camera:                 640x480 YUV420 @ 30 FPS
Measured camera FPS:    30.013
Processing drops:       0
Average camera latency: 10.317 ms
Average AprilTag time:  16.342 ms
Preview rate limit:     10 FPS
Preview frame payload:  307200 bytes
```

Final ARM64 package SHA-256:

```text
7c69e7fc501d86a2e55d69fb6ecf7cb98274c5a5c9c7536b3ecda9e72e6ed4b9
```

## Реальна AprilTag-детекція

Фізичний тест використовував офіційну мітку
`tagStandard41h12 / ID 0`, показану на моніторі. Camera Module 3 Wide
дивилась на екран, а CompanionLab аналізував live YUV420 потік на
Raspberry Pi 5.

Після стабілізації камери різниця двох runtime snapshots за 10 секунд
дала:

```text
Processed frames:        301
Frames with target:      301
Detection rate:          100%
Detected family:         tagStandard41h12
Detected ID:             0
Corrected bits:          0
Latest decision margin:  181.43
Measured camera FPS:     30.013
Dropped frames:          0
Average vision time:     13.385 ms
```

`corrected_bits = 0` означає, що detector прочитав код без виправлення
помилкових бітів. `decision_margin = 181.43` є detector quality metric:
у цьому тесті правильна детекція мала великий запас, але це не
ймовірність і не відсоток confidence.

Цей тест підтверджує повний read-only hardware path:

```text
Camera Module 3
  -> rpicam-vid
  -> YUV420 frame
  -> AprilTagTargetDetector
  -> TargetObservation ID 0
  -> HTTP preview overlay
```

Pose estimation і MAVLink `LANDING_TARGET` залишаються наступними
окремими кроками.

HTTP зараз не має authentication або TLS і призначений лише для
локальної довіреної bench-мережі. Це diagnostic UI, не production
video streaming protocol.
