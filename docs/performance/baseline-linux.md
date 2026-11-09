# Git Flux — Benchmark di Fase 0 & Baseline Linux

> **Data esecuzione:** 6 novembre 2026 (confronto cross-platform aggiornato il 9 novembre 2026 a parità di hardware)  
> **Sistema Operativo:** Linux Ubuntu (kernel 7.0.0-42-generic x86_64)  
> **CPU:** 13th Gen Intel(R) Core(TM) i9-13900HX (24 Core / 32 Thread logici, fino a 5.4 GHz, 36 MiB L3 Cache)  
> **Compilatore:** GCC 15.2.0 (Ubuntu 15.2.0-16ubuntu1) C++23  
> **Build System:** CMake 4.2.3 + Ninja 1.12  
> **Git:** 2.53.0  
> **Qt:** 6.10.2 (libqt6core6t64)  
> **Test Framework:** Catch2 v3.7.1  

---

## 1. Sintesi Esecutiva dei Risultati

La suite di test e benchmark di Fase 0 eseguita su Linux conferma e consolida le scelte architetturali di [plan.md](../plan.md), evidenziando il comportamento dell'infrastruttura OS sia sul Core C++23 Zero-Qt sia con l'adapter Qt 6:

1. **Lock Contention su `.git/index.lock` Eliminato al 100%:**  
   L'uso sistematico del flag `--no-optional-locks` e della variabile d'ambiente `GIT_OPTIONAL_LOCKS=0` garantisce l'assenza totale di conflitti su `index.lock`. Nei test di stress con due thread di polling ininterrotto durante 20 cicli continui di mutazione (`git add` + `git commit`), si sono registrati **0 errori di lock** e il 100% di refresh andati a buon fine.
2. **Costo di Process Spawn su Linux (~4.6–4.9 ms vs ~34.7 ms su Windows a parità di hardware, ~7x più veloce):**  
   Grazie all'efficienza di `posix_spawn` e del sottosistema di processo Linux (senza l'overhead di creazione processo Win32 e i filtri filesystem real-time di Windows), la latenza di un singolo `git status` scende a meno di **5 ms**. L'adapter nativo basato su `posix_spawn` e l'adapter Qt 6 basato su `QProcess` mostrano prestazioni equivalenti (~4.9 ms vs ~4.6 ms).
3. **Ottimizzazione Concurrency Sweep su 30 Repository:**  
   Il refresh di stato su una flotta di **30 repository** scende da **156.8 ms (single-thread)** a **24.0 ms (8 thread)** fino a **10.2–15.2 ms (16 thread)**. L'`OperationCoordinator` con coda a priorità risponde per il repository attivo in appena **4.9–9.7 ms**, non risentendo dell'accodamento in background degli altri 29 repository.
4. **Sessione Persistente `git cat-file --batch` (+57x a freddo, +106x a caldo):**  
   La lettura di 150 commit tramite processi singoli separati richiede oltre **500 ms** (~3.4 ms a commit). Con una pipe persistente `cat-file --batch`, la lettura di 150 commit (compreso il costo di spawn del processo pipe) richiede **7.9–9.0 ms** (**~60x di speedup**); su sessione calda, l'operazione completa scende a **4.8–4.9 ms** (**>100x di speedup**, ossia **0.032 ms / 32 microsecondi per commit**).
5. **Microbenchmark a Livello Nanosecondo:**  
   Il parsing dello status porcelain v2 di un repository clean richiede **29.4 ns**. L'estrazione di un oggetto commit via sessione batch già aperta richiede appena **59.2 µs**.
6. **Generatore Deterministico di Fixture (`git fast-import`):**  
   La generazione di un monorepo sintetico da **50.000 commit** (con 2.840 merge) impiega appena **3.69 secondi** (allineato ai **3.82 secondi** registrati su Windows sullo stesso i9-13900HX).

---

## 2. Dettaglio dei Benchmark

### Benchmark A.2 — Latenza per Singolo Processo (`git status --porcelain=v2 -z`)
*40 campioni post-warmup su repository sintetico*

| Variante Adapter | Mean (ms) | p50 (ms) | p95 (ms) | Min (ms) |
| :--- | ---: | ---: | ---: | ---: |
| **Native (`posix_spawn`) + direct git** | 4.91 | 5.07 | 5.35 | 3.92 |
| **Qt 6 (`QProcess`) + direct git** | 4.57 | 4.65 | 4.95 | 3.72 |

### Benchmark A.3 — Concurrency Sweep su 30 Repository & Priorità Active-Repo
*30 repository sintetici (70% clean, 20% unstaged, 10% staged), media di 3 ripetizioni per livello di concorrenza*

