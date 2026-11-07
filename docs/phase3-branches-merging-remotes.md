# Git Flux — Specifica e Stato di Avanzamento Fase 3 (Branching, Merging & Remote Operations)

> **Stato:** Fase 3 Completata al 100% (Task 3.1, 3.2, 3.3, 3.4)  
> **Architettura di riferimento:** [plan.md](plan.md) (Sezioni 2.1, 7, 8, 15)  
> **Wireframe UI di riferimento:** [`docs/ui/git-flux-ui-v2.drawio`](ui/git-flux-ui-v2.drawio) (Pagine 07, 08)  

---

## 1. Suddivisione dei Task Logici di Fase 3

La Fase 3 estende Git Flux per gestire la topologia del repository (branch, tag, commit history), le operazioni di rete con repository remoti (fetch, pull, push) e la risoluzione dei conflitti:

| Task | Area / Modulo | Descrizione | Stato |
| :--- | :--- | :--- | :---: |
| **3.1** | **Branching & Checkout Engine** | Domain BO (`GitBranch`, `BranchType`, `CreateBranchOptions`, `CheckoutOptions`), port `IRepositoryGitOps`, service `GitBranchService` (con validatore `git-check-ref-format` e locking atomico), implementazione nativa `for-each-ref` e `checkout`. | **Completato** |
| **3.2** | **Tagging & Commit History / Log Engine** | Domain BO (`GitCommitLogItem`, `GitTag`, `TagType`), parser deterministico record separator (`\x1e`/`\0`), service `GitHistoryService` con validazione tag, filtri e paginazione `git log`, supporto tag lightweight e annotated. | **Completato** |
| **3.3** | **Remote Operations & Merge Engine** | Gestione remotes (`listRemotes`, `addRemote`, `removeRemote`, `fetch`, `pull`, `push`), merge automatico (FF e 3-way no-ff), rilevamento conflitti di merge e abort merge atomico. | **Completato** |
| **3.4** | **Branch Manager & Commit Graph UI** | ViewModels MVVM (`BranchListModel`, `CommitHistoryListModel`, `BranchManagerViewModel`), estensione `WorkspaceSessionViewModel`, e viste QML (`BranchSwitcherDropdown`, `BranchManagerModal`, `CommitHistoryPanel`, `WorkspaceSessionView`). | **Completato** |

---

## 2. Dettaglio Task 3.1 — Branching & Checkout Engine (Completato)

### Obiettivi e Principi Architetturali
- **Zero-Qt nel Core:** Entità di dominio e servizi completamente indipendenti da Qt (`gf_domain`, `gf_application`).
- **Validazione Rigida dei Riferimenti Git:** Validazione preventiva dei nomi di branch (`isValidBranchName`) conforme a `git-check-ref-format --branch` (rifiuto di sequenze `..`, `//`, `@{`, caratteri ASCII illegali, spazi, controlli iniziali/finali `.lock`, `-`, `/`, `.`).
- **Ispezione ad Alte Prestazioni:** Utilizzo di `git for-each-ref` con formattazione deterministica delimitata da tabulatori (`%(refname)%09%(objectname)%09%(HEAD)%09%(upstream:short)%09%(upstream:track)`) per estrarre atomicamente nome, commit SHA, stato corrente (HEAD), upstream tracciato e tracking ahead/behind.
- **Protezione Concorrenza:** `RepositoryLockCoordinator` per-repo thread-safe mutex integrato in ogni mutazione branch (`createBranch`, `checkoutBranch`, `deleteBranch`, `renameBranch`).
- **Indipendenza da Locale e Piattaforma:** Error mapping bilingue (italiano e inglese) per intercettare accuratamente conflitti di dirty working tree, branch duplicati, branch correnti non eliminabili o branch non trovati.

### File Introdotti / Modificati
- `src/core/modules/git/bo/branch_models.hpp`: Modelli puri `BranchType`, `GitBranch`, `CreateBranchOptions`, `CheckoutOptions`.
- `src/core/modules/git/ports/repository_git_ops.hpp`: Interfaccia estesa con i metodi `listBranches`, `createBranch`, `checkoutBranch`, `deleteBranch`, `renameBranch`.
- `src/core/modules/git/bl/git_branch_service.hpp` & `.cpp`: Logica di business applicativa, validazione refname preventiva, locking per-repository e filtri `getLocalBranches`, `getRemoteBranches`, `getCurrentBranch`.
- `src/infrastructure/git/native/native_repository_git_ops.hpp` & `.cpp`: Esecuzione CLI nativa di `for-each-ref`, `checkout`, `branch -d/-D`, `branch -m`.
- `tests/unit/branch_unit_tests.cpp`: Unit test esaustivi con mock e validazione nomi (37 asserzioni).
- `tests/integration/branch_integration_tests.cpp`: Test di integrazione su repository Git reale (115 asserzioni).

---

