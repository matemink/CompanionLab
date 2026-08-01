# Camera Module 3: перший hardware benchmark

## Що перевірено

Raspberry Pi 5 визначив камеру як:

```text
imx708_wide [4608x2592 10-bit RGGB]
```

Це Sony IMX708 у версії Camera Module 3 Wide. Окремий JPEG довів, що
працює не лише виявлення сенсора через control bus, а весь шлях:

```text
IMX708
  -> MIPI CSI-2
  -> Raspberry Pi 5 CFE
  -> PiSP
  -> processed frame
```

## Чому benchmark використовує YUV420

Пробний `rpicam-vid --codec h264` завершився:

```text
Unable to find an appropriate H.264 codec
```

У поточній збірці `rpicam-apps` також вказано `libav:0`. Тому H.264
зараз не можна змішувати з перевіркою самої камери: невідомо було б, що
саме зламалось, capture чи encoder.

Baseline використовує:

```text
1280x720 YUV420 @ 30 FPS
```

Raw output іде в `/dev/null`, але `rpicam-vid` окремо зберігає:

- PTS кожного кадру;
- JSON metadata кожного кадру;
- повний runtime log.

## Поділ відповідальності

### `scripts/benchmark_pi_camera.sh`

Hardware runner:

1. Перевіряє `rpicam-apps` і наявність камери.
2. Запускає bounded capture на точну кількість кадрів.
3. Кожні `100 ms` читає CPU ticks і RSS із `/proc/<pid>`.
4. Передає artifacts у analyzer.
5. Зберігає все під
   `~/.local/state/onboard_autonomy/camera/<run-id>/`.

`trap cleanup EXIT INT TERM` гарантує завершення camera process після
помилки або `Ctrl+C`.

### `python/camera_benchmark.py`

Analyzer не запускає hardware. Він отримує готові файли та обчислює:

```text
measured FPS
frame interval min/average/p95/max
estimated missing frames
average process CPU
peak RSS
sensor temperature/exposure/gain/lux/focus metadata
```

Результат записується одночасно у `report.json` для automation і
`report.md` для людини.

### `python/tests/test_camera_benchmark.py`

Unit-тести не потребують Raspberry Pi. Synthetic timestamps перевіряють
нормальний cadence, gap із двома пропущеними кадрами та failed capture.

## Формули

Для `N` timestamp-ів:

```text
FPS = (N - 1) / (lastPTS - firstPTS)
```

Перший кадр не створює інтервал, тому чисельник саме `N - 1`.

Estimated drops для кожного gap:

```text
round(actualInterval / expectedInterval) - 1
```

Це оцінка за PTS, а не внутрішній hardware counter сенсора.

CPU береться з Linux process accounting:

```text
(delta userTicks + delta systemTicks)
------------------------------------- * 100%
      CLK_TCK * elapsedSeconds
```

Для багатопотокового процесу значення теоретично може бути більше 100%,
бо 100% відповідає одному повністю зайнятому CPU core.

## Результат на Raspberry Pi 5

```text
Camera:                    imx708_wide
Frames:                    300 / 300
Measured FPS:              30.013
Estimated dropped frames:  0
Frame interval:            33.311-33.327 ms
Frame interval p95:        33.322 ms
Average process CPU:       3.95%
Peak RSS:                  30.67 MiB
Sensor temperature:        30.08 C average
Result:                    PASS
```

У metadata було рівно 300 записів для 300 PTS. Це сильніше за просте
«відео наче працює»: cadence рівний і machine-readable acceptance checks
пройшли.

## Що це ще не доводить

PTS доводить frame cadence і gaps, але не end-to-end latency. Для
latency потрібен timestamp безпосередньо в callback, коли наш application
отримав frame. На цьому етапі C++ `CameraSource` навмисно ще не
вигадувався наперед.

Наступний frame receiver повинен дати щонайменше:

```cpp
struct VideoFrame {
    FrameId id;
    CaptureTimestamp captured_at;
    ReceiveTimestamp received_at;
    PixelFormat format;
    FrameSize size;
    FrameBuffer buffer;
};
```

Різниця `received_at - captured_at` стане реальною pipeline latency.

Цей наступний крок уже реалізований і перевірений у
[27-cpp-camera-runtime.uk.md](27-cpp-camera-runtime.uk.md).

## Зафіксовані warnings

`libcamera` повідомив про відсутні static sensor properties для
`imx708_wide` та використання unverified default delays. Capture,
кількість metadata, cadence і ISP output при цьому були коректні.
Warning збережено в raw `rpicam.log`; його не приховано і не названо
помилкою hardware.

ARM64 package SHA-256:

```text
314f6211580f89e8c540c935d5b22bc507b1b4c5e3c976d0b913768882f14b5b
```
