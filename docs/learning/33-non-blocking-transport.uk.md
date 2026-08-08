# Non-blocking transport contract

## Що було не так

Serial transport уже повертав доступні байти або `0`, а UDP
`recvfrom()` міг чекати до 250 мс. Через це одна реалізація спільного
`Transport` port мала іншу часову поведінку й могла затримати heartbeat,
camera polling та flight logic.

## Новий контракт

```text
bytes available     -> return them immediately
no bytes available  -> return 0 immediately
```

`Transport::read()` не знає, де починається або закінчується MAVLink
frame. Один read може повернути половину frame, кілька frames або цілий
burst. Parser state належить `MavlinkDecoder`, тому framing не залежить
від UDP datagram чи serial chunk boundaries.

## Реалізація на рівні ОС

Linux UDP socket отримує прапорець `O_NONBLOCK` через `fcntl()`. У
Windows еквівалентом є `ioctlsocket(..., FIONBIO, ...)`. Для serial на
Linux уже використовуються `VMIN=0` і `VTIME=0`.

```cpp
const int flags = fcntl(socket_handle, F_GETFL, 0);
fcntl(socket_handle, F_SETFL, flags | O_NONBLOCK);
```

Оператор `|` додає один bit flag, не стираючи попередні socket flags.
Коли даних немає, POSIX повертає `EAGAIN`/`EWOULDBLOCK`, а Windows
`WSAEWOULDBLOCK`; adapter мапить ці стани в звичайний результат `0`.

## Хто тепер володіє часом

Production `CompanionApplication::poll()` захоплює `steady_clock`
після non-blocking read і використовує цей timestamp для freshness та
deadline checks. Окремий overload з явним `TimePoint` залишений для
детермінованих unit tests.

Main loop робить паузу 5 мс після кожного проходу. Це не transport
timeout: application продовжує регулярно працювати, але не забирає
ціле CPU core, коли UDP, serial і keyboard одночасно мовчать.

## Регресійні перевірки

- десять читань тихого реального UDP socket не успадковують старий
  250-мс timeout;
- partial MAVLink frame зберігається між двома ingest calls;
- кілька frames в одному read декодуються всі;
- burst із 32 frames не губить повідомлення;
- heartbeat scheduler продовжує працювати, коли transport повертає `0`.
