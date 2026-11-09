<#
.SYNOPSIS
    Git Flux — Windows Test Runner Script (PowerShell)

.DESCRIPTION
    Runs CTest test suites (unit, MVVM, integration) and headless smoke test on Windows.

.PARAMETER Qt
    Run Desktop Qt preset tests [default] (qt-release).

.PARAMETER Core
    Run Zero-Qt Core preset tests (core-release).

.PARAMETER All
    Run tests for both presets (core + desktop).

.PARAMETER Filter
    Filter tests to run using a regular expression (-R).

.PARAMETER Exclude
    Exclude tests matching a regular expression (-E).

.PARAMETER Smoke
    Run only the headless application startup smoke test.

.PARAMETER Verbose
    Print detailed output for each individual test.

.PARAMETER Help
    Show this help message.

.EXAMPLE
    .\scripts\windows\test.ps1
    .\scripts\windows\test.ps1 -Core
    .\scripts\windows\test.ps1 -All
    .\scripts\windows\test.ps1 -Filter viewmodels
    .\scripts\windows\test.ps1 -Smoke
#>

param(
    [Alias("q")][switch]$Qt,
    [Alias("c")][switch]$Core,
    [Alias("a")][switch]$All,
    [Alias("f")][string]$Filter,
    [Alias("e")][string]$Exclude,
    [Alias("s")][switch]$Smoke,
    [Alias("v")][switch]$Verbose,
    [Alias("h")][switch]$Help
)

$ErrorActionPreference = "Stop"
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8

function Show-Usage {
    Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Cyan
    Write-Host "Git Flux — Windows Test Runner Script" -ForegroundColor Green
    Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Cyan
    Write-Host ""
    Write-Host "Usage:" -ForegroundColor Cyan
    Write-Host "  .\scripts\windows\test.ps1 [OPTIONS]"
    Write-Host ""
    Write-Host "Options:" -ForegroundColor Cyan
    Write-Host "  -q, -Qt               Run Desktop Qt Quick 6 tests [default] (qt-release)"
    Write-Host "  -c, -Core             Run Zero-Qt Core C++23 tests (core-release)"
    Write-Host "  -a, -All              Run all tests across both presets"
    Write-Host "  -f, -Filter <REGEX>   Filter tests by name or suite (e.g. -f unit, -f viewmodels)"
    Write-Host "  -e, -Exclude <REGEX>  Exclude tests matching the pattern"
    Write-Host "  -s, -Smoke            Run only headless startup smoke test (git_flux --test-startup)"
    Write-Host "  -v, -Verbose          Detailed output with full test logs"
    Write-Host "  -h, -Help             Show this help message"
    Write-Host ""
    Write-Host "Examples:" -ForegroundColor Cyan
    Write-Host "  .\scripts\windows\test.ps1                         # Run all desktop tests"
    Write-Host "  .\scripts\windows\test.ps1 -Core                   # Run all C++23 core tests"
    Write-Host "  .\scripts\windows\test.ps1 -All                    # Run entire test suite"
    Write-Host "  .\scripts\windows\test.ps1 -Filter viewmodels      # Run only MVVM test suite"
    Write-Host "  .\scripts\windows\test.ps1 -Smoke                  # Verify headless startup"
}

if ($Help) {
    Show-Usage
    exit 0
}

$ScriptDir = Split-Path -Parent (Resolve-Path $MyInvocation.MyCommand.Path)
$RootDir = Split-Path -Parent (Split-Path -Parent $ScriptDir)
Set-Location $RootDir

# Auto-detect Qt and OpenSSL in PATH / QT_PLUGIN_PATH
$QtDir = "C:\Qt\6.6.2\msvc2019_64"
if (Test-Path $QtDir) {
    if (-not $env:QT_PLUGIN_PATH) {
        $env:QT_PLUGIN_PATH = Join-Path $QtDir "plugins"
    }
    $QtBin = Join-Path $QtDir "bin"
    if ($env:PATH -notlike "*$QtBin*") {
        $env:PATH = "$QtBin;$env:PATH"
    }
}
if (-not $env:OPENSSL_ROOT_DIR) {
    if (Test-Path "$env:USERPROFILE\anaconda3\Library\include\openssl") {
        $env:OPENSSL_ROOT_DIR = "$env:USERPROFILE\anaconda3\Library"
    } elseif (Test-Path "C:\Program Files\OpenSSL-Win64") {
        $env:OPENSSL_ROOT_DIR = "C:\Program Files\OpenSSL-Win64"
    } elseif (Test-Path "C:\Program Files\OpenSSL") {
        $env:OPENSSL_ROOT_DIR = "C:\Program Files\OpenSSL"
    }
}
if ($env:OPENSSL_ROOT_DIR) {
    $SslBin = Join-Path $env:OPENSSL_ROOT_DIR "bin"
    if ((Test-Path $SslBin) -and ($env:PATH -notlike "*$SslBin*")) {
        $env:PATH = "$SslBin;$env:PATH"
    }
}

