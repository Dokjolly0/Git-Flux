# Git Flux — Specifica e Stato di Avanzamento Fase 4 (Stashing, Rebase & Advanced Conflict Resolution)

> **Stato:** Completato Task 4.3 — 3-Way Conflict Resolver Engine | Prossimo: Task 4.4 — Stash Manager & 3-Way Conflict Resolver UI  
> **Architettura di riferimento:** [plan.md](plan.md) (Sezioni 2.1, 7, 8, 15)  
> **Wireframe UI di riferimento:** [`docs/ui/git-flux-ui-v2.drawio`](ui/git-flux-ui-v2.drawio) (Pagine 05, 07)  

---

## 1. Suddivisione dei Task Logici di Fase 4

La Fase 4 completa le capacità avanzate di gestione dell'albero di lavoro, riscrittura locale della storia e risoluzione guidata dei conflitti:

| Task | Area / Modulo | Descrizione | Stato |
| :--- | :--- | :--- | :---: |
| **4.1** | **Stashing Engine** | Domain BO (`GitStashItem`, `StashPushOptions`), port `IRepositoryGitOps`, service `GitStashService` (validazione, per-repo locking), esecuzione nativa `git stash list/push/pop/apply/drop/clear` con parser formattato deterministico. | **Completato** |
| **4.2** | **Rebase & Cherry-Pick Engine** | Domain BO (`RebaseOptions`, `RebaseResult`, `RebaseStatus`, `CherryPickOptions`), port `IRepositoryGitOps`, service `GitRebaseService` con gestione stati in-progress (`rebase --continue/abort/skip`) e cherry-pick atomico. | **Completato** |
| **4.3** | **3-Way Conflict Resolver Engine** | Domain BO (`ConflictFile`, `ConflictHunk`, `ConflictSide`, `ApplyConflictResolutionOptions`), estrazione precisa degli stage 1 (base), 2 (ours), 3 (theirs) tramite `git show :stage:path` o marker di conflitto, applicazione risoluzioni e staging atomico. | **Completato** |
| **4.4** | **Stash Manager & 3-Way Conflict Resolver UI** | ViewModels MVVM (`StashListModel`, `ConflictResolverViewModel`) e viste QML (`StashManagerModal`, `ConflictResolverModal`). | *In coda* |

---

## 2. Dettaglio Task 4.1 — Stashing Engine

### Obiettivi e Principi Architetturali
- **Zero-Qt nel Core:** Modelli BO puri e service applicativo privi di dipendenze da librerie esterne o Qt.
- **Parsing Deterministico di `git stash list`:**
  Utilizzo del formato delimitato per estrarre con certezza indice (`stash@{0}`), hash del commit dello stash, branch di origine, data ISO e messaggio:
  `git stash list --format="%gd%x00%H%x00%aI%x00%gs%x1e"`
  - `%gd`: selettore stash (es. `stash@{0}`)
  - `%H`: commit SHA dello stash
  - `%aI`: data ISO 8601
  - `%gs`: messaggio del reflog dello stash (es. `WIP on main: 1a2b3c4 Commit message` o custom message)
  - `0x1E`: record separator
- **Opzioni Flessibili di Stash Push:**
  - `includeUntracked` (`-u`)
  - `keepIndex` (`--keep-index`)
  - `message` opzionale
  - `paths` opzionali per stash parziale di file specifici (`git stash push -m "msg" -- <paths>`)
- **Operazioni sullo Stash:**
  - `listStashes`: elenca gli stash esistenti ordinati per indice.
  - `stashPush`: salva lo stato attuale del working tree/index.
  - `stashPop`: applica e rimuove uno stash specifico (default `stash@{0}` o indice numerico).
  - `stashApply`: applica uno stash senza rimuoverlo.
  - `stashDrop`: rimuove uno stash specifico.
  - `stashClear`: rimuove tutti gli stash del repository.
- **Error Mapping Tipizzato:**
  - `StashNotFound`
  - `NothingToStash`
  - `DirtyWorkingTree` / `ConflictPresent` su pop/apply con conflitti
