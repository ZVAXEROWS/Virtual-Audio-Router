# build_release.ps1
$ErrorActionPreference = "Stop"

$script:StartTime = Get-Date

Write-Host "=========================================" -ForegroundColor Cyan
Write-Host " Virtual Audio Router - Release Builder" -ForegroundColor Cyan
Write-Host "=========================================" -ForegroundColor Cyan

# 1. Build C++ Engine in Release Mode
Write-Host "`n[1/4] Building C++ Engine (Release Mode)..." -ForegroundColor Yellow
$cmake = "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
& $cmake --preset release-windows
& $cmake --build --preset release-build

# Check if var_engine.cp313-win_amd64.pyd exists
$pydPath = "gui\var_engine.cp313-win_amd64.pyd"
if (-Not (Test-Path $pydPath)) {
    Write-Error "Failed to build C++ extension. Cannot find $pydPath"
}

# 2. Build Python App with PyInstaller (One-Dir for Installer)
Write-Host "`n[2/4] Building PyInstaller Directory Output (for Setup Wizard)..." -ForegroundColor Yellow
$py = "C:\Users\ahmed mohamed\AppData\Local\Programs\Python\Python313\python.exe"
Set-Location .\gui
& $py -m PyInstaller --noconfirm --onedir --windowed --icon "..\icon.ico" --name "VirtualAudioRouter" --add-data "resources;resources" --add-data "var_engine.cp313-win_amd64.pyd;." main.py

# 3. Build Python App with PyInstaller (One-File Portable)
Write-Host "`n[3/4] Building PyInstaller Single-File Portable EXE..." -ForegroundColor Yellow
& $py -m PyInstaller --noconfirm --onefile --windowed --icon "..\icon.ico" --name "VirtualAudioRouter_Portable" --add-data "resources;resources" --add-data "var_engine.cp313-win_amd64.pyd;." main.py
Set-Location ..

# 4. Generate Installer with Inno Setup
Write-Host "`n[4/4] Generating Installer using Inno Setup..." -ForegroundColor Yellow
$iscc = "C:\Program Files (x86)\Inno Setup 6\ISCC.exe"
if (Test-Path $iscc) {
    & $iscc "installer.iss"
    Write-Host "`n>>> SUCCESS! Output files are available in the 'Output' folder and 'gui/dist' folder." -ForegroundColor Green
} else {
    Write-Host "`nInno Setup (ISCC.exe) not found at default location." -ForegroundColor Red
    Write-Host "Attempting to install Inno Setup via winget..." -ForegroundColor Yellow
    winget install --id JRSoftware.InnoSetup -e --accept-package-agreements --accept-source-agreements --silent
    if (Test-Path $iscc) {
        & $iscc "installer.iss"
        Write-Host "`n>>> SUCCESS! Output files are available in the 'Output' folder and 'gui/dist' folder." -ForegroundColor Green
    } else {
        Write-Host "`n>>> Inno Setup installation failed. Installer was not built." -ForegroundColor Red
        Write-Host "You can find the portable EXE at 'gui/dist/VirtualAudioRouter_Portable.exe'" -ForegroundColor Cyan
    }
}
