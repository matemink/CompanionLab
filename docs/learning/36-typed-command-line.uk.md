# Типізований command-line interface

## Проблема старого main.cpp

Раніше `main.cpp` одночасно парсив усі аргументи, перевіряв частину
dependencies і створював runtime adapters. Transport визначався непрямо:
якщо `serial_device` порожній, використовувався UDP. Через це CLI contract не
мав окремого типу й майже не тестувався.

## Нова межа

`presentation/cli/CommandLine` перетворює текстові аргументи на
`CommandLineOptions` до створення socket, serial port або camera process.

```cpp
enum class TransportBackend {
    udp,
    serial,
};
```

Це схоже на Kotlin `enum class`, але значення не має неявного переходу в
integer. `main.cpp` тепер робить явний `switch`-подібний вибір за типом, а не
виводить transport зі стороннього string field.

## Що перевіряється

- UDP і serial options взаємовиключні.
- Serial потребує `--serial-device`; UDP має документовані defaults.
- Camera tuning не існує окремо від `--camera`.
- RTP port дозволений лише для GStreamer, FPS лише для `rpicam`.
- Calibration, AprilTag size, extrinsics і autonomy утворюють перевірений
  dependency chain.
- `--interactive` не змішується з machine-readable `--json`.
- Старі `--serial` і scenario options повертають migration message.

Parser не відкриває I/O і тому тестується звичайними наборами
`std::string_view`. Це робить помилки конфігурації детермінованими та не
вимагає Pixhawk, Gazebo або Linux device file.
