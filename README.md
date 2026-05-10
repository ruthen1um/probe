# Media Finder
Программа для поиска мультимедийных файлов в домашнем каталоге пользователя

# Требования
* Linux
* C++23
* Conan

# Сборка
Установите зависимости (при необходимости используйте выбранный
[профиль](https://docs.conan.io/2/reference/config_files/profiles.html) Conan):
```shell
$ conan install --build=missing .
```

Обновите окружение текущей оболочки для корректной сборки в зависимости от выбранного типа сборки:
(`Debug` или `Release` и т.д):
```shell
$ source build/<target>/generators/conanbuild.sh
```

Инициализируйте директорию сборки в соответствии с пресетом, созданным Conan (например, `conan-release`):
```shell
$ cmake --preset=<preset>
```

Соберите проект:
```shell
$ cmake --build --preset=<preset>
```

> [!IMPORTANT]
> Для корректной работы программы необходимо также обновить окружение, выполнив скрипт `conanrun.sh`:
> ```shell
> $ source build/<target>/generators/conanrun.sh
> ```
