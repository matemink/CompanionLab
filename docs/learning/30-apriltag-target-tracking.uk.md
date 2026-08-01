# Трекінг AprilTag між кадрами

## Навіщо він потрібен

AprilTag detector відповідає на питання: «що видно в цьому конкретному
кадрі?». Навіть для нерухомої мітки оцінені `right/down/forward` трохи
відрізняються між кадрами, а один кадр може загубитися через blur або
експозицію.

Перед guidance потрібна інша абстракція: «чи маємо ми підтверджену й
достатньо свіжу ціль?». Це робить `TargetTracker`.

```text
Camera frame
  -> AprilTag detector: raw observations
  -> TargetTracker: identity + acquisition + smoothing + timeout
  -> confirmed metric track
  -> future camera-to-body transform
  -> future MAVLink LANDING_TARGET
```

## Стани

- `searching`: валідної цілі немає або попередня протухла;
- `acquiring`: правильна мітка вже є, але ще немає трьох послідовних
  підтверджень;
- `tracking`: три або більше послідовних спостереження підтвердили lock.

Коротке зникнення не обнуляє lock миттєво. Tracker зберігає позицію,
але збільшує `observation_age_ms`. Після 500 мс стан стає `searching`, а
позиція видаляється, тому stale-значення не можна випадково використати.

## Згладжування

Для translation використовується exponential moving average:

```text
filtered = previous * (1 - alpha) + measurement * alpha
```

При `alpha = 0.35` новий кадр має вагу 35%, історія 65%. Це прибирає
дрібний jitter, але не створює великої затримки. Rotation тут навмисно
не усереднюється: звичайне середнє елементів матриці може перестати бути
коректним обертанням. Для цього потрібна окрема quaternion/SO(3) логіка.

## Файли

`include/onboard_autonomy/application/TargetTracker.hpp`
: публічний контракт, config, стани й snapshot.

`src/application/TargetTracker.cpp`
: відбір валідного pose, утримання ID, state machine, EMA і timeout.

`src/application/VisionMonitor.cpp`
: передає raw detections у tracker і підставляє згладжену translation у
поточне preview-спостереження.

`src/application/AppSnapshot.cpp`
: серіалізує track у JSON для логів та інтеграційних тестів.

`src/adapters/preview/HttpCameraPreviewServer.cpp`
: переносить той самий snapshot у browser preview, не реалізуючи tracker
повторно в JavaScript.

`tests/TargetTrackerTests.cpp`
: перевіряє acquisition, EMA, freshness, timeout, зміну tag ID і відмову
від unsafe observations.

## Цікавий C++20 синтаксис

`std::span<const TargetObservation>` передає read-only view на масив або
`std::vector` без копіювання й без прив'язки API до конкретного контейнера.

`std::optional<CameraFramePosition>` явно відрізняє «позиції немає» від
фальшивої позиції `0, 0, 0`.

`enum class TargetTrackPhase` не конвертується неявно в `int`, тому стани
не можна випадково змішати з іншими числовими значеннями.

`std::chrono::milliseconds` зберігає одиницю часу в типі. Timeout 500 мс
не можна непомітно переплутати з 500 секундами.

## Межа безпеки

Tracker нічого не надсилає в Pixhawk. Він лише формує надійніший read-only
стан. Підключення до `LANDING_TARGET` почнеться після фізичної перевірки
масштабу та camera-optical -> body-FRD трансформації.
