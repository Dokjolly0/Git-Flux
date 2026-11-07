# Git Flux — Specifica e Stato di Avanzamento Fase 2 (Gestione Operativa Repository & Sessione di Lavoro)

> **Stato:** in corso — avviato Task 2.1  
> **Architettura di riferimento:** [plan.md](plan.md) (Sezioni 2.1, 6, 8, 15)  
> **Wireframe UI di riferimento:** [`docs/ui/git-flux-ui-v2.drawio`](ui/git-flux-ui-v2.drawio) (Pagine 04, 05, 06, 09)  

---

## 1. Suddivisione dei Task Logici di Fase 2

La Fase 2 trasforma Git Flux da catalogo di repository e workspace a client Git pienamente operativo sul working tree, staging, commit e diff:

| Task | Area / Modulo | Descrizione | Stato |
| :--- | :--- | :--- | :---: |
| **2.1** | **Git Status & Working Tree Engine** | Domain BO (`RepoWorkingTreeStatus`), contratto port `IRepositoryGitOps`, use case `GitWorkingTreeService` basato su `PorcelainStatusParser` e `NativeGitCommandExecutor`. | **In corso** |
| **2.2** | **Staging, Discard & Commit Service** | Use case `GitStagingService`: stage (`git add`), unstage (`git restore --staged`), discard (`git restore`), commit atomico con lock e coordinamento concorrenza (`OperationCoordinator`). | *Prossimo* |
| **2.3** | **Diff Extraction Engine** | Domain BO (`FileDiff`, `DiffHunk`, `DiffLine`), parser diff unified e use case `GitDiffService` (staged, unstaged, untracked). | *In coda* |
| **2.4** | **Workspace Multi-Repo Session & UI** | ViewModels MVVM (`WorkspaceSessionViewModel`, `WorkingTreeViewModel`, `DiffViewModel`, `CommitBoxViewModel`) e viste QML (`WorkspaceView`, `WorkspaceSidebar`, `StagingPanel`, `DiffViewerPanel`). | *In coda* |

---

## 2. Dettaglio Task 2.1 — Git Status & Working Tree Engine

### Obiettivi e Principi Architetturali
- **Zero-Qt nel Core:** Le entità di dominio, i parser e i servizi operano esclusivamente con `std::string`, `std::filesystem::path`, `std::vector`, `std::expected` e cronometri standard C++23.
- **Robustezza ed Efficienza:** Utilizza l'infrastruttura di esecuzione `NativeGitCommandExecutor` e il `PorcelainStatusParser` v2 (in formato `-z` con terminatori `\0`) validato nei benchmark di Fase 0.
- **Rappresentazione Unificata del Working Tree:** Il servizio `GitWorkingTreeService` aggrega in un'unica chiamata atomica:
  - Dati di branch: nome head, stato detached/unborn, OID commit corrente, branch upstream tracciato, conteggi ahead e behind.
  - Modifiche Staged: elenco file pronti per il commit (`stagedStatus != '.'`).
  - Modifiche Unstaged: file modificati o cancellati nel working tree ma non aggiunti all'index.
  - File Untracked: nuovi file non ancora tracciati da Git.
  - File in Conflitto: file con conflitti di merge non risolti (`isConflict == true`).

---

## 3. Matrice dei Criteri di Accettazione (DoD Fase 2)

1. Recupero dello status completo di un repository in meno di 15ms su repository medi (~10.000 file).
2. Riconoscimento accurato di modifiche staged, unstaged, file untracked, rename e conflitti.
3. Operazioni atomiche di stage, unstage e discard senza effetti collaterali o desincronizzazione dell'index.
4. Esecuzione del commit con verifica preventiva dell'index, messaggi non vuoti e coordinamento atomico per-repository.
5. Estrazione e parsing del diff con numerazione righe coerente per diff staged e unstaged.
6. Presentazione desktop fluida in Qt Quick 6 con layout responsive e transizioni immediate tra repository aperti.
