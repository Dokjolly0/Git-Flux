# Git Flux — Specifica e Stato di Avanzamento Fase 1 (Fondamenta della V1)

> **Stato:** completata Fase 1 (Task 1.1, 1.2, 1.3, 1.4)  
> **Architettura di riferimento:** [plan.md](plan.md) (Sezioni 3, 6, 8)  
> **Wireframe UI di riferimento:** [`docs/ui/git-flux-ui-v2.drawio`](ui/git-flux-ui-v2.drawio) (Pagine 01, 02, 03, 10)

---

## 1. Suddivisione dei Task Logici di Fase 1

La Fase 1 costruisce le fondamenta del catalogo, dei gruppi, dei workspace e della persistenza locale secondo i principi
di **Clean Architecture pragmatica** e **Zero-Qt Core**:

| Task    | Area / Modulo                    | Descrizione                                                                                                                                                                                                     |     Stato      |
|:--------|:---------------------------------|:----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|:--------------:|
| **1.1** | **Core Domain (BO) & Ports**     | Definizione entità tipizzate (`Repository`, `Group`, `Workspace`, `WorkspaceSession`), ID sicuri (`GroupId`, `WorkspaceId`) e contratti di storage (`IRepositoryStore`, `IGroupStore`, `IWorkspaceStore`).      | **Completato** |
| **1.2** | **Persistenza SQLite (DAL/DML)** | Schema relazionale con foreign key e vincoli, migrazioni versionate, connessioni RAII con WAL mode, implementazione concreta degli Store SQLite Zero-Qt e suite di test DB.                                     | **Completato** |
| **1.3** | **Application Services (BL)**    | Use case: `RepositoryService` (validazione `.git`, missing repo, update path), `GroupService` (membership, deduplicazione per Quick Open), `WorkspaceService` (composizione e ripristino sessione).             | **Completato** |
| **1.4** | **Desktop Bootstrap & MVVM UI**  | Composition Root desktop, ViewModels (`HomeHubViewModel`, `QuickOpenViewModel`), modelli `QAbstractListModel` e prima schermata funzionante della Home a 3 colonne (Pagina 02) e Modale Quick Open (Pagina 03). | **Completato** |

---

## 2. Dettaglio Task 1.1 — Core Domain & Ports

### Entità di Business (BO)

- **`RepositoryId` & `Repository`** (`src/core/modules/repositories/bo/`):  
  Identificativo fortemente tipizzato tramite `StrongId`. L'entità memorizza ID, nome visualizzato, percorso su
  filesystem (`std::filesystem::path`), stato del percorso (`Valid`, `Missing`, `NotAGitRepository`) e timestamp di
  registrazione/accesso.
- **`GroupId` & `Group`** (`src/core/modules/groups/bo/`):  
  Raccolta logica di repository ordinati (`std::vector<RepositoryId>`). Include metodi invarianti come `contains()` e
  validazione dell'entità.
- **`WorkspaceId` & `Workspace`** (`src/core/modules/workspaces/bo/`):  
  Ambiente di lavoro persistente che supporta sia gruppi inclusi (`std::vector<GroupId>`) sia repository diretti
  (`std::vector<RepositoryId>`), mantenendo l'ordine e l'integrità referenziale.
- **`WorkspaceSession` & `PanelLayoutState`** (`src/core/modules/workspaces/bo/`):  
  Stato di sessione per-workspace: versione dello schema (default `1`), elenco dei repository aperti (tab), repository
  attivo selezionato, stato dei pannelli (collassamento e larghezze della sidebar sinistra/destra, vista attiva) e
  payload JSON opaco per filtri addizionali.

### Contratti di Storage (Ports)

- **`IRepositoryStore`** (`src/core/modules/repositories/ports/`): `save`, `getById`, `getByPath`, `getAll`, `remove`,
  `updatePath`.
- **`IGroupStore`** (`src/core/modules/groups/ports/`): `save`, `getById`, `getAll`, `remove`,
  `getGroupsContainingRepository`.
- **`IWorkspaceStore`** (`src/core/modules/workspaces/ports/`): `save`, `getById`, `getAll`, `remove`, `saveSession`,
  `getSession`.

Tutti i contratti restituiscono `std::expected<T, StoreError>` con errori fortemente tipizzati (`NotFound`,
`DuplicateId`, `DuplicatePath`/`DuplicateName`, `StorageError`).

---

## 3. Test di Correttezza (Task 1.1)

Test implementati in [`tests/unit/domain_entities_unit_tests.cpp`](../tests/unit/domain_entities_unit_tests.cpp):

