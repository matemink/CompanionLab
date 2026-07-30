# Raspberry Pi 5 bring-up package

## Мета ітерації

До цього CompanionLab працював на x86-64 Ubuntu у WSL:

```text
CompanionLab -> MAVLink/UDP -> ArduCopter SITL -> Gazebo
```

Тепер готуємо той самий application code до реального ARM64 Linux:

```text
CompanionLab on Raspberry Pi 5
    -> USB serial
    -> Pixhawk 6C
```

Польотна батарея та пропелери для цього етапу не потрібні. Hardware mode
не є буквально read-only: CompanionLab надсилає безпечні
`MAV_CMD_SET_MESSAGE_INTERVAL`, щоб отримати потрібну телеметрію. Він не
надсилає ARM, TAKEOFF, LAND або route commands.

## Два різні поняття

### ARM64 cross-build

Компілятор працює на x86-64 Ubuntu, але генерує ARM64 ELF:

```text
x86-64 host
  aarch64-linux-gnu-g++
      -> ARM aarch64 executable
```

Це доводить, що C++ код, типи й Linux API компілюються під ARM64.

### Target runtime compatibility

Архітектури ARM64 недостатньо. Binary також залежить від ABI та версії
glibc. Ubuntu host toolchain може вимагати новішу glibc, ніж установлена
у Raspberry Pi OS.

Тому deployment candidate проходить другий gate вже на Pi:

```bash
file bin/companionlab
ldd bin/companionlab
```

Якщо runtime несумісний, правильне рішення для першої ітерації —
нативно зібрати код на Pi. Копіювати випадкові `.so` вручну не треба.

Перший cross-build виявив конкретний ризик:

```text
GLIBC_2.34
GLIBCXX_3.4.32
```

glibc була достатньо старою, але `GLIBCXX_3.4.32` походить від GCC 13 і
може бути відсутня у стандартному Raspberry Pi OS Bookworm.

Спроба статично додати GCC 13 C++ runtime прибрала `GLIBCXX`, але
підняла вимогу glibc до:

```text
GLIBC_2.38
```

Цей варіант відхилено. Статична бібліотека сама використовувала нові
glibc symbols.

Фінальний package build використовує GCC 12:

```text
aarch64-linux-gnu-g++-12
```

Його dynamic C++ ABI відповідає поколінню Raspberry Pi OS Bookworm
краще, ніж GCC 13. Остаточні `GLIBC_*` та `GLIBCXX_*` вимоги все одно
перевіряються після кожної збірки й на самому Pi через `ldd`.

Фінальний candidate має:

```text
GLIBC_2.34
GLIBCXX_3.4.29
```

Packaging script містить ABI gate й відхиляє binary, якщо вимога
перевищує `GLIBC_2.36` або `GLIBCXX_3.4.30`. Це не замінює запуск на
реальному Pi, але захищає від випадкового повернення до GCC 13 ABI.

## CMake install contract

У `CMakeLists.txt` додано:

```cmake
include(GNUInstallDirs)
install(
    TARGETS companionlab
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
```

Тепер packaging script не вгадує, де лежить executable. Він використовує
стандартний CMake install layout:

```text
companionlab-pi5/
  bin/
    companionlab
    configure_pi5_uart.sh
    diagnose_pi_hardware.sh
    run_companionlab_pi.sh
  BENCH.md
  LICENSE
```

## Безпечне визначення serial device

Linux може назвати контролер `/dev/ttyACM0`, але цей номер не є стабільною
ідентичністю пристрою. Після reconnect він може змінитися.

Launcher перевіряє джерела в такому порядку:

```text
/dev/serial/by-id/*
/dev/ttyACM*
/dev/ttyUSB*
```

Symlink і його `/dev/tty*` target дедуплікуються через `readlink -f`.
Якщо кандидатів більше одного, launcher завершується помилкою замість
вибору навмання. Потрібний пристрій тоді задається явно:

```bash
COMPANIONLAB_SERIAL=/dev/serial/by-id/usb-... \
    bin/run_companionlab_pi.sh
```

Це важлива production-властивість: ambiguity перетворюється на явну
помилку, а не на підключення до випадкового модема чи debug adapter.

## Safety має два шари

Перший шар — launcher. Він передає лише:

```text
--serial DEVICE --baud 115200 --snapshot-ms 1000 --json
```

Другий шар — C++ CLI. Навіть якщо хтось вручну додасть `--interactive`
або `--scenario`, `main.cpp` відхилить комбінацію із `--serial` ще до
відкриття transport.

## Що перевіряє diagnostics

`diagnose_pi_hardware.sh` нічого не встановлює і не змінює:

- `uname -m` має повернути `aarch64`;
- користувач має належати до `dialout`;
- serial candidate має бути readable/writable;
- `rpicam-hello --list-cameras` має знайти `imx708`;
- binary має бути ARM64 ELF;
- `ldd` не має показувати `not found`.

Камера й Pixhawk діагностуються окремо, щоб дві hardware-проблеми не
маскували одна одну.

## Лог телеметрії

Safe launcher зберігає machine-readable snapshots:

```text
~/.local/state/companionlab/
  telemetry-YYYYMMDDTHHMMSSZ.jsonl
```

Один рядок — один JSON snapshot. Це зручно для подальшого аналізу,
регресійних тестів і прикладання як portfolio evidence.

## Acceptance criteria першого bench

Етап завершений, коли на реальному Raspberry Pi 5:

1. Diagnostics бачить `aarch64`, IMX708 і один Pixhawk serial device.
2. CompanionLab запускається без missing libraries.
3. HEARTBEAT переводить `connected` у `true`.
4. У JSON видно реальний ArduPilot system ID та тип vehicle.
5. USB disconnect не запускає жодної motion command.
6. Створено JSONL-лог реальної телеметрії.

Після цього наступний етап — стабільний Camera Module 3 pipeline через
`rpicam`/GStreamer із вимірюванням FPS, latency, CPU та пам'яті.

## Що перевірено на development host

```text
Compiler: GNU aarch64-linux-gnu-g++ 12.4
ELF:      64-bit ARM aarch64
GLIBC:    2.34
GLIBCXX:  3.4.29
Archive:  companionlab-pi5-arm64.tar.gz
```

Archive містить лише:

```text
companionlab-pi5/
  BENCH.md
  LICENSE
  bin/companionlab
  bin/configure_pi5_uart.sh
  bin/diagnose_pi_hardware.sh
  bin/run_companionlab_pi.sh
```

Diagnostics запущено у WSL як negative bench: він правильно розпізнав
`x86_64`, відсутність IMX708/Pixhawk, побачив ARM64 ELF і пропустив
беззмістовний `ldd` для іншої архітектури.

Safe launcher без serial candidate завершився очікуваним кодом `2` і не
запустив CompanionLab. Наступна неперевірена межа — виконання того самого
archive безпосередньо на Raspberry Pi 5.
