# ==============================================================================
# Git Flux — Windows Packaging Script (PowerShell)
# ==============================================================================
param(
    [string]$BuildDir = "build/qt-msvc",
    [string]$OutputDir = "build/packages",
    [string]$Version = "0.1.0"
)

$ErrorActionPreference = "Stop"

Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Cyan
Write-Host "🚀 Git Flux — Building Windows x64 Package" -ForegroundColor Green
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

# 3. Run windeployqt
Write-Host "📦 Running windeployqt with QML scanning..." -ForegroundColor Yellow
$QmlDir = "src/desktop/ui/qml"
windeployqt --qmldir $QmlDir --release --no-translations "$StageDir/git_flux.exe"

# 4. Copy assets & icons
$AssetsTarget = Join-Path $StageDir "assets"
New-Item -ItemType Directory -Path $AssetsTarget -Force | Out-Null
if (Test-Path "assets/logo.png") {
    Copy-Item "assets/logo.png" -Destination $AssetsTarget
}
if (Test-Path "assets/git-flux.ico") {
    Copy-Item "assets/git-flux.ico" -Destination $AssetsTarget
}

# 5. Create ZIP Archive (Portable)
$ZipOutput = Join-Path $OutputDir "git-flux-$Version-windows-x64.zip"
Write-Host "📦 Compressing portable package into $ZipOutput..." -ForegroundColor Yellow
if (Test-Path $ZipOutput) { Remove-Item -Force $ZipOutput }
Compress-Archive -Path "$StageDir/*" -DestinationPath $ZipOutput

Write-Host "✔ Windows x64 portable package created: $ZipOutput" -ForegroundColor Green
Get-Item $ZipOutput | Select-Object Name, Length, LastWriteTime

# 6. Create Inno Setup Installer (.exe)
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
