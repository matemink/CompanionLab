# Перше завантаження Raspberry Pi 5

## Мета ітерації

Перенести OnboardAutonomy із симуляційного x86-64 середовища на реальний
ARM64 Linux і перевірити deployment до підключення Pixhawk та камери.

На цьому етапі жодні команди польоту не надсилаються.

## Встановлена система

Через Raspberry Pi Imager записано офіційний Raspberry Pi OS Lite
(64-bit) на microSD-карту 64 GB:

```text
Board:        Raspberry Pi 5 Model B
Architecture: aarch64
OS:           Debian GNU/Linux 13 (trixie)
Hostname:     companionpi
User:         <pi-user>
Remote shell: SSH with public-key authentication
```

Raspberry Pi Connect залишено вимкненим. ПК і Raspberry Pi працюють у
тій самій локальній мережі, тому окремого SSH gateway немає.

## Що знаходиться на SD-карті

Після запису образу карта має щонайменше два розділи:

```text
bootfs  FAT32  -> firmware, kernel and first-boot configuration
rootfs  ext4   -> Raspberry Pi OS and user files
```

Windows читає `bootfs`, але без додаткових драйверів не читає Linux
`rootfs`. Тому службову конфігурацію першого запуску можна змінити без
редагування Linux filesystem.

Imager записав у `bootfs/user-data`:

- hostname `companionpi`;
- обраного в Imager користувача;
- наш публічний ED25519 key;
- заборону password authentication;
- команду ввімкнення SSH.

Паролі, password hashes і Wi-Fi credentials у документацію не
копіюються.

## Чому знадобився файл `ssh`

Після першого запуску Raspberry Pi відповідав у мережі, але TCP port 22
не був відкритий. На FAT32 boot partition створено порожній marker:

```text
ssh
```

Raspberry Pi OS підтримує цей marker як headless-механізм ввімкнення
вже встановленого SSH server. Ми не копіювали SSH binary у Linux і не
редагували `rootfs`.

Для безпечної операції додано:

```text
tools/provisioning/Enable-SshOnSdCard.ps1
```

Скрипт перед записом перевіряє disk number, USB bus, serial number,
розмір карти та розмір boot partition. Лише після цього він монтує
bootfs як `R:` і створює нульовий файл `R:\ssh`.

## Помилка з username

Після ввімкнення SSH server відповідав, але відхиляв наш key. Причиною
був не ключ: перша спроба використовувала припущене ім'я
`companion`, тоді як Imager створив іншого користувача.

Ми прочитали `bootfs/user-data` і окремо звірили публічні частини
ключів:

```text
User:             <pi-user>
PublicKeyMatches: true
SshConfigured:    true
```

Правильний connection contract:

```bash
ssh <pi-user>@companionpi.local
```

Private key залишається на Windows PC. Raspberry Pi зберігає лише
public key, з якого неможливо відновити private key.

## Deployment OnboardAutonomy

Готовий cross-compiled ARM64 package:

```text
artifacts/onboard_autonomy-pi5-arm64.tar.gz
```

Локальний і віддалений SHA-256 збіглися:

```text
472faf53cfb4499820b076ef152d59dd1223b915313b8594e1209055c6fbf95b
```

Архів розгорнуто в:

```text
$HOME/onboard_autonomy-pi5/
```

Це не native build на Raspberry Pi. Binary зібрано cross-compiler під
ARM64 у WSL, скопійовано через SCP і запущено на цільовій машині.

## Результат першого hardware diagnostic

```text
[PASS] 64-bit ARM operating system (aarch64)
[PASS] Current user belongs to the dialout group
[PASS] OnboardAutonomy binary is ARM64
[PASS] OnboardAutonomy runtime libraries are available
[WARN] No USB serial controller detected
[WARN] Camera Module 3 was not detected
```

Чотири основні deployment checks пройдено. Два warning очікувані:
Pixhawk 6C і Camera Module 3 ще фізично не підключені.

Diagnostics є read-only щодо flight controller. Вони не ARM-лять
апарат і не надсилають TAKEOFF, LAND або route commands.

## Наступний крок

1. На вимкненому Raspberry Pi підключити Camera Module 3.
2. Підключити Pixhawk 6C через USB без пропелерів.
3. Повторно запустити `diagnose_pi_hardware.sh`.
4. Запустити safe serial launcher і отримати реальний MAVLink
   `HEARTBEAT`.
5. Зберегти перший hardware JSONL telemetry log.
