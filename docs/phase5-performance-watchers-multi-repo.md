# Git Flux — Specifica e Stato di Avanzamento Fase 5 (Performance, Background Workers, Watchers & Multi-Repo Automation)

> **Stato:** Fase 5 Completata al 100% (Tutti i Task 5.1, 5.2, 5.3, 5.4 Completati)  
> **Architettura di riferimento:** [plan.md](plan.md) (Sezioni 9.3, 9.4, 10, 11)  
> **Wireframe UI di riferimento:** [`docs/ui/git-flux-ui-v2.drawio`](ui/git-flux-ui-v2.drawio) (Pagine 01, 02, 05)

---

## 1. Suddivisione dei Task Logici di Fase 5

La Fase 5 implementa la reattività ad alte prestazioni, il monitoraggio del filesystem, l'esecuzione asincrona non
bloccante e l'automazione multi-repository per gruppi e workspace:

| Task    | Area / Modulo                                    | Descrizione                                                                                                                                                                                                                |     Stato      |
|:--------|:-------------------------------------------------|:---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|:--------------:|
| **5.1** | **File System Watcher & Debouncing Engine**      | Domain BO (`FsChangeEvent`, `FsDebouncedBatch`, `FsChangeClassification`), port `IFsWatcherPort`, classificazione eventi `.git` vs working tree, debouncer temporale a finestra di quiete e `GitRepositoryWatcherService`. | **Completato** |
| **5.2** | **Background Task Runner & Async Git Execution** | Worker pool e task runner asincrono con `std::stop_token` per cancellazione, telemetria progressi thread-safe e coordinamento lock non bloccante.                                                                          | **Completato** |
| **5.3** | **Multi-Repository Batch Coordinator**           | Operazioni aggregate su gruppi/workspace (Fetch All, Pull All, Bulk Status) con limitatore di parallelismo (concurrency limiter/semaforo) e report atomico per-repository.                                                 | **Completato** |
| **5.4** | **Reactivity & Multi-Repo UI Integration**       | Auto-refresh reattivo nei ViewModels e UI su eventi filesystem, pannello di controllo batch e indicatori di operazione in background.                                                                                      | **Completato** |

---

## 2. Dettaglio Task 5.1 — File System Watcher & Debouncing Engine

### Obiettivi e Principi Architetturali

- **Zero-Qt nel Core:** Regole di classificazione, modelli di evento e logica di debouncing implementati in C++ standard
  puro (`std::chrono`, `std::filesystem`).
- **Classificazione Intelligente degli Eventi:**
    - Separazione tra modifiche interne di Git (`.git/HEAD`, `.git/refs/`, `.git/index`, `.git/rebase-merge/`,
      `.git/MERGE_HEAD`) e modifiche ai file dell'albero di lavoro (working tree).
    - Esclusione immediata di percorsi rumorosi non significativi (`.git/objects/`, `.git/logs/`, `.git/hooks/`, file
      temporanei di editor `.swp`, `*~`, file `.lock`, ecc.) per minimizzare l'overhead CPU.
- **Algoritmo di Debouncing Deterministico:**
    - Quando avvengono modifiche massive e veloci (es. compilazione, branch checkout, installazione pacchetti), gli
      eventi ravvicinati vengono raggruppati in un batch consolidato.
    - Finestra di quiete configurabile (`quietPeriod`, default: 200ms) e finestra massima assoluta (`maxWindow`,
      default: 1000ms) per prevenire starvation.
    - Erogazione di un singolo evento consolidato per repository che specifica se è variato il branch/HEAD
      (`headChanged`, `refsChanged`), l'index (`indexChanged`), lo stato operativo (`operationStateChanged`) o il
      working tree (`workingTreeChanged`).
- **Portabilità e Testabilità:**
    - Il port `IFsWatcherPort` consente di collegare watcher nativi di sistema o un watcher periodico leggero O (1) in
      C++ puro.
    - Testabilità deterministica del debouncer tramite clock simulabile senza attese di sleep bloccanti.

### Risultati del Task 5.1

