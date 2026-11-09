<#
.SYNOPSIS
    Git Flux - Windows Build Utility Script (PowerShell)

.DESCRIPTION
    Builds the project using CMake Presets (Desktop or Zero-Qt Core) on Windows.

.PARAMETER Qt
    Build Desktop Qt Quick 6 preset [default] (qt-release).

.PARAMETER Core
    Build Zero-Qt Core C++23 preset (core-release).

.PARAMETER All
    Build both presets (core + desktop) in sequence.

.PARAMETER Clean
    Clean build directory before building.

.PARAMETER Configure
    Force CMake reconfiguration before build.

.PARAMETER Target
    Build only the specified target (e.g. git_flux, gf_viewmodels_tests, gf_unit_tests).

.PARAMETER Jobs
    Number of parallel build threads (default: $env:NUMBER_OF_PROCESSORS).

.PARAMETER Verbose
    Verbose compiler and build output.

.PARAMETER Help
    Show this help message.

.EXAMPLE
    .\scripts\windows\build.ps1
    .\scripts\windows\build.ps1 -Core
    .\scripts\windows\build.ps1 -All
    .\scripts\windows\build.ps1 -Clean -Qt
    .\scripts\windows\build.ps1 -Target git_flux
#>

param(
    [Alias("q")][switch]$Qt,
    [Alias("c")][switch]$Core,
    [Alias("a")][switch]$All,
    [Alias("cl")][switch]$Clean,
    [Alias("g")][switch]$Configure,
    [Alias("t")][string]$Target,
    [Alias("j")][int]$Jobs = 0,
    [Alias("v")][switch]$Verbose,
    [Alias("h")][switch]$Help
)

$ErrorActionPreference = "Stop"
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8

function Show-Usage {
    Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Cyan
    Write-Host "Git Flux - Windows Build Script" -ForegroundColor Green
    Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Cyan
    Write-Host ""
    Write-Host "Usage:" -ForegroundColor Cyan
    Write-Host "  .\scripts\windows\build.ps1 [OPTIONS]"
    Write-Host ""
    Write-Host "Options:" -ForegroundColor Cyan
    Write-Host "  -q, -Qt              Build Desktop Qt Quick 6 preset [default] (qt-release)"
    Write-Host "  -c, -Core            Build Zero-Qt Core C++23 preset (core-release)"
    Write-Host "  -a, -All             Build both Core and Desktop in sequence"
    Write-Host "  -cl, -Clean          Remove build directory before building"
    Write-Host "  -g, -Configure       Force CMake reconfiguration before build"
    Write-Host "  -t, -Target <NAME>   Build only the specified target (e.g. git_flux)"
    Write-Host "  -j, -Jobs <NUM>      Number of parallel build jobs (default: $env:NUMBER_OF_PROCESSORS)"
    Write-Host "  -v, -Verbose         Verbose compiler and build output"
    Write-Host "  -h, -Help            Show this help message"
    Write-Host ""
    Write-Host "Examples:" -ForegroundColor Cyan
    Write-Host "  .\scripts\windows\build.ps1                         # Full build of standard desktop target"
    Write-Host "  .\scripts\windows\build.ps1 -Core                   # Build pure C++23 core (Zero-Qt)"
    Write-Host "  .\scripts\windows\build.ps1 -All                    # Full build of both presets"
    Write-Host "  .\scripts\windows\build.ps1 -Clean -Qt              # Clean build of desktop"
    Write-Host "  .\scripts\windows\build.ps1 -Target git_flux        # Build only the main executable"
    Write-Host "  .\scripts\windows\build.ps1 -Target gf_viewmodels_tests # Build MVVM test suite"
}

if ($Help) {
    Show-Usage
    exit 0
}

# Repository root directory
$ScriptDir = Split-Path -Parent (Resolve-Path $MyInvocation.MyCommand.Path)
$RootDir = Split-Path -Parent (Split-Path -Parent $ScriptDir)
Set-Location $RootDir

# Auto-detect OpenSSL location on Windows if not set
if (-not $env:OPENSSL_ROOT_DIR) {
    if (Test-Path "$env:USERPROFILE\anaconda3\Library\include\openssl") {
        $env:OPENSSL_ROOT_DIR = "$env:USERPROFILE\anaconda3\Library"
    } elseif (Test-Path "C:\Program Files\OpenSSL-Win64") {
        $env:OPENSSL_ROOT_DIR = "C:\Program Files\OpenSSL-Win64"
    } elseif (Test-Path "C:\Program Files\OpenSSL") {
        $env:OPENSSL_ROOT_DIR = "C:\Program Files\OpenSSL"
    }
}

# Default preset selection
if (-not $Qt -and -not $Core -and -not $All) {
    $Qt = $true
}

if ($Jobs -le 0) {
    $Jobs = [int]$env:NUMBER_OF_PROCESSORS
    if ($Jobs -le 0) { $Jobs = 4 }
}

function Build-Preset {
    param(
        [string]$TargetPreset
    )

    $BuildFolder = switch ($TargetPreset) {
        "qt-release"   { "build/qt-msvc" }
        "core-release" { "build/core-msvc" }
        default        { "build/$TargetPreset" }
    }

    Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Blue
    Write-Host "▶ Starting build for preset: $TargetPreset" -ForegroundColor Cyan
    Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Blue

    if ($Clean -and (Test-Path $BuildFolder)) {
        Write-Host "🧹 Cleaning build directory: $BuildFolder..." -ForegroundColor Yellow
        Remove-Item -Recurse -Force $BuildFolder
    }

    if ($Configure -or (-not (Test-Path $BuildFolder))) {
        Write-Host "⚙️ Configuring CMake with preset '$TargetPreset'..." -ForegroundColor Cyan
        cmake --preset $TargetPreset
        if ($LASTEXITCODE -ne 0) {
            throw "CMake configuration failed for preset '$TargetPreset' with exit code $LASTEXITCODE"
        }
    }

    $BuildArgs = @("--build", "--preset", $TargetPreset, "-j", "$Jobs")

    if ($Target) {
        Write-Host "🎯 Building specific target: $Target" -ForegroundColor Cyan
        $BuildArgs += @("--target", $Target)
    }

    if ($Verbose) {
        $BuildArgs += "--verbose"
    }

    Write-Host "🔨 Running: cmake $($BuildArgs -join ' ')" -ForegroundColor Cyan
    $Stopwatch = [System.Diagnostics.Stopwatch]::StartNew()

    & cmake $BuildArgs
    if ($LASTEXITCODE -ne 0) {
        throw "Build failed for preset '$TargetPreset' with exit code $LASTEXITCODE"
    }

    $Stopwatch.Stop()
    $ElapsedSec = [math]::Round($Stopwatch.Elapsed.TotalSeconds, 1)

    Write-Host "✔ Build for '$TargetPreset' completed successfully in ${ElapsedSec}s!`n" -ForegroundColor Green
}

$TotalStopwatch = [System.Diagnostics.Stopwatch]::StartNew()

if ($All) {
    Build-Preset "core-release"
    Build-Preset "qt-release"
} elseif ($Core) {
    Build-Preset "core-release"
} else {
    Build-Preset "qt-release"
}

$TotalStopwatch.Stop()
$TotalElapsed = [math]::Round($TotalStopwatch.Elapsed.TotalSeconds, 1)

Write-Host "============================================================" -ForegroundColor Green
Write-Host "  ALL BUILD OPERATIONS COMPLETED (${TotalElapsed}s)" -ForegroundColor Green
Write-Host "============================================================" -ForegroundColor Green
