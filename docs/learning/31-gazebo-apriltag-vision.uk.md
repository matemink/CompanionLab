# Gazebo AprilTag vision pipeline

> Status: this chapter records the camera-ingestion milestone before flight
> guidance was connected. The completed vision-to-`LANDING_TARGET` path is
> documented in `32-vision-guided-precision-landing.uk.md`.

## Що додано

`simulation/models/iris_with_landing_camera` розширює офіційний Iris
ArduPilot: під корпусом є фіксована камера, спрямована вниз. Вона не є новою
реалізацією фізики дрона, тому польотна модель і зв'язок із SITL лишаються
офіційними.

`simulation/models/apriltag_landing_pad` містить статичний майданчик із
`tagStandard41h12`, а `simulation/worlds/apriltag_landing.sdf` складає з
нього, Iris, землі та світла один відтворюваний світ.

`GStreamerCameraSource` є Linux-адаптером application-порту `CameraSource`.
Він запускає дочірній `gst-launch-1.0`, приймає RTP/H.264 на UDP `5601`,
декодує кадри до I420 і віддає застосунку лише найсвіжіший кадр. Якщо vision
loop відстає, старі кадри не накопичуються, бо для керування важливіша свіжа
позиція цілі, а не обробка історії.

## Потік даних

```text
Gazebo camera
    -> RTP/H.264 UDP 5601
    -> GStreamerCameraSource
    -> I420 CameraFrame
    -> AprilTagTargetDetector
    -> metric TargetObservation
    -> TargetTracker
    -> JSON, TUI and browser preview
```

Фізична Camera Module 3 входить у цей ланцюг через інший адаптер,
`RpicamCameraSource`, але починаючи з `CameraFrame` бізнес-логіка однакова.

## Що вже доведено

- SDF моделі й світ проходять `gz sdf -k`.
- Gazebo створює `image`, `camera_info` та `enable_streaming` topics.
- GStreamer декодує фактичні кадри `640x480` з UDP `5601`.
- C++ runtime переходить у `camera.phase=streaming` і обробляє ці кадри.
- Python-тести звіряють геометрію камери, аналітичне калібрування і кожну
  клітинку текстури мітки з pinned source asset.

## Що це ще не доводить

Поки дрон стоїть на мітці, нижня камера занадто близько, щоб бачити всі
чотири кути. Наступна інтеграційна перевірка виконується після зльоту. Після
цього потрібні camera-to-body-FRD transform і заміна synthetic
`LANDING_TARGET` у precision-сценарії на свіжий confirmed track.
