# Halo Local Search

Halo is a Linux desktop application for finding source code and text files on your machine. It crawls a selected root directory, builds a persistent SQLite index, and keeps that index current as files change. The interface is intentionally small; most of the work happens in the crawler, indexing pipeline, database, and query engine.

For an overview of the internals, see [How the project works](docs/architecture.md).

## Requirements

- Linux (the file watcher uses Linux `inotify`)
- A C++20 compiler
- CMake 3.20 or newer
- Git
- OpenGL development libraries and GLFW platform dependencies (Wayland or X11)
- vcpkg (the project manifest installs SQLite, GLFW, Dear ImGui, and GoogleTest)

On Arch Linux, the repository includes `bootstrap.sh` to install GLFW's Wayland build prerequisites, bootstrap a local vcpkg checkout, and configure/build the project. It uses `pacman` and `sudo`, so on other distributions install the equivalent system development packages and use the manual steps below.

## Build and install from source

Run these commands from the project directory. The vcpkg checkout is local to the repository and is ignored by Git.

```bash
git clone https://github.com/microsoft/vcpkg.git
./vcpkg/bootstrap-vcpkg.sh
cmake -S . -B build \
  -DCMAKE_TOOLCHAIN_FILE="$PWD/vcpkg/scripts/buildsystems/vcpkg.cmake"
cmake --build build --target search_engine
```

The first configure downloads and builds the manifest dependencies, which can take a while. Start the app with:

```bash
./build/search_engine
```

To install the executable for your user:

```bash
install -Dm755 build/search_engine "$HOME/.local/bin/search_engine"
```

Then start it with `search_engine` (make sure `$HOME/.local/bin` is on your `PATH`).

The app scans the home directory by default. Its SQLite database is named `search_engine.db` and is created in the process's current working directory. Logs are written to `$XDG_STATE_HOME/halo/halo.log`, or to `~/.local/state/halo/halo.log` when `XDG_STATE_HOME` is unset.

### Arch Linux bootstrap helper

```bash
./bootstrap.sh
```

The script configures and builds the full project in `build/`. On subsequent runs, rebuild with `cmake --build build`.

## Tests

The default CMake configuration enables tests and requires GoogleTest through the vcpkg manifest. Build and run them with:

```bash
cmake --build build --target search_engine_tests
ctest --test-dir build --output-on-failure
```

To configure without the test target, add `-DBUILD_TESTING=OFF` to the CMake configure command.

## Project layout

- `src/crawler/` and `include/crawler/`: recursive filesystem discovery and file filters
- `src/indexing/` and `include/indexing/`: tokenization, worker pool, and indexing pipeline
- `src/database/` and `include/database/`: SQLite schema, filesystem catalog, and search queries
- `src/search/` and `include/search/`: query normalization and search API
- `src/watcher/` and `include/watcher/`: live filesystem change handling
- `src/engine/` and `include/engine/`: application lifecycle, configuration, and logging
- `src/ui/` and `include/ui/`: desktop interface
- `tests/`: GoogleTest coverage for the engine components
