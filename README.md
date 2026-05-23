# HSE Game

This is an academic C/C++ course project. We will come up with a project name later.

## Build

Dependencies are vendored via CMake's FetchContent; you only need a C++ compiler and CMake (3.26+ recommended). From the repo root:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
./build/bin/game
```

For Release:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
./build/bin/game
```

Notes:
- IDEs (e.g., CLion) may have created `cmake-build-debug` or `cmake-build-local`; you can reuse them or make your own `build/` dir as above.
- For a clean rebuild, remove only generated directories:

```sh
rm -rf build cmake-build-*
```

## Assets

Каждая сущность хранит свои атласы в отдельной директории внутри `assets/`,
например:

```
assets/
	player/
		run.png
		idle.png
```

В коде `Player` ожидает путь к run-атласу и (опционально) к idle-атласу.

## License

This project is licensed under the GNU General Public License v3.0.
