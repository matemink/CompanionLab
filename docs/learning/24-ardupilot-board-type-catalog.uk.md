# Повний ArduPilot Board Type Catalog

## Проблема hardcode

Перша версія UI містила:

```cpp
if (board_type == 56) {
    return "PX4 [BL] FMU v6C.x";
}
```

Це працювало лише для нашого Pixhawk. Будь-який інший контролер
повертав би `BOARD TYPE N`, хоча ArduPilot уже має офіційну таблицю.

## Джерело даних

OnboardAutonomy містить незмінений snapshot:

```text
ArduPilot/Tools/AP_Bootloader/board_types.txt
commit 92b0cd788ec29406f26c6f9c31d5ceedbd1cc538
```

У snapshot:

```text
369 table rows
358 unique numeric board IDs
10 IDs with more than one alias
```

Файл встановлюється разом із ARM64 package:

```text
share/onboard_autonomy/ardupilot-board-types.txt
```

Таблиця та її GPL-3.0 notice зберігаються як окремі third-party data.
Parser OnboardAutonomy написаний окремо і залишається MIT-кодом.

## Архітектура

```text
AUTOPILOT_VERSION
    -> numeric board_type
    -> BoardTypeResolver interface
    -> ArduPilot BoardTypeCatalog adapter
    -> preferred name + every alias
    -> ConsoleView
```

`ConsoleView` не читає файли й не знає формат ArduPilot. Він залежить
лише від `BoardTypeResolver`.

Це dependency inversion: presentation визначає потрібний contract, а
adapter реалізує його через зовнішню таблицю.

## Правила mapper

Кожний непорожній рядок містить назву та numeric ID. Parser:

1. Видаляє comment після `#`.
2. Читає останнє число як board ID.
3. Зберігає решту рядка як alias.
4. Для `Reserved "friendly name"` прибирає `Reserved` і quotes.
5. Вибирає friendly reserved name як preferred.
6. Не викидає інші aliases з тим самим ID.

Для нашого контролера:

```text
board_type 56 -> PX4 [BL] FMU v6C.x
```

## Чому alias важливий

Numeric ID не завжди однозначний. Наприклад ID `9` має кілька записів
для різних історичних hardware names. За одного numeric ID програма не
може довести точну маркетингову модель.

Тому результат mapper має структуру:

```cpp
struct BoardTypeMatch {
    std::string preferred_name;
    std::vector<std::string> aliases;
};
```

Якщо aliases більше одного, UI показує їх кількість. Якщо ID відсутній
у pinned snapshot, UI показує `BOARD TYPE N`, а не вгадує назву.

## Перевірки

- synthetic parser test для quoted та unquoted `Reserved`;
- duplicate-ID test зі збереженням усіх aliases;
- full-table gate на 369 rows і 358 IDs;
- реальний lookup `56 -> PX4 [BL] FMU v6C.x`;
- fallback test для невідомого ID;
- console test для ambiguous ID.

## Hardware acceptance

Packaged ARM64 binary на Raspberry Pi 5 прочитав таблицю з:

```text
$HOME/onboard_autonomy-pi5/share/onboard_autonomy/
    ardupilot-board-types.txt
```

Реальний Pixhawk повернув ID `56`, а runtime mapper показав:

```text
PX4 [BL] FMU v6C.x / ID 56 / SILICON 0
```

Package SHA-256:

```text
546eb24d627520721350b5fd6059efa53b45e8a4665982137de2d9cc5320beb6
```

Офіційне джерело:

- https://github.com/ArduPilot/ardupilot/blob/master/Tools/AP_Bootloader/board_types.txt
