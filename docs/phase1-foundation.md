# Git Flux — Specifica e Stato di Avanzamento Fase 1 (Fondamenta della V1)

> **Stato:** in corso — completati Task 1.1, Task 1.2 e Task 1.3  
> **Architettura di riferimento:** [plan.md](plan.md) (Sezioni 3, 6, 8)  
> **Wireframe UI di riferimento:** [`docs/ui/git-flux-ui-v2.drawio`](ui/git-flux-ui-v2.drawio) (Pagine 01, 02, 03, 10)  

---

## 1. Suddivisione dei Task Logici di Fase 1

La Fase 1 costruisce le fondamenta del catalogo, dei gruppi, dei workspace e della persistenza locale secondo i principi di **Clean Architecture pragmatica** e **Zero-Qt Core**:

| Task | Area / Modulo | Descrizione | Stato |
| :--- | :--- | :--- | :---: |
| **1.1** | **Core Domain (BO) & Ports** | Definizione entità tipizzate (`Repository`, `Group`, `Workspace`, `WorkspaceSession`), ID sicuri (`GroupId`, `WorkspaceId`) e contratti di storage (`IRepositoryStore`, `IGroupStore`, `IWorkspaceStore`). | **Completato** |
| **1.2** | **Persistenza SQLite (DAL/DML)** | Schema relazionale con foreign key e vincoli, migrazioni versionate, connessioni RAII con WAL mode, implementazione concreta degli Store SQLite Zero-Qt e suite di test DB. | **Completato** |
| **1.3** | **Application Services (BL)** | Use case: `RepositoryService` (validazione `.git`, missing repo, update path), `GroupService` (membership, deduplicazione per Quick Open), `WorkspaceService` (composizione e ripristino sessione). | **Completato** |
| **1.4** | **Desktop Bootstrap & MVVM UI** | Composition Root desktop, ViewModels (`HomeHubViewModel`, `QuickOpenViewModel`), modelli `QAbstractListModel` e prima schermata funzionante della Home a 3 colonne (Pagina 02). | *Prossimo* |

---

## 2. Dettaglio Task 1.1 — Core Domain & Ports

### Entità di Business (BO)
- **`RepositoryId` & `Repository`** (`src/core/modules/repositories/bo/`):  
  Identificativo fortemente tipizzato tramite `StrongId`. L'entità memorizza ID, nome visualizzato, percorso su filesystem (`std::filesystem::path`), stato del percorso (`Valid`, `Missing`, `NotAGitRepository`) e timestamp di registrazione/accesso.
- **`GroupId` & `Group`** (`src/core/modules/groups/bo/`):  
  Raccolta logica di repository ordinati (`std::vector<RepositoryId>`). Include metodi invarianti come `contains()` e validazione dell'entità.
- **`WorkspaceId` & `Workspace`** (`src/core/modules/workspaces/bo/`):  
  Ambiente di lavoro persistente che supporta sia gruppi inclusi (`std::vector<GroupId>`) sia repository diretti (`std::vector<RepositoryId>`), mantenendo l'ordine e l'integrità referenziale.
- **`WorkspaceSession` & `PanelLayoutState`** (`src/core/modules/workspaces/bo/`):  
  Stato di sessione per-workspace: versione dello schema (default `1`), elenco dei repository aperti (tab), repository attivo selezionato, stato dei pannelli (collassamento e larghezze della sidebar sinistra/destra, vista attiva) e payload JSON opaco per filtri addizionali.

### Contratti di Storage (Ports)
- **`IRepositoryStore`** (`src/core/modules/repositories/ports/`): `save`, `getById`, `getByPath`, `getAll`, `remove`, `updatePath`.
- **`IGroupStore`** (`src/core/modules/groups/ports/`): `save`, `getById`, `getAll`, `remove`, `getGroupsContainingRepository`.
- **`IWorkspaceStore`** (`src/core/modules/workspaces/ports/`): `save`, `getById`, `getAll`, `remove`, `saveSession`, `getSession`.

