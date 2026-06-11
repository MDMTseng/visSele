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
  [switch]$NoBundle,                               # compile only; skip the -e export/bundle
  [switch]$Vcpkg,                                  # static vcpkg build; default = pacman OpenCV (dynamic, no dep builds)
  [string]$Out,                                    # override the export dir (default: dist\win | dist\win_debug)
  [switch]$ForceRuntime,                           # re-copy the heavy MVS runtime even if already bundled
  [switch]$Fast,                                   # incremental: skip the configure step (was export.ps1's default)
  [switch]$NoInplaceSymbols                        # opt OUT of bundling addr2line + .debug (default ON: on-target crash symbolication)
)

$ErrorActionPreference = 'Stop'

# Default toolchain: MSYS2/pacman prebuilt OpenCV (win-mingw-msys). -Vcpkg uses
# the static/pinned vcpkg path (win-mingw) for reproducible release bundles.
$Preset = if ($Vcpkg) { 'win-mingw' } else { 'win-mingw-msys' }

# ---- usage ---------------------------------------------------------------
function Show-Help {
@"
build.ps1 - full build + runtime DLL bundle (deploy). Wraps ../build.sh.
Default toolchain: prebuilt MSYS2 OpenCV (no dep builds); -Vcpkg = static/pinned.

  .\build.ps1 -Release         Release build, bundled + runnable -> dist\win
  .\build.ps1 -Debug           Debug build,   bundled           -> dist\win_debug
  .\build.ps1 -Debug -Release  build both, in order
  .\build.ps1 -Release -Clean  wipe the build dir first
  .\build.ps1 -Debug -NoBundle compile only, skip the DLL bundle
  .\build.ps1 -Release -Out dist\test   export to a custom folder
  .\build.ps1 -Release -Fast   incremental: skip configure (fast re-bundle)
  .\build.ps1 -Release -ForceRuntime    re-copy the heavy MVS runtime
  .\build.ps1 -Release -Vcpkg  static vcpkg build (reproducible release)
  .\build.ps1 -Release -NoInplaceSymbols  smaller bundle; symbolicate crashes off-box (default ships addr2line + .debug)
  .\build.ps1 <build.sh args>  raw pass-through, e.g. -p win-mingw -c RelWithDebInfo -e dist\rdi
  .\build.ps1 -h               show build.sh's own help

For the fast edit-compile-run loop use dev.ps1 instead.
(export.ps1 was merged into this script: -Out + -Fast + -ForceRuntime.)
"@ | Write-Host
}

# ---- no params -> print usage and exit ----------------------------------
if ($PSBoundParameters.Count -eq 0 -and (-not $args -or $args.Count -eq 0)) {
  Show-Help; return
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
if ($Vcpkg -and -not (Test-Path "$VcpkgRoot\vcpkg.exe")) { throw "vcpkg not found at $VcpkgRoot (set `$env:VCPKG_ROOT)" }

# ---- path conversion Windows -> MSYS (/c/...) ---------------------------
function To-BashPath([string]$p) {
  $full = (Resolve-Path $p).Path
  '/' + $full.Substring(0, 1).ToLower() + ($full.Substring(2) -replace '\\', '/')
}
$InspectionCore = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$bashCore  = To-BashPath $InspectionCore
$bashMingw = To-BashPath $MingwBin
# vcpkg env only for the static path; the msys preset needs no vcpkg.
$vcpkgEnv  = if ($Vcpkg) { "export VCPKG_ROOT=`"$(To-BashPath $VcpkgRoot)`"`nexport VCPKG_DEFAULT_HOST_TRIPLET=x64-mingw-static`nexport VCPKG_OVERLAY_TRIPLETS=`"$(To-BashPath (Join-Path $InspectionCore 'triplets'))`"" } else { '' }

# ---- run build.sh with a given arg array --------------------------------
function Invoke-BuildSh([string[]]$shArgs) {
  $argStr = ($shArgs | ForEach-Object { "'" + ($_ -replace "'", "'\''") + "'" }) -join ' '
  $forceRtEnv = if ($ForceRuntime) { 'export FORCE_RUNTIME=1' } else { '' }
  $cmd = @"
export PATH="${bashMingw}:`$PATH"
$vcpkgEnv
$forceRtEnv
cd "$bashCore" || exit 1
./build.sh $argStr
"@
  Write-Host "==> bash ../build.sh $($shArgs -join ' ')" -ForegroundColor Cyan
  & $Bash -lc $cmd
  if ($LASTEXITCODE -ne 0) { throw "build.sh failed ($LASTEXITCODE) for: $($shArgs -join ' ')" }
}

# ---- assemble the build job(s) ------------------------------------------
# Build an arg list for one config: -p <preset> -c <cfg> [--clean] [--no-configure] [-e <dir>]
function Make-Args([string]$cfg, [string]$exportDir) {
  $a = @('-p', $Preset, '-c', $cfg)
  if ($Clean)               { $a += '--clean' }
  if ($Fast)                { $a += '--no-configure' }   # incremental: skip reconfigure (was export.ps1)
  if ($NoInplaceSymbols)    { $a += '--no-inplace-symbols' } # default ON; this opts out (smaller bundle)
  if (-not $NoBundle -and $exportDir) { $a += @('-e', $exportDir) }
  return $a
}

# -Out overrides the export dir (default: dist\win | dist\win_debug). Normalize
# backslashes for build.sh.
$relOut = if ($Out) { $Out -replace '\\','/' } else { 'dist/win' }
$dbgOut = if ($Out) { $Out -replace '\\','/' } else { 'dist/win_debug' }

$jobs = @()
if ($Debug)   { $jobs += ,(Make-Args 'Debug'   $dbgOut) }
if ($Release) { $jobs += ,(Make-Args 'Release' $relOut) }

if ($jobs.Count -gt 0) {
  foreach ($j in $jobs) { Invoke-BuildSh $j }
}
elseif ($args -and $args.Count -gt 0) {
  # raw pass-through (e.g. -h, custom flags) via the automatic $args
  Invoke-BuildSh $args
}
else {
  # params passed but no config (-Debug/-Release) selected -> show help.
  Show-Help; return
}

Write-Host "==> All done." -ForegroundColor Green
