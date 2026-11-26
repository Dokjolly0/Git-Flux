# Git Flux — Linux Developer Utility Scripts

This directory contains utility scripts for building, testing, launching, and maintaining **Git Flux** on Linux
operating systems.

All scripts are self-contained, support both short and long options (`--help`), and adhere strictly to the project
architecture (Zero-Qt Core and Desktop Qt Quick 6).

---

## Script Index

| Script                 | Description                                                                                                         | Quick Usage                |
|:-----------------------|:--------------------------------------------------------------------------------------------------------------------|:---------------------------|
| [`build.sh`](build.sh) | Builds CMake presets (Desktop or Zero-Qt Core), with support for specific targets and clean builds.                 | `./scripts/linux/build.sh` |
| [`test.sh`](test.sh)   | Executes CTest test suites (unit, MVVM, integration) and headless smoke test.                                       | `./scripts/linux/test.sh`  |
| [`run.sh`](run.sh)     | Launches the Git Flux desktop application, optionally with auto-rebuild, GDB, or Valgrind.                          | `./scripts/linux/run.sh`   |
| [`clean.sh`](clean.sh) | Selectively or globally cleans build artifact directories (`build/qt-linux`, `build/core-linux`).                   | `./scripts/linux/clean.sh` |
| [`check.sh`](check.sh) | Complete local health check / pre-commit verification pipeline (build and test both Core and Desktop + smoke test). | `./scripts/linux/check.sh` |

---

## 1. `build.sh` — Compilation

Builds the project using CMake Presets.

```bash
# Standard build of the desktop application (linux-qt-release)
./scripts/linux/build.sh

# Build only the Zero-Qt Core (linux-core-release)
./scripts/linux/build.sh --core

# Build both presets in sequence
./scripts/linux/build.sh --all

# Clean build forcing removal of cache and build directory
./scripts/linux/build.sh --clean --qt

# Build a single specific target
./scripts/linux/build.sh -t git_flux
./scripts/linux/build.sh -t gf_viewmodels_tests
./scripts/linux/build.sh -t gf_unit_tests
```

### Supported Options:

- `-q, --qt`: `linux-qt-release` preset (default).
- `-c, --core`: `linux-core-release` preset (Zero-Qt).
- `-a, --all`: builds both Core and Desktop.
- `-C, --clean`: deletes build directory before building.
- `-g, --configure`: forces `cmake --preset` reconfiguration.
- `-t, --target <name>`: builds a specific target.
- `-j, --jobs <num>`: number of parallel build threads (default: `nproc`).
- `-v, --verbose`: verbose compiler output.

---

## 2. `test.sh` — Test Runner

Runs CTest test suites and integration tests with native Git.

```bash
# Run all tests in the Desktop preset (unit, viewmodels, integration)
./scripts/linux/test.sh

# Run Zero-Qt Core tests
./scripts/linux/test.sh --core

# Run the complete test suite across both presets
./scripts/linux/test.sh --all

# Filter by test suite or test name (regex)
./scripts/linux/test.sh -f viewmodels
./scripts/linux/test.sh -f watcher
./scripts/linux/test.sh -f batch

# Run only the headless startup smoke test
./scripts/linux/test.sh --smoke
```

---

## 3. `run.sh` — Application Launcher

Launches the `git_flux` desktop executable. If the binary is missing, it is built automatically.

```bash
# Standard graphical user interface launch
./scripts/linux/run.sh

# Rebuild if needed, then launch
./scripts/linux/run.sh --build

# Run headless startup smoke test without showing UI window
./scripts/linux/run.sh --smoke

# Launch application inside GDB for debugging sessions
./scripts/linux/run.sh --gdb

# Pass custom arguments to the application
./scripts/linux/run.sh -- --help
```

---

## 4. `clean.sh` — Build Cleanup

Cleans compiled artifact directories.

```bash
# Remove all generated build folders
./scripts/linux/clean.sh

# Remove only the desktop build
./scripts/linux/clean.sh --qt

# Remove only the core build
./scripts/linux/clean.sh --core

# Show paths that would be removed without deleting them
./scripts/linux/clean.sh --dry-run
```

---

## 5. `check.sh` — Full Health Check (CI / Pre-commit)

Performs a full repository health verification:

1. Build `linux-core-release`
2. Run CTest on `linux-core-release`
3. Build `linux-qt-release`
4. Run CTest on `linux-qt-release`
5. Headless Smoke Test `./build/qt-linux/git_flux --test-startup`

```bash
./scripts/linux/check.sh
```

Upon completion, prints a summary table showing duration and pass/fail status for each check.