Tutti i contratti restituiscono `std::expected<T, StoreError>` con errori fortemente tipizzati (`NotFound`, `DuplicateId`, `DuplicatePath`/`DuplicateName`, `StorageError`).

---

## 3. Test di Correttezza (Task 1.1)

Test implementati in [`tests/unit/domain_entities_unit_tests.cpp`](../tests/unit/domain_entities_unit_tests.cpp):
1. Type-safety assoluta di `StrongId`: `RepositoryId`, `GroupId` e `WorkspaceId` non sono implicitamente convertibili né scambiabili tra loro (`STATIC_REQUIRE_FALSE`).
2. Validazione e comparazione dell'entità `Repository` (campi obbligatori e gestione percorsi vuoti o mancanti).
3. Preservazione dell'ordine e appartenenza dei repository nell'entità `Group`.
4. Composizione multi-gruppo e repo diretti nell'entità `Workspace`.
5. Serializzazione e validità dello stato layout/sessione in `WorkspaceSession`.

Risultato suite unit test: **62 asserzioni superate in 10 test case** (0 fallimenti).

---

## 4. Dettaglio Task 1.2 — Persistenza SQLite (DAL/DML)

### Connessione, Transazioni e Migrazioni
- **`SqliteConnection` & `SqliteStatement`** (`src/infrastructure/persistence/sqlite/connection/`):  
  Wrapper RAII a zero allocazioni superflue per `sqlite3*` e `sqlite3_stmt*`. Supporta database su file e `:memory:`, abilita sistematicamente `PRAGMA foreign_keys = ON;`, `busy_timeout = 5000ms`, `WAL mode` e `synchronous = NORMAL`.
- **`SqliteTransaction`**:  
  Gestore RAII delle transazioni atomiche: esegue `BEGIN IMMEDIATE TRANSACTION;` all'avvio e garantisce il rollback automatico nel distruttore se non esplicitamente confermata con `commit()`.
- **`SchemaMigrator`** (`src/infrastructure/persistence/sqlite/migrations/`):  
  Gestione versionata dello schema tramite tabella `schema_migrations`.  
  **Migration V1:** creazione delle tabelle `repositories`, `groups`, `group_repositories` (con indice su `sort_order` e `repository_id`), `workspaces`, `workspace_groups`, `workspace_repositories` e `workspace_sessions` (con foreign key `ON DELETE CASCADE` su join e sessioni).

### Implementazione Concreta degli Store (DML)
- **`SqliteRepositoryStore`** (`src/infrastructure/persistence/repositories/`):  
  Implementa `IRepositoryStore`. Upsert atomico, lookup per ID o percorso su filesystem (con gestione corretta UTF-8), prevenzione percorsi duplicati (`DuplicatePath`) e aggiornamento atomico del percorso (`updatePath`).
- **`SqliteGroupStore`** (`src/infrastructure/persistence/groups/`):  
  Implementa `IGroupStore`. Gestione atomica transazionale di testata e join `group_repositories`, preservazione dell'ordine dei repository, lookup inverso dei gruppi che contengono un dato repository e pulizia a cascata.
- **`SqliteWorkspaceStore`** (`src/infrastructure/persistence/workspaces/`):  
  Implementa `IWorkspaceStore`. Composizione di gruppi e repository diretti, persistenza e ripristino di `WorkspaceSession` (tab aperte, repository attivo, stato layout, dimensioni pannelli e payload JSON di filtri).

### Test di Integrazione Persistence
Test implementati in [`tests/integration/sqlite_stores_integration_tests.cpp`](../tests/integration/sqlite_stores_integration_tests.cpp):
1. Inizializzazione database in memoria, verifica versione schema 1 e verifica di idempotenza delle migrazioni.
2. Verifica atomicità transazionale: commit esplicito vs rollback automatico in caso di errore o uscita anticipata.
3. CRUD del catalogo repository, percorsi Unicode complessi e collisione vincolo UNIQUE su path.
4. Preservazione dell'ordinamento dei repository nel gruppo e pulizia automatica in cascata alla rimozione del repository dal catalogo.
5. Composizione workspace, salvataggio e recupero completo della `WorkspaceSession`, e cancellazione in cascata della sessione all'eliminazione del workspace.

