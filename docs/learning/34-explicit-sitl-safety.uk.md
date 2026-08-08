# Explicit SITL safety mode

## Проблема

Попереднє правило фактично було таким:

```text
UDP -> SITL -> motion allowed
serial -> hardware -> motion blocked
```

Але реальний Pixhawk також може бути доступний через MAVLink router,
Wi-Fi або Ethernet bridge. Transport описує спосіб передавання байтів,
а не те, чи перед нами симулятор.

## Нова модель

```cpp
enum class RuntimeEnvironment {
    hardware_or_unknown,
    sitl,
};

enum class MavlinkTransport {
    udp,
    serial,
};
```

`evaluate_motion_safety()` отримує environment, transport і факт запиту
рухових команд. Команди дозволені тільки для explicit `sitl + udp`.
Default environment є `hardware_or_unknown`, тому звичайний UDP startup
залишається observation-only.

`--sitl` не детектує симулятор автоматично. Це явне твердження оператора,
яке додає тільки project SITL launcher. Такий opt-in помітний у команді
запуску та не виникає випадково через вибір transport adapter.

## Перевірені комбінації

| Environment | Transport | Motion | Результат |
|---|---|---:|---|
| unknown/real | UDP | no | observation-only |
| unknown/real | UDP | yes | rejected |
| SITL | UDP | yes | allowed |
| unknown/real | serial | no | observation-only |
| SITL | serial | yes | invalid configuration |
