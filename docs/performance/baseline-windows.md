# Git Flux — Benchmark di Fase 0 & Baseline Windows

> **Data esecuzione:** 9 novembre 2026  
> **Sistema Operativo:** Windows 11 Pro 26H2 (build 26300)  
> **CPU:** 13th Gen Intel (R) Core (TM) i9-13900HX (24 Core / 32 Thread logici, fino a 5.4 GHz, 36 MiB L3 Cache)  
> **Compilatore:** MSVC 19.44.35229 (Visual Studio 2022 Community) C++23  
> **Build System:** CMake 4.4.4 + MSBuild 17.14  
> **Git:** 2.56.0.windows.4 (`ucrt64\bin\git.exe` diretto vs shim `cmd\git.exe`)  
> **Qt:** 6.6.2 (MSVC 2019 x64)  
> **Test Framework:** Catch2 v3.7.1

---

## 1. Sintesi Esecutiva dei Risultati

I test e i benchmark di Fase 0, rieseguiti sullo stesso hardware di riferimento della baseline Linux (**Intel Core
i9-13900HX, 24 Core / 32 Thread**), hanno confermato e quantificato le scelte architetturali di [plan.md](../plan.md):

1. **Lock Contention su `.git/index.lock` Eliminato al 100%:**  
   L'adozione sistematica del flag `--no-optional-locks` (e dell'ambiente `GIT_OPTIONAL_LOCKS=0`) per tutti i refresh
   passivi ha permesso di eseguire polling e refresh continui mentre in parallelo venivano eseguiti cicli concorrenti di
   `git add` e `git commit`, con **0 conflitti di lock**.
2. **Bypass dello Shim `cmd\git.exe` e Costo di Process Spawn (~34.7 ms vs ~46.3 ms, -25% latenza):**  
   Su Windows, l'eseguibile predefinito nel `PATH` (`C:\Program Files\Git\cmd\git.exe`) è uno shim wrapper di ~44 KB che
   avvia un processo figlio aggiuntivo ad ogni comando (~46.3–47.7 ms). Risolvendo automaticamente il binario nativo
   diretto (`C:\Program Files\Git\ucrt64\bin\git.exe` o `mingw64\bin\git.exe`) tramite `GitDiscovery::discover()`, la
   latenza media per un singolo `git status` scende a **34.71 ms** con `CreateProcessW` (p50 **33.39 ms**, min **30.04
   ms**) e **35.46 ms** con `QProcess`, risparmiando **~12 ms** ad ogni invocazione.
3. **Ottimizzazione Concurrency Sweep su 30 Repository:**  
   Il refresh di stato su una flotta di **30 repository** (generati in **1.553 ms**) scende da **982,4 ms (
   single-thread)** a **332,9 ms (4 thread)** fino a **216,2 ms (8 thread)** con `CreateProcessW` (**204,4 ms** con
   `QProcess`). L'`OperationCoordinator` con coda a priorità garantisce che il repository attivo venga servito in soli
   **31,7–54,9 ms**, anche mentre gli altri 29 repository sono in coda di scansione.
4. **Sessione Persistente `git cat-file --batch` (+121x a freddo, +1.749x a caldo):**  
   Leggere i metadati di 150 commit tramite processi singoli richiede **4.066,1 ms** (~27,1 ms a commit). Con una pipe
   persistente `cat-file --batch`, la lettura di 150 commit (inclusa la creazione del processo pipe) richiede **33,60
   ms** (**121,0x di speedup**); su sessione calda, richiede appena **2,32 ms** (**1.749,1x di speedup**, ossia **0,0155
   ms / 15,5 microsecondi per commit**).
5. **Microbenchmark a Livello Nanosecondo:**  
   Il parsing dello status porcelain v2 di un repository clean richiede **65,45 ns**. L'estrazione di un oggetto commit
   via sessione batch già aperta richiede **657,3 µs**.
6. **Generatore Deterministico di Fixture (`git fast-import`):**  
   Il tool C++ di test genera un repository monolite da **50.000 commit** (con 2.840 merge) in appena **3.821 ms (3,82
   secondi)**, prestazione allineata ai **3,69 secondi** registrati su Linux a parità di hardware.

