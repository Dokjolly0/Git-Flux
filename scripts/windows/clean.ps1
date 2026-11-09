<#
.SYNOPSIS
    Git Flux — Windows Clean Utility Script (PowerShell)

.DESCRIPTION
    Selectively or globally cleans compiled build artifact directories on Windows.

.PARAMETER Qt
    Remove only Desktop Qt build directory (build/qt-msvc).

.PARAMETER Core
    Remove only Zero-Qt Core build directory (build/core-msvc).

.PARAMETER Packages
    Remove generated packages and staging directories (build/packages).

.PARAMETER All
    Remove all build folders (build/*) [default].

.PARAMETER DryRun
    Show directories that would be removed without deleting anything.

.PARAMETER Help
    Show this help message.

.EXAMPLE
    .\scripts\windows\clean.ps1
    .\scripts\windows\clean.ps1 -Qt
    .\scripts\windows\clean.ps1 -Core
    .\scripts\windows\clean.ps1 -DryRun
#>

param(
    [Alias("q")][switch]$Qt,
    [Alias("c")][switch]$Core,
    [Alias("p")][switch]$Packages,
    [Alias("a")][switch]$All,
    [Alias("d")][switch]$DryRun,
    [Alias("h")][switch]$Help
)

$ErrorActionPreference = "Stop"
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8

function Show-Usage {
    Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Cyan
    Write-Host "Git Flux — Windows Build Cleaning Script" -ForegroundColor Green
    Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Cyan
    Write-Host ""
    Write-Host "Usage:" -ForegroundColor Cyan
    Write-Host "  .\scripts\windows\clean.ps1 [OPTIONS]"
    Write-Host ""
    Write-Host "Options:" -ForegroundColor Cyan
    Write-Host "  -a, -All       Remove all generated build directories [default]"
    Write-Host "  -q, -Qt        Remove only Desktop Qt build (build/qt-msvc)"
    Write-Host "  -c, -Core      Remove only Zero-Qt Core build (build/core-msvc)"
    Write-Host "  -p, -Packages  Remove only packages directory (build/packages)"
    Write-Host "  -d, -DryRun    Show what would be removed without deleting anything"
    Write-Host "  -h, -Help      Show this help message"
    Write-Host ""
    Write-Host "Examples:" -ForegroundColor Cyan
    Write-Host "  .\scripts\windows\clean.ps1                         # Clean all builds"
    Write-Host "  .\scripts\windows\clean.ps1 -Qt                     # Clean only build/qt-msvc"
    Write-Host "  .\scripts\windows\clean.ps1 -Core                   # Clean only build/core-msvc"
    Write-Host "  .\scripts\windows\clean.ps1 -Packages               # Clean only packages"
    Write-Host "  .\scripts\windows\clean.ps1 -DryRun                 # Preview actions"
}

if ($Help) {
    Show-Usage
    exit 0
}

$ScriptDir = Split-Path -Parent (Resolve-Path $MyInvocation.MyCommand.Path)
$RootDir = Split-Path -Parent $ScriptDir
Set-Location $RootDir

if (-not $Qt -and -not $Core -and -not $Packages -and -not $All) {
    $All = $true
}

function Remove-TargetPath {
    param([string]$Path)

    if (Test-Path $Path) {
        if ($DryRun) {
            Write-Host "[DRY-RUN] Would remove: $Path" -ForegroundColor Cyan
        } else {
            Write-Host "🗑️ Removing: $Path..." -ForegroundColor Yellow
            Remove-Item -Recurse -Force $Path
            Write-Host "✔ Removed: $Path" -ForegroundColor Green
        }
    } else {
        Write-Host "ℹ️ Path does not exist (already clean): $Path" -ForegroundColor DarkGray
    }
}

Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Blue
Write-Host "🧹 Git Flux — Build Cleanup" -ForegroundColor Cyan
if ($DryRun) {
    Write-Host "   (DRY-RUN mode enabled — no files will be deleted)" -ForegroundColor Yellow
}
Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Blue

if ($All) {
    Remove-TargetPath "build/qt-msvc"
    Remove-TargetPath "build/core-msvc"
    Remove-TargetPath "build/packages"
    if (Test-Path "build") {
        Get-ChildItem -Path "build" -ErrorAction SilentlyContinue | ForEach-Object {
            Remove-TargetPath $_.FullName
        }
    }
} else {
    if ($Qt) {
        Remove-TargetPath "build/qt-msvc"
    }
    if ($Core) {
        Remove-TargetPath "build/core-msvc"
    }
    if ($Packages) {
        Remove-TargetPath "build/packages"
    }
}

Write-Host "`n✔ Cleanup complete!" -ForegroundColor Green