1. Type-safety assoluta di `StrongId`: `RepositoryId`, `GroupId` e `WorkspaceId` non sono implicitamente convertibili né
   scambiabili tra loro (`STATIC_REQUIRE_FALSE`).
2. Validazione e comparazione dell'entità `Repository` (campi obbligatori e gestione percorsi vuoti o mancanti).
3. Preservazione dell'ordine e appartenenza dei repository nell'entità `Group`.
4. Composizione multi-gruppo e repo diretti nell'entità `Workspace`.
5. Serializzazione e validità dello stato layout/sessione in `WorkspaceSession`.

Risultato suite unit test: **62 asserzioni superate in 10 test case** (0 fallimenti).

---

## 4. Dettaglio Task 1.2 — Persistenza SQLite (DAL/DML)

### Connessione, Transazioni e Migrazioni

- **`SqliteConnection` & `SqliteStatement`** (`src/infrastructure/persistence/sqlite/connection/`):  
  Wrapper RAII a zero allocazioni superflue per `sqlite3*` e `sqlite3_stmt*`. Supporta database su file e `:memory:`,
  abilita sistematicamente `PRAGMA foreign_keys = ON;`, `busy_timeout = 5000ms`, `WAL mode` e `synchronous = NORMAL`.
- **`SqliteTransaction`**:  
  Gestore RAII delle transazioni atomiche: esegue `BEGIN IMMEDIATE TRANSACTION;` all'avvio e garantisce il rollback
  automatico nel distruttore se non esplicitamente confermata con `commit()`.
- **`SchemaMigrator`** (`src/infrastructure/persistence/sqlite/migrations/`):  
  Gestione versionata dello schema tramite tabella `schema_migrations`.  
  **Migration V1:** creazione delle tabelle `repositories`, `groups`, `group_repositories` (con indice su `sort_order` e
  `repository_id`), `workspaces`, `workspace_groups`, `workspace_repositories` e `workspace_sessions` (con foreign key
  `ON DELETE CASCADE` su join e sessioni).

### Implementazione Concreta degli Store (DML)

- **`SqliteRepositoryStore`** (`src/infrastructure/persistence/repositories/`):  
  Implementa `IRepositoryStore`. Upsert atomico, lookup per ID o percorso su filesystem (con gestione corretta UTF-8),
  prevenzione percorsi duplicati (`DuplicatePath`) e aggiornamento atomico del percorso (`updatePath`).
- **`SqliteGroupStore`** (`src/infrastructure/persistence/groups/`):  
  Implementa `IGroupStore`. Gestione atomica transazionale di testata e join `group_repositories`, preservazione
  dell'ordine dei repository, lookup inverso dei gruppi che contengono un dato repository e pulizia a cascata.
- **`SqliteWorkspaceStore`** (`src/infrastructure/persistence/workspaces/`):  
  Implementa `IWorkspaceStore`. Composizione di gruppi e repository diretti, persistenza e ripristino di
  `WorkspaceSession` (tab aperte, repository attivo, stato layout, dimensioni pannelli e payload JSON di filtri).

### Test di Integrazione Persistence

Test implementati in [
`tests/integration/sqlite_stores_integration_tests.cpp`](../tests/integration/sqlite_stores_integration_tests.cpp):

1. Inizializzazione database in memoria, verifica versione schema 1 e verifica di idempotenza delle migrazioni.
2. Verifica atomicità transazionale: commit esplicito vs rollback automatico in caso di errore o uscita anticipata.
3. CRUD del catalogo repository, percorsi Unicode complessi e collisione vincolo UNIQUE su path.
4. Preservazione dell'ordinamento dei repository nel gruppo e pulizia automatica in cascata alla rimozione del
   repository dal catalogo.
5. Composizione workspace, salvataggio e recupero completo della `WorkspaceSession`, e cancellazione in cascata della
   sessione all'eliminazione del workspace.

Risultato suite integration test: **240 asserzioni superate in 8 test case su core-linux** e **304 asserzioni superate
in 8 test case su qt-linux** (0 fallimenti).

---

## 5. Dettaglio Task 1.3 — Application Services / Business Logic (BL)

### Servizi Applicativi del Core

- **`RepositoryService`** (`src/core/modules/repositories/bl/`):
    - Verifica che il percorso esista su filesystem e contenga una cartella `.git` valida (inclusi worktree e submodule
      con `.git` file).
    - Normalizzazione del percorso (`weakly_canonical`), calcolo automatico del `displayName` da folder name se omesso.
    - Generazione di `RepositoryId` univoci e stabili (`repo_<timestamp>_<random>`).
    - Prevenzione duplicazione percorsi (`AlreadyRegistered`).
    - Rilevamento dello stato del repository su disco (`checkRepositoryStatus`: `Valid`, `Missing`,
      `NotAGitRepository`).
    - Riconnessione atomica del percorso (`reconnectRepositoryPath`) preservando inalterato il `RepositoryId`
      (fondamentale per la Pagina 15 dei wireframe: i gruppi e i workspace rimangono intatti).
    - Tracciamento data ultimo accesso (`touchLastAccessed`) e de-registrazione dal catalogo (`unregisterRepository`).