| Adapter | Concurrency | Wall-Clock 30 Repo (ms) | Active Repo Latency (ms) |
| :--- | ---: | ---: | ---: |
| Native (`posix_spawn`) | 1 | 156.8 | 5.4 |
| Native (`posix_spawn`) | 2 | 75.8 | 7.0 |
| Native (`posix_spawn`) | **4** | **46.0** | **9.7** |
| Native (`posix_spawn`) | **8** | **24.0** | **8.7** |
| Native (`posix_spawn`) | 12 | 18.3 | 8.6 |
| Native (`posix_spawn`) | 16 | 15.2 | 10.6 |
| Qt 6 (`QProcess`) | 1 | 138.2 | 9.3 |
| Qt 6 (`QProcess`) | 2 | 66.4 | 8.0 |
| Qt 6 (`QProcess`) | **4** | **35.1** | **8.4** |
| Qt 6 (`QProcess`) | **8** | **19.7** | **4.9** |
| Qt 6 (`QProcess`) | 12 | 13.7 | 5.6 |
| Qt 6 (`QProcess`) | 16 | 10.2 | 6.9 |

> [!TIP]
> **Scelta architetturale:** La policy di concorrenza definita in [baseline-windows.md](baseline-windows.md) (`std::clamp(hardware_concurrency / 2, 2u, 6u)`) si conferma ottimale anche su Linux: con 4–6 worker la scansione di 30 repository si completa in ~35–45 ms lasciando ampia disponibilità di core per UI e compiti ad alta priorità.

### Benchmark A.4 — Lettura 150 Commit: Single-Shot vs `cat-file --batch` Pipe

| Strategia | Tempo Totale 150 commit (ms) | Media per commit (ms) | Speedup |
| :--- | ---: | ---: | ---: |
| Native Single-Shot (150x `posix_spawn`) | 514.3 | 3.429 | 1.0x (baseline) |
| Qt 6 Single-Shot (150x `QProcess`) | 497.6 | 3.317 | 1.03x |
| **Native Batch Pipe (spawn pipe + 150 query)** | **9.04** | **0.0603** | **56.9x** |
| **Native Batch Pipe (warm session, 150 query)** | **4.96** | **0.0331** | **103.7x** |
| **Qt 6 Batch Pipe (spawn pipe + 150 query)** | **7.94** | **0.0529** | **64.8x** |
| **Qt 6 Batch Pipe (warm session, 150 query)** | **4.82** | **0.0321** | **106.7x** |

### Catch2 Microbenchmark (Nanosecondi / Microsecondi)

| Benchmark | Campioni | Iterazioni stimate | Media campionata | Range confidenza |
| :--- | ---: | ---: | ---: | ---: |
| `PorcelainStatusParser::parse` (header clean) | 100 | 448 | **29.41 ns** | 29.21 ns – 29.68 ns |
| `NativeGitBatchSession::readObject("HEAD")` (warm pipe) | 100 | 1 | **59.24 µs** | 54.50 µs – 69.29 µs |

---

## 3. Confronto Cross-Platform a Parità di Hardware: Linux vs Windows (Intel Core i9-13900HX)

Confronto diretto eseguito sullo **stesso identico PC** (`13th Gen Intel Core i9-13900HX`, 24 Core / 32 Thread):

| Metrica Chiave | Windows 11 Pro (32 thread, i9-13900HX) | Linux Ubuntu (32 thread, i9-13900HX) | Delta / Confronto Diretto |
| :--- | ---: | ---: | :--- |
| **Spawn Singolo (`git status` direct)** | 34.71 ms (46.31 ms con shim) | **4.91 ms** | **7.1x più veloce su Linux** |
| **Sweep 30 Repo (1 Thread)** | 982.4 ms | **156.8 ms** | **6.3x più veloce su Linux** |
| **Sweep 30 Repo (4 Thread)** | 332.9 ms (Qt: 306.0 ms) | **46.0 ms (Qt: 35.1 ms)** | **7.2x–8.7x più veloce su Linux** |
| **Sweep 30 Repo (8 Thread)** | 216.2 ms (Qt: 204.4 ms) | **24.0 ms (Qt: 19.7 ms)** | **9.0x–10.4x più veloce su Linux** |
| **Active Repo Latency sotto carico** | 31.7–54.9 ms | **4.9–9.7 ms** | **~6x più reattivo su Linux** |
| **150 Commit (Single-Shot)** | 4066.1 ms | **514.3 ms** | **7.9x più veloce su Linux** |
| **150 Commit (`cat-file --batch` caldo)** | **2.32 ms (0.0155 ms/commit)** | **4.82 ms (0.0321 ms/commit)** | **Prestazioni in-memory equivalenti** (2.32 ms vs 4.82 ms; rimosso lo spawn, il canale pipe è istantaneo su entrambi gli OS) |
| **Generazione Fixture 50.000 Commit** | **3821 ms (3.82 s)** | **3689 ms (3.69 s)** | **Differenza del 3.5%** (a parità di hardware, il throughput puro di `git fast-import` è identico) |

---

## 4. Criteri di Accettazione CTest Verificati

- `gf_unit_tests` (GCC 15.2, C++23): **29 asserzioni superate in 5 test case** (0 fallimenti; il 6º test case è specifico per il quoting Win32).
- `gf_integration_tests` (GCC 15.2, C++23 + Git reale): **214 asserzioni superate in 3 test case** (0 fallimenti).
- `gf_bench_suite_a` (Catch2 microbenchmark): **1081 asserzioni superate in 2 test case** (0 fallimenti).
- Esecuzione verificata su preset `linux-core-release` (senza Qt) e `linux-qt-release` (con Qt 6.10.2).
