# Відновлення camera source

## Де має жити reconnect

`CompanionApplication` працює з `CameraSource` і не повинна знати, що кадри
створює `rpicam-vid` або `gst-launch-1.0`. Тому reconnect реалізовано всередині
Linux adapters, поруч із `fork`, pipe та дочірнім process lifecycle.

## Як визначається відмова

Завершення process або помилка pipe очевидні. Складніший випадок: process живий,
але нових байтів немає. `poll()` прокидається кожні 100 ms, а adapter вимірює
час від останнього прогресу читання. Після 2000 ms незавершений кадр відкидається,
child process зупиняється, і source переходить у `reconnecting`.

Retry має паузу 500 ms, тому відсутня busy loop. Лічильник рестартів і остання
помилка проходять через `CameraSourceStatus` у JSON та console. Перший повний
YUV420 frame очищає помилку і повертає `streaming`.

## Чому sequence ставиться після read

Sequence описує реальний завершений кадр, а не спробу його прочитати. Якщо
збільшити sequence до `read_exact()`, кожен timeout після відновлення виглядатиме
як втрачений кадр. Тому номер присвоюється тільки після повного frame buffer.

Integration harness доводить весь цикл на справжніх Gazebo RTP/H.264 і
GStreamer: 11 кадрів, outage, видимий reconnect, 22 кадри тим самим C++ process.
