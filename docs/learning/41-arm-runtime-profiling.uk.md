# Профілювання onboard runtime на ARM

## Що саме вимірюємо

Один PID C++-процесу не описує реальне навантаження. Camera adapter запускає
`rpicam`, launcher має Python JSONL sink, а preview обслуговує HTTP. Тому
`profile_onboard_autonomy_pi.sh` запускає весь runtime в окремій Linux process
group через `setsid` і агрегує всі PID цієї групи.

## CPU

Linux зберігає user і kernel CPU time процесу в `/proc/<pid>/stat` як ticks.
Profiler пам'ятає попереднє значення для кожного PID і накопичує лише приріст.
Це важливо, бо дочірня камера може перезапуститися і отримати новий PID.

`100% CPU` у звіті означає одне повністю зайняте ядро. На чотириядерному Pi
процес-група теоретично може показати до `400%`; це звичне Linux-подання, а не
помилка нормалізації.

## Пам'ять і температура

RSS з `/proc/<pid>/status` показує resident memory кожного живого процесу.
Profiler сумує RSS групи, тому число включає runtime, camera process і log
sink, але shared pages можуть бути пораховані більше одного разу. Це стабільна
операційна метрика, не точний підрахунок унікальних фізичних сторінок.

Температура читається з thermal sysfs, а `vcgencmd get_throttled` показує
undervoltage, frequency capping і thermal throttling. Швидкий алгоритм із
throttling не є чесною оптимізацією, тому такий run не проходить acceptance.

## Чому поки немає оптимізації

Cross-build доводить сумісність ARM64, але не дає продуктивність Cortex-A76,
камери та пам'яті Raspberry Pi 5. Спочатку потрібен відтворюваний 60-секундний
baseline на реальному target, потім профіль конкретного bottleneck, зміна коду
і повторне вимірювання за тих самих умов.
