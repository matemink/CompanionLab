# MAVLink metadata handshake: шість потоків і AUTOPILOT_VERSION

## Навіщо ця ітерація

Один `HEARTBEAT` доводить, що Raspberry Pi отримує пакети від
контролера. Він не доводить, що Pixhawk отримує наші команди.

Повний двонаправлений тест виглядає так:

```text
Raspberry Pi 5                  Pixhawk 6C
      |                              |
      | MAV_CMD_SET_MESSAGE_INTERVAL |
      |----------------------------->|
      |          COMMAND_ACK         |
      |<-----------------------------|
```

CompanionLab послідовно налаштовує шість потоків телеметрії. Наступний
запит надсилається лише після `MAV_RESULT_ACCEPTED` для попереднього.
Технічний стан `6/6 ACK` означає, що працюють обидва напрямки link і
контролер прийняв усі шість налаштувань. В операторському UI він
показаний зрозуміліше: `TELEMETRY READY / 6 STREAMS`.

## Чому HEARTBEAT не містить версію

`HEARTBEAT` навмисно малий і періодичний. Він передає тип апарата,
тип autopilot, mode, armed state і system status, але не конкретну
версію firmware або hardware.

Версія запитується окремо:

```text
CompanionLab
    -> COMMAND_LONG
       MAV_CMD_REQUEST_MESSAGE
       param1 = AUTOPILOT_VERSION (message id 148)

Pixhawk
    -> COMMAND_ACK
    -> AUTOPILOT_VERSION
```

`COMMAND_ACK` підтверджує прийняття команди. `AUTOPILOT_VERSION`
містить результат. Це два різні повідомлення.

## Які дані повертаються

CompanionLab зберігає тільки документовані MAVLink-поля:

- firmware `major.minor.patch`;
- release type: `DEV`, `ALPHA`, `BETA`, `RC` або `OFFICIAL`;
- capability bitmap;
- raw board version;
- vendor ID;
- product ID.

Поле `flight_sw_version` має 32 біти:

```text
| major | minor | patch | release type |
   8       8       8          8 bits
```

Decoder виконує bit shift і mask, а domain отримує вже нормальну
`AutopilotMetadata`. Через це domain не залежить від способу пакування
MAVLink.

`board_version` не гарантує маркетингову назву `Pixhawk 6C`. За
стандартом верхні 16 біт містять board type, а молодші 8 біт можуть
містити silicon ID. Якщо контролер не повідомив значення, UI показує
`BOARD UNREPORTED`, а не вигадує модель.

## Де лежить код

- `MavlinkEncoder.cpp` створює read-only `MAV_CMD_REQUEST_MESSAGE`.
- `MavlinkDecoder.cpp` розпаковує `AUTOPILOT_VERSION`.
- `VehicleState.hpp/.cpp` зберігає protocol-independent metadata.
- `CompanionApplication.cpp` запускає запит після прийняття всіх шести
  telemetry streams.
- `ConsoleView.cpp` агрегує технічні результати в один рядок.

## Чому це безпечно

Запит читає metadata і не змінює параметри, mode, armed state або
actuator outputs. Hardware launcher і далі блокує всі motion commands.

## Перевірки

- encoder test перевіряє command ID, message ID і адресу відповіді;
- decoder test перевіряє byte-level розпакування версії;
- application test перевіряє порядок після шести ACK;
- console test перевіряє людський формат результату;
- повний C++ test suite проходить у WSL.

## Результат на реальному Pixhawk 6C

Read-only probe на Raspberry Pi 5 отримав:

```text
firmware:      4.5.6 OFFICIAL
capabilities:  64495
board_version: 3670016
board_type:    56
silicon_id:    0
vendor_id:     12642
product_id:    83
```

Локальна таблиця з ArduPilot `Tools/AP_Bootloader/board_types.txt`
визначає board type `56` як:

```text
Reserved "PX4 [BL] FMU v6C.x"
```

Це узгоджується з USB product `Holybro Pixhawk6C-bdshot`. UI все одно
показує окремо MAVLink board type і USB-назву, бо це два незалежні
джерела даних.

ARM64 package SHA-256:

```text
546eb24d627520721350b5fd6059efa53b45e8a4665982137de2d9cc5320beb6
```

Офіційні визначення зафіксовані в pinned MAVLink C library:

- `MAV_CMD_REQUEST_MESSAGE`;
- `AUTOPILOT_VERSION`;
- `FIRMWARE_VERSION_TYPE`.
