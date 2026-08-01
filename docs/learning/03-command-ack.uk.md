# Перша двостороння MAVLink-команда

## Навіщо вона потрібна

До цього OnboardAutonomy лише читав телеметрію, яку вже надсилав
ArduPilot. Тепер він сам просить рівно ті повідомлення та частоти, які
потрібні його алгоритмам:

```text
SYS_STATUS       1 Hz
GPS_RAW_INT      2 Hz
BATTERY_STATUS   1 Hz
```

Це ще не керування рухом. Команда змінює лише частоту телеметрії.

## COMMAND_LONG

`MAV_CMD_SET_MESSAGE_INTERVAL` передається всередині універсального
MAVLink-повідомлення `COMMAND_LONG`:

```text
source:           system 1, component 191 (OnboardAutonomy)
target:           system 1, component 0 (autopilot default)
command:          511 (MAV_CMD_SET_MESSAGE_INTERVAL)
param1:           MAVLink message ID
param2:           interval in microseconds
param3..param7:   0
```

Наприклад, інтервал `500000` мкс означає два повідомлення за секунду.
Frame створює
`encode_set_message_interval()` у
`src/adapters/mavlink/MavlinkEncoder.cpp`.

## Чому потрібен COMMAND_ACK

Успішний `Transport::write()` означає лише, що байти передані в UDP
socket або serial file descriptor. Він нічого не доводить про обробку
команди польотником.

ArduPilot відповідає `COMMAND_ACK`:

```text
ACCEPTED     команда виконана
DENIED       команда заборонена
UNSUPPORTED  команда не підтримується
FAILED       виконання завершилося помилкою
IN_PROGRESS  виконання ще триває
```

Decoder перетворює MAVLink frame на `CommandAck`, а
`TelemetryStreamConfigurator` вирішує, що робити далі.

## Чому запити йдуть послідовно

`COMMAND_ACK` повертає ID команди `511`, але не повторює `param1` із
ID запитаного повідомлення. Якщо одночасно надіслати три однакові
команди, неможливо надійно визначити, до якого запиту належить ACK.

Тому state machine працює послідовно:

```text
WAITING
   |
   v
request SYS_STATUS -> ACK
   |
   v
request GPS_RAW_INT -> ACK
   |
   v
request BATTERY_STATUS -> ACK
   |
   v
ACTIVE
```

Якщо ACK не приходить за дві секунди, запит повторюється. Після трьох
спроб стан переходить у `FAILED`. Після reconnect весь процес
починається заново.

## Де дивитися код

- `MavlinkEncoder.cpp` формує `COMMAND_LONG`.
- `MavlinkDecoder.cpp` розбирає `COMMAND_ACK`.
- `TelemetryStreamConfigurator.cpp` містить state machine і retries.
- `main.cpp` з'єднує decoder, configurator і transport.
- `TelemetryStreamConfiguratorTests.cpp` перевіряє порядок, ACK,
  retries та відмову.

## Межа безпеки

У коді досі немає arm, зміни flight mode або position setpoint.
Наступні команди руху спочатку з'являться лише в SITL і матимуть
окремий safety gate.

## Відновлення файла після ручного редагування

У `TelemetryStreamConfigurator.cpp` було пошкоджено початок локального
блока: зникли MAVLink namespace, anonymous namespace,
назва `struct StreamRequest` і декларація `kStreamRequests`. Решта
state machine залишилася цілою, тому відновлено лише ці декларації.
Після виправлення target `onboard_autonomy` успішно зібрався, а
`onboard_autonomy_tests` пройшов повністю.
