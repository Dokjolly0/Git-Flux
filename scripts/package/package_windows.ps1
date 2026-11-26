# ==============================================================================
# Git Flux - Windows Packaging Script (PowerShell)
# ==============================================================================
param(
    [string]$BuildDir = "build/qt-msvc",
    [string]$OutputDir = "build/packages",
    [string]$Version = "0.1.0"
)

$ErrorActionPreference = "Stop"

Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Cyan
Write-Host "🚀 Git Flux - Building Windows x64 Package" -ForegroundColor Green
Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Cyan

# 1. Build git_flux
Write-Host "🔨 Building target git_flux..." -ForegroundColor Yellow
cmake --build $BuildDir --config Release --target git_flux

$ExePath = Join-Path $BuildDir "Release/git_flux.exe"
if (-not (Test-Path $ExePath)) {
    $ExePath = Join-Path $BuildDir "git_flux.exe"
}

if (-not (Test-Path $ExePath)) {
    throw "Executable git_flux.exe not found in $BuildDir"
}

$ExeDir = Split-Path -Parent (Resolve-Path $ExePath)

# Ensure OutputDir exists
if (-not (Test-Path $OutputDir)) {
    New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null
}

# 2. Prepare staging folder
$StageDir = Join-Path $OutputDir "stage-windows-x64"
if (Test-Path $StageDir) {
    Remove-Item -Recurse -Force $StageDir
}
New-Item -ItemType Directory -Path $StageDir | Out-Null

Copy-Item $ExePath -Destination $StageDir

# Also copy any runtime DLLs built into $ExeDir (from CMake POST_BUILD)
Get-ChildItem -Path $ExeDir -Filter "*.dll" -ErrorAction SilentlyContinue | ForEach-Object {
    Copy-Item $_.FullName -Destination $StageDir -Force
}

# 3. Run windeployqt
Write-Host "📦 Running windeployqt with QML scanning..." -ForegroundColor Yellow
$QmlDir = "src/desktop/ui/qml"
windeployqt --qmldir $QmlDir --release --no-translations "$StageDir/git_flux.exe"

# 4. Resolve & bundle OpenSSL runtime dependencies (libcrypto & libssl)
Write-Host "📦 Resolving OpenSSL runtime dependencies..." -ForegroundColor Yellow
$CandidateSslDirs = @()

# A. From CMakeCache.txt
$CacheFile = Join-Path $BuildDir "CMakeCache.txt"
if (Test-Path $CacheFile) {
    $CryptoLibLine = Select-String -Path $CacheFile -Pattern "OPENSSL_CRYPTO_LIBRARY:FILEPATH=(.*)" | Select-Object -First 1
    if ($CryptoLibLine -and $CryptoLibLine.Matches.Groups.Count -gt 1) {
        $LibPath = $CryptoLibLine.Matches.Groups[1].Value.Trim()
        if (Test-Path $LibPath) {
            $CandidateSslDirs += (Join-Path (Split-Path (Split-Path $LibPath -Parent) -Parent) "bin")
            $CandidateSslDirs += (Join-Path (Split-Path $LibPath -Parent) "bin")
            $CandidateSslDirs += (Split-Path $LibPath -Parent)
        }
    }
    $IncludeDirLine = Select-String -Path $CacheFile -Pattern "OPENSSL_INCLUDE_DIR:PATH=(.*)" | Select-Object -First 1
    if ($IncludeDirLine -and $IncludeDirLine.Matches.Groups.Count -gt 1) {
        $IncPath = $IncludeDirLine.Matches.Groups[1].Value.Trim()
        if (Test-Path $IncPath) {
            $CandidateSslDirs += (Join-Path (Split-Path $IncPath -Parent) "bin")
        }
    }
}

# B. From environment variables
if ($env:OPENSSL_ROOT_DIR) {
    $CandidateSslDirs += (Join-Path $env:OPENSSL_ROOT_DIR "bin")
    $CandidateSslDirs += $env:OPENSSL_ROOT_DIR
}
if ($env:OPENSSL_DIR) {
    $CandidateSslDirs += (Join-Path $env:OPENSSL_DIR "bin")
    $CandidateSslDirs += $env:OPENSSL_DIR
}

# C. Standard Windows OpenSSL locations
$CandidateSslDirs += @(
    "C:\Program Files\OpenSSL\bin",
    "C:\Program Files\OpenSSL-Win64\bin",
    "C:\Program Files (x86)\OpenSSL-Win32\bin",
    "C:\OpenSSL-Win64\bin",
    "C:\OpenSSL\bin"
)

# D. From PATH commands
$OpensslCmd = Get-Command openssl.exe -ErrorAction SilentlyContinue
if ($OpensslCmd) {
    $CandidateSslDirs += (Split-Path $OpensslCmd.Source -Parent)
}
$CryptoCmd = Get-Command libcrypto*.dll -ErrorAction SilentlyContinue
if ($CryptoCmd) {
    foreach ($c in $CryptoCmd) {
        $CandidateSslDirs += (Split-Path $c.Source -Parent)
    }
}

