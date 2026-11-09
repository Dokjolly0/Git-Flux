<#
.SYNOPSIS
    Git Flux - Windows Application Runner Script (PowerShell)

.DESCRIPTION
    Launches the Git Flux desktop application on Windows, optionally with auto-rebuild,
    debug console logging, or Visual Studio / WinDbg debugger integration.

.PARAMETER Build
    Build git_flux target before launching.

.PARAMETER Smoke
    Run in headless smoke test mode (--test-startup).

.PARAMETER Debug
    Launch application with debug console logging enabled (--debug).

.PARAMETER VisualStudio
    Launch application inside Visual Studio debugger (devenv.exe /debugexe).

.PARAMETER WinDbg
    Launch application under WinDbg debugger if installed.

.PARAMETER Help
    Show this help message.

.EXAMPLE
    .\scripts\windows\run.ps1
    .\scripts\windows\run.ps1 -Build
    .\scripts\windows\run.ps1 -Debug
    .\scripts\windows\run.ps1 -Smoke
    .\scripts\windows\run.ps1 -VisualStudio
    .\scripts\windows\run.ps1 --db C:\my\gitflux.db
#>

param(
    [Alias("b")][switch]$Build,
    [Alias("s")][switch]$Smoke,
    [Alias("d")][switch]$Debug,
    [Alias("vs")][switch]$VisualStudio,
    [Alias("w")][switch]$WinDbg,
    [Alias("h")][switch]$Help
)

$ErrorActionPreference = "Stop"
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8

function Show-Usage {
    Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Cyan
    Write-Host "Git Flux - Windows Application Runner Script" -ForegroundColor Green
    Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Cyan
    Write-Host ""
    Write-Host "Usage:" -ForegroundColor Cyan
    Write-Host "  .\scripts\windows\run.ps1 [OPTIONS] [EXTRA_ARGS...]"
    Write-Host ""
    Write-Host "Options:" -ForegroundColor Cyan
    Write-Host "  -b, -Build          Automatically build git_flux target before launching"
    Write-Host "  -s, -Smoke          Run headless startup smoke test (--test-startup)"
    Write-Host "  -d, -Debug          Launch application with console logging enabled (--debug)"
    Write-Host "  -vs, -VisualStudio  Launch application inside Visual Studio debugger"
    Write-Host "  -w, -WinDbg         Launch application under WinDbg if installed"
    Write-Host "  -h, -Help           Show this help message"
    Write-Host ""
    Write-Host "Examples:" -ForegroundColor Cyan
    Write-Host "  .\scripts\windows\run.ps1                         # Launch Git Flux desktop interface"
    Write-Host "  .\scripts\windows\run.ps1 -Build                  # Build and then launch application"
    Write-Host "  .\scripts\windows\run.ps1 -Debug                  # Run with debug console logging"
    Write-Host "  .\scripts\windows\run.ps1 -Smoke                  # Run fast headless smoke test"
    Write-Host "  .\scripts\windows\run.ps1 -VisualStudio           # Interactive debugging with Visual Studio"
    Write-Host "  .\scripts\windows\run.ps1 --help                  # Pass arguments directly to Git Flux"
}

if ($Help) {
    Show-Usage
    exit 0
}

$ScriptDir = Split-Path -Parent (Resolve-Path $MyInvocation.MyCommand.Path)
$RootDir = Split-Path -Parent (Split-Path -Parent $ScriptDir)
Set-Location $RootDir

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

$BinPath = Find-GitFluxExe

if ($Build -or (-not $BinPath)) {
    Write-Host "🔨 Building target git_flux..." -ForegroundColor Cyan
    & "$ScriptDir\build.ps1" -Target git_flux
    $BinPath = Find-GitFluxExe
    if (-not $BinPath) {
        throw "Executable git_flux.exe not found after build"
    }
}

$FinalArgs = @()
if ($Smoke) {
    $FinalArgs += "--test-startup"
}
if ($Debug -and ($FinalArgs -notcontains "--debug")) {
    $FinalArgs += "--debug"
}
if ($args) {
    $FinalArgs += $args
}

Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Blue
Write-Host "▶ Launching Git Flux Desktop: $BinPath" -ForegroundColor Cyan
if ($FinalArgs.Count -gt 0) {
    Write-Host "  Arguments: $($FinalArgs -join ' ')" -ForegroundColor Yellow
}
Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Blue

if ($VisualStudio) {
    Write-Host "🐛 Launching under Visual Studio Debugger..." -ForegroundColor Yellow
    $Devenv = Get-Command devenv.exe -ErrorAction SilentlyContinue
    if (-not $Devenv) {
        $DevenvCandidates = @(
            "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\Common7\IDE\devenv.exe",
            "C:\Program Files\Microsoft Visual Studio\2022\Professional\Common7\IDE\devenv.exe",
            "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\devenv.exe"
        )
        foreach ($cand in $DevenvCandidates) {
            if (Test-Path $cand) {
                $Devenv = $cand
                break
            }
        }
    }
    if ($Devenv) {
        & $Devenv /debugexe "$BinPath" $FinalArgs
    } else {
        throw "Visual Studio executable (devenv.exe) not found."
    }
} elseif ($WinDbg) {
    Write-Host "🔍 Launching under WinDbg..." -ForegroundColor Yellow
    $Windbg = Get-Command windbg.exe -ErrorAction SilentlyContinue
    if (-not $Windbg) {
        throw "WinDbg (windbg.exe) not found in PATH."
    }
    & $Windbg -G "$BinPath" $FinalArgs
} else {
    & "$BinPath" $FinalArgs
}
