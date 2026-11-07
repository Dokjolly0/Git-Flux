# Git Flux — Specifica e Stato di Avanzamento Fase 4 (Stashing, Rebase & Advanced Conflict Resolution)

> **Stato:** Completato Task 4.1 — Stashing Engine | Prossimo: Task 4.2 — Rebase & Cherry-Pick Engine  
> **Architettura di riferimento:** [plan.md](plan.md) (Sezioni 2.1, 7, 8, 15)  
> **Wireframe UI di riferimento:** [`docs/ui/git-flux-ui-v2.drawio`](ui/git-flux-ui-v2.drawio) (Pagine 05, 07)  

---

## 1. Suddivisione dei Task Logici di Fase 4

La Fase 4 completa le capacità avanzate di gestione dell'albero di lavoro, riscrittura locale della storia e risoluzione guidata dei conflitti:

| Task | Area / Modulo | Descrizione | Stato |
| :--- | :--- | :--- | :---: |
| **4.1** | **Stashing Engine** | Domain BO (`GitStashItem`, `StashPushOptions`), port `IRepositoryGitOps`, service `GitStashService` (validazione, per-repo locking), esecuzione nativa `git stash list/push/pop/apply/drop/clear` con parser formattato deterministico. | **Completato** |
| **4.2** | **Rebase & Cherry-Pick Engine** | Domain BO (`RebaseOptions`, `RebaseResult`, `RebaseStatus`, `CherryPickOptions`), port `IRepositoryGitOps`, service `GitRebaseService` con gestione stati in-progress (`rebase --continue/abort/skip`) e cherry-pick atomico. | *In coda* |
| **4.3** | **3-Way Conflict Resolver Engine** | Domain BO (`ConflictFile`, `ConflictHunk`, `ConflictSide`, `ConflictResolution`), estrazione precisa degli stage 1 (base), 2 (ours), 3 (theirs) tramite `git show :stage:path` o marker di conflitto, applicazione risoluzioni e staging atomico. | *In coda* |
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

