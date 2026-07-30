# Жива MAVLink-активність у TUI

## Чому старі підписи були неправильними

Стрілки спочатку читали останній `LinkEvent`. Це семантична історія:
`ARM`, `LAND`, зміна mode, результат `COMMAND_ACK` тощо. Подія не
протухала, тому давня команда могла виглядати як пакет, що йде зараз.

Тепер у snapshot є два різні поняття:

```cpp
std::vector<LinkEvent> link_events;      // історія бізнес-подій
std::optional<LinkActivity> tx_activity; // останній реальний TX frame
std::optional<LinkActivity> rx_activity; // останній реальний RX frame
```

`LinkEvent` не видалено: він потрібний сценаріям, помилкам і майбутньому
журналу. Але стрілки більше його не використовують.

## RX: що реально прийшло

`MavlinkDecoder` викликає `MessageHandler` лише після:

```cpp
framing == MAVLINK_FRAMING_OK
```

Тому половина пакета не створює імпульсу. Коли кадр повністю зібраний і
перевірений MAVLink parser, декодер передає:

- numeric message ID;
- source system ID;
- source component ID;
- офіційну MAVLink-назву повідомлення.

Назва береться з metadata pinned `c_library_v2` через
`mavlink_get_message_info_by_id()`. Навіть повідомлення, яке ще не
змінює `VehicleState`, видно як реальний RX traffic.

## TX: що реально відправлено

Application передає в `write_frame()` протокольну назву й коротку
деталь. Активність записується тільки після успішного запису всього
кадру в transport:

```text
HEARTBEAT
COMMAND_LONG: SET_INTERVAL SYS_STATUS
PARAM_REQUEST_READ: BATT_ARM_VOLT
COMMAND_LONG: ARM
SET_POSITION_TARGET_LOCAL_NED: MOVE_LOCAL
LANDING_TARGET
```

Невдала спроба лишається `LinkEvent` із помилкою, але не підсвічується
як переданий пакет.

## Як працює миготіння

Інтерактивний TUI перемальовується кожні `100 ms`. Жива активність
видима `550 ms`; потім підпис зникає і лишається порожній провід.
Кожні `120 ms` провід перемикається між двома фазами:

```text
==[ HEARTBEAT ]========================>
--[ HEARTBEAT ]------------------------>
--------------------------------------->
```

RX працює так само, але стрілка спрямована до Raspberry Pi. JSON mode
не прискорено: він і далі використовує `--snapshot-ms`.

У головному циклі є `5 ms` sleep. Це залишає анімацію плавною, але не
створює busy loop із постійним навантаженням одного CPU core.

## Цікаві місця C++20

`std::optional<LinkActivity>` прямо виражає три стани: активності ще не
було, є свіжа активність або є старий запис, який UI уже не показує.

`std::string_view` у decoder не копіює ім'я зі статичної generated
MAVLink-таблиці. Application копіює його у власний `std::string`, коли
створює snapshot state.

`std::chrono::steady_clock` і `milliseconds` використовуються замість
wall-clock time: зміна системної дати не може зламати freshness або
blink phase.

## Тести й hardware acceptance

Regression-тести перевіряють:

- partial frame не створює активності;
- повний `HEARTBEAT` повертає реальну назву та source IDs;
- application розділяє RX і TX;
- яскрава та приглушена фази відрізняються;
- старий підпис зникає;
- довга MAVLink-назва безпечно обрізається до ширини стрілки.

Останній тест додано після реального hardware smoke-test: довга назва
виявила unsigned underflow у розрахунку ширини. Після виправлення
Raspberry Pi 5 із Pixhawk 6C показав:

```text
TX  COMMAND_LONG: SET_INTERVAL SYS...
TX  PARAM_REQUEST_READ: BATT_ARM_VOLT
TX  HEARTBEAT
RX  ATTITUDE
```

ARM64 package SHA-256:

```text
6ab8e00b05e175b02571a2cdfed550cb8e9762ed4ddf6d34a72d6bc81d45df33
```

Motion safety policy не змінювалась: serial hardware mode не отримав
жодної нової команди керування.