- **File Creati:**
    - `src/core/modules/git/bo/fs_watcher_models.hpp`: modelli BO puri (`FsChangeType`, `FsChangeClassification`,
      `FsRawEvent`, `FsDebouncedBatch`).
    - `src/core/modules/git/ports/fs_watcher_port.hpp`: interfaccia port `IFsWatcherPort` e `FsWatcherError`.
    - `src/core/modules/git/bl/fs_event_classifier.hpp` e `.cpp`: classificatore puro di eventi Git e working tree con
      filtraggio file rumorosi.
    - `src/core/modules/git/bl/fs_event_debouncer.hpp` e `.cpp`: debouncer thread-safe a finestra di quiete e finestra
      massima.
    - `src/core/modules/git/bl/git_repository_watcher_service.hpp` e `.cpp`: service applicativo con coordinamento
      watcher, classificazione e auto-flush in background.
    - `src/infrastructure/watcher/native_polling_fs_watcher.hpp` e `.cpp`: watcher leggero a basso consumo (O (1) stat
      su HEAD, index, refs e root dir) universale per Linux, macOS e Windows.
    - `tests/unit/fs_watcher_unit_tests.cpp`: test unitari del classificatore, del debouncer con clock simulato e del
      service con mock port.
    - `tests/integration/fs_watcher_integration_tests.cpp`: test di integrazione su repository Git reale con modifiche
      esterne a file, branch e index.
- **File Modificati:**
    - `CMakeLists.txt`: aggiunta dei sorgenti a `gf_application`, definizione libreria `gf_watcher_native` e
      registrazione test unitari e di integrazione.
- **Metriche Test:** Suite completa passata con successo (3/3 test suite su `linux-qt-release`, 2/2 su
  `linux-core-release`, zero warning con `-Werror`).

---

## 3. Dettaglio Task 5.2 — Background Task Runner & Async Git Execution

### Obiettivi e Principi Architetturali

- **Zero-Qt nel Core:** Worker pool e logica asincrona scritti interamente con thread nativi C++20 (`std::jthread`),
  `std::stop_token` per cancellazione cooperativa e synchronization primitives standard (`std::mutex`,
  `std::condition_variable`).
- **Modelli di Dominio dei Task (`TaskReport`, `TaskProgress`, `TaskState`):**
    - Tracciamento esaustivo degli stati (`Pending`, `Running`, `Completed`, `Failed`, `Cancelled`).
    - Streaming dei progressi (percentuale numerica e stringa di stato descrittiva).
    - Telemetria precisa della durata di esecuzione in millisecondi.
- **Task Runner a Priorità con Prevenzione Starvation:**
    - Coda con priorità (`High`, `Normal`, `Low`) con ordinamento FIFO a parità di priorità.
    - Cancellazione sia dei task già in esecuzione che di quelli ancora in attesa in coda.
    - Metodi di sincronizzazione e coordinamento (`waitForAll`, `shutdown`).
- **Integrazione con Git (`AsyncGitService`):**
    - Esecuzione asincrona di comandi potenzialmente pesanti o legati alla rete (`fetchRemoteAsync`, `pullRemoteAsync`,
      `pushRemoteAsync`, `refreshWorkingTreeAsync`).
    - Callback atomici di completamento con `std::expected` e streaming progressi.
    - Cancellazione cooperativa dell'operazione Git.

### Risultati del Task 5.2

- **File Creati:**
    - `src/core/modules/tasks/bo/task_models.hpp`: modelli di stato, progresso e report dei task.
    - `src/core/modules/tasks/ports/task_progress_sink.hpp`: interfaccia port per il reporting dei progressi e
      l'interrogazione dello stop token.
    - `src/core/modules/tasks/bl/background_task_runner.hpp` e `.cpp`: worker pool nativo multi-thread a priorità con
      cancellazione cooperativa.
    - `src/core/modules/git/bl/async_git_service.hpp` e `.cpp`: service Git asincrono per fetch, pull, push e refresh
      status non bloccanti.
    - `tests/unit/task_runner_unit_tests.cpp`: test unitari del runner (esecuzione, fallimento, progress streaming,
      cancellazione cooperativa, priorità e stress test concorrente).
    - `tests/integration/async_git_integration_tests.cpp`: test di integrazione su repository Git reale per scansione
      asincrona, fetch e cancellazione.