- **Protezione Concorrenza:**
  Tutte le mutazioni (`stashPush`, `stashPop`, `stashApply`, `stashDrop`, `stashClear`) protette da `RepositoryLockCoordinator`.

### Risultati del Task 4.1
- **File Creati:**
  - `src/core/modules/git/bo/stash_models.hpp`: modelli di dominio puri (`GitStashItem`, `StashPushOptions`, `StashApplyOptions`).
  - `src/core/modules/git/bl/git_stash_service.hpp` e `.cpp`: service applicativo di stashing (validazione percorsi, coordinamento locking atomico).
  - `tests/unit/stash_unit_tests.cpp`: test unitari isolati con mock (46 asserzioni Catch2).
  - `tests/integration/stash_integration_tests.cpp`: test di integrazione su repository Git reale (59 asserzioni Catch2).
- **File Modificati:**
  - `src/core/modules/git/ports/repository_git_ops.hpp`: estensione port con errori `NothingToStash`, `StashNotFound` e 6 metodi virtuali puri di stash.
  - `src/infrastructure/git/native/native_repository_git_ops.hpp` e `.cpp`: implementazione comandi nativi git stash con parsing delimitato e gestione errori bilingue (EN/IT).
  - `CMakeLists.txt`: registrazione di `git_stash_service.cpp`, `stash_unit_tests.cpp`, `stash_integration_tests.cpp`.
- **Metriche Test:** 105 asserzioni superate (46 unitari + 59 integrazione). Zero errori, zero warning con `-Werror`.

---

## 3. Dettaglio Task 4.2 — Rebase & Cherry-Pick Engine

### Obiettivi e Principi Architetturali
- **Zero-Qt nel Core:** Modelli BO puri (`RebaseStatus`, `RebaseOptions`, `RebaseResult`, `CherryPickOptions`, `CherryPickResult`) e service applicativo `GitRebaseService` privi di dipendenze Qt.
- **Rilevamento Rebase In-Progress O(1):**
  Rilevamento immediato dell'esistenza di `.git/rebase-merge` o `.git/rebase-apply` senza invocazione di sottoprocessi lenti.
- **Telemetria di Progresso Dettagliata:**
  Estrazione automatica dello step corrente (`msgnum`), del numero totale di commit da applicare (`end`) e dell'hash del commit interrotto (`stopped-sha`) direttamente dallo stato di Git in caso di conflitto.
- **Isolamento Editor Headless:**
  Configurazione centralizzata dell'ambiente di esecuzione con `GIT_EDITOR=true` e passaggio di `-c core.editor=true` per impedire blocchi interattivi su rebase continue/skip e cherry-pick.
- **Operazioni Implementate:**
  - `startRebase`: avvia rebase con opzioni `upstream`, `onto`, `branch`, `autostash`.
  - `continueRebase`: prosegue il rebase dopo la risoluzione dei conflitti.
  - `skipRebase`: salta il commit corrente.
  - `abortRebase`: annulla la sessione di rebase e ripristina lo stato precedente.
  - `isRebaseInProgress`: verifica stato attivo.
  - `cherryPick`: applica un commit singolo (`--no-commit` opzionale).
  - `abortCherryPick`: annulla il cherry-pick in conflitto.
- **Protezione Concorrenza:**
  Tutte le mutazioni protette da `RepositoryLockCoordinator` per-repo.

### Risultati del Task 4.2
- **File Creati:**
  - `src/core/modules/git/bo/rebase_models.hpp`: modelli di dominio puri per rebase e cherry-pick.
  - `src/core/modules/git/bl/git_rebase_service.hpp` e `.cpp`: service applicativo di rebase & cherry-pick.
  - `tests/unit/rebase_unit_tests.cpp`: test unitari isolati con mock (52 asserzioni Catch2).
  - `tests/integration/rebase_integration_tests.cpp`: test di integrazione su repository Git reale (83 asserzioni Catch2).
