# Калібрування Camera Module 3 Wide

## Навіщо це потрібно

AprilTag detector уже повертає координати центра й кутів мітки у
пікселях. За самими пікселями не можна достовірно сказати, чи мітка
перебуває за 40 сантиметрів, чи за два метри.

Калібрування визначає дві групи параметрів:

- intrinsics `fx`, `fy`, `cx`, `cy`, які описують проєкцію 3D-точок у
  пікселі конкретного режиму камери;
- distortion `k1`, `k2`, `p1`, `p2`, `k3`, яка описує радіальне й
  тангенціальне викривлення лінзи.

Це характеристики не просто моделі IMX708. Вони залежать від
конкретної лінзи, роздільності, crop/scaler mode і фокусування. Тому в
репозиторії немає вигаданого готового JSON для Camera Module 3 Wide.

## Потік даних

```text
A4 checkerboard 9x6 / 25 mm
  -> rpicam-vid MJPEG, 24 ракурси 640x480
  -> findChessboardCornersSB
  -> calibrateCamera
  -> reprojection quality gate
  -> calibration.json
```

OpenCV називає `9x6` кількістю **внутрішніх кутів**, а не кількістю
квадратів. Наша мішень має 10x7 квадратів і біле поле навколо, яке
потрібне detector-у для стабільного знаходження зовнішніх квадратів.

## Нові файли

`assets/calibration/checkerboard-9x6-25mm-a4.svg`
: Векторна A4-мішень із фізичними одиницями `mm`. Під час друку треба
вибрати 100% scale і перевірити лінійкою контрольну лінію 100 mm.

`scripts/capture_camera_calibration.sh`
: На Raspberry Pi робить серію JPEG через офіційний `rpicam-vid`, тобто той
самий video pipeline, що й C++ runtime.
Між кадрами користувач змінює нахил і положення плоскої мішені.

Camera Module 3 має рухому autofocus-лінзу. Для калібрування і runtime ми
використовуємо однаковий режим `manual` з `lens-position=default`, який
відповідає штатній hyperfocal-позиції. Інакше continuous autofocus міг би
змінювати intrinsics уже після калібрування.

`python/calibrate_camera.py`
: Відкидає непридатні кадри, виконує calibration і записує строгий
JSON із параметрами, quality checks, OpenCV version та SHA-256 кожного
прийнятого зображення.

`python/tests/test_calibrate_camera.py`
: Створює математично відомі synthetic 3D/2D observations і перевіряє,
що calibration відновлює початкові intrinsics.

## Ключова функція Python

```python
rms_error, camera_matrix, distortion, rotations, translations = (
    cv.calibrateCamera(
        list(object_points),
        list(image_points),
        image_size,
        None,
        None,
    )
)
```

`object_points` містить відомі координати кутів мішені у метрах.
`image_points` містить ті самі кути, знайдені у пікселях кожного кадру.
OpenCV підбирає параметри камери, за яких проєкція 3D-точок найкраще
збігається зі спостереженими 2D-точками.

`rms_error_px` є помилкою зворотної проєкції. Ми також рахуємо її для
кожного кадру окремо, щоб один поганий ракурс не загубився у середньому.
Поточний gate: загальний RMS не більше 1.0 px, кожен ракурс не більше
1.5 px і щонайменше 10 повних ракурсів.

## Межа цієї ітерації

Цей код ще не визначає положення AprilTag і не надсилає MAVLink
`LANDING_TARGET`. Наступний крок завантажить підтверджений JSON у C++ і
перетворить pixel-space corners на 3D pose у системі координат камери.