- **File Modificati:**
    - `CMakeLists.txt`: aggiunta dei sorgenti a `gf_application` e registrazione delle nuove suite di test.
- **Metriche Test:** Suite completa passata con successo (3/3 test suite su `linux-qt-release`, 2/2 su
  `linux-core-release`, zero warning con `-Werror`).

---

## 4. Dettaglio Task 5.3 — Multi-Repository Batch Coordinator

### Obiettivi e Principi Architetturali

- **Zero-Qt nel Core:** Modelli di report (`BatchOperationReport`, `RepoBatchItemResult`, `BatchProgressUpdate`,
  `RepoBatchTarget`), logica di deduplicazione ed esecuzione batch scritti in C++20 puro.
- **Risoluzione e Deduplicazione Automatica:**
    - Espansione immediata dei membri di un gruppo (`resolveGroupTargets`) o di un workspace
      (`resolveWorkspaceTargets`).
    - Deduplicazione trasparente dei repository condivisi tra più gruppi o aggiunti come repository diretti nel
      workspace.
- **Controllo di Concorrenza & Concurrency Limiter:**
    - Parametro configurabile `maxConcurrency` (default: 4) per limitare il numero di operazioni Git/rete simultanee ed
      evitare saturazione di banda e socket descriptor.
    - Streaming dei progressi (`BatchProgressUpdate`) ad ogni repository completato con percentuale complessiva.
- **Isolamento dei Fallimenti:**
    - Il fallimento di un singolo repository (es. credenziali errate o remote non raggiungibile) non interrompe le
      operazioni sugli altri repository del gruppo o del workspace.
    - Generazione di un report aggregato atomico (`BatchOperationReport`) con il dettaglio dello stato (`Succeeded`,
      `Failed`, `Skipped`) e della durata individuale per ciascun repository.
- **Supporto alla Cancellazione Cooperativa:**
    - Metodo `cancelBatch(operationId)` che permette di interrompere un'operazione batch in corso, marcando i repository
      pendenti come `Skipped`.

### Risultati del Task 5.3

- **File Creati:**
    - `src/core/modules/git/bo/batch_operation_models.hpp`: modelli di dominio per target, progressi e report batch.
    - `src/core/modules/git/bl/multi_repo_batch_coordinator.hpp` e `.cpp`: coordinatore batch multi-repository con
      risoluzione e deduplicazione da gruppi e workspace, supporto per Fetch, Pull e Status sia sincrono che asincrono
      su `BackgroundTaskRunner`.
    - `tests/unit/batch_coordinator_unit_tests.cpp`: test unitari di deduplicazione, gestione fallimenti parziali,
      calcolo report e cancellazione.
    - `tests/integration/batch_coordinator_integration_tests.cpp`: test di integrazione su repository Git reali con bare
      remote multipli e gruppi SQLite.
- **File Modificati:**
    - `CMakeLists.txt`: aggiunta del coordinatore a `gf_application` e registrazione delle nuove suite di test.
- **Metriche Test:** Suite completa passata con successo (3/3 test suite su `linux-qt-release`, 2/2 su
  `linux-core-release`, zero warning con `-Werror`).

---

## 5. Dettaglio Task 5.4 — Reactivity & Multi-Repo UI Integration

### Obiettivi e Principi Architetturali

- **Auto-Refresh Reattivo Event-Driven:**
    - `WorkspaceSessionViewModel` osserva automaticamente tutti i repository aperti nei tab tramite
      `GitRepositoryWatcherService`.
    - All'apertura di un repository viene attivato il monitoraggio del filesystem (`startWatching`); alla chiusura del
      tab o di tutti i tab l'osservazione viene interrotta (`stopWatching`).
    - Alla ricezione di un `FsDebouncedBatch` da parte del worker thread, il dispatching viene instradato in modo
      thread-safe sul main GUI thread di Qt tramite `QMetaObject::invokeMethod(this, ..., Qt::QueuedConnection)`.
    - In base alla classificazione dell'evento (`requiresWorkingTreeRefresh`, `requiresBranchRefresh`), vengono
      aggiornati selettivamente il working tree model, il branch corrente, lo stato dirty e lo status di
      rebase/conflitti.