# Copy OpenSSL DLLs into staging
foreach ($dir in ($CandidateSslDirs | Select-Object -Unique)) {
    if ($dir -and (Test-Path $dir)) {
        $cryptoFiles = @(Get-ChildItem -Path $dir -Filter "*crypto*.dll" -ErrorAction SilentlyContinue |
            Where-Object { $_.Name -match "^(lib)?crypto.*\.dll$" })
        $sslFiles = @(Get-ChildItem -Path $dir -Filter "*ssl*.dll" -ErrorAction SilentlyContinue |
            Where-Object { $_.Name -match "^(lib)?ssl.*\.dll$" })

        $allSsl = @($cryptoFiles) + @($sslFiles)
        foreach ($f in $allSsl) {
            $targetPath = Join-Path $StageDir $f.Name
            if (-not (Test-Path $targetPath)) {
                Copy-Item $f.FullName -Destination $StageDir -Force
                Write-Host "✔ Bundled OpenSSL DLL: $($f.Name) (from $dir)" -ForegroundColor Green
            }
        }
        if ($cryptoFiles.Count -gt 0) {
            break
        }
    }
}

# Verify libcrypto presence
$StagedCrypto = Get-ChildItem -Path $StageDir -Filter "*crypto*.dll" -ErrorAction SilentlyContinue
if ($StagedCrypto) {
    Write-Host "✔ Staging verified with: $($StagedCrypto.Name -join ', ')" -ForegroundColor Green
} else {
    throw "CRITICAL ERROR: libcrypto DLL was not found or bundled into the staging folder! Halting to prevent broken package."
}

# 5. Copy assets & icons
$AssetsTarget = Join-Path $StageDir "assets"
New-Item -ItemType Directory -Path $AssetsTarget -Force | Out-Null
if (Test-Path "assets/logo.png") {
    Copy-Item "assets/logo.png" -Destination $AssetsTarget
}
if (Test-Path "assets/git-flux.ico") {
    Copy-Item "assets/git-flux.ico" -Destination $AssetsTarget
}

# 6. Create ZIP Archive (Portable)
$ZipOutput = Join-Path $OutputDir "git-flux-$Version-windows-x64.zip"
Write-Host "📦 Compressing portable package into $ZipOutput..." -ForegroundColor Yellow
if (Test-Path $ZipOutput) { Remove-Item -Force $ZipOutput }
Compress-Archive -Path "$StageDir/*" -DestinationPath $ZipOutput

Write-Host "✔ Windows x64 portable package created: $ZipOutput" -ForegroundColor Green
Get-Item $ZipOutput | Select-Object Name, Length, LastWriteTime

# 7. Create Inno Setup Installer (.exe)
Write-Host "📦 Locating Inno Setup compiler (ISCC.exe)..." -ForegroundColor Yellow

$CandidatePaths = @(
    (Get-Command iscc.exe -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Source -ErrorAction SilentlyContinue),
    (Join-Path $env:LOCALAPPDATA "Programs\Inno Setup 6\ISCC.exe"),
    (Join-Path ${env:ProgramFiles(x86)} "Inno Setup 6\ISCC.exe"),
    (Join-Path $env:ProgramFiles "Inno Setup 6\ISCC.exe")
)

$IsccExe = $null
foreach ($path in $CandidatePaths) {
    if ($path -and (Test-Path $path)) {
        $IsccExe = $path
        break
    }
}

if ($IsccExe) {
    Write-Host "🔨 Found Inno Setup at: $IsccExe" -ForegroundColor Cyan
    $FullStageDir = (Resolve-Path $StageDir).Path
    $FullOutputDir = (Resolve-Path $OutputDir).Path
    $IssScript = (Resolve-Path "packaging/windows/git-flux.iss").Path

    Write-Host "⚙️ Compiling Windows installer ($IssScript)..." -ForegroundColor Yellow
    & $IsccExe "/DAppVersion=$Version" "/DStageDir=$FullStageDir" "/DOutputDir=$FullOutputDir" $IssScript
    if ($LASTEXITCODE -ne 0) {
        throw "Inno Setup compilation failed with exit code $LASTEXITCODE"
    }

    $InstallerOutput = Join-Path $OutputDir "git-flux-$Version-windows-x64-setup.exe"
    if (Test-Path $InstallerOutput) {
        Write-Host "✔ Windows x64 setup created: $InstallerOutput" -ForegroundColor Green
        Get-Item $InstallerOutput | Select-Object Name, Length, LastWriteTime
    } else {
        throw "Installer output not found at: $InstallerOutput"
    }
} else {
    Write-Warning "ISCC.exe (Inno Setup) not found. Portable ZIP was created, but setup installer was skipped."
    Write-Warning "To build installer locally, run: winget install JRSoftware.InnoSetup"
}
