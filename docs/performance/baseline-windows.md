# Git Flux — Benchmark di Fase 0 & Baseline Windows

> **Data esecuzione:** 6 novembre 2026  
> **Sistema Operativo:** Windows 11 Pro (build 26200)  
> **CPU:** 12 Logical Cores (Intel/AMD x64)  
> **Compilatore:** MSVC 19.44.35229 (Visual Studio 2022 Community)  
> **Git:** 2.56.0.windows.4  
> **Qt:** 6.6.2 (MSVC 2019 x64)  
> **Test Framework:** Catch2 v3.7.1  

---

## 1. Sintesi Esecutiva dei Risultati

I test e i benchmark di Fase 0 hanno confermato e quantificato le ipotesi architetturali di [plan.md](../plan.md):

1. **Lock Contention su `.git/index.lock` Eliminato al 100%:**  
   L'adozione sistematica del flag `--no-optional-locks` (e dell'ambiente `GIT_OPTIONAL_LOCKS=0`) per tutti i refresh passivi ha permesso di eseguire polling e refresh continui mentre in parallelo venivano eseguiti cicli concorrenti di `git add` e `git commit`, con **0 conflitti di lock**.
2. **Costo di Process Spawn su Windows (~77–79 ms):**  
   Ogni invocazione a freddo di `git.exe` ha un costo base intrinseco tra i 67 ms e i 95 ms su Windows (antivirus + fork emulation/Win32 overhead). L'adapter nativo `CreateProcessW` e `QProcess` registrano prestazioni quasi identiche (~78 ms vs ~77 ms).
3. **Ottimizzazione Concurrency Sweep:**  
   Il refresh di stato su una flotta di **30 repository** passa da **2.296 ms (single thread)** a **473 ms (8 thread)**. L'`OperationCoordinator` con coda a priorità garantisce che il repository attivo venga servito in soli **74–85 ms**, anche mentre gli altri 29 repository sono in coda di scansione.
4. **Sessione Persistente `git cat-file --batch` (+117x / +956x di Speedup):**  
   Leggere i dati di 150 commit singolarmente richiede quasi **10 secondi** (9.931 ms, ~66 ms a commit). Con una pipe persistente `cat-file --batch`, la lettura di 150 commit (inclusa la creazione del processo) richiede **84 ms** (**117x di speedup**); su sessione calda, richiede solo **10.39 ms** (**956x di speedup**, ossia **0.069 ms per commit**).
5. **Generatore Deterministico di Fixture (`git fast-import`):**  
   Il tool C++ di test genera un repository monolite da **50.000 commit** (con 2.840 merge) in appena **9,5 secondi**.

---

## 2. Dettaglio dei Benchmark

### Benchmark A.2 — Latenza per Singolo Processo (`git status --porcelain=v2 -z`)
*40 campioni post-warmup su repository sintetico*

| Variante Adapter | Mean (ms) | p50 (ms) | p95 (ms) | Min (ms) |
| :--- | ---: | ---: | ---: | ---: |
| **Native (`CreateProcessW`)** | 79.05 | 78.70 | 94.27 | 66.93 |
| **Qt 6 (`QProcess`)** | 77.86 | 75.55 | 94.66 | 67.24 |

### Benchmark A.3 — Concurrency Sweep su 30 Repository & Priorità Active-Repo
*30 repository sintetici (70% clean, 20% unstaged, 10% staged), media di 3 ripetizioni per livello di concorrenza*

| Adapter | Concurrency | Wall-Clock 30 Repo (ms) | Active Repo Latency (ms) |
| :--- | ---: | ---: | ---: |
| Native (`CreateProcessW`) | 1 | 2.296,3 | 97,2 |
| Native (`CreateProcessW`) | 2 | 1.058,7 | 74,8 |
| Native (`CreateProcessW`) | **4** | **683,3** | **85,8** |
| Native (`CreateProcessW`) | **8** | **473,1** | **131,1** |
| Native (`CreateProcessW`) | 12 | 513,2 | 196,1 |
| Native (`CreateProcessW`) | 16 | 465,3 | 259,3 |
| Qt 6 (`QProcess`) | 1 | 2.296,4 | 103,1 |
| Qt 6 (`QProcess`) | 2 | 1.073,6 | 77,5 |
| Qt 6 (`QProcess`) | **4** | **679,8** | **91,0** |
| Qt 6 (`QProcess`) | **8** | **473,6** | **123,4** |
| Qt 6 (`QProcess`) | 12 | 474,8 | 195,4 |
| Qt 6 (`QProcess`) | 16 | 448,8 | 204,7 |

> [!TIP]
> **Scelta architetturale:** La concorrenza di default consigliata su macchine desktop a 8+ core è fissata a **`std::clamp(hardware_concurrency / 2, 2u, 6u)`** (tipicamente 4–6 worker). Con questo range si ottiene l'80% della riduzione di tempo totale senza degradare la reattività dell'Active Repo.

### Benchmark A.4 — Lettura 150 Commit: Single-Shot vs `cat-file --batch` Pipe

| Strategia | Tempo Totale 150 commit (ms) | Media per commit (ms) | Speedup |
| :--- | ---: | ---: | ---: |
| Native Single-Shot (150x `CreateProcessW`) | 9.931,7 | 66,211 | 1.0x (baseline) |
| Qt 6 Single-Shot (150x `QProcess`) | 9.618,7 | 64,124 | 1.03x |
| **Native Batch Pipe (spawn + 150 query)** | **84,66** | **0,5644** | **117.3x** |
| **Native Batch Pipe (warm session, 150 query)** | **10,39** | **0,0692** | **956.1x** |
| **Qt 6 Batch Pipe (spawn + 150 query)** | **100,16** | **0,6678** | **99.2x** |
| **Qt 6 Batch Pipe (warm session, 150 query)** | **15,88** | **0,1059** | **625.4x** |

---

## 3. Criteri di Accettazione CTest Verificati

- `gf_unit_tests` (MSVC x64, C++23): **30 asserzioni superate in 6 test case** (0 fallimenti).
- `gf_integration_tests` (MSVC x64, C++23 + Git reale): **214 asserzioni superate in 3 test case** (0 fallimenti).
- Esecuzione validata sia su preset puro `core-release` (senza Qt) sia su `qt-release` (con Qt 6.6.2).