---

## 2. Dettaglio dei Benchmark

### Benchmark A.2 — Latenza per Singolo Processo (`git status --porcelain=v2 -z`)

*40 campioni post-warmup su repository sintetico (confronto Shim vs Direct Binary e Native vs Qt 6)*

| Variante Adapter                                 | Mean (ms) |  p50 (ms) |  p95 (ms) |  Min (ms) |
|:-------------------------------------------------|----------:|----------:|----------:|----------:|
| Native (`CreateProcessW`) + `cmd\git.exe` (shim) |     46.31 |     45.41 |     50.91 |     42.22 |
| **Native (`CreateProcessW`) + direct `git.exe`** | **34.71** | **33.39** | **41.39** | **30.04** |
| Qt 6 (`QProcess`) + `cmd\git.exe` (shim)         |     47.72 |     47.03 |     54.39 |     41.43 |
| **Qt 6 (`QProcess`) + direct `git.exe`**         | **35.46** | **34.22** | **38.12** | **31.48** |

### Benchmark A.3 — Concurrency Sweep su 30 Repository & Priorità Active-Repo

*30 repository sintetici (70% clean, 20% unstaged, 10% staged), media di 3 ripetizioni per livello di concorrenza con
binario diretto `ucrt64\bin\git.exe`*

| Adapter                   | Concurrency | Wall-Clock 30 Repo (ms) | Active Repo Latency (ms) |
|:--------------------------|------------:|------------------------:|-------------------------:|
| Native (`CreateProcessW`) |           1 |                   982.4 |                     31.7 |
| Native (`CreateProcessW`) |           2 |                   545.3 |                     48.6 |
| Native (`CreateProcessW`) |       **4** |               **332.9** |                 **42.8** |
| Native (`CreateProcessW`) |       **8** |               **216.2** |                 **54.9** |
| Native (`CreateProcessW`) |          12 |                   208.7 |                     79.1 |
| Native (`CreateProcessW`) |          16 |                   198.8 |                    105.8 |
| Qt 6 (`QProcess`)         |           1 |                  1018.1 |                     55.7 |
| Qt 6 (`QProcess`)         |           2 |                   537.1 |                     48.2 |
| Qt 6 (`QProcess`)         |       **4** |               **306.0** |                 **41.9** |
| Qt 6 (`QProcess`)         |       **8** |               **204.4** |                 **54.3** |
| Qt 6 (`QProcess`)         |          12 |                   193.8 |                     74.0 |
| Qt 6 (`QProcess`)         |          16 |                   200.5 |                     99.5 |

> [!TIP]
> **Scelta architetturale:** La concorrenza di default consigliata su macchine desktop multi-core è fissata a **
`std::clamp(hardware_concurrency / 2, 2u, 6u)`** (tipicamente 4–6 worker). Con 4–8 worker si abbatte il tempo di
scansione di 30 repository a ~200–330 ms mantenendo la latenza del repository attivo intorno a ~42–55 ms, evitando la
contesa sul sottosistema filesystem/processi di Windows che emerge oltre gli 8 thread.

### Benchmark A.4 — Lettura 150 Commit: Single-Shot vs `cat-file --batch` Pipe

| Strategia                                       | Tempo Totale 150 commit (ms) | Media per commit (ms) |         Speedup |
|:------------------------------------------------|-----------------------------:|----------------------:|----------------:|
| Native Single-Shot (150x `CreateProcessW`)      |                       4066.1 |                27.107 | 1.0x (baseline) |
| Qt 6 Single-Shot (150x `QProcess`)              |                       4085.5 |                27.237 |           1.00x |
| **Native Batch Pipe (open + 150 query)**        |                    **33.60** |            **0.2240** |      **121.0x** |
| **Native Batch Pipe (warm session, 150 query)** |                     **2.32** |            **0.0155** |     **1749.1x** |
| **Qt 6 Batch Pipe (open + 150 query)**          |                    **36.82** |            **0.2455** |      **110.4x** |
| **Qt 6 Batch Pipe (warm session, 150 query)**   |                     **3.78** |            **0.0252** |     **1075.5x** |

