# Git Flux — Windows Package Manager (WinGet) Distribution

Questo modulo gestisce la distribuzione e l'aggiornamento automatico di **Git Flux** sul catalogo ufficiale di Windows Package Manager (`microsoft/winget-pkgs`).

---

## 📦 Dettagli Pacchetto

| Campo | Valore |
|---|---|
| **PackageIdentifier** | `GitFlux.GitFlux` |
| **InstallerType** | `zip` (portable) |
| **NestedInstallerType** | `portable` |
| **Target Architecture** | `x64` |
| **Executable** | `git_flux.exe` |
| **Command Alias** | `git-flux` |
| **Install Command** | `winget install GitFlux.GitFlux` |

Nelle GitHub Releases ufficiali sono distribuiti due formati per Windows:
- **git-flux-<version>-windows-x64-setup.exe**: Installer moderno Inno Setup (scelta installazione utente/macchina, aggiunta al PATH, icona Start/Desktop).
- **git-flux-<version>-windows-x64.zip**: Pacchetto portable standalone (utilizzato attualmente dal manifest WinGet).

I file manifest inclusi in questa directory seguono lo standard **WinGet Manifest Schema v1.6.0**:
- `GitFlux.GitFlux.yaml`: Manifest della versione
- `GitFlux.GitFlux.installer.yaml`: Configurazione dell'installer ZIP portable e mappatura dell'eseguibile
- `GitFlux.GitFlux.locale.en-US.yaml`: Metadati, descrizione, licenza e URL ufficiali

---

## ⚙️ Automazione CI/CD (GitHub Actions)

La pipeline in `.github/workflows/release.yml` include il job automatico `publish-winget`:

1. Viene eseguito al rilascio di ogni tag (`v*`) o su trigger manuale (`workflow_dispatch`).
2. Individua l'archivio `git-flux-<version>-windows-x64.zip` caricato nella GitHub Release.
3. Utilizza [`vedantmgoyal9/winget-releaser@v2`](https://github.com/vedantmgoyal9/winget-releaser) (basato su **Komac**).
4. Calcola lo SHA-256 reale dell'archivio, aggiorna i manifest e invia automaticamente una Pull Request a [`microsoft/winget-pkgs`](https://github.com/microsoft/winget-pkgs).
5. Non blocca la CI se il secret `WINGET_TOKEN` non è ancora configurato.

---

## 🚀 Setup Iniziale (Una Tantum)

Per abilitare il rilascio automatico continuo su WinGet sono necessari due passaggi una tantum:

### 1. Primo inserimento su `microsoft/winget-pkgs`

Poiché `winget-releaser` aggiorna pacchetti già presenti nel catalogo comunitario, la prima versione (`0.1.0`) deve essere approvata una volta nel repository `microsoft/winget-pkgs`.

È possibile effettuare il primo submit in due modi:

#### Opzione A — Tramite CLI `wingetcreate` (Consigliato)
Da una macchina Windows con terminale PowerShell:
```powershell
# Installa wingetcreate se non presente
winget install wingetcreate

# Invia la prima release
wingetcreate new https://github.com/dokjolly/GitFlux/releases/download/v0.1.0/git-flux-0.1.0-windows-x64.zip
```
Segui le istruzioni interattive impostando l'ID `GitFlux.GitFlux`. `wingetcreate` creerà la PR per te.

#### Opzione B — Tramite PR manuale
1. Esegui un fork di [microsoft/winget-pkgs](https://github.com/microsoft/winget-pkgs).
2. Crea la cartella `manifests/g/GitFlux/GitFlux/0.1.0/`.
3. Copia i tre file di `packaging/winget/` al suo interno.
4. Sostituisci `InstallerSha256` in `GitFlux.GitFlux.installer.yaml` con l'hash calcolato presente in `SHA256SUMS.txt` della release.
5. Apri la Pull Request verso `microsoft/winget-pkgs:master`.

---

### 2. Configurazione del Secret `WINGET_TOKEN` in GitHub

Per consentire a GitHub Actions di aprire le PR automatiche per ogni futura versione:

1. Su GitHub, vai in **Settings** > **Developer Settings** > **Personal access tokens** > **Tokens (classic)**.
2. Clicca su **Generate new token (classic)**.
3. Seleziona i permessi:
   - `public_repo` (necessario per creare fork e aprire la PR su `microsoft/winget-pkgs`)
   - `workflow` (consigliato per prevenire lock di sincronizzazione)
4. Copia il token generato.
5. Nel repository di **GitFlux**:
   - Vai in **Settings** > **Secrets and variables** > **Actions**.
   - Clicca su **New repository secret**.
   - Nome: `WINGET_TOKEN`.
   - Valore: Incolla il PAT generato.

Da questo momento, ogni nuovo tag `v*` pubblicato genererà istantaneamente la PR di aggiornamento su WinGet in automatico!