## 3. Dettaglio Task 3.2 — Tagging & Commit History / Log Engine (Completato)

### Obiettivi e Principi Architetturali
- **Parsing Deterministico ad Alte Prestazioni (`CommitLogParser`):** Utilizzo dei delimitatori binari ASCII Record Separator (`0x1E`) tra i commit e NUL (`\0`) tra i singoli metadati di ogni commit. Questo garantisce immunità totale a newline, virgolette, backslash o caratteri speciali presenti nel subject o nel corpo del commit.
- **Supporto Topologico e Multi-Parent:** Estrazione deterministica dei genitori (`parents`) per grafi di commit complessi, merge commit a 2 o più vie, e ordinamento `--topo-order`.
- **Decorazioni di Riferimento (`refDecorations`):** Estrazione automatica e parsing dei tag e branch associati a ciascun commit (`HEAD -> main`, `tag: v1.0.0`, `origin/main`).
- **Gestione Completa dei Tag Git:** Supporto per tag leggeri (lightweight) e annotati (annotated) con messaggi, estrazione del tagger name/email/date, estrazione del commit target sbucciato (`%(*objectname)` vs `%(objectname)`).
- **Validazione e Protezione Concorrenza:** Validazione preventiva dei tag conformi a `git-check-ref-format` e serializzazione delle operazioni di scrittura con `RepositoryLockCoordinator`.

### File Introdotti / Modificati
- `src/core/modules/git/bo/history_models.hpp`: Modelli puri `GitCommitLogItem`, `CommitLogOptions`, `TagType`, `GitTag`, `CreateTagOptions`.
- `src/core/modules/git/ports/repository_git_ops.hpp`: Metodi virtuali puri `getCommitLog`, `listTags`, `createTag`, `deleteTag` ed errori `TagAlreadyExists`, `TagNotFound`, `InvalidTagName`.
- `src/core/modules/git/bl/commit_log_parser.hpp` & `.cpp`: Parser O(N) zero-allocazioni con `std::string_view` per estrazione campi da record NUL/0x1E.
- `src/core/modules/git/bl/git_history_service.hpp` & `.cpp`: Servizio applicativo con validatore tag, coordinate locking e lookup commit singolo (`getCommitDetails`).
- `src/infrastructure/git/native/native_repository_git_ops.hpp` & `.cpp`: Esecuzione CLI nativa di `git log` con formattazione custom e paginazione, `git tag -l` con tab-delimiters, `git tag -a/-m`, `git tag -d`.
- `tests/unit/history_unit_tests.cpp`: Unit test su parsing log, merge commit, validazione tag e deleghe (37 asserzioni).
- `tests/integration/history_integration_tests.cpp`: Test di integrazione su repository Git reale con merge commit a 2 genitori, paginazione, filtri su file, tag lightweight e annotated (115 asserzioni).

---

## 4. Dettaglio Task 3.3 — Remote Operations & Merge Engine (Completato)

### Obiettivi e Principi Architetturali
- **Gestione Flessibile dei Remotes:** Lettura e separazione automatica delle URL di fetch e push via `git remote -v`, validazione dei nomi dei remote, aggiunta e rimozione sicura.
- **Sincronizzazione di Rete Robusta (Fetch, Pull, Push):**
  - `fetchRemote`: supporto opzioni `--prune`, `--tags`, `--force`.
  - `pullRemote`: rilevamento conflitti di merge concorrenti (`MergeConflict`) ed errori di autenticazione (`NetworkAuthenticationFailed`).
  - `pushRemote`: supporto `--set-upstream` (`-u`), `--force-with-lease`, rilevamento rigetti remoti (`RemoteRejected`).
- **Merge Engine Completo con Rilevamento Conflitti:**
  - Classificazione accurata dello stato: `UpToDate`, `FastForward`, `Merged`, `Conflict`, `Failed`.
  - Supporto per merge con `--no-ff` e `--squash`.
  - In caso di conflitto, estrazione atomica dei file bloccati tramite `git diff --name-only --diff-filter=U`.
  - Possibilità di annullamento immediato tramite `abortMerge` (`git merge --abort`) con ripristino verificato dell'albero di lavoro allo stato pre-merge.
- **Concorrenza Thread-Safe:** Tutte le mutazioni remote e di merge sono serializzate su ciascun repository tramite `RepositoryLockCoordinator`.

