# Git Flux — Specifica e Stato di Avanzamento Fase 2 (Gestione Operativa Repository & Sessione di Lavoro)

> **Stato:** Fase 2 Completata al 100% (Task 2.1, 2.2, 2.3, 2.4 completati con successo)  
> **Architettura di riferimento:** [plan.md](plan.md) (Sezioni 2.1, 6, 8, 15)  
> **Wireframe UI di riferimento:** [`docs/ui/git-flux-ui-v2.drawio`](ui/git-flux-ui-v2.drawio) (Pagine 04, 05, 06, 09)

---

## 1. Suddivisione dei Task Logici di Fase 2

La Fase 2 trasforma Git Flux da catalogo di repository e workspace a client Git pienamente operativo sul working tree,
staging, commit e diff:

| Task    | Area / Modulo                         | Descrizione                                                                                                                                                                                                                                                            |     Stato      |
|:--------|:--------------------------------------|:-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|:--------------:|
| **2.1** | **Git Status & Working Tree Engine**  | Domain BO (`RepoWorkingTreeStatus`), contratto port `IRepositoryGitOps`, use case `GitWorkingTreeService` basato su `PorcelainStatusParser` e `NativeGitCommandExecutor`.                                                                                              | **Completato** |
| **2.2** | **Staging, Discard & Commit Service** | Use case `GitStagingService`: stage (`git add`), unstage (`git restore --staged` con fallback unborn HEAD), discard (`git restore` tracciati + `git clean` untracked), commit atomico (`createCommit`), mutua esclusione per-repository (`RepositoryLockCoordinator`). | **Completato** |
| **2.3** | **Diff Extraction Engine**            | Domain BO (`FileDiff`, `DiffHunk`, `DiffLine`), parser diff unificato streaming zero-allocazioni (`UnifiedDiffParser`), use case `GitDiffService` e adattatore `NativeRepositoryGitOps` (staged, unstaged, untracked con `--no-index`).                                | **Completato** |
| **2.4** | **Workspace Multi-Repo Session & UI** | ViewModels MVVM (`WorkspaceSessionViewModel`, `WorkingTreeViewModel`, `DiffViewModel`, `CommitBoxViewModel`, `WorkingTreeFileListModel`, `DiffLineListModel`) e viste QML (`WorkspaceSessionView`, `WorkingTreePanel`, `CommitBox`, `DiffViewerPanel`).                | **Completato** |

---

## 2. Dettaglio Task 2.1 — Git Status & Working Tree Engine

### Obiettivi e Principi Architetturali

- **Zero-Qt nel Core:** Le entità di dominio, i parser e i servizi operano esclusivamente con `std::string`,
  `std::filesystem::path`, `std::vector`, `std::expected` e cronometri standard C++23.
- **Robustezza ed Efficienza:** Utilizza l'infrastruttura di esecuzione `NativeGitCommandExecutor` e il
  `PorcelainStatusParser` v2 (in formato `-z` con terminatori `\0`) validato nei benchmark di Fase 0.
- **Rappresentazione Unificata del Working Tree:** Il servizio `GitWorkingTreeService` aggrega in un'unica chiamata
  atomica:
    - Dati di branch: nome head, stato detached/unborn, OID commit corrente, branch upstream tracciato, conteggi ahead e
      behind.
    - Modifiche Staged: elenco file pronti per il commit (`stagedStatus != '.'`).
    - Modifiche Unstaged: file modificati o cancellati nel working tree ma non aggiunti all'index.
    - File Untracked: nuovi file non ancora tracciati da Git.
    - File in Conflitto: file con conflitti di merge non risolti (`isConflict == true`).

---

## 3. Dettaglio Task 2.2 — Staging, Discard & Commit Service

### Componenti Realizzati

1. **Business Objects (`gf::git::bo`):**
    - `CommitAuthor`: nome, email e validatore `isValid()`.
    - `CommitOptions`: messaggio di commit, autore opzionale, flag `amend` e `allowEmpty`.
    - `CommitResult`: SHA a 40 caratteri, short SHA (8 caratteri), summary prima riga del messaggio e nome branch.
