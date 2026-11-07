# Git Flux — Specifica e Stato di Avanzamento Fase 1 (Fondamenta della V1)

> **Stato:** in corso — completato Task 1.1 (Core Domain BO & Ports)  
> **Architettura di riferimento:** [plan.md](plan.md) (Sezioni 3, 6, 8)  
> **Wireframe UI di riferimento:** [`docs/ui/git-flux-ui-v2.drawio`](ui/git-flux-ui-v2.drawio) (Pagine 01, 02, 03, 10)  

---

## 1. Suddivisione dei Task Logici di Fase 1

La Fase 1 costruisce le fondamenta del catalogo, dei gruppi, dei workspace e della persistenza locale secondo i principi di **Clean Architecture pragmatica** e **Zero-Qt Core**:

| Task | Area / Modulo | Descrizione | Stato |
| :--- | :--- | :--- | :---: |
| **1.1** | **Core Domain (BO) & Ports** | Definizione entità tipizzate (`Repository`, `Group`, `Workspace`, `WorkspaceSession`), ID sicuri (`GroupId`, `WorkspaceId`) e contratti di storage (`IRepositoryStore`, `IGroupStore`, `IWorkspaceStore`). | **Completato** |
| **1.2** | **Persistenza SQLite (DAL/DML)** | Schema relazionale con foreign key e vincoli, migrazioni versionate, connessioni RAII con WAL mode, implementazione concreta degli Store SQLite e suite di test DB. | *Prossimo* |
| **1.3** | **Application Services (BL)** | Use case: `RepositoryService` (validazione `.git`, missing repo, update path), `GroupService` (membership, deduplicazione per Quick Open), `WorkspaceService` (composizione e ripristino sessione). | *In coda* |
| **1.4** | **Desktop Bootstrap & MVVM UI** | Composition Root desktop, ViewModels (`HomeHubViewModel`, `QuickOpenViewModel`), modelli `QAbstractListModel` e prima schermata funzionante della Home a 3 colonne (Pagina 02). | *In coda* |

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
