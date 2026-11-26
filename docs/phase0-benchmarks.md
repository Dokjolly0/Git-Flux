# Git Flux — Specifica Test & Benchmark di Fase 0

> **Stato:** in implementazione (Fase 0 — Validazione Architetturale)  
> **Framework scelto:** Catch2 v3 (Unit/Integration Test + Micro-benchmark)  
> **Confronto infrastruttura:** API native OS (`Win32 CreateProcessW` / `ReadDirectoryChangesW`) vs Qt 6 (`QProcess` /
> `QFileSystemWatcher`) dietro gli stessi Port del Core.

Questo documento definisce la suite di test di correttezza e di benchmark quantitativi della **Fase 0**
di [plan.md](plan.md), progettata per validare con misure reali i confini architetturali prima della Fase 1.

---

## 1. Obiettivi e collegamento con i Port del Core

Ogni suite valida una decisione architetturale precisa:

1. **Suite A — Costo di Spawn `git.exe`, Lock Contention e Batching (`git-cli` & `OperationCoordinator`)**
    - Verifica che i refresh passivi con `--no-optional-locks` eliminino al 100% gli errori `index.lock` durante
      operazioni concorrenti dell'utente o di un IDE esterno.
    - Misura su Windows la differenza tra lo shim `cmd\git.exe` e il binario nativo diretto (`ucrt64\bin\git.exe` /
      `mingw64\bin\git.exe`).
    - Confronta **`Win32 CreateProcessW`** vs **`Qt QProcess`** dietro lo stesso port `IGitCommandExecutor`.
    - Individua il limite ottimale di concorrenza dell'`OperationCoordinator` su 30 e 100 repository e verifica la
      priorità al repository attivo (*Active-Repo-First*).
    - Misura il guadagno di una sessione persistente **`git cat-file --batch`** (`IGitBatchSession`) rispetto a
      invocazioni singole ripetute.

2. **Suite B — Filesystem Watching a Due Livelli (`IRepositoryWatcher` & `EventCoalescer`)**
    - Confronta il monitoraggio ricorsivo completo su 30 repository (con 5.000 file ignorati ciascuno, es.
      `node_modules/` o `build/`) contro una strategia **2-Tier**:
        - **Tier 1 (Background):** osserva solo metadati chiave in `.git/` (`HEAD`, `index`, `packed-refs`, `refs/`).
        - **Tier 2 (Repository attivo):** osserva il working tree attivo con coalescing/debounce contro tempeste di
          eventi (es. compilazioni o `npm install`).
    - Confronta `ReadDirectoryChangesW` nativo vs `QFileSystemWatcher`.

3. **Suite C — Gate di Scala nel Core C++23 Zero-Qt (100 Repository & 50.000 Commit)**
    - **Cold Start 100 Repo:** caricamento metadati SQLite + disponibilità immediata del contesto attivo senza bloccare
      il chiamante.
    - **Streaming & Layout C++ per 50.000 Commit:** tempo per la prima finestra visibile (*First Paint Slice*, 200
      commit), tempo totale di ingestione + *Swimlane Graph Layout* e footprint RAM per nodo.

4. **Suite D — Prototipo Rendering Qt Quick del Commit Graph (50.000 righe)**
    - Confronta in uno scroll test automatizzato su `ListView` + `QAbstractListModel` C++ i tempi di frame (`p50`,
      `p95`, `p99`, jank `> 16.67 ms`) tra delegati QML standard (`Shape`/`Canvas`) e un `QQuickItem` C++ basato su
      `QSGGeometryNode`.

---

## 2. Generatore deterministico di Fixture (`gf_fixture_gen`)

Tutte le suite utilizzano repository sintetici generati localmente in modo deterministico (`seed = 42`), senza dipendere
dalla rete:

- **Fast-Import Monorepo Generator:** alimenta `git fast-import` via pipe diretta dalla memoria C++, producendo un
  repository con **50.000 commit**, biforcazioni, merge e tag in pochi secondi.
- **Fleet Generator (30 / 100 repository):** genera una flotta di repository con distribuzione controllata di stati
  (`clean`, `modified`, `staged`, `untracked`, `conflict`).

---

## 3. Esecuzione Build, Test e Benchmark

### Su Windows (PowerShell / Visual Studio MSVC)

```powershell
# 1. Build Core pura (ZERO Qt — verifica ADR-002)
cmake --preset core-release
cmake --build --preset core-release
ctest --preset core-release --output-on-failure

# 2. Build completa con Qt 6 (abilita adapter QProcess per confronto diretto)
cmake --preset qt-release
cmake --build --preset qt-release
ctest --preset qt-release --output-on-failure

# 3. Esecuzione Suite A Benchmark (con report comparativo)
.\build\qt-msvc\Release\gf_bench_suite_a.exe
.\build\qt-msvc\Release\gf_bench_suite_a.exe "[micro]"
```

### Su Linux (Bash / GCC o Clang + Ninja)

```bash
# 1. Build Core pura (ZERO Qt — verifica ADR-002)
cmake --preset linux-core-release
cmake --build --preset linux-core-release
ctest --preset linux-core-release --output-on-failure

# 2. Build completa con Qt 6
cmake --preset linux-qt-release
cmake --build --preset linux-qt-release
ctest --preset linux-qt-release --output-on-failure

# 3. Esecuzione Suite A Benchmark (con report comparativo e microbenchmark)
./build/qt-linux/gf_bench_suite_a
./build/qt-linux/gf_bench_suite_a "[micro]"
```

I risultati raccolti vengono registrati in `docs/performance/`:

- Windows: [`baseline-windows.md`](performance/baseline-windows.md)
- Linux: [`baseline-linux.md`](performance/baseline-linux.md)