2. **Port `IRepositoryGitOps`:**
    - Esteso con `stageFiles`, `stageAll`, `unstageFiles`, `unstageAll`, `discardFiles`, `createCommit`.
    - Errori tipizzati `GitOpsError::Kind`: `EmptyCommitMessage`, `NothingToCommit`, `IndexLocked`, `PathNotFound`,
      `InvalidRepositoryPath`.
3. **Coordinamento Concorrenza & Locking (`RepositoryLockCoordinator`):**
    - Serializza le operazioni mutanti (stage/commit/discard) sullo stesso percorso canonico di repository per evitare
      contese su `.git/index.lock`.
    - Consente piena esecuzione parallela asincrona su repository differenti.
4. **Application Service (`GitStagingService`):**
    - Validazione rigida: verifica esistenza repository, impedisce path traversal (`..`) o percorsi assoluti
      indesiderati, convalida messaggi di commit non vuoti.
    - Operazione atomica protetta da lock per-repository.
5. **Infrastructure Adapter (`NativeRepositoryGitOps`):**
    - **Staging:** `git add -- <files>` e `git add -A` con policy `GitLockPolicy::Standard`.
    - **Unstage con Unborn HEAD Handling:** tenta `git restore --staged -- <files>`, e in presenza di repository neonato
      (unborn HEAD con assenza di commit radice) esegue fallback trasparente a `git rm --cached -- <files>`.
    - **Discard Completo (Tracciati e Untracked):** per i file tracciati (modificati o cancellati nel working tree)
      esegue `git restore -- <file>`. Se il file è untracked, invoca `git clean -f -d -q -- <file>` o rimozione diretta,
      garantendo il ripristino o l'eliminazione pulita in tutti gli scenari.
    - **Commit Atomico & Controllo Staging Indipendente dal Locale:** esegue `git diff --cached --quiet` preventivo per
      intercettare staging vuoto senza dipendere dalla localizzazione linguistica dei messaggi di Git (`LC_ALL`).
      Supporta `--author`, `--amend`, ed estrae SHA e branch tramite `rev-parse`.

---

## 4. Dettaglio Task 2.3 — Diff Extraction Engine

### Componenti Realizzati

1. **Business Objects (`gf::git::bo::diff_models.hpp`):**
    - `DiffScope`: enumerazione che distingue `Staged`, `Unstaged` e `Untracked`.
    - `DiffLineKind`: `Context`, `Addition`, `Deletion`, `Header`, `NoNewline`.
    - `DiffLine`: riga testuale con tracciamento coerente dei numeri di riga originari e destinazione (`oldLineNumber`,
      `newLineNumber`).
    - `DiffHunk`: header unificato, offset righe originari/nuovi e collezione di `DiffLine`.
    - `FileDiff`: vecchio e nuovo percorso, totali `additionsCount` e `deletionsCount`, flag `isBinary`, `isNewFile`,
      `isDeletedFile`, `isRenamed`, e collezione di `DiffHunk`.
2. **Unified Diff Parser (`gf::git::bl::UnifiedDiffParser`):**
    - Algoritmo ad alte prestazioni privo di espressioni regolari lente, basato su `std::string_view` e
      `std::from_chars`.
    - Parsing completo di file singoli (`parseSingle`) e stream multi-file (`parse`).
    - Riconoscimento accurato di percorsi con spazi e virgolette, metadati di file (`new file mode`,
      `deleted file mode`, `similarity index`, `rename from/to`, `Binary files ... differ`), e numeri di riga per diff
      side-by-side e inline.
3. **Application Service (`gf::git::bl::GitDiffService`):**
    - Validazione preliminare dei percorsi del repository e del file (protezione da path traversal `..` e percorsi
      assoluti).
    - Metodi di comodo: `getStagedFileDiff`, `getUnstagedFileDiff`, `getUntrackedFileDiff`, `getFileDiff`,
      `getAllDiffs`.
