# Git Flux — Specifica e Stato di Avanzamento Fase 5 (Performance, Background Workers, Watchers & Multi-Repo Automation)

> **Stato:** Completato Task 5.1 — File System Watcher & Debouncing Engine | Prossimo: Task 5.2 — Background Task Runner & Async Git Execution  
> **Architettura di riferimento:** [plan.md](plan.md) (Sezioni 9.3, 9.4, 10, 11)  
> **Wireframe UI di riferimento:** [`docs/ui/git-flux-ui-v2.drawio`](ui/git-flux-ui-v2.drawio) (Pagine 01, 02, 05)  

---

## 1. Suddivisione dei Task Logici di Fase 5

La Fase 5 implementa la reattività ad alte prestazioni, il monitoraggio del filesystem, l'esecuzione asincrona non bloccante e l'automazione multi-repository per gruppi e workspace:

| Task | Area / Modulo | Descrizione | Stato |
| :--- | :--- | :--- | :---: |
| **5.1** | **File System Watcher & Debouncing Engine** | Domain BO (`FsChangeEvent`, `FsDebouncedBatch`, `FsChangeClassification`), port `IFsWatcherPort`, classificazione eventi `.git` vs working tree, debouncer temporale a finestra di quiete e `GitRepositoryWatcherService`. | **Completato** |
| **5.2** | **Background Task Runner & Async Git Execution** | Worker pool e task runner asincrono con `std::stop_token` per cancellazione, telemetria progressi thread-safe e coordinamento lock non bloccante. | *In coda* |
| **5.3** | **Multi-Repository Batch Coordinator** | Operazioni aggregate su gruppi/workspace (Fetch All, Pull All, Bulk Status) con limitatore di parallelismo (concurrency limiter/semaforo) e report atomico per-repository. | *In coda* |
| **5.4** | **Reactivity & Multi-Repo UI Integration** | Auto-refresh reattivo nei ViewModels e UI su eventi filesystem, pannello di controllo batch e indicatori di operazione in background. | *In coda* |

---

## 2. Dettaglio Task 5.1 — File System Watcher & Debouncing Engine

### Obiettivi e Principi Architetturali
- **Zero-Qt nel Core:** Regole di classificazione, modelli di evento e logica di debouncing implementati in C++ standard puro (`std::chrono`, `std::filesystem`).
- **Classificazione Intelligente degli Eventi:**
  - Separazione tra modifiche interne di Git (`.git/HEAD`, `.git/refs/`, `.git/index`, `.git/rebase-merge/`, `.git/MERGE_HEAD`) e modifiche ai file dell'albero di lavoro (working tree).
  - Esclusione immediata di percorsi rumorosi non significativi (`.git/objects/`, `.git/logs/`, `.git/hooks/`, file temporanei di editor `.swp`, `*~`, file `.lock`, ecc.) per minimizzare l'overhead CPU.
- **Algoritmo di Debouncing Deterministico:**
  - Quando avvengono modifiche massive e veloci (es. compilazione, branch checkout, installazione pacchetti), gli eventi ravvicinati vengono raggruppati in un batch consolidato.
  - Finestra di quiete configurabile (`quietPeriod`, default: 200ms) e finestra massima assoluta (`maxWindow`, default: 1000ms) per prevenire starvation.
  - Erogazione di un singolo evento consolidato per repository che specifica se è variato il branch/HEAD (`headChanged`, `refsChanged`), l'index (`indexChanged`), lo stato operativo (`operationStateChanged`) o il working tree (`workingTreeChanged`).
- **Portabilità e Testabilità:**
  - Il port `IFsWatcherPort` consente di collegare watcher nativi di sistema o un watcher periodico leggero O(1) in C++ puro.
  - Testabilità deterministica del debouncer tramite clock simulabile senza attese di sleep bloccanti.

### Risultati del Task 5.1
- **File Creati:**
  - `src/core/modules/git/bo/fs_watcher_models.hpp`: modelli BO puri (`FsChangeType`, `FsChangeClassification`, `FsRawEvent`, `FsDebouncedBatch`).
  - `src/core/modules/git/ports/fs_watcher_port.hpp`: interfaccia port `IFsWatcherPort` e `FsWatcherError`.
  - `src/core/modules/git/bl/fs_event_classifier.hpp` e `.cpp`: classificatore puro di eventi Git e working tree con filtraggio file rumorosi.
  - `src/core/modules/git/bl/fs_event_debouncer.hpp` e `.cpp`: debouncer thread-safe a finestra di quiete e finestra massima.
  - `src/core/modules/git/bl/git_repository_watcher_service.hpp` e `.cpp`: service applicativo con coordinamento watcher, classificazione e auto-flush in background.
  - `src/infrastructure/watcher/native_polling_fs_watcher.hpp` e `.cpp`: watcher leggero a basso consumo (O(1) stat su HEAD, index, refs e root dir) universale per Linux, macOS e Windows.
  - `tests/unit/fs_watcher_unit_tests.cpp`: test unitari del classificatore, del debouncer con clock simulato e del service con mock port.
  - `tests/integration/fs_watcher_integration_tests.cpp`: test di integrazione su repository Git reale con modifiche esterne a file, branch e index.
- **File Modificati:**
  - `CMakeLists.txt`: aggiunta dei sorgenti a `gf_application`, definizione libreria `gf_watcher_native` e registrazione test unitari e di integrazione.
- **Metriche Test:** Suite completa passata con successo (3/3 test suite su `linux-qt-release`, 2/2 su `linux-core-release`, zero warning con `-Werror`).

---
