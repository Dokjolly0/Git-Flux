<#
.SYNOPSIS
    Git Flux — Windows Full Health Check (Pre-Commit / Local Verification Pipeline)

.DESCRIPTION
    Runs the complete health check pipeline on Windows in sequence:
      1. Zero-Qt Core C++23 build (core-release)
      2. Core Unit & Integration Tests (ctest core-release)
      3. Desktop Qt Quick 6 build (qt-release)
      4. Desktop Unit, MVVM & Integration Tests (ctest qt-release)
      5. Headless Application Startup Smoke Test (git_flux --test-startup)

.PARAMETER Help
    Show this help message.

.EXAMPLE
    .\scripts\windows\check.ps1
#>

param(
    [Alias("h")][switch]$Help
)

$ErrorActionPreference = "Stop"
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8

function Show-Usage {
    Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Cyan
    Write-Host "Git Flux — Windows Full Health Check Pipeline" -ForegroundColor Green
    Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Cyan
    Write-Host ""
    Write-Host "Usage:" -ForegroundColor Cyan
    Write-Host "  .\scripts\windows\check.ps1"
    Write-Host ""
    Write-Host "Runs in sequence:" -ForegroundColor Cyan
    Write-Host "  1. Build Zero-Qt Core (core-release)"
    Write-Host "  2. Test Suite Zero-Qt Core"
    Write-Host "  3. Build Desktop Qt Quick (qt-release)"
    Write-Host "  4. Test Suite Desktop & MVVM"
    Write-Host "  5. Headless Smoke Test (git_flux --test-startup)"
}

if ($Help) {
    Show-Usage
    exit 0
}

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

$TotalStopwatch = [System.Diagnostics.Stopwatch]::StartNew()
$StepNames = @()
$StepDurations = @()
$StepStatuses = @()

function Print-Summary {
    $TotalStopwatch.Stop()
    $TotalElapsedSec = [math]::Round($TotalStopwatch.Elapsed.TotalSeconds, 1)

    Write-Host "`n============================================================" -ForegroundColor White
    Write-Host "              GIT FLUX HEALTH CHECK SUMMARY                 " -ForegroundColor White
    Write-Host "============================================================" -ForegroundColor White

    for ($i = 0; $i -lt $StepNames.Count; $i++) {
        $status = $StepStatuses[$i]
        $color = if ($status -eq "PASS") { "Green" } else { "Red" }
        $namePadded = $StepNames[$i].PadRight(44)
        Write-Host "  $namePadded [" -NoNewline
        Write-Host "$status" -ForegroundColor $color -NoNewline
        Write-Host "] ($($StepDurations[$i]))"
    }

    Write-Host "------------------------------------------------------------" -ForegroundColor White
    Write-Host "  Total time elapsed: ${TotalElapsedSec}s" -ForegroundColor White
    Write-Host "============================================================`n" -ForegroundColor White
}

function Run-Step {
    param(
        [string]$Name,
        [scriptblock]$Action
    )

    Write-Host "`n============================================================" -ForegroundColor Blue
    Write-Host "▶ STEP: $Name" -ForegroundColor Cyan
    Write-Host "============================================================" -ForegroundColor Blue

    $StepStopwatch = [System.Diagnostics.Stopwatch]::StartNew()
    $status = "PASS"

    try {
        & $Action
        Write-Host "✔ $Name completed successfully!" -ForegroundColor Green
    } catch {
        $status = "FAIL"
        Write-Host "✖ $Name FAILED: $_" -ForegroundColor Red
    }

    $StepStopwatch.Stop()
    $duration = "$([math]::Round($StepStopwatch.Elapsed.TotalSeconds, 1))s"

    $script:StepNames += $Name
    $script:StepDurations += $duration
    $script:StepStatuses += $status

    if ($status -eq "FAIL") {
        Print-Summary
        exit 1
    }
}

Run-Step "1. Build Zero-Qt Core (core-release)" {
    & "$ScriptDir\build.ps1" -Core
}

Run-Step "2. Test Suite Zero-Qt Core" {
    & "$ScriptDir\test.ps1" -Core
}

Run-Step "3. Build Desktop Qt Quick (qt-release)" {
    & "$ScriptDir\build.ps1" -Qt
}

Run-Step "4. Test Suite Desktop & MVVM" {
    & "$ScriptDir\test.ps1" -Qt
}

Run-Step "5. Headless Smoke Test (git_flux)" {
    & "$ScriptDir\test.ps1" -Smoke
}

Print-Summary
Write-Host "🎉 ALL CHECKS PASSED SUCCESSFULLY!`n" -ForegroundColor Green