- **`GroupService`** (`src/core/modules/groups/bl/`):
    - Creazione e rinomina gruppi con validazione del nome e prevenzione duplicati (`DuplicateName`).
    - Verifica preventiva che i `RepositoryId` assegnati al gruppo esistano nel catalogo (`RepositoryNotFound`).
    - Modifica incrementale della membership: aggiunta, rimozione e riordinamento esplicito dei repository.
    - **Algoritmo di Deduplicazione per Quick Open (`resolveQuickOpenContext`):**  
      Data una selezione arbitraria di gruppi, raccoglie e **deduplica in modo deterministico** i repository preservando
      l'ordine di prima apparizione. Risolve esattamente il requisito di Pagina 03 del wireframe (un repository
      condiviso tra più gruppi compare una sola volta nel contesto volatile).

- **`WorkspaceService`** (`src/core/modules/workspaces/bl/`):
    - Creazione workspace con composizione mista (gruppi inclusi + repository diretti).
    - Verifica dell'integrità referenziale di gruppi e repository diretti inclusi.
    - Inizializzazione automatica della prima `WorkspaceSession` (imposta la prima tab aperta e il repository attivo se
      presenti).
    - **Risoluzione completa dei repository nel workspace (`resolveWorkspaceRepositories`):**  
      Aggrega i repository provenienti da tutti i gruppi inclusi assieme ai repository diretti ed applica la
      deduplicazione deterministica.
    - Gestione del ciclo di vita della sessione: `loadOrCreateSession` e `saveSession` (stato pannelli, tab e filtri).

### Test di Unità Business Logic

Test implementati in [`tests/unit/services_unit_tests.cpp`](../tests/unit/services_unit_tests.cpp):

1. **RepositoryService:** percorsi non validi (`InvalidPath`, `PathNotFound`, `NotAGitRepository`), registrazione
   corretta con display name calcolato, rilevamento duplicato, transizione a `Missing` e riconnessione percorso con
   preservazione di `RepositoryId`.
2. **GroupService:** validazione nome, collisione duplicati, manipolazione membri, e verifica rigorosa dell'algoritmo di
   deduplicazione per Quick Open su 2 gruppi con repository condivisi.
3. **WorkspaceService:** composizione gruppi + repo diretti, deduplicazione deterministica dei repository effettivi nel
   workspace, inizializzazione automatica della sessione, persistenza delle modifiche al layout (sidebar, viste) ed
   eliminazione con pulizia a cascata.

Risultato suite unit test: **128 asserzioni superate in 13 test case** (0 fallimenti).

---

## 6. Dettaglio Task 1.4 — Desktop Bootstrap & MVVM UI (Qt Quick 6)

### Architettura Bootstrap & Desktop Composition Root

- **`AppBootstrap`** (`src/desktop/bootstrap/app_bootstrap.hpp` & `.cpp`):
    - Inizializza la persistenza SQLite locale (apertura RAII su file o in-memory `:memory:`).
    - Esegue automaticamente le migrazioni versionate (`SchemaMigrator::migrateToLatest`).
    - Istanzia gli Store concreti del Core (`SqliteRepositoryStore`, `SqliteGroupStore`, `SqliteWorkspaceStore`).
    - Istanzia i Servizi applicativi (`RepositoryService`, `GroupService`, `WorkspaceService`).
    - Istanzia i ViewModels (`HomeHubViewModel`, `QuickOpenViewModel`) e registra i tipi e i contesti QML (`homeHubVm`,
      `quickOpenVm`).
    - Fornisce il metodo statico `defaultDatabasePath()` tramite `QStandardPaths::AppDataLocation`.

### Modelli e ViewModels (Pattern MVVM)

- **Modelli `QAbstractListModel`**:
    - `RepositoryListModel` (`src/desktop/viewmodels/repository_list_model.hpp`): espone ruoli `id`, `displayName`,
      `localPath`, `status`, `isMissing`, `lastAccessedAt`. Supporta aggiornamenti atomici reattivi con `dataChanged`.
    - `GroupListModel` (`src/desktop/viewmodels/group_list_model.hpp`): espone ruoli `id`, `name`, `repositoryCount`,
      `repositoryIds`.
    - `WorkspaceListModel` (`src/desktop/viewmodels/workspace_list_model.hpp`): espone ruoli `id`, `name`, `groupCount`,
      `directRepoCount`, `totalRepoCount`, `summary` (es. *"1 gruppo, 1 repo singolo"*), `lastOpenedAt`.
