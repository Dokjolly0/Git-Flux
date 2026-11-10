# Git Flux — Windows Developer Utility Scripts

This directory contains PowerShell utility scripts for building, testing, launching, maintaining, and verifying **Git
Flux** on Windows operating systems.

All scripts are self-contained, support standard PowerShell parameters and short aliases (`-h`, `-Help`), and adhere
strictly to the project architecture (Zero-Qt Core and Desktop Qt Quick 6).

---

## 📑 Script Index

| Script                   | Description                                                                                                                         | Quick Usage                   |
|:-------------------------|:------------------------------------------------------------------------------------------------------------------------------------|:------------------------------|
| [`build.ps1`](build.ps1) | Builds CMake presets (`qt-release` or `core-release`), with support for specific targets, clean builds, and parallel threads.       | `.\scripts\windows\build.ps1` |
| [`test.ps1`](test.ps1)   | Executes CTest test suites (unit, MVVM, integration) and headless smoke test.                                                       | `.\scripts\windows\test.ps1`  |
| [`run.ps1`](run.ps1)     | Launches the Git Flux desktop application, optionally with auto-rebuild, debug console logging, or Visual Studio / WinDbg debugger. | `.\scripts\windows\run.ps1`   |
| [`clean.ps1`](clean.ps1) | Selectively or globally cleans build artifact directories (`build/qt-msvc`, `build/core-msvc`, `build/packages`).                   | `.\scripts\windows\clean.ps1` |
| [`check.ps1`](check.ps1) | Complete local health check / pre-commit verification pipeline (build and test both Core and Desktop + smoke test).                 | `.\scripts\windows\check.ps1` |

---

## 🔨 1. `build.ps1` — Compilation

Builds the project using Windows CMake Presets (`build/qt-msvc` and `build/core-msvc`).

```powershell
# Standard build of the desktop application (qt-release)
.\scripts\windows\build.ps1

# Build only the Zero-Qt Core (core-release)
.\scripts\windows\build.ps1 -Core

# Build both presets in sequence
.\scripts\windows\build.ps1 -All

# Clean build forcing removal of cache and build directory
.\scripts\windows\build.ps1 -Clean -Qt

# Force CMake reconfiguration before build
.\scripts\windows\build.ps1 -Configure

# Build a single specific target
.\scripts\windows\build.ps1 -Target git_flux
.\scripts\windows\build.ps1 -Target gf_viewmodels_tests
.\scripts\windows\build.ps1 -Target gf_unit_tests
```

### Supported Options:

* `-q, -Qt`: `qt-release` preset [default].
* `-c, -Core`: `core-release` preset (Zero-Qt).
* `-a, -All`: builds both Core and Desktop in sequence.
* `-C, -Clean`: deletes build directory before building.
* `-g, -Configure`: forces `cmake --preset` reconfiguration.
* `-t, -Target <name>`: builds a specific target.
* `-j, -Jobs <num>`: number of parallel build threads (default: `$env:NUMBER_OF_PROCESSORS`).
* `-v, -Verbose`: verbose compiler output.
* `-h, -Help`: show help message.

---

## 🧪 2. `test.ps1` — Test Runner

Runs CTest test suites and integration tests on Windows.

```powershell
# Run all tests in the Desktop preset (unit, viewmodels, integration)
.\scripts\windows\test.ps1

# Run Zero-Qt Core tests
.\scripts\windows\test.ps1 -Core

# Run the complete test suite across both presets
.\scripts\windows\test.ps1 -All

# Filter by test suite or test name (regex)
.\scripts\windows\test.ps1 -Filter viewmodels
.\scripts\windows\test.ps1 -Filter watcher
.\scripts\windows\test.ps1 -Filter batch

# Run only the headless startup smoke test
.\scripts\windows\test.ps1 -Smoke
```

### Supported Options:

* `-q, -Qt`: run desktop tests (`qt-release`) [default].
* `-c, -Core`: run Zero-Qt core tests (`core-release`).
* `-a, -All`: run tests across both presets.
* `-f, -Filter <regex>`: include test filter (`ctest -R`).
* `-e, -Exclude <regex>`: exclude test filter (`ctest -E`).
* `-s, -Smoke`: run headless startup smoke test (`git_flux.exe --test-startup`).
* `-v, -Verbose`: detailed test logs.
* `-h, -Help`: show help message.

---

## 🚀 3. `run.ps1` — Application Launcher

Launches the `git_flux.exe` desktop executable. If the binary is missing, it is built automatically.

```powershell
# Standard graphical user interface launch (no CMD window)
.\scripts\windows\run.ps1

# Launch with live debug console logging attached
.\scripts\windows\run.ps1 -Debug

# Rebuild if needed, then launch
.\scripts\windows\run.ps1 -Build

# Run headless startup smoke test without showing UI window
.\scripts\windows\run.ps1 -Smoke

# Launch application under Visual Studio debugger (devenv.exe)
.\scripts\windows\run.ps1 -VisualStudio

# Pass custom arguments to the application
.\scripts\windows\run.ps1 -- --help
.\scripts\windows\run.ps1 -- --db "C:\data\gitflux.db"
```

### Supported Options:

* `-b, -Build`: automatically build `git_flux` target before launching.
* `-s, -Smoke`: run headless startup smoke test (`--test-startup`).
* `-d, -Debug`: enable live console logging window or terminal output (`--debug`).
* `-vs, -VisualStudio`: launch under Visual Studio debugger (`devenv.exe /debugexe`).
* `-w, -WinDbg`: launch under WinDbg if installed.
* `-h, -Help`: show help message.
* `[AppArgs]`: arbitrary arguments forwarded directly to `git_flux.exe`.

---

## 🧹 4. `clean.ps1` — Build Cleanup

Cleans compiled artifact directories.

```powershell
# Remove all generated build folders (build/*)
.\scripts\windows\clean.ps1

# Remove only the desktop build (build/qt-msvc)
.\scripts\windows\clean.ps1 -Qt

# Remove only the core build (build/core-msvc)
.\scripts\windows\clean.ps1 -Core

# Remove only the packaging artifacts (build/packages)
.\scripts\windows\clean.ps1 -Packages

# Show paths that would be removed without deleting them
.\scripts\windows\clean.ps1 -DryRun
```

### Supported Options:

* `-a, -All`: remove all generated build directories [default].
* `-q, -Qt`: remove only `build/qt-msvc`.
* `-c, -Core`: remove only `build/core-msvc`.
* `-p, -Packages`: remove only `build/packages`.
* `-d, -DryRun`: preview what would be removed.
* `-h, -Help`: show help message.

---

## ✅ 5. `check.ps1` — Full Health Check (Pre-Commit / CI Local Verification)

Performs a full repository health verification in sequence:

1. Build `core-release` (Zero-Qt)
2. Run CTest on `core-release`
3. Build `qt-release` (Qt Quick 6 Desktop)
4. Run CTest on `qt-release`
5. Headless Smoke Test (`git_flux.exe --test-startup`)

```powershell
.\scripts\windows\check.ps1
```

Upon completion, prints a summary table showing duration and pass/fail status for each check.
