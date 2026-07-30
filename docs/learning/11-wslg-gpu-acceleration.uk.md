# WSLg GPU-прискорення для Gazebo

## Симптом

Gazebo відкривав 3D-сцену, але інтерфейс і камера помітно гальмували. Сам факт,
що вікно працює у WSLg, ще не доводить використання відеокарти.

Перевірка:

```bash
glxinfo -B
```

До виправлення вона показувала:

```text
Device: llvmpipe (LLVM 20.1.2, 256 bits)
Accelerated: no
```

`llvmpipe` є програмним OpenGL-рендерером Mesa. Він виконує графічні операції
на CPU, тому важка Ogre2-сцена Gazebo працює повільно.

## Що перевірили

- WSL бачить `/dev/dxg`, тобто Windows передає Linux доступ до GPU.
- У Windows встановлена NVIDIA GeForce RTX 5070.
- Mesa має драйвер `d3d12_dri.so`.
- Змінні середовища не примушували використовувати software rendering.

Пробний запуск із явним вибором D3D12:

```bash
MESA_D3D12_DEFAULT_ADAPTER_NAME=NVIDIA \
GALLIUM_DRIVER=d3d12 \
glxinfo -B
```

Результат:

```text
Device: D3D12 (NVIDIA GeForce RTX 5070)
Accelerated: yes
OpenGL core profile version: 4.6
```

Отже, Windows-драйвер і WSL GPU bridge справні. Помилковим був лише
автоматичний вибір Mesa-рендерера.

## Зміна в лаунчері

У `scripts/run_gazebo_iris.sh` додано:

```bash
if [[ -e /dev/dxg ]]; then
    export GALLIUM_DRIVER="${GALLIUM_DRIVER:-d3d12}"
    export MESA_D3D12_DEFAULT_ADAPTER_NAME="${MESA_D3D12_DEFAULT_ADAPTER_NAME:-NVIDIA}"
fi
```

Перевірка `/dev/dxg` важлива: на звичайному Ubuntu без WSL скрипт не повинен
примусово вибирати D3D12. Синтаксис `${NAME:-default}` означає: використати
значення змінної `NAME`, якщо воно задане, інакше взяти `default`. Тому
налаштування можна перевизначити без редагування скрипту.

## Роль компонентів

```text
Gazebo / Ogre2
      |
    OpenGL
      |
Mesa Gallium D3D12
      |
  WSL /dev/dxg
      |
Windows NVIDIA driver
      |
GeForce RTX 5070
```

WSL не отримує Linux-драйвер NVIDIA для GUI напряму. Mesa перекладає OpenGL
в D3D12, а WSLg передає його Windows-драйверу.

## Жива перевірка

Після перезапуску середовище процесу `gz sim gui` містить обидві змінні, а в
його memory map присутні:

```text
/usr/lib/wsl/lib/libd3d12.so
/usr/lib/wsl/lib/libd3d12core.so
/usr/lib/wsl/lib/libdxcore.so
```

Gazebo також повідомив:

```text
real_time_factor: 0.99765252361194112
step_size: 0.001 s
```

`real_time_factor` близький до `1.0` означає, що physics simulation встигає
працювати в реальному часі. Це окрема метрика від FPS інтерфейсу: GPU
прискорює рендеринг, тоді як physics переважно виконується на CPU.
