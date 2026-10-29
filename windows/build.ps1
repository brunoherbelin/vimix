#Requires -Version 5.1
<#
.SYNOPSIS
    Build vimix for Windows in a self-contained MSYS2 UCRT64 toolchain.

.DESCRIPTION
    Everything is kept inside the repository checkout:
      .win-toolchain\msys64   private MSYS2 install (nothing is installed system-wide)
      build-win\              CMake/Ninja build directory
      dist\                   portable package (vimix.exe + DLLs + GStreamer plugins)
    Delete those folders to return the machine to its previous state.

.PARAMETER Step
    all (default), toolchain, configure, build or package.

.PARAMETER Locked
    Install the exact package versions recorded in windows\packages.lock
    instead of the latest ones, to reproduce a previously verified build.

.PARAMETER BuildType
    CMake build type (default Release).

.PARAMETER Console
    Build vimix as a console application, to see its messages
    (run it with --verbose). The default is a GUI application.

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File windows\build.ps1
    powershell -ExecutionPolicy Bypass -File windows\build.ps1 -Locked
    powershell -ExecutionPolicy Bypass -File windows\build.ps1 -Step build
#>
param(
    [ValidateSet('all', 'toolchain', 'configure', 'build', 'package')]
    [string]$Step = 'all',
    [switch]$Locked,
    [string]$BuildType = 'Release',
    [switch]$Console
)

$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'

# Pinned MSYS2 base system
$Msys2Release = '2026-09-27'
$Msys2File    = 'msys2-base-x86_64-20260927.sfx.exe'
$Msys2Sha256  = 'ad336cccfda47758b5e15cda993fbba421115cb0b126697daef1ee4dfe37209f'
$PkgPrefix    = 'mingw-w64-ucrt-x86_64-'
$PkgRepo      = 'https://repo.msys2.org/mingw/ucrt64'

$Repo      = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$Toolchain = Join-Path $Repo '.win-toolchain'
$MsysRoot  = Join-Path $Toolchain 'msys64'
$BuildDir  = Join-Path $Repo 'build-win'
$DistDir   = Join-Path $Repo 'dist'
$PkgList   = Join-Path $PSScriptRoot 'packages.txt'
$PkgLock   = Join-Path $PSScriptRoot 'packages.lock'

function Invoke-Msys([string]$Command) {
    # Run a command in a UCRT64 login shell, with the repository as working directory
    $env:MSYSTEM = 'UCRT64'
    $env:CHERE_INVOKING = '1'
    $env:MSYS2_PATH_TYPE = 'minimal'
    Push-Location $Repo
    try {
        & (Join-Path $MsysRoot 'usr\bin\bash.exe') -lc $Command
        if ($LASTEXITCODE -ne 0) { throw "MSYS2 command failed ($LASTEXITCODE): $Command" }
    }
    finally { Pop-Location }
}

function Get-PackageNames {
    Get-Content $PkgList | ForEach-Object { ($_ -replace '#.*', '').Trim() } |
        Where-Object { $_ } | ForEach-Object { $PkgPrefix + $_ }
}

function Install-Toolchain {
    if (-not (Test-Path (Join-Path $MsysRoot 'usr\bin\bash.exe'))) {
        New-Item -ItemType Directory -Force $Toolchain | Out-Null
        $sfx = Join-Path $Toolchain $Msys2File
        if (-not (Test-Path $sfx)) {
            Write-Host "Downloading $Msys2File ..."
            Invoke-WebRequest "https://github.com/msys2/msys2-installer/releases/download/$Msys2Release/$Msys2File" -OutFile $sfx
        }
        $hash = (Get-FileHash $sfx -Algorithm SHA256).Hash.ToLower()
        if ($hash -ne $Msys2Sha256) {
            Remove-Item $sfx
            throw "Checksum mismatch for $Msys2File (got $hash)"
        }
        Write-Host 'Extracting MSYS2 ...'
        & $sfx -y "-o$Toolchain" | Out-Null
        if ($LASTEXITCODE -ne 0) { throw 'MSYS2 extraction failed' }
        # first login initializes the keyring
        Invoke-Msys 'true'
    }

    if ($Locked) {
        if (-not (Test-Path $PkgLock)) { throw "No lock file at $PkgLock" }
        # download the exact recorded versions, check their signature and install them
        Invoke-Msys "sh windows/install-locked.sh windows/packages.lock $PkgRepo"
    }
    else {
        Write-Host 'Updating MSYS2 and installing packages ...'
        # the first update may only upgrade the core system, so it is run twice
        Invoke-Msys 'pacman -Syuu --noconfirm'
        Invoke-Msys 'pacman -Syuu --noconfirm'
        Invoke-Msys ("pacman -S --noconfirm --needed " + ((Get-PackageNames) -join ' '))
        # record what was installed, so the build can be reproduced with -Locked
        Invoke-Msys "pacman -Q | grep '^$PkgPrefix' > windows/packages.lock"
    }
}

function Invoke-Configure {
    $consoleOption = if ($Console) { 'ON' } else { 'OFF' }
    Invoke-Msys "cmake -S . -B build-win -G Ninja -DCMAKE_BUILD_TYPE=$BuildType -DUSE_NCNN=OFF -DUSE_ONNX=OFF -DWIN32_CONSOLE=$consoleOption"
}

function Invoke-Build {
    Invoke-Msys 'cmake --build build-win'
}

function Invoke-Package {
    Invoke-Msys "sh windows/package.sh build-win dist"
    Write-Host "Portable package in $DistDir"
}

switch ($Step) {
    'toolchain' { Install-Toolchain }
    'configure' { Invoke-Configure }
    'build'     { Invoke-Build }
    'package'   { Invoke-Package }
    'all'       { Install-Toolchain; Invoke-Configure; Invoke-Build; Invoke-Package }
}