Risultato suite integration test: **240 asserzioni superate in 8 test case su core-linux** e **304 asserzioni superate in 8 test case su qt-linux** (0 fallimenti).

---

## 5. Dettaglio Task 1.3 — Application Services / Business Logic (BL)

### Servizi Applicativi del Core
- **`RepositoryService`** (`src/core/modules/repositories/bl/`):
  - Verifica che il percorso esista su filesystem e contenga una cartella `.git` valida (inclusi worktree e submodule con `.git` file).
  - Normalizzazione del percorso (`weakly_canonical`), calcolo automatico del `displayName` da folder name se omesso.
  - Generazione di `RepositoryId` univoci e stabili (`repo_<timestamp>_<random>`).
  - Prevenzione duplicazione percorsi (`AlreadyRegistered`).
  - Rilevamento dello stato del repository su disco (`checkRepositoryStatus`: `Valid`, `Missing`, `NotAGitRepository`).
  - Riconnessione atomica del percorso (`reconnectRepositoryPath`) preservando inalterato il `RepositoryId` (fondamentale per la Pagina 15 dei wireframe: i gruppi e i workspace rimangono intatti).
  - Tracciamento data ultimo accesso (`touchLastAccessed`) e de-registrazione dal catalogo (`unregisterRepository`).

- **`GroupService`** (`src/core/modules/groups/bl/`):
  - Creazione e rinomina gruppi con validazione del nome e prevenzione duplicati (`DuplicateName`).
  - Verifica preventiva che i `RepositoryId` assegnati al gruppo esistano nel catalogo (`RepositoryNotFound`).
  - Modifica incrementale della membership: aggiunta, rimozione e riordinamento esplicito dei repository.
  - **Algoritmo di Deduplicazione per Quick Open (`resolveQuickOpenContext`):**  
    Data una selezione arbitraria di gruppi, raccoglie e **deduplica in modo deterministico** i repository preservando l'ordine di prima apparizione. Risolve esattamente il requisito di Pagina 03 del wireframe (un repository condiviso tra più gruppi compare una sola volta nel contesto volatile).

- **`WorkspaceService`** (`src/core/modules/workspaces/bl/`):
  - Creazione workspace con composizione mista (gruppi inclusi + repository diretti).
  - Verifica dell'integrità referenziale di gruppi e repository diretti inclusi.
  - Inizializzazione automatica della prima `WorkspaceSession` (imposta la prima tab aperta e il repository attivo se presenti).
  - **Risoluzione completa dei repository nel workspace (`resolveWorkspaceRepositories`):**  
    Aggrega i repository provenienti da tutti i gruppi inclusi assieme ai repository diretti ed applica la deduplicazione deterministica.
  - Gestione del ciclo di vita della sessione: `loadOrCreateSession` e `saveSession` (stato pannelli, tab e filtri).

### Test di Unità Business Logic
Test implementati in [`tests/unit/services_unit_tests.cpp`](../tests/unit/services_unit_tests.cpp):
1. **RepositoryService:** percorsi non validi (`InvalidPath`, `PathNotFound`, `NotAGitRepository`), registrazione corretta con display name calcolato, rilevamento duplicato, transizione a `Missing` e riconnessione percorso con preservazione di `RepositoryId`.
2. **GroupService:** validazione nome, collisione duplicati, manipolazione membri, e verifica rigorosa dell'algoritmo di deduplicazione per Quick Open su 2 gruppi con repository condivisi.
3. **WorkspaceService:** composizione gruppi + repo diretti, deduplicazione deterministica dei repository effettivi nel workspace, inizializzazione automatica della sessione, persistenza delle modifiche al layout (sidebar, viste) ed eliminazione con pulizia a cascata.

Risultato suite unit test: **128 asserzioni superate in 13 test case** (0 fallimenti).
