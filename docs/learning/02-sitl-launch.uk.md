# Як запускаються SITL і OnboardAutonomy

## Що запускає Windows

У корені проєкту є `StartOnboardAutonomyDemo.cmd`. Він відкриває два окремі WSL
процеси:

```text
StartOnboardAutonomyDemo.cmd
├── WSL → scripts/run_arducopter_sitl.sh
└── WSL → scripts/run_onboard_autonomy_sitl.sh
```

Ключова частина Windows-команди:

```bat
wsl.exe -d Ubuntu-24.04 --cd "%~dp0" -- bash scripts/run_arducopter_sitl.sh
```

- `-d Ubuntu-24.04` вибирає конкретний WSL-дистрибутив.
- `--cd "%~dp0"` переходить у папку, де лежить launcher.
- `--` відділяє параметри WSL від Linux-команди.
- `bash ...` запускає потрібний shell script усередині Ubuntu.

## Що запускає ArduCopter script

`scripts/run_arducopter_sitl.sh` запускає два Linux-процеси:

```text
arducopter ← TCP 5760 → MAVProxy ← UDP 14550 → OnboardAutonomy
```

Першим стартує зібраний ArduCopter:

```bash
build/sitl/bin/arducopter \
    -S \
    --model + \
    --defaults Tools/autotest/default_params/copter.parm \
    -I0
```

- `-S` вмикає synthetic clock для часу симуляції.
- `--model +` вибирає стандартну модель квадрокоптера.
- `--defaults` завантажує стартові Copter-параметри.
- `-I0` вибирає instance 0 і його стандартні порти.

ArduCopter відкриває MAVLink TCP server на `127.0.0.1:5760`. Потім
script запускає MAVProxy:

```bash
mavproxy.py \
    --master=tcp:127.0.0.1:5760 \
    --sitl=127.0.0.1:5501 \
    --out=udp:127.0.0.1:14550 \
    --streamrate=-1 \
    --non-interactive
```

MAVProxy підключається до ArduCopter як MAVLink-клієнт і пересилає
потік на UDP-порт `14550`. `--streamrate=-1` забороняє MAVProxy
самостійно змінювати частоту телеметрії: потрібні потоки налаштовує
OnboardAutonomy через `MAV_CMD_SET_MESSAGE_INTERVAL`. Так ми уникаємо
конфлікту двох клієнтів. Параметр `--non-interactive` вимикає
командний prompt, але не перетворює MAVProxy на фоновий daemon.

ArduCopter працює у background цього script, а MAVProxy у foreground.
Shell `trap` завершує ArduCopter, коли MAVProxy зупиняється.

## Що запускає OnboardAutonomy script

`scripts/run_onboard_autonomy_sitl.sh` виконує:

```bash
~/build/onboard_autonomy/onboard_autonomy \
    --udp-bind 127.0.0.1 \
    --udp-port 14550 \
    --snapshot-ms 1000
```

`exec` замінює shell-процес самим C++-бінарником. OnboardAutonomy слухає
MAVLink UDP на `14550` і раз на секунду друкує JSON snapshot.

## Як дані проходять в обидва боки

Вхідний напрямок:

```text
ArduCopter → TCP 5760 → MAVProxy → UDP 14550
→ UdpTransport → MavlinkDecoder → VehicleState → JSON
```

Зворотний напрямок:

```text
MavlinkEncoder → HEARTBEAT або COMMAND_LONG → UdpTransport
→ MAVProxy → ArduCopter
```

Після першого пакета `UdpTransport` запам'ятовує UDP peer MAVProxy.
OnboardAutonomy бере system ID з autopilot heartbeat і надсилає власний
heartbeat як component `191`, `MAV_TYPE_ONBOARD_CONTROLLER`. Після
цього він послідовно налаштовує потоки health, GPS і battery та чекає
`COMMAND_ACK` після кожного запиту.

## Ручний запуск

Перша Ubuntu-консоль:

```bash
cd "/mnt/c/Users/<windows-user>/path/to/OnboardAutonomy"
bash scripts/run_arducopter_sitl.sh
```

Друга Ubuntu-консоль:

```bash
cd "/mnt/c/Users/<windows-user>/path/to/OnboardAutonomy"
bash scripts/run_onboard_autonomy_sitl.sh
```

Кожен процес зупиняється через `Ctrl+C` у відповідній консолі.
