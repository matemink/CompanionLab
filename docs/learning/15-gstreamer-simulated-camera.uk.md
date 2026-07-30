# GStreamer-потік із симульованої камери

## Мета ітерації

Провести кадр не лише всередині Gazebo, а через той самий клас video pipeline,
який пізніше використає companion computer:

```text
Gazebo camera
  -> RGB frame
  -> I420
  -> x264 encoder
  -> RTP/H.264
  -> UDP 127.0.0.1:5600
  -> jitter buffer
  -> H.264 decoder
  -> RGB frame
```

Це ще не computer vision. На цьому етапі доведено, що application side може
стабільно отримати і декодувати зображення із симулятора.

## Що насправді робить офіційний плагін

Використовується pinned commit ArduPilot Gazebo:

```text
65937b77aace16735df6f192badb0e6b4eddd056
```

У `src/GstCameraPlugin.cc` офіційний плагін створює:

```text
appsrc
  -> queue
  -> videoconvert
  -> x264enc
  -> rtph264pay
  -> udpsink
```

Його параметри для моделі `gimbal_small_3d`:

```text
host: 127.0.0.1
port: 5600
input: I420
resolution: 640x480
sensor update rate: 10 fps
x264 bitrate: 800 kbit/s
x264 tune: zerolatency
keyframe interval: 10
```

Джерело:
[ArduPilot GstCameraPlugin.cc](https://github.com/ArduPilot/ardupilot_gazebo/blob/65937b77aace16735df6f192badb0e6b4eddd056/src/GstCameraPlugin.cc).

## Receiver pipeline

`scripts/check_gazebo_camera_stream.sh` збирає зворотний шлях:

```text
udpsrc
  -> rtpjitterbuffer
  -> rtph264depay
  -> h264parse
  -> avdec_h264
  -> videoconvert
  -> RGB
  -> fakesink
```

Призначення елементів:

| Елемент | Функція |
|---|---|
| `udpsrc` | Отримує UDP datagram-и з порту `5600` |
| `rtpjitterbuffer` | Відновлює порядок RTP-пакетів і згладжує jitter |
| `rtph264depay` | Прибирає RTP-обгортку |
| `h264parse` | Нормалізує H.264 bitstream |
| `avdec_h264` | Декодує compressed video у raw frames |
| `videoconvert` | Перетворює raw frame у потрібний pixel format |
| `fakesink` | Приймає кадри без GUI; зручно для автоматичного тесту |

`scripts/view_gazebo_camera.sh` відрізняється лише останнім sink:
`autovideosink` відкриває звичайне WSLg-вікно з відео.

## Важливий lifecycle

Плагін не починає кодувати лише через те, що у SDF задано UDP port. Йому
потрібна окрема Gazebo-команда:

```text
/world/iris_runway/model/iris_with_gimbal/model/gimbal/link/pitch_link/
sensor/camera/image/enable_streaming
```

Payload:

```text
gz.msgs.Boolean { data: true }
```

Тому скрипт виконує такий порядок:

1. Запускає UDP receiver у background.
2. Відправляє `data: true`.
3. Чекає 60 декодованих кадрів або timeout.
4. Через `trap` завжди відправляє `data: false`.

`trap cleanup EXIT INT TERM` тут виконує роль `finally`: cleanup працює при
успішному завершенні, помилці та `Ctrl+C`.

## Дві знайдені пастки

Перший тест завершився timeout, бо receiver не надсилав explicit
`enable_streaming`.

Другий тест дійшов до producer-а, але Gazebo написав:

```text
GstCameraPlugin: failed to create GStreamer elements
```

Офіційний plugin використовує `x264enc`, який у Ubuntu міститься в окремому
пакеті `gstreamer1.0-plugins-ugly`. Після його встановлення Gazebo server
треба перезапустити: GStreamer registry уже був ініціалізований старим
процесом.

Відтворюваний install-скрипт тепер явно встановлює CLI, base, good, bad,
ugly та libav plugins.

## Запуск

Коли офіційний `iris_runway` уже працює:

```bash
bash scripts/check_gazebo_camera_stream.sh
```

Очікуваний результат:

```text
PASS: decoded 60 camera frames.
```

Для ручного перегляду:

```bash
bash scripts/view_gazebo_camera.sh
```

Одночасно запускається лише один receiver порту `5600`.

## Жива перевірка

Перевірено у WSL2 Ubuntu 24.04 з Gazebo Harmonic 8:

```text
input: RTP/H.264, UDP 5600
target: 60 decoded RGB frames
timeout: 15 seconds
result: PASS
elapsed: approximately 8 seconds
camera rate: 10 fps; elapsed time includes encoder startup
producer cleanup: streaming stopped
```

Таким чином, перший пункт Milestone 3 video pipeline завершено. Наступний
крок не полягає в прикрашанні world: треба підключити GStreamer receiver до
коду application, визначити frame API і вимірювати FPS/latency.