- **`HomeHubViewModel`** (`src/desktop/viewmodels/home_hub_viewmodel.hpp`):
    - Coordina i dati per la schermata principale a 3 colonne (Pagina 02 dei wireframe).
    - Gestisce la ricerca/filtro reattiva su repository, gruppi e workspace in tempo reale.
    - Espone metodi `Q_INVOKABLE` per la registrazione repository, ricollegamento percorso mancante, rimozione,
      creazione ed eliminazione di gruppi e workspace.
    - Notifica segnali strongly-typed: `repositoryOpened`, `groupOpened`, `workspaceOpened`, `errorOccurred`.
- **`QuickOpenViewModel`** (`src/desktop/viewmodels/quick_open_viewmodel.hpp`):
    - Implementa l'Apertura Rapida multi-gruppo conforme a Pagina 03 dei wireframe.
    - Selezione multipla ordinata di gruppi con calcolo istantaneo del contesto volatile deduplicato tramite
      `GroupService::resolveQuickOpenContext`.
    - Calcola live il conteggio dei duplicati unificati (`deduplicatedCount`).
    - Emittanza del segnale `quickSessionStarted(QStringList repoIds)`.

### Componenti e Viste QML (Qt Quick 6)

- **`FluxTheme.qml`** (`src/desktop/ui/qml/themes/`):
    - Palette dark moderna derivata dal logo di Git Flux: Canvas `#0E1117`, Surface `#161B22`, Card `#21262D`, Bordi
      `#30363D`, Accento Cyan `#00E5FF`, Accento Purple `#7C4DFF`.
- **`HomeHubView.qml`** (`src/desktop/ui/qml/views/`):
    - Top bar con brand, campo di ricerca centrale reattivo e bottoni per "+ Repository", "+ Gruppo", "+ Workspace", "⚡
      Quick Open".
    - SplitView orizzontale a 3 colonne ridimensionabili:
        1. **Repositories:** elenco card, badge valid/missing, pulsante "Ricollega" immediato per repo non trovati,
           rimozione.
        2. **Gruppi:** elenco card con chip conteggio repo e rimozione.
        3. **Workspace:** elenco card con riepilogo composizione gruppi/repo e ultimo accesso.
    - Dialoghi modali integrati per la registrazione repo, riconnessione missing, creazione gruppi e workspace.
    - Status bar inferiore con messaggi di stato ed errori.
- **`QuickOpenModal.qml`** (`src/desktop/ui/qml/views/`):
    - Dialogo modale dark con split interno a due colonne: a sinistra la selezione dei gruppi con checkbox e conteggi, a
      destra l'anteprima in tempo reale dei repository unificati con badge di deduplicazione ("⚡ X duplicati uniti").
- **`main.qml` & `main.cpp`** (`src/desktop/`):
    - ApplicationWindow radice con supporto alla CLI: flag `--db <path>` per percorsi personalizzati e switch
      `--test-startup` per smoke test headless in pipeline CI.

### Test di Unità Desktop MVVM

Test implementati in [`tests/unit/viewmodels_unit_tests.cpp`](../tests/unit/viewmodels_unit_tests.cpp) (utilizzano
Catch2 v3 e `QSignalSpy` su driver offscreen headless):

1. **RepositoryListModel:** verifica popolazione, ruoli, stato missing e aggiornamento live di stato.
2. **GroupListModel:** verifica metadati, ruoli e recupero per ID.
3. **WorkspaceListModel:** verifica composizione, risoluzione conteggio totale repo tramite service e formattazione
   summary.
4. **HomeHubViewModel:** ciclo di vita completo (aggiunta repo, apertura con segnale, creazione gruppo con protezione
   duplicati, creazione workspace, filtraggio della ricerca e cancellazione a cascata).
5. **QuickOpenViewModel:** selezione multi-gruppo con 2 gruppi aventi un repository in comune; verifica della
   deduplicazione corretta (3 repo totali, 1 duplicato unito), emissione di `quickSessionStarted` con lista ID e
   protezione su selezione vuota.

Risultato suite MVVM unit test: **94 asserzioni superate in 5 test case** (0 fallimenti).  
Risultato smoke test headless dell'applicazione desktop (`git_flux --test-startup`): **Avvio ed esecuzione del motore
QML completati con successo (exit code 0)**.