function Find-GitFluxExe {
    $Candidates = @(
        (Join-Path $RootDir "build/qt-msvc/Release/git_flux.exe"),
        (Join-Path $RootDir "build/qt-msvc/git_flux.exe")
    )
    foreach ($cand in $Candidates) {
        if (Test-Path $cand) {
            return (Resolve-Path $cand).Path
        }
    }
    return $null
}

function Run-SmokeTest {
    Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Blue
    Write-Host "▶ Running Headless Smoke Test: git_flux --test-startup" -ForegroundColor Cyan
    Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Blue

    $BinPath = Find-GitFluxExe
    if (-not $BinPath) {
        Write-Host "⚠️ Executable git_flux.exe not found. Starting build..." -ForegroundColor Yellow
        & "$ScriptDir\build.ps1" -Target git_flux
        $BinPath = Find-GitFluxExe
        if (-not $BinPath) {
            throw "Unable to find git_flux.exe after build"
        }
    }

    Write-Host "🚀 Executing: $BinPath --test-startup" -ForegroundColor Cyan
    $Stopwatch = [System.Diagnostics.Stopwatch]::StartNew()

    & $BinPath --test-startup
    if ($LASTEXITCODE -ne 0) {
        throw "Smoke Test failed with exit code $LASTEXITCODE"
    }

    $Stopwatch.Stop()
    $ElapsedMs = $Stopwatch.ElapsedMilliseconds

    Write-Host "✔ Smoke Test passed successfully in ${ElapsedMs}ms!`n" -ForegroundColor Green
}

function Run-PresetTests {
    param(
        [string]$TargetPreset
    )

    $BuildFolder = switch ($TargetPreset) {
        "qt-release"   { "build/qt-msvc" }
        "core-release" { "build/core-msvc" }
        default        { "build/$TargetPreset" }
    }

    Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Blue
    Write-Host "▶ Running tests for preset: $TargetPreset" -ForegroundColor Cyan
    Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Blue

    if (-not (Test-Path $BuildFolder)) {
        Write-Host "⚠️ Build directory not found. Running preliminary build..." -ForegroundColor Yellow
        & "$ScriptDir\build.ps1" -Preset $TargetPreset
    }

    $CtestArgs = @("--preset", $TargetPreset, "--output-on-failure")

    if ($Filter) {
        Write-Host "🔍 Inclusion filter: $Filter" -ForegroundColor Cyan
        $CtestArgs += @("-R", $Filter)
    }

    if ($Exclude) {
        Write-Host "🚫 Exclusion filter: $Exclude" -ForegroundColor Cyan
        $CtestArgs += @("-E", $Exclude)
    }

    if ($Verbose) {
        $CtestArgs += "--verbose"
    }

    Write-Host "🧪 Running: ctest $($CtestArgs -join ' ')" -ForegroundColor Cyan
    $Stopwatch = [System.Diagnostics.Stopwatch]::StartNew()

    & ctest $CtestArgs
    if ($LASTEXITCODE -ne 0) {
        throw "CTest failed for preset '$TargetPreset' with exit code $LASTEXITCODE"
    }

    $Stopwatch.Stop()
    $ElapsedSec = [math]::Round($Stopwatch.Elapsed.TotalSeconds, 1)

    Write-Host "✔ Tests for '$TargetPreset' completed successfully in ${ElapsedSec}s!`n" -ForegroundColor Green
}

if ($Smoke) {
    Run-SmokeTest
    exit 0
}

$TotalStopwatch = [System.Diagnostics.Stopwatch]::StartNew()

if ($All) {
    Run-PresetTests "core-release"
    Run-PresetTests "qt-release"
} elseif ($Core) {
    Run-PresetTests "core-release"
} else {
    Run-PresetTests "qt-release"
}

$TotalStopwatch.Stop()
$TotalElapsed = [math]::Round($TotalStopwatch.Elapsed.TotalSeconds, 1)

Write-Host "============================================================" -ForegroundColor Green
Write-Host "  ALL TESTS PASSED SUCCESSFULLY (${TotalElapsed}s)" -ForegroundColor Green
Write-Host "============================================================" -ForegroundColor Green
