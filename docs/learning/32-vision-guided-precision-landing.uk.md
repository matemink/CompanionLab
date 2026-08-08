# Vision-guided precision landing

## Що змінилося

Раніше сценарій 5 обчислював вектор до home з `LOCAL_POSITION_NED` і видавав
його за синтетичний `LANDING_TARGET`. Це перевіряло MAVLink, але не computer
vision. Тепер джерелом координат є лише підтверджений AprilTag track.

```text
camera optical pose
  -> camera extrinsics
  -> body FRD pose
  -> ScenarioRunner
  -> MAVLink LANDING_TARGET
```

## Два різні калібрування

`gazebo-landing-camera-640x480.json` описує intrinsics: як пікселі
перетворюються на напрямок і метричну позу. `gazebo-landing-camera-extrinsics.json`
описує extrinsics: де камера стоїть і як повернута відносно корпуса.

Для нижньої Gazebo-камери:

```text
body forward = -camera down
body right   =  camera right
body down    =  camera forward + 0.16 m
```

Матриця перевіряється як справжнє обертання: одиничні взаємно перпендикулярні
рядки та determinant `+1`. Невалідний конфіг не запускає guidance.

## Safety state

Одна детекція не запускає посадку. Потрібні три послідовні валідні
observations для track lock і ще одна секунда безперервно свіжої цілі перед
LAND. Observation старше 250 ms не надсилається. Втрата до LAND обнуляє
warmup; повторне захоплення починає його заново.

## Результат SITL

ArduCopter отримав 72 валідні `LANDING_TARGET` у `MAV_FRAME_BODY_FRD`, прийняв
LAND і автоматично disarm-нувся. Фінальна горизонтальна похибка від центра
мітки склала 0.456 m. Повний доказ і обмеження записані в
`docs/evidence/precision-landing-sitl.md`.
