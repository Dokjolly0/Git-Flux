# Git Flux — Specifica e Stato di Avanzamento Fase 3 (Branching, Merging & Remote Operations)

> **Stato:** Task 3.1 & 3.2 Completati | In avvio Task 3.3 — Remote Operations & Merge Engine  
> **Architettura di riferimento:** [plan.md](plan.md) (Sezioni 2.1, 7, 8, 15)  
> **Wireframe UI di riferimento:** [`docs/ui/git-flux-ui-v2.drawio`](ui/git-flux-ui-v2.drawio) (Pagine 07, 08)  

---

## 1. Suddivisione dei Task Logici di Fase 3

La Fase 3 estende Git Flux per gestire la topologia del repository (branch, tag, commit history), le operazioni di rete con repository remoti (fetch, pull, push) e la risoluzione dei conflitti:

| Task | Area / Modulo | Descrizione | Stato |
| :--- | :--- | :--- | :---: |
| **3.1** | **Branching & Checkout Engine** | Domain BO (`GitBranch`, `BranchType`, `CreateBranchOptions`, `CheckoutOptions`), port `IRepositoryGitOps`, service `GitBranchService` (con validatore `git-check-ref-format` e locking atomico), implementazione nativa `for-each-ref` e `checkout`. | **Completato** |
| **3.2** | **Tagging & Commit History / Log Engine** | Domain BO (`GitCommitLogItem`, `GitTag`, `TagType`), parser deterministico record separator (`\x1e`/`\0`), service `GitHistoryService` con validazione tag, filtri e paginazione `git log`, supporto tag lightweight e annotated. | **Completato** |
| **3.3** | **Remote Operations & Merge Engine** | Gestione remotes (`listRemotes`, `fetch`, `pull`, `push`), rilevamento conflitti di merge a 3 vie e marker di conflitto. | **In corso** |
| **3.4** | **Branch Manager & Commit Graph UI** | ViewModels MVVM (`BranchListModel`, `BranchManagerViewModel`, `CommitHistoryViewModel`) e viste QML (`BranchSwitcherDropdown`, `BranchManagerModal`, `CommitHistoryGraphView`). | *In coda* |

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

### Risultati di Verifica
- **Unit Tests:** `gf_unit_tests` 100% passed.
- **Integration Tests:** `gf_integration_tests` 100% passed.
- **Compilazione Clean:** Compilazione con `-Werror` su GCC 14/Ninja senza warning sia su preset `linux-core-release` (Zero-Qt) che su preset `linux-qt-release`.
