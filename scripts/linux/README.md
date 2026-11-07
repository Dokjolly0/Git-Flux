# Git Flux — Linux Developer Utility Scripts

Questa directory contiene gli script di utilità per la compilazione, il test, l'avvio e la manutenzione di **Git Flux** su sistemi operativi Linux.

Tutti gli script sono autosufficienti, supportano parametri sia abbreviati che estesi (`--help`) e rispettano l'architettura del progetto (Zero-Qt Core e Desktop Qt Quick 6).

---

## Indice degli Script

| Script | Descrizione | Utilizzo Rapido |
| :--- | :--- | :--- |
| [`build.sh`](build.sh) | Compila i preset CMake (Desktop o Zero-Qt Core), con supporto a target specifici e clean build. | `./scripts/linux/build.sh` |
| [`test.sh`](test.sh) | Esegue la test suite CTest (unitari, MVVM, integrazione) e lo smoke test headless. | `./scripts/linux/test.sh` |
| [`run.sh`](run.sh) | Avvia l'applicazione desktop Git Flux, opzionalmente con ricompilazione, GDB o Valgrind. | `./scripts/linux/run.sh` |
| [`clean.sh`](clean.sh) | Pulisce in modo selettivo o globale le directory di build (`build/qt-linux`, `build/core-linux`). | `./scripts/linux/clean.sh` |
| [`check.sh`](check.sh) | Pipeline completa di health check locale / pre-commit (build e test sia Core che Desktop + smoke test). | `./scripts/linux/check.sh` |

---

## 1. `build.sh` — Compilazione

Compila il progetto utilizzando CMake Presets.

```bash
# Compilazione standard dell'applicazione desktop (linux-qt-release)
./scripts/linux/build.sh

# Compilazione del solo Core Zero-Qt (linux-core-release)
./scripts/linux/build.sh --core

# Compilazione di entrambi i preset in sequenza
./scripts/linux/build.sh --all

# Clean build forzando la rimozione della cache e directory di build
./scripts/linux/build.sh --clean --qt

# Compilazione di un singolo target specifico
./scripts/linux/build.sh -t git_flux
./scripts/linux/build.sh -t gf_viewmodels_tests
./scripts/linux/build.sh -t gf_unit_tests
```

### Opzioni supportate:
- `-q, --qt`: preset `linux-qt-release` (default).
- `-c, --core`: preset `linux-core-release` (Zero-Qt).
- `-a, --all`: compila sia Core che Desktop.
- `-C, --clean`: elimina la cartella prima di compilare.
- `-g, --configure`: forza l'esecuzione di `cmake --preset`.
- `-t, --target <nome>`: compila un target specifico.
- `-j, --jobs <num>`: numero di thread di compilazione paralleli (default: `nproc`).
- `-v, --verbose`: output dettagliato del compilatore.

---

## 2. `test.sh` — Test Runner

Esegue le suite di test CTest e i test di integrazione con Git nativo.

```bash
# Esegue tutti i test del preset Desktop (unit, viewmodels, integration)
./scripts/linux/test.sh

# Esegue i test del Core Zero-Qt
./scripts/linux/test.sh --core

# Esegue l'intera suite di entrambi i preset
./scripts/linux/test.sh --all

# Filtra per suite o nome del test (regex)
./scripts/linux/test.sh -f viewmodels
./scripts/linux/test.sh -f watcher
./scripts/linux/test.sh -f batch

# Esegue solo lo smoke test headless di avvio dell'applicazione
./scripts/linux/test.sh --smoke
```

---

## 3. `run.sh` — Avvio Applicazione

Avvia l'eseguibile desktop `git_flux`. Se l'eseguibile non è presente, viene compilato automaticamente.

```bash
# Avvio standard dell'interfaccia utente grafica
./scripts/linux/run.sh

# Ricompila se necessario e poi avvia
./scripts/linux/run.sh --build

# Esegue lo smoke test di avvio senza finestra UI (headless)
./scripts/linux/run.sh --smoke

# Avvia l'applicazione con GDB per sessioni di debug
./scripts/linux/run.sh --gdb

# Passa parametri personalizzati all'applicazione
./scripts/linux/run.sh -- --help
```

---

## 4. `clean.sh` — Pulizia Build

Pulisce le directory degli artefatti compilati.

```bash
# Rimuove tutte le cartelle di build generate
./scripts/linux/clean.sh

# Rimuove solo la build desktop
./scripts/linux/clean.sh --qt

# Rimuove solo la build core
./scripts/linux/clean.sh --core

# Mostra i percorsi che verrebbero rimossi senza eliminarli
./scripts/linux/clean.sh --dry-run
```

---

## 5. `check.sh` — Health Check Completo (CI / Pre-commit)

Esegue una verifica completa di salute del repository:
1. Build `linux-core-release`
2. Test CTest `linux-core-release`
3. Build `linux-qt-release`
4. Test CTest `linux-qt-release`
5. Headless Smoke Test `./build/qt-linux/git_flux --test-startup`

```bash
./scripts/linux/check.sh
```

Al termine stampa una tabella riassuntiva con durata e stato di ciascun controllo.
