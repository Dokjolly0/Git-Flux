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
windeployqt --qmldir $QmlDir --release --no-translations $StageDir/git_flux.exe

# 4. Copy assets & icons
$AssetsTarget = Join-Path $StageDir "assets"
New-Item -ItemType Directory -Path $AssetsTarget -Force | Out-Null
Copy-Item "assets/logo.png" -Destination $AssetsTarget

# 5. Create ZIP Archive
$ZipOutput = Join-Path $OutputDir "git-flux-$Version-windows-x64.zip"
Write-Host "📦 Compressing into $ZipOutput..." -ForegroundColor Yellow
if (Test-Path $ZipOutput) { Remove-Item -Force $ZipOutput }
Compress-Archive -Path "$StageDir/*" -DestinationPath $ZipOutput

Write-Host "✔ Windows x64 package created: $ZipOutput" -ForegroundColor Green
Get-Item $ZipOutput | Select-Object Name, Length, LastWriteTime
