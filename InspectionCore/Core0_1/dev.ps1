<#
.SYNOPSIS
  Fast inner-loop for the C++ core: incremental compile + run in place.
  Skips cmake-configure (after the first run), vcpkg, and the DLL bundling that
  build.ps1/build.sh do -- those are for deploying, not iterating.

.DESCRIPTION
  * Configures ONCE (only if the build dir isn't set up yet).
  * Runs `cmake --build` directly -> just the changed .o + relink (ccache warm).
  * Runs the exe straight from build\win-mingw, with dist\win on PATH so the
    camera/runtime DLLs resolve (no copy, no 71MB bundle).

  Prereq: run `.\build.ps1 -Release` (or -Debug) ONCE first to create dist\win
  with the runtime DLLs. After that, just loop with dev.ps1.

.EXAMPLE
  .\dev.ps1                 # incremental build + run (port 4090)
  .\dev.ps1 -NoRun          # build only
  .\dev.ps1 -Probe          # run with CORE_STATE_PROBE=1 (JPEGq logging etc.)
  .\dev.ps1 -Port 4091      # run on a different WS port
  .\dev.ps1 -Config Debug   # (re)configure to Debug, then build+run
#>

param(
  [switch]$Run,                         # build + run (explicit trigger; bare call prints help)
  [ValidateSet('', 'Debug', 'Release', 'RelWithDebInfo')]
  [string]$Config = '',                 # leave empty = keep whatever the build dir is configured with
  [string]$Target = 'visSele',          # build only this target (skips the 52MB calib_chessboard + test exes)
  [switch]$All,                         # build everything instead of just $Target
  [switch]$NoRun,
  [switch]$Probe,
  [int]$Port = 4090,
  [int]$Jobs = [int]$env:NUMBER_OF_PROCESSORS
)

$ErrorActionPreference = 'Stop'

# ---- no args -> print usage and exit ------------------------------------
if ($PSBoundParameters.Count -eq 0 -and (-not $args -or $args.Count -eq 0)) {
@"
dev.ps1 - fast inner loop: incremental compile (+ run) of the core.
Skips configure/bundle; builds only the visSele target. ccache-warm = seconds.

  .\dev.ps1 -Run             incremental build + run (port 4090)
  .\dev.ps1 -Run -Probe      ... with CORE_STATE_PROBE=1 (JPEGq/state logging)
  .\dev.ps1 -Run -Port 4091  ... on another WS port
  .\dev.ps1 -NoRun           build only, don't run
  .\dev.ps1 -All             build every target (not just visSele)
  .\dev.ps1 -Config Debug    (re)configure to Debug, then build

Prereq: run  .\build.ps1 -Release  once to stage dist\win runtime DLLs.
"@ | Write-Host
  return
}

$MingwBin  = if ($env:MINGW_BIN)  { $env:MINGW_BIN }  else { 'C:\msys64\mingw64\bin' }
$VcpkgRoot = if ($env:VCPKG_ROOT) { $env:VCPKG_ROOT } else { 'C:\vcpkg' }

$InspectionCore = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$BuildDir = Join-Path $InspectionCore 'build\win-mingw'
$DistWin  = Join-Path $InspectionCore 'dist\win'
$Exe      = Join-Path $BuildDir 'visSele.exe'

function Info($m) { Write-Host "==> $m" -ForegroundColor Cyan }

# ---- toolchain env ------------------------------------------------------
$env:Path = "$MingwBin;$DistWin;$env:Path"     # DistWin supplies MVCAMSDK_X64.DLL + HikRobot + mingw runtime
$env:VCPKG_ROOT = $VcpkgRoot
$env:VCPKG_DEFAULT_HOST_TRIPLET = 'x64-mingw-static'

# ---- configure only when needed -----------------------------------------
$needConfigure = -not (Test-Path (Join-Path $BuildDir 'CMakeCache.txt'))
if ($Config -and -not $needConfigure) {
  $cur = (Select-String -Path (Join-Path $BuildDir 'CMakeCache.txt') -Pattern '^CMAKE_BUILD_TYPE:.*=(.*)$').Matches.Groups[1].Value
  if ($cur -ne $Config) { $needConfigure = $true }   # switching config requires reconfigure
}
if ($needConfigure) {
  $cfg = if ($Config) { $Config } else { 'Release' }
  Info "configure (one-time / config change) -> $cfg"
  # quoted -D arg: a bare `-DKEY=$var` does NOT expand $var in Windows PowerShell 5.1
  $btArg = "-DCMAKE_BUILD_TYPE=$cfg"
  & cmake -S $InspectionCore --preset win-mingw $btArg
  if ($LASTEXITCODE -ne 0) { throw "configure failed ($LASTEXITCODE)" }
}

# ---- incremental build --------------------------------------------------
$buildCmd = @('--build', $BuildDir, '-j', "$Jobs")
if (-not $All) { $buildCmd += @('--target', $Target) }   # default: just visSele (skip calib_chessboard 52MB link + test exes)
Info ("cmake {0}" -f ($buildCmd -join ' '))
$sw = [System.Diagnostics.Stopwatch]::StartNew()
& cmake @buildCmd
if ($LASTEXITCODE -ne 0) { throw "build failed ($LASTEXITCODE)" }
$sw.Stop()
Info ("built in {0:n1}s" -f $sw.Elapsed.TotalSeconds)

# ---- run ----------------------------------------------------------------
if (-not $NoRun) {
  if (-not (Test-Path $DistWin)) {
    Write-Warning "dist\win not found -- run .\build.ps1 -Release once to stage the runtime DLLs, or the exe won't find MVCAMSDK_X64.DLL."
  }
  if ($Probe) { $env:CORE_STATE_PROBE = '1' } else { Remove-Item Env:CORE_STATE_PROBE -ErrorAction SilentlyContinue }
  Info "run: visSele.exe port=$Port  (Ctrl+C to stop)"
  & $Exe "port=$Port"
}
