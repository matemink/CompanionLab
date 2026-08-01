# Невдалий експеримент із декоративним світом Gazebo

## Статус

**Не працює для користувача. Повністю відкочено.**

Після rollback OnboardAutonomy знову запускає офіційний
`ardupilot_gazebo/worlds/iris_runway.sdf`. У ньому залишилися лише
штатні `axes`, `runway` та `iris_with_gimbal`.

Цей документ не описує завершену фічу. Це postmortem невдалого
експерименту, щоб не повторювати ті самі зміни та діагностику.

## Що було додано

Було створено project-owned world:

```text
simulation/worlds/onboard_autonomy_airfield.sdf
```

До нього додавалися:

- текстурована трава;
- злітна смуга, розмітка та сині вогні;
- руліжна доріжка;
- вертолітний майданчик;
- окремий майданчик для майбутньої precision landing;
- ангар та операційна будівля;
- вітровказівник;
- дерева й пагорби;
- небо, сонце, тіні та власна стартова camera pose;
- прихований NavSat sensor;
- офіційна модель `iris_with_gimbal`;
- custom `<gui>` layout із `MinimalScene`, camera controls і
  `WorldControl`.

`scripts/run_gazebo_iris.sh` було змінено так, щоб цей world
запускався за замовчуванням. До `GZ_SIM_RESOURCE_PATH` додавався
project directory, а README і roadmap описували нову сцену.

## Що технічно працювало

Ці перевірки були успішними:

- `gz sdf -k` визнав SDF валідним;
- Gazebo server створив моделі `airfield_ground`, `flight_surfaces`,
  `precision_landing_pad`, `airfield_buildings`, `landscape`,
  `world_navsat` та `iris_with_gimbal`;
- ArduPilotPlugin, IMU, NavSat і camera plugin завантажилися;
- автоматичний політ виконав `GUIDED -> ARM -> TAKEOFF -> LAND`;
- максимальна висота у tlog становила 5.04 м;
- усі `COMMAND_ACK` мали success result;
- апарат пройшов `DISARMED -> ARMED -> DISARMED`;
- серверна gimbal-camera віддала реальний кадр `640x480`, у якому
  рендерилися частини Iris, земля, небо й будівля.

Це доводить лише те, що server-side simulation, physics і sensor
rendering працювали. Це не означає, що фіча була придатна для
користувача.

## Що не працювало

Головний Gazebo GUI не показував аеродром та дрон. Після відновлення
WSLg-вікна користувач бачив лише суцільний блакитний background.

Не допомогли:

- задана у SDF стартова camera pose;
- переміщення GUI-камери через `/gui/move_to/pose` прямо до Iris;
- запуск GUI через D3D12;
- запуск другого GUI через software renderer `llvmpipe`;
- restart Ubuntu WSL;
- повний restart Windows;
- повторний запуск чистого stack.

Під час діагностики WSLg також тимчасово створював OpenGL-вікна, які
були видимі лише як taskbar icons. Звичайний `xterm` при цьому
відображався. Після restart Windows саме вікно Gazebo знову стало
видимим, але custom scene усе одно лишилася блакитним фоном.

## Чого ми не довели

Root cause остаточно не локалізовано.

Server-side світ рендерився, але GUI viewport не показував scene
entities. Це могло бути спричинено custom `<gui>` layout, взаємодією
Gazebo GUI state з новим world або проблемою WSLg/OpenGL presentation.
Даних недостатньо, щоб чесно назвати одну з цих причин підтвердженою.

Тому неправильні висновки:

- що SDF повністю не завантажувався;
- що ArduPilot або MAVLink були зламані;
- що проблема точно була лише у WSLg;
- що експеримент можна вважати успішним через зелені server logs.

## Rollback

Видалено:

```text
simulation/worlds/onboard_autonomy_airfield.sdf
docs/learning/14-gazebo-airfield-world.uk.md
```

Повернено:

```bash
readonly world_file="${ONBOARD_AUTONOMY_GAZEBO_WORLD:-${gazebo_source_dir}/worlds/iris_runway.sdf}"
```

Також прибрано project world directory з
`GZ_SIM_RESOURCE_PATH` і видалено позитивні згадки про custom airfield
із README та roadmap.

Після rollback перевірено:

```text
world: iris_runway
models: axes, runway, iris_with_gimbal
Gazebo GUI clients: 1
```

## Як робити наступну спробу

Не створювати великий світ одним комітом.

Безпечна послідовність:

1. Скопіювати офіційний `iris_runway.sdf` без змін.
2. Переконатися, що користувач бачить той самий дрон і runway.
3. Додати один простий `box` без custom `<gui>`.
4. Перезапустити й отримати візуальне підтвердження від користувача.
5. Лише після цього додавати по одному: ground, building, texture,
   light.
6. Не змінювати camera layout і scene assets одночасно.
7. Після першого синього або порожнього viewport одразу відкотити
   останню маленьку зміну.

Acceptance criterion для будь-якого наступного кроку:

> Користувач реально бачить новий об'єкт у Gazebo GUI.

Server logs, SDF validation, camera frames і успішний flight test є
необхідними перевірками, але вони не замінюють цей критерій.
