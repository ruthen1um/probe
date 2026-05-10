# Media Finder
Программа для поиска мультимедийных файлов в домашнем каталоге пользователя, использующая libmagic
для распознавания mime-type на основе сигнатур файлов. Программа поддерживает аргументы командной
строки:
```
Usage: media-finder {OPTIONS} [directory]

Utility to scan specified directory for media files

OPTIONS:

  -h, --help                            display this help menu
  -d, --database                        specify database path
  -i, --interval                        specify scan interval (in seconds)
  directory                             specify scan directory
```

# Требования
* Linux
* C++23
* Conan

# Сборка
Установите зависимости (при необходимости используйте выбранный
[профиль](https://docs.conan.io/2/reference/config_files/profiles.html) Conan):
```
$ conan install --build=missing .
```

Обновите окружение текущей оболочки для корректной сборки в зависимости от выбранного типа сборки:
(`Debug` или `Release` и т.д):
```
$ source build/<target>/generators/conanbuild.sh
```

Инициализируйте директорию сборки в соответствии с пресетом, созданным Conan (например, `conan-release`):
```
$ cmake --preset=<preset>
```

Соберите проект:
```
$ cmake --build --preset=<preset>
```

> [!IMPORTANT]
> Для корректной работы программы необходимо также обновить окружение, выполнив скрипт `conanrun.sh`:
> ```
> $ source build/<target>/generators/conanrun.sh
> ```

# TODO
* Добавить тесты
* Добавить рекурсивное сканирование
* Добавить возможность получения результатов сканирования через HTTP GET-запрос
* Провести рефакторинг кода
* Обработать ситуации, такие как перемещение файла прямо во время сканирования