- **Batch Operations ViewModel:**
    - `BatchOperationsViewModel` fa da ponte tra il `MultiRepoBatchCoordinator` (Core C++ Zero-Qt) e la GUI Qt Quick.
    - Metodi `fetchGroup`, `fetchWorkspace`, `fetchRepositories`, `pullRepositories`, `checkStatusRepositories` e
      `cancelCurrentOperation`.
    - Proprietà reattive per il QML: `isRunning`, `progressPercentage`, `currentOperationName`, `currentRepoName`,
      `statusMessage`, `totalRepos`, `completedRepos`, `succeededCount`, `failedCount` e `lastReportItems`.
- **UI & Modal Interattivi QML:**
    - `BatchProgressModal.qml`: dialog modale con progress bar sfumata, percentuale in tempo reale, conteggio repository
      completati/totali, pulsante di cancellazione cooperativa in esecuzione, e report atomico finale con esito (badge
      verde/rosso), messaggi descrittivi e durata in millisecondi.
    - `HomeHubView.qml`: pulsanti "⬇ Fetch" integrati direttamente sui badge/card di ciascun Gruppo e Workspace.
    - `WorkspaceSessionView.qml`: indicatore a pillola dinamico nella toolbar che segnala l'attività in background e la
      percentuale di completamento, con apertura immediata del modale al click.
- **Integrazione nel Composition Root:**
    - `AppBootstrap` istanzia `NativePollingFsWatcher` (avviando il monitoraggio nativo), `GitRepositoryWatcherService`
      (con auto-flush attivo), `BackgroundTaskRunner` (pool di 4 thread), `AsyncGitService`, `MultiRepoBatchCoordinator`
      e registra `BatchOperationsViewModel` in QML sotto la proprietà di contesto globale `batchOperationsVm`.

### Risultati del Task 5.4

- **File Creati:**
    - `src/desktop/viewmodels/batch_operations_viewmodel.hpp` e `.cpp`: ViewModel per operazioni batch multi-repository.
    - `src/desktop/ui/qml/views/BatchProgressModal.qml`: dialog QML per il monitoraggio in tempo reale e il report
      aggregato delle operazioni batch.
- **File Modificati:**
    - `src/desktop/viewmodels/workspace_session_viewmodel.hpp` e `.cpp`: integrazione del watcher service, auto-refresh
      reattivo e gestione thread-safe dei batch via `QMetaObject::invokeMethod`.
    - `src/desktop/bootstrap/app_bootstrap.hpp` e `.cpp`: composizione dei service di Fase 5 (watcher, task runner,
      batch coordinator) e registrazione in QML.
    - `src/desktop/ui/qml/views/HomeHubView.qml`: aggiunta delle azioni di fetch su gruppi e workspace e integrazione
      del modal di progresso.
    - `src/desktop/ui/qml/views/WorkspaceSessionView.qml`: integrazione dell'indicatore di attività in background nella
      toolbar e del modal di progresso.
    - `CMakeLists.txt`: inclusione dei nuovi file sorgenti e QML.
    - `tests/unit/viewmodels_unit_tests.cpp`: test unitari per l'auto-refresh reattivo su `WorkspaceSessionViewModel` e
      l'esecuzione/reportistica di `BatchOperationsViewModel`.
- **Metriche Test:**
    - 100% test passati su `linux-qt-release` (3/3 test suites).
    - 100% test passati su `linux-core-release` (2/2 test suites).
    - Smoke test headless `./build/qt-linux/git_flux --test-startup` superato con successo.
    - Zero warning di compilazione con `-Wall -Wextra -Wpedantic -Werror`.

---
