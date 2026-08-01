# 10. Основа інтеграції з Gazebo

## Мета першої ітерації

Не будуємо власний світ і ще не торкаємося computer vision. Спочатку
маємо довести мінімальний runtime:

```text
Gazebo Iris
<-> ArduCopter SITL
<-> OnboardAutonomy
```

Очікуваний результат — у Gazebo видно Iris, ArduCopter отримує
simulated sensor data і керує його моторами, а OnboardAutonomy отримує
MAVLink telemetry так само, як у попередніх SITL-тестах.

## Хто за що відповідає

```text
Gazebo
  world, gravity, collisions, motor/vehicle physics, simulated sensors

ArduPilot Gazebo plugin
  bridge between Gazebo simulation data and ArduCopter JSON backend

ArduCopter SITL
  flight modes, stabilization, estimators, arming checks, motor outputs

MAVProxy
  MAVLink routing and developer console/map

OnboardAutonomy
  companion-computer telemetry and future autonomy logic
```

Ключова різниця від попереднього запуску: раніше ArduCopter використовував
просту вбудовану SITL vehicle model. Тепер зовнішню фізичну модель
рахує Gazebo.

## Два різні протоколи

Між Gazebo plugin та ArduCopter використовується ArduPilot JSON
simulation interface:

```text
Gazebo <-> JSON <-> ArduCopter
```

OnboardAutonomy не підключається до цього JSON. Його production boundary
залишається MAVLink:

```text
ArduCopter <-> MAVProxy <-> MAVLink UDP 14550 <-> OnboardAutonomy
```

Це важлива архітектурна межа: зміна physics simulator не повинна
змінювати MAVLink adapter нашого застосунку.

## Чому Gazebo Harmonic

Для Ubuntu 24.04 Gazebo має офіційні binary packages. Ми використовуємо
Harmonic, бо це LTS-реліз, який підтримує `gz-sim8` і офіційний
ArduPilot plugin.

ROS або ROS 2 для цього шляху не потрібен. Plugin може працювати
безпосередньо з Gazebo та ArduPilot SITL.

Офіційний plugin зафіксовано на commit:

```text
65937b77aace16735df6f192badb0e6b4eddd056
```

Pin потрібен, щоб через кілька місяців installer не зібрав іншу
версію `main` із неочікуваною поведінкою.

## Де лежать зовнішні файли

Репозиторій OnboardAutonomy не містить копії Gazebo plugin:

```text
~/src/ardupilot_gazebo
~/build/ardupilot_gazebo
```

У нашому Git залишаються тільки installer, launch scripts і
документація. Це така сама модель, яку вже використовуємо для
ArduPilot:

```text
project repository != third-party source != generated build
```

## Нові скрипти

`scripts/install_gazebo_harmonic.sh`:

- додає офіційний OSRF apt repository;
- встановлює Gazebo Harmonic і build dependencies;
- завантажує pinned official plugin;
- збирає його через CMake та Ninja;
- відмовляється працювати з неочікуваним existing checkout.

`scripts/run_gazebo_iris.sh`:

- перевіряє `gz` та `libArduPilotPlugin.so`;
- задає Gazebo plugin/resource search paths;
- запускає офіційний `iris_runway.sdf`.

`scripts/run_arducopter_gazebo.sh`:

- запускає готовий ArduCopter SITL із external model `JSON`;
- додає офіційні `gazebo-iris.parm`;
- запускає MAVProxy як стабільний non-interactive MAVLink router;
- гарантовано завершує ArduCopter через cleanup trap.

Офіційна документація показує `sim_vehicle.py`, але під час WSL
launcher його interactive MAVProxy child отримував закритий stdin і
завершував увесь stack. Сам ArduCopter до цього встигав підключитися до
Gazebo й віддати heartbeat.

Тому runtime script використовує ті самі аргументи, які сформував
`sim_vehicle.py`, але запускає два процеси напряму:

```text
arducopter --model JSON --defaults ...,gazebo-iris.parm
mavproxy.py --out=udp:127.0.0.1:14550 --non-interactive
```

Це не інший simulation path. Ми прибрали лише нестабільний terminal
wrapper, а JSON backend, default parameters і MAVLink routing
залишилися тими самими.

`scripts/run_onboard_autonomy_sitl.sh` не змінювався. Для OnboardAutonomy
джерело MAVLink залишається UDP `14550`.

## Цікавий Bash-синтаксис

```bash
set -euo pipefail
```

- `-e` завершує script після failed command;
- `-u` забороняє мовчки читати undefined variables;
- `pipefail` повертає failure, якщо впала будь-яка команда pipeline.

```bash
readonly gazebo_source_dir="${
    ARDUPILOT_GAZEBO_SOURCE_DIR:-${HOME}/src/ardupilot_gazebo
}"
```

`${VARIABLE:-default}` використовує environment override, якщо він
заданий, інакше бере default. Це дозволяє CI або іншому developer
змінити path без редагування script.

```bash
exec gz sim -v4 -r iris_runway.sdf
```

`exec` не створює зайвий wrapper process, а замінює поточний Bash
процес на Gazebo. Тому `Ctrl+C`, exit code та process lifetime
належать реальній програмі.

## Результат живої перевірки

В Ubuntu 24.04 реально встановлено:

```text
Gazebo Harmonic metapackage
gz-sim 8.14.0
OpenCV 4.6.0
GStreamer 1.24
```

Pinned plugin зібрано через CMake та Ninja:

```text
libArduPilotPlugin.so
libGstCameraPlugin.so
libCameraZoomPlugin.so
libParachutePlugin.so
```

Headless smoke test завантажив `iris_runway.sdf`, DART physics,
Ogre2 rendering, ArduPilot plugin та camera stream на UDP `5600`.

Після запуску Windows launcher одночасно працювали:

```text
gz sim server
gz sim gui
arducopter --model JSON
mavproxy.py
onboard_autonomy
```

One-click launcher також перевірено після повного cleanup попереднього
stack. `StartOnboardAutonomyGazeboDemo.cmd` з нуля сам відкрив Gazebo,
ArduCopter/MAVProxy та OnboardAutonomy; ручного проміжного запуску не
знадобилося.

Активні runtime endpoints:

```text
Gazebo JSON plugin: UDP 9002
ArduCopter MAVLink: TCP 5760
OnboardAutonomy MAVLink: UDP 14550
Gazebo camera H264: UDP 5600
```

Gazebo опублікував реальні topics:

```text
/world/iris_runway/pose/info
/world/iris_runway/stats
.../imu_sensor/imu
.../camera/image
```

Поточний MAVLink `SYS_STATUS`:

```text
messages: 63
enabled:   0x52619DAF
healthy:   0x57719DAF
unhealthy: 0x00000000
```

Отже physics world, simulated sensors, ArduCopter і MAVLink telemetry
працюють одним runtime stack.

Статична перевірка repository:

```text
shell syntax: passed for all four runtime scripts
git diff --check: passed
official package/plugin commands: verified against current docs
```

Після цього наступною окремою ітерацією буде Gazebo camera ->
GStreamer stream, а не custom landing logic.
