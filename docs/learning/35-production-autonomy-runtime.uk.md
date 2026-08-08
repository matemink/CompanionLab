# Production autonomy runtime

## Чому прибрали ScenarioRunner

`ScenarioRunner` виконував наперед заданий маршрут: переключити режим,
озброїтися, злетіти, пройти точки та сісти. Це корисно як рання демонстрація
MAVLink, але не є автономією. Такі місії вже вміє виконувати ArduPilot.

Компаніон-комп'ютер має постійно читати актуальний стан світу і вирішувати,
що робити зараз. Тому numbered scenarios видалені з production C++ коду.
Python лишився зовнішнім test harness: він запускає SITL, створює умови й
перевіряє результат, але не підміняє логіку польоту.

## Новий потік

```text
VehicleState + fresh AprilTag position
                 |
                 v
             WorldState
                 |
                 v
           DecisionEngine
                 |
                 v
            DesiredMotion
                 |
                 v
         SafetySupervisor
                 |
                 v
          AutonomyRuntime
                 |
                 v
        MAVLink LANDING_TARGET
```

`FlightStartupController` існує окремо. Він чекає готовності, переводить
ArduCopter у GUIDED, озброює і підтверджує зліт до 8 м. Після цього керування
передається довгоживучому `AutonomyRuntime`.

## Межі безпеки

- `DesiredMotion` живе 250 мс. Старе рішення не можна випадково відправити
  пізніше як актуальне.
- `SafetySupervisor` окремо перевіряє link, armed state, lifetime і числа
  target position.
- Схвалений `LANDING_TARGET` надсилається з частотою 5 Hz, тобто кожні
  200 мс.
- Для переходу в LAND потрібна одна секунда безперервно свіжої цілі.
- Втрата цілі одразу зупиняє vision setpoints і скидає warmup.
- Якщо цілі немає п'ять секунд до початку LAND, runtime просить звичайний
  ArduPilot LAND замість нескінченного зависання.
- Завершення визначає не ACK, а фактичний telemetry state `DISARMED`.

## Цікаві конструкції C++20

`std::optional<T>` виражає чесну відсутність значення. Якщо камера не бачить
мітку, `DecisionEngine` повертає `std::nullopt`, а не координати з нулями.

`enum class` створює типізовані фази та дії. Значення
`FlightStartupPhase::arming` не можна випадково змішати з MAVLink result або
іншим integer enum.

`std::chrono::steady_clock` використовується для TTL, retry та timeout. Його
час монотонний, тому зміна системного годинника не ламає safety deadline.

Компоненти з'єднані композицією у `CompanionApplication`, але кожен має одну
відповідальність і тестується без Gazebo, Pixhawk або камери.
