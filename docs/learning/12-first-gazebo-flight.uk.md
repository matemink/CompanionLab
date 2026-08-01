# Перший керований політ у Gazebo

## Хто за що відповідає

```text
MAVProxy command
      |
      | MAVLink
      v
ArduCopter SITL
      |
      | motor commands / simulated sensor data
      v
Gazebo + ArduPilot plugin
      |
      v
3D model movement
```

Gazebo не приймає команду `takeoff` і не реалізує логіку польоту. Він рахує
фізику, рух моделі та показання віртуальних сенсорів. Режими, arming,
стабілізація і посадка виконуються повним ArduCopter SITL.

MAVProxy тут є ручною ground-control консоллю. Пізніше ці самі MAVLink-команди
надсилатиме OnboardAutonomy, але перший політ корисно один раз провести вручну:
це ізолює simulator/autopilot integration від нашої майбутньої flight logic.

## Інтерактивна консоль

Раніше `scripts/run_arducopter_gazebo.sh` запускав MAVProxy з параметром:

```text
--non-interactive
```

Це підходило для фонового smoke run, але вимикало введення команд. Параметр
видалено. Тепер вікно `ArduCopter SITL - MAVProxy Flight Console` має prompt:

```text
MAV>
```

## Перший політ

Команди потрібно вводити по одній, чекаючи підтвердження попередньої:

```text
mode GUIDED
arm throttle
takeoff 5
mode LAND
```

- `mode GUIDED` дозволяє зовнішньому клієнту задавати цілі польоту.
- `arm throttle` переводить simulated vehicle в armed state.
- `takeoff 5` задає цільову висоту 5 метрів відносно точки старту.
- `mode LAND` передає подальше зниження і посадку логіці ArduCopter.

Якщо arming відхилено, не потрібно вимикати перевірки. Треба прочитати
`PreArm:` повідомлення, дочекатися готовності симульованих сенсорів або
виправити конкретну причину. Це та сама readiness-модель, яку вже обробляє
OnboardAutonomy.

## Зміна у Bash

```bash
if [[ -t 1 ]]; then
    printf '\033]0;ArduCopter SITL - MAVProxy Flight Console\007'
fi
```

`[[ -t 1 ]]` перевіряє, що stdout з номером file descriptor `1` підключений
до інтерактивного термінала. Escape sequence змінює заголовок вікна, але не
забруднює логи під час автоматизованого запуску без TTY.
