# Відновлення POSIX serial-з'єднання

## Чому `read() == 0` недостатньо

Serial налаштований як non-blocking, тому нуль байтів зазвичай означає лише
"зараз немає нового MAVLink кадру". Це не те саме, що висмикнутий USB. Перед
читанням adapter викликає `poll(..., timeout=0)` і окремо бачить `POLLIN`,
`POLLHUP`, `POLLERR` або `POLLNVAL`.

## Життєвий цикл file descriptor

`PosixSerialTransport` володіє одним Linux file descriptor. Після hangup або
невідновної I/O-помилки він:

1. закриває fd;
2. повертає `0`, не валячи application loop;
3. чекає reconnect interval;
4. повторно викликає `open()` і налаштовує raw `termios`;
5. продовжує роботу тим самим C++ object.

Backoff потрібен, щоб loop з періодом 5 ms не робив сотні `open()` за секунду,
поки кабель відсутній.

## Чому потрібен `/dev/serial/by-id`

Ім'я `/dev/ttyACM0` описує порядок появи пристрою і після reconnect може стати
`/dev/ttyACM1`. Symlink у `/dev/serial/by-id` описує конкретний USB-пристрій і
знову вказує на актуальний tty після його появи. Adapter навмисно повторює
саме переданий stable path, а не вгадує інший контролер.

## Як це тестується без Pixhawk

Pseudo-terminal є парою справжніх Linux tty endpoints. Тест закриває першу
master-сторону, створює другу пару і переносить stable symlink на неї. Це
перевіряє `poll`, `close`, `open`, `termios` і передачу байтів через kernel,
а не лише поведінку mock-класу.

Після повернення HEARTBEAT наявний `TelemetryStreamConfigurator` скидає стару
сесію і знову запитує всі шість MAVLink потоків. Тому reconnect відновлює не
тільки файл пристрою, а й потрібний набір телеметрії.