### File Introdotti / Modificati
- `src/core/modules/git/bo/remote_merge_models.hpp`: Modelli puri `GitRemote`, `RemoteOperationOptions`, `RemoteOperationResult`, `MergeStatus`, `MergeOptions`, `MergeResult`.
- `src/core/modules/git/ports/repository_git_ops.hpp`: Metodi virtuali `listRemotes`, `addRemote`, `removeRemote`, `fetchRemote`, `pullRemote`, `pushRemote`, `mergeBranch`, `abortMerge` ed errori tipizzati `RemoteNotFound`, `RemoteAlreadyExists`, `NetworkAuthenticationFailed`, `RemoteRejected`, `MergeConflict`.
- `src/core/modules/git/bl/git_remote_merge_service.hpp` & `.cpp`: Logica applicativa, validatore `isValidRemoteName`, locking coordinate e deleghe sicure.
- `src/infrastructure/git/native/native_repository_git_ops.hpp` & `.cpp`: Esecuzione CLI nativa di `remote -v/add/remove`, `fetch`, `pull`, `push`, `merge`, `merge --abort` e `diff --diff-filter=U`.
- `tests/unit/remote_merge_unit_tests.cpp`: Unit test su validazione nomi remote, opzioni e deleghe (34 asserzioni).
- `tests/integration/remote_merge_integration_tests.cpp`: Test di integrazione su bare remote reale + 2 repository locali sincronizzati, fast-forward, 3-way merge, rilevamento conflitti e `abortMerge` (99 asserzioni).

---

## 5. Dettaglio Task 3.4 — Branch Manager & Commit Graph UI (Completato)

### Obiettivi e Principi Architetturali
- **Architettura MVVM Separata:**
  - `BranchListModel` (`QAbstractListModel`): espone branch locali e remoti con ruoli `name`, `shortName`, `isCurrent`, `isRemote`, `upstream`, `ahead`, `behind`, `targetSha`.
  - `CommitHistoryListModel` (`QAbstractListModel`): espone l'elenco dei commit con ruoli `sha`, `shortSha`, `subject`, `body`, `authorName`, `authorEmail`, `authorDate`, `decorations`, `parents`.
  - `BranchManagerViewModel` (`QObject`): orchestratore delle operazioni di branch, checkout, merge, abort e remote fetch/pull/push.
- **Integrazione fluida nella Sessione Multi-Repo (`WorkspaceSessionViewModel`):**
  - Coordinamento automatico al cambio di tab o commit riuscito: aggiornamento trasparente del branch corrente, dello stato dirty e della cronologia commit.
- **Componenti UI QML Moderni:**
  - `BranchSwitcherDropdown.qml`: pillola con icona branch, nome branch attivo (o indicazione detached/conflict), popup con ricerca e switch istantaneo, e dialog per la creazione di nuovi branch.
  - `BranchManagerModal.qml`: pannello modale per la gestione avanzata di branch, eliminazione, ridenominazione, merge nel branch attivo, sincronizzazione con remote (Fetch, Pull, Push) e pulsante di emergenza `Abort Merge` in caso di conflitti.
  - `CommitHistoryPanel.qml`: vista della cronologia commit con grafo/bullet dei commit, pillole colorate per i tag e branch di riferimento, e pannello laterale di dettaglio commit (autore, email, data, SHA esteso, genitori, corpo del messaggio).
  - `WorkspaceSessionView.qml`: header arricchito con selettore branch, pulsanti rapidi di rete e toggle di vista tra "Changes" (Working Tree + Diff) e "History" (Commit Graph).

### File Introdotti / Modificati
- `src/desktop/viewmodels/branch_list_model.hpp` & `.cpp`: Modello Qt per lista branch.
- `src/desktop/viewmodels/commit_history_list_model.hpp` & `.cpp`: Modello Qt per commit log.
- `src/desktop/viewmodels/branch_manager_viewmodel.hpp` & `.cpp`: ViewModel di gestione branch e remote.
- `src/desktop/viewmodels/workspace_session_viewmodel.hpp` & `.cpp`: Composizione con `BranchManagerViewModel`.
- `src/desktop/bootstrap/app_bootstrap.hpp` & `.cpp`: Registrazione tipi QML e wiring dei nuovi servizi Git.
- `src/desktop/ui/qml/views/BranchSwitcherDropdown.qml`: Componente QML per selezione e creazione rapida di branch.
- `src/desktop/ui/qml/views/BranchManagerModal.qml`: Finestra di dialogo per gestione completa branch e sincronizzazione remote.
- `src/desktop/ui/qml/views/CommitHistoryPanel.qml`: Vista completa del commit log con dettagli.
- `src/desktop/ui/qml/views/WorkspaceSessionView.qml`: Header arricchito e switch di modalità Changes / History.
- `tests/unit/viewmodels_unit_tests.cpp`: Test unitari MVVM (188 asserzioni Catch2 + QSignalSpy).

### Risultati di Verifica
- **Unit Tests MVVM:** `gf_viewmodels_tests` 100% passed (188 asserzioni).
- **Smoke Test Headless Desktop:** `./build/qt-linux/git_flux --test-startup` completato con codice 0.
- **CTest Suite Completa:** `gf_unit_tests`, `gf_viewmodels_tests`, `gf_integration_tests` 100% passed.
- **Compilazione Clean:** Compilazione con `-Werror` su GCC 14/Ninja senza warning sia su preset `linux-core-release` (Zero-Qt) che su preset `linux-qt-release`.
