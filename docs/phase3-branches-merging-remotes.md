# Git Flux — Specifica e Stato di Avanzamento Fase 3 (Branching, Merging & Remote Operations)

> **Stato:** Task 3.1 Completato | In avvio Task 3.2 — Tagging & Commit History / Log Engine  
> **Architettura di riferimento:** [plan.md](plan.md) (Sezioni 2.1, 7, 8, 15)  
> **Wireframe UI di riferimento:** [`docs/ui/git-flux-ui-v2.drawio`](ui/git-flux-ui-v2.drawio) (Pagine 07, 08)  

---

## 1. Suddivisione dei Task Logici di Fase 3

La Fase 3 estende Git Flux per gestire la topologia del repository (branch, tag, commit history), le operazioni di rete con repository remoti (fetch, pull, push) e la risoluzione dei conflitti:

| Task | Area / Modulo | Descrizione | Stato |
| :--- | :--- | :--- | :---: |
| **3.1** | **Branching & Checkout Engine** | Domain BO (`GitBranch`, `BranchType`, `CreateBranchOptions`, `CheckoutOptions`), port `IRepositoryGitOps`, service `GitBranchService` (con validatore `git-check-ref-format` e locking atomico), implementazione nativa `for-each-ref` e `checkout`. | **Completato** |
| **3.2** | **Tagging & Commit History / Log Engine** | Domain BO (`GitCommitLogItem`, `GitTag`), parsing formattato ad alte prestazioni di `git log` con grafi topologici e gestione tag. | **In corso** |
| **3.3** | **Remote Operations & Merge Engine** | Gestione remotes (`listRemotes`, `fetch`, `pull`, `push`), rilevamento conflitti di merge a 3 vie e marker di conflitto. | *In coda* |
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
- `src/core/modules/git/ports/repository_git_ops.hpp`: Interfaccia estesa con i metodi `listBranches`, `createBranch`, `checkoutBranch`, `deleteBranch`, `renameBranch` ed errori tipizzati `BranchAlreadyExists`, `BranchNotFound`, `InvalidBranchName`, `DirtyWorkingTree`, `InvalidOperation`.
- `src/core/modules/git/bl/git_branch_service.hpp` & `.cpp`: Logica di business applicativa, validazione refname preventiva, locking per-repository e filtri `getLocalBranches`, `getRemoteBranches`, `getCurrentBranch`.
- `src/infrastructure/git/native/native_repository_git_ops.hpp` & `.cpp`: Esecuzione CLI nativa di `for-each-ref`, `checkout`, `branch -d/-D`, `branch -m`.
- `tests/unit/branch_unit_tests.cpp`: Unit test esaustivi con mock e validazione nomi (37 asserzioni).
- `tests/integration/branch_integration_tests.cpp`: Test di integrazione su repository Git reale (115 asserzioni).

### Risultati di Verifica
- **Unit Tests:** `gf_unit_tests` 100% passed.
- **Integration Tests:** `gf_integration_tests` 100% passed (115 asserzioni nel caso branch).
- **Compilazione Clean:** Compilazione con `-Werror` su GCC 14/Ninja senza alcun warning, sia su preset `linux-core-release` (Zero-Qt) che su preset `linux-qt-release`.
