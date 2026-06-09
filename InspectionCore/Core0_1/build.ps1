<#
.SYNOPSIS
  Thin PowerShell wrapper around ../build.sh for the win-mingw build.
  Sets up the toolchain env (MinGW on PATH, vcpkg, mingw host triplet) and
  forwards to build.sh. Convenience switches for the two common builds.

.EXAMPLE
  .\build.ps1 -Debug                 # Debug build  -> bundle to dist\win_debug
  .\build.ps1 -Release               # Release build -> bundle to dist\win
  .\build.ps1 -Debug -Release        # do both, in order
  .\build.ps1 -Release -Clean        # wipe build dir first
  .\build.ps1 -Debug -NoBundle       # compile only, no DLL bundle
  .\build.ps1 -p win-mingw -c RelWithDebInfo -e dist\rdi   # raw build.sh pass-through
  .\build.ps1 -h                     # build.sh help

.NOTES
  Requires MSYS2 MinGW-w64 (C:\msys64\mingw64\bin), vcpkg (C:\vcpkg), Git Bash.
  Override toolchain paths via $env:MINGW_BIN / $env:VCPKG_ROOT before calling.
#>

# NB: deliberately NOT an advanced function (no [CmdletBinding]/[Parameter]),
# otherwise PowerShell reserves a common -Debug parameter that collides with
# our -Debug switch. Leftover args land in the automatic $args (pass-through).
param(
  [switch]$Debug,                                  # Debug build (bundle -> dist\win_debug)
  [switch]$Release,                                # Release build (bundle -> dist\win)
  [switch]$Clean,                                  # wipe build dir before configuring
  [switch]$NoBundle                                # compile only; skip the -e export/bundle
)

$ErrorActionPreference = 'Stop'

# ---- no args -> print usage and exit ------------------------------------
if ($PSBoundParameters.Count -eq 0 -and (-not $args -or $args.Count -eq 0)) {
@"
build.ps1 - full build + runtime DLL bundle (deploy). Wraps ../build.sh.

  .\build.ps1 -Release        Release build, bundled + runnable -> dist\win
  .\build.ps1 -Debug          Debug build,   bundled           -> dist\win_debug
  .\build.ps1 -Debug -Release  build both, in order
  .\build.ps1 -Release -Clean  wipe the build dir first
  .\build.ps1 -Debug -NoBundle compile only, skip the DLL bundle
  .\build.ps1 <build.sh args>  raw pass-through, e.g. -p win-mingw -c RelWithDebInfo -e dist\rdi
  .\build.ps1 -h               show build.sh's own help

For the fast edit-compile-run loop use dev.ps1 instead.
"@ | Write-Host
  return
}

# ---- locations (override via env before calling) ------------------------
$MingwBin  = if ($env:MINGW_BIN)  { $env:MINGW_BIN }  else { 'C:\msys64\mingw64\bin' }
$VcpkgRoot = if ($env:VCPKG_ROOT) { $env:VCPKG_ROOT } else { 'C:\vcpkg' }

$Bash = (Get-Command bash.exe -ErrorAction SilentlyContinue).Source
if (-not $Bash) {
  foreach ($p in 'C:\Program Files\Git\bin\bash.exe', 'C:\Program Files\Git\usr\bin\bash.exe') {
    if (Test-Path $p) { $Bash = $p; break }
  }
}
if (-not $Bash)                              { throw "bash.exe not found (install Git for Windows)" }
if (-not (Test-Path "$MingwBin\gcc.exe"))    { throw "MinGW gcc not found at $MingwBin (set `$env:MINGW_BIN)" }
if (-not (Test-Path "$VcpkgRoot\vcpkg.exe")) { throw "vcpkg not found at $VcpkgRoot (set `$env:VCPKG_ROOT)" }

# ---- path conversion Windows -> MSYS (/c/...) ---------------------------
function To-BashPath([string]$p) {
  $full = (Resolve-Path $p).Path
  '/' + $full.Substring(0, 1).ToLower() + ($full.Substring(2) -replace '\\', '/')
}
$InspectionCore = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$bashCore  = To-BashPath $InspectionCore
$bashMingw = To-BashPath $MingwBin
$bashVcpkg = To-BashPath $VcpkgRoot

# ---- run build.sh with a given arg array --------------------------------
function Invoke-BuildSh([string[]]$shArgs) {
  $argStr = ($shArgs | ForEach-Object { "'" + ($_ -replace "'", "'\''") + "'" }) -join ' '
  $cmd = @"
export PATH="${bashMingw}:`$PATH"
export VCPKG_ROOT="$bashVcpkg"
export VCPKG_DEFAULT_HOST_TRIPLET=x64-mingw-static
cd "$bashCore" || exit 1
./build.sh $argStr
"@
  Write-Host "==> bash ../build.sh $($shArgs -join ' ')" -ForegroundColor Cyan
  & $Bash -lc $cmd
  if ($LASTEXITCODE -ne 0) { throw "build.sh failed ($LASTEXITCODE) for: $($shArgs -join ' ')" }
}

# ---- assemble the build job(s) ------------------------------------------
# Build an arg list for one config: -p win-mingw -c <cfg> [--clean] [-e <dir>]
function Make-Args([string]$cfg, [string]$exportDir) {
  $a = @('-p', 'win-mingw', '-c', $cfg)
  if ($Clean)               { $a += '--clean' }
  if (-not $NoBundle -and $exportDir) { $a += @('-e', $exportDir) }
  return $a
}

$jobs = @()
if ($Debug)   { $jobs += ,(Make-Args 'Debug'   'dist/win_debug') }
if ($Release) { $jobs += ,(Make-Args 'Release' 'dist/win') }

if ($jobs.Count -gt 0) {
  foreach ($j in $jobs) { Invoke-BuildSh $j }
}
elseif ($args -and $args.Count -gt 0) {
  # raw pass-through (e.g. -h, custom flags) via the automatic $args
  Invoke-BuildSh $args
}
else {
  # bare call -> default to the Release export build
  Invoke-BuildSh (Make-Args 'Release' 'dist/win')
}

Write-Host "==> All done." -ForegroundColor Green