4. **Infrastructure Adapter (`gf::infra::git::NativeRepositoryGitOps`):**
    - Diff Staged (`--cached`), Unstaged e Untracked (`--no-index` rispetto a `/dev/null`) con policy
      `GitLockPolicy::NoOptionalLocks`.

---

## 5. Dettaglio Task 2.4 — Workspace Multi-Repo Session & UI (MVVM + Qt Quick 6)

### Componenti Realizzati

1. **Modelli e ViewModels Desktop (`src/desktop/viewmodels/`):**
    - `WorkingTreeFileListModel`: `QAbstractListModel` con ruoli `path`, `originalPath`, `kind`, `statusCode`,
      `badgeColor` per le sezioni staged, unstaged e untracked.
    - `DiffLineListModel`: `QAbstractListModel` piatto ottimizzato per `ListView` QML con ruoli `lineKind`,
      `oldLineNumber`, `newLineNumber`, `content`, `bgColor`, `fgColor`.
    - `DiffViewModel`: caricamento reattivo del diff per file selezionato e scope (`staged`, `unstaged`, `untracked`),
      metadati e conteggi additions/deletions.
    - `CommitBoxViewModel`: composizione del commit con supporto a commit multi-linea, autore, checkbox amend,
      validazione reattiva e notifica segnali `commitSucceeded` / `commitFailed`.
    - `WorkingTreeViewModel`: orchestrazione dello stato del working tree, branch attivo, conteggi ahead/behind, pulizia
      e metodi operativi `stageFile`, `stageAll`, `unstageFile`, `unstageAll`, `discardFile`.
    - `WorkspaceSessionViewModel`: Composition Root della sessione di lavoro multi-tab. Gestisce apertura di repository
      singoli, gruppi o interi workspace, switch istantaneo tra tab aperti, indicatore dirty per-repo, e chiusura tab.
2. **Componenti QML (`src/desktop/ui/qml/`):**
    - `WorkspaceSessionView.qml`: vista della sessione con barra superiore dei tab (`RepoTabsHeader`, Pagina 04 del
      wireframe), pulsante "Home" e "Quick Open".
    - `WorkingTreePanel.qml`: tre sezioni con badge colorati per Staged, Unstaged e Untracked con bottoni per
      stage/unstage/discard.
    - `CommitBox.qml`: box di commit con textarea scura stilizzata, scorciatoia `Ctrl+Enter` e pulsante dinamico "Commit
      (N) to <branch>".
    - `DiffViewerPanel.qml`: visualizzatore del diff con numeri di riga vecchio/nuovo su colonne allineate a destra con
      font monospace e colori ad alto contrasto dark (verde tenue per addizioni, rosso tenue per delezioni).
    - `main.qml`: gestione dello switch reattivo tra `HomeHubView` e `WorkspaceSessionView`.
3. **App Bootstrap & Composition Root:**
    - Inizializzazione integrata dei servizi Git e registrazione di tutti i modelli e ViewModels nel motore QML.

---

## 6. Matrice dei Criteri di Accettazione (DoD Fase 2)

1. Recupero dello status completo di un repository in meno di 15ms su repository medi (~10.000 file). *(Verificato nel
   Task 2.1)*
2. Riconoscimento accurato di modifiche staged, unstaged, file untracked, rename e conflitti. *(Verificato nel Task
   2.1)*
3. Operazioni atomiche di stage, unstage e discard senza effetti collaterali o desincronizzazione dell'index. *(
   Verificato nel Task 2.2)*
4. Esecuzione del commit con verifica preventiva dell'index, messaggi non vuoti e coordinamento atomico per-repository.
   *(Verificato nel Task 2.2)*
5. Estrazione e parsing del diff con numerazione righe coerente per diff staged e unstaged. *(Verificato nel Task 2.3)*
6. Presentazione desktop fluida in Qt Quick 6 con layout responsive e transizioni immediate tra repository aperti. *(
   Verificato nel Task 2.4 con smoke test headless e 149 asserzioni MVVM)*