- **File Modificati:**
  - `src/core/modules/git/ports/repository_git_ops.hpp`: estensione port con errori `NoRebaseInProgress`, `CherryPickConflict`, `CommitNotFound` e 7 metodi virtuali puri.
  - `src/infrastructure/git/native/native_repository_git_ops.hpp` e `.cpp`: implementazione nativa dei 7 metodi con telemetria e gestione conflitti.
  - `src/infrastructure/git/native/native_git_executor.cpp`: configurazione centralizzata dell'ambiente Linux (filtraggio e forzatura `GIT_EDITOR=true`).
  - `src/infrastructure/git/qt/qt_git_executor.cpp`: allineamento variabili ambiente.
  - `CMakeLists.txt`: registrazione di `git_rebase_service.cpp`, `rebase_unit_tests.cpp`, `rebase_integration_tests.cpp`.
- **Metriche Test:** 135 asserzioni superate (52 unitari + 83 integrazione). Zero errori, zero warning con `-Werror`.

---

## 4. Dettaglio Task 4.3 — 3-Way Conflict Resolver Engine

### Obiettivi e Principi Architetturali
- **Zero-Qt nel Core:** Modelli BO puri (`ConflictSide`, `ConflictHunk`, `ConflictFile`, `ApplyConflictResolutionOptions`), parser marker `ConflictMarkerParser` e service `GitConflictService` rigorosamente separati da Qt.
- **Parser Marker Robusto e Flessibile:**
  - Parsing a stati per rilevare marker `<<<<<<<`, `|||||||` (diff3 style), `=======`, `>>>>>>>`.
  - Estrazione multilinea preservando il contesto intermedio e i terminatori di riga.
  - Ricostruzione deterministica del file finale sostituendo ogni hunk con la relativa risoluzione (Base, Ours, Theirs o Custom).
- **Estrazione Granulare degli Stage di Git:**
  - `git show :1:<path>`: comune antenato (Base).
  - `git show :2:<path>`: ramo corrente (Ours).
  - `git show :3:<path>`: ramo in arrivo (Theirs).
  - Fallback automatico per file senza diff3 nel working tree arricchendo i blocchi mancanti.
- **Applicazione Risoluzione Atomica:**
  - Scrittura del contenuto unificato sul filesystem.
  - Staging automatico opzionale (`git add -- <path>`) per marcare il conflitto come risolto.
  - Validazione che lo status del working tree passi da `Conflicted` a staged `Modified`.
- **Protezione Concorrenza:**
  Mutazioni protette con lock mutex per-repository via `RepositoryLockCoordinator`.

### Risultati del Task 4.3
- **File Creati:**
  - `src/core/modules/git/bo/conflict_models.hpp`: modelli di dominio per conflitti, hunk e opzioni di risoluzione.
  - `src/core/modules/git/bl/conflict_marker_parser.hpp` e `.cpp`: parser a stati puri e ricostruttore di file risolti.
  - `src/core/modules/git/bl/git_conflict_service.hpp` e `.cpp`: service applicativo di risoluzione conflitti.
  - `tests/unit/conflict_unit_tests.cpp`: test unitari del parser e del service con mock (55 asserzioni Catch2).
  - `tests/integration/conflict_integration_tests.cpp`: test di integrazione su repository Git reale con merge divergente in conflitto (38 asserzioni Catch2).
- **File Modificati:**
  - `src/core/modules/git/ports/repository_git_ops.hpp`: estensione port con i metodi `getConflictFile`, `getFileStageContent`, `applyConflictResolution`.
  - `src/infrastructure/git/native/native_repository_git_ops.hpp` e `.cpp`: implementazione comandi nativi `git show :stage:path`, estrazione file conflitto e salvataggio/staging atomico.
  - `CMakeLists.txt`: registrazione di `conflict_marker_parser.cpp`, `git_conflict_service.cpp`, `conflict_unit_tests.cpp`, `conflict_integration_tests.cpp`.
- **Metriche Test:** 93 asserzioni superate (55 unitari + 38 integrazione). Zero errori, zero warning con `-Werror`.