### Catch2 Microbenchmark (Nanosecondi / Microsecondi)

| Benchmark                                               | Campioni | Iterazioni stimate | Media campionata |      Range confidenza |
|:--------------------------------------------------------|---------:|-------------------:|-----------------:|----------------------:|
| `PorcelainStatusParser::parse` (header clean)           |      100 |                193 |     **65.45 ns** |   65.09 ns – 66.14 ns |
| `NativeGitBatchSession::readObject("HEAD")` (warm pipe) |      100 |                  1 |    **657.32 µs** | 640.40 µs – 683.71 µs |

---

## 3. Confronto Cross-Platform a Parità di Hardware (i9-13900HX: Windows 11 vs Linux Ubuntu)

Confronto diretto sullo **stesso identico PC** (`13th Gen Intel Core i9-13900HX`, 24 Core / 32 Thread):

| Metrica Chiave                            |    Windows 11 Pro (i9-13900HX) |      Linux Ubuntu (i9-13900HX) | Osservazioni Architetturali                                                                                                                   |
|:------------------------------------------|-------------------------------:|-------------------------------:|:----------------------------------------------------------------------------------------------------------------------------------------------|
| **Spawn Singolo (`git status` direct)**   |   34.71 ms (46.31 ms con shim) |                    **4.91 ms** | `posix_spawn` è ~7.1x più rapido di `CreateProcessW` per il minor overhead kernel/VFS; il bypass di `cmd\git.exe` risparmia ~12 ms su Windows |
| **Sweep 30 Repo (1 Thread)**              |                       982.4 ms |                   **156.8 ms** | Riflette linearmente il costo di spawn di 30 processi sequenziali (6.3x)                                                                      |
| **Sweep 30 Repo (4 Thread)**              |        332.9 ms (Qt: 306.0 ms) |      **46.0 ms (Qt: 35.1 ms)** | Lo scaling multi-thread riduce il wall-clock di oltre 3x su entrambi gli OS                                                                   |
| **Sweep 30 Repo (8 Thread)**              |        216.2 ms (Qt: 204.4 ms) |      **24.0 ms (Qt: 19.7 ms)** | Scansione completa di 30 repo in ~0.2 s su Windows e ~0.02 s su Linux                                                                         |
| **Active Repo Latency sotto carico**      |                   31.7–54.9 ms |                 **4.9–9.7 ms** | La coda a priorità serve il repository attivo al costo di un singolo comando                                                                  |
| **150 Commit (Single-Shot)**              |                      4066.1 ms |                   **514.3 ms** | 150 spawn separati amplificano il divario di creazione processo (7.9x)                                                                        |
| **150 Commit (`cat-file --batch` caldo)** | **2.32 ms (0.0155 ms/commit)** | **4.82 ms (0.0321 ms/commit)** | Eliminato lo spawn via pipe persistente, il throughput I/O in-memory su Windows eguaglia e supera Linux                                       |
| **Generazione Fixture 50.000 Commit**     |           **3821 ms (3.82 s)** |           **3689 ms (3.69 s)** | Scarto di appena il **3.5%**: su stream continuo (`git fast-import`) le prestazioni CPU/NVMe dei due OS sullo stesso hardware sono identiche  |

---

## 4. Criteri di Accettazione CTest Verificati

- `gf_unit_tests` (MSVC x64, C++23): **30 asserzioni superate in 6 test case** (0 fallimenti).
- `gf_integration_tests` (MSVC x64, C++23 + Git reale): **214 asserzioni superate in 3 test case** (0 fallimenti).
- `gf_bench_suite_a` (Suite A Report + Catch2 microbenchmark): **1251 asserzioni superate in 2 test case** (0
  fallimenti).
- Esecuzione validata sia su preset puro `core-release` (senza Qt) sia su `qt-release` (con Qt 6.6.2).
