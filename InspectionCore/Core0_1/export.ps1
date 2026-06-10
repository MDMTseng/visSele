<#
.SYNOPSIS
  Fast, self-contained export of the core for testing on another machine.
  Incremental build (no vcpkg / no configure) + bundle everything (exe + the
  3 MinGW runtime DLLs + camera SDK runtimes) into one folder you can copy as-is.

.DESCRIPTION
  The win-mingw build links OpenCV/protobuf/etc. STATICALLY (x64-mingw-static),
  so a runnable bundle only needs: visSele.exe, libgcc/libstdc++/libwinpthread,
  MVCAMSDK_X64.DLL (MindVision) and the HikRobot MVS runtime. This script:
    1. configures ONCE (only if the build dir isn't set up yet),
    2. does an incremental `cmake --build` (just the visSele target by default),
    3. bundles via build.sh --bundle-only (skips re-copying the heavy MVS
       runtime when the target already has it).
  It never re-runs vcpkg, so it can't trigger a dependency rebuild.

  For the occasional SUPER-CAREFUL rebuild (fresh deps, both configs, etc.)
  use build.ps1 -Release -Clean instead.

.EXAMPLE
  .\export.ps1                       # incremental visSele -> bundle to dist\win
  .\export.ps1 -Out dist\test        # export to a fresh folder (copies MVS runtime)
  .\export.ps1 -All                  # bundle every exe (jpeg_bench, calib_chessboard, ...)
  .\export.ps1 -Config Debug         # (re)configure to Debug, then export
  .\export.ps1 -ForceRuntime         # re-copy the HikRobot MVS runtime even if present
#>

param(
  [string]$Out = 'dist\win',                 # bundle target folder (relative -> under InspectionCore)
  [ValidateSet('', 'Debug', 'Release', 'RelWithDebInfo')]
  [string]$Config = '',                      # empty = keep whatever the build dir is configured with
  [switch]$All,                              # bundle every target, not just visSele
  [switch]$ForceRuntime,                     # re-copy the heavy MVS runtime even if already bundled
  [int]$Jobs = [int]$env:NUMBER_OF_PROCESSORS
)

$ErrorActionPreference = 'Stop'

$MingwBin  = if ($env:MINGW_BIN)  { $env:MINGW_BIN }  else { 'C:\msys64\mingw64\bin' }
$VcpkgRoot = if ($env:VCPKG_ROOT) { $env:VCPKG_ROOT } else { 'C:\vcpkg' }

$InspectionCore = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$BuildDir = Join-Path $InspectionCore 'build\win-mingw'

function Info($m) { Write-Host "==> $m" -ForegroundColor Cyan }

# Refuse to (re)configure when the toolchain vcpkg hashes drifted from the pin
# -- that's what forces a surprise ~30-min dependency rebuild. Override with
# $env:ALLOW_REBUILD=1.
function Assert-ToolchainPinned($core, $vcpkgRoot) {
  if ($env:ALLOW_REBUILD) { return }
  $reasons = @()
  $vj = Join-Path $core 'vcpkg.json'
  if ((Test-Path $vj) -and (Test-Path (Join-Path $vcpkgRoot '.git'))) {
    $bl = ([regex]'"builtin-baseline"\s*:\s*"([0-9a-f]{40})"').Match((Get-Content -Raw $vj)).Groups[1].Value
    if ($bl) {
      $hd = (& git -C $vcpkgRoot rev-parse HEAD 2>$null)
      if ($hd -and ($hd -ne $bl)) { $reasons += "vcpkg HEAD $($hd.Substring(0,12)) != baseline $($bl.Substring(0,12))" }
    }
  }
  $lock = Join-Path $core 'tools\toolchain.lock'
  if (Test-Path $lock) {
    $m = (Select-String -Path $lock -Pattern '^GCC_VERSION=(.+)$').Matches
    if ($m) {
      $wantGcc = $m.Groups[1].Value.Trim()
      $haveGcc = (& gcc -dumpfullversion 2>$null)
      if ($wantGcc -and $haveGcc -and ($wantGcc -ne $haveGcc)) { $reasons += "gcc $haveGcc != locked $wantGcc" }
    }
  }
  if ($reasons) {
    throw ("TOOLCHAIN DRIFT -> a reconfigure would rebuild all vcpkg deps (~30 min): " +
           ($reasons -join '; ') + ". Re-point `$VCPKG_ROOT at the baseline commit and keep MSYS2 gcc pinned, " +
           "or set `$env:ALLOW_REBUILD=1 to proceed deliberately.")
  }
}

if (-not (Test-Path "$MingwBin\gcc.exe"))    { throw "MinGW gcc not found at $MingwBin (set `$env:MINGW_BIN)" }
if (-not (Test-Path "$VcpkgRoot\vcpkg.exe")) { throw "vcpkg not found at $VcpkgRoot (set `$env:VCPKG_ROOT)" }

# ---- toolchain env ------------------------------------------------------
$env:Path = "$MingwBin;$env:Path"
$env:VCPKG_ROOT = $VcpkgRoot
$env:VCPKG_DEFAULT_HOST_TRIPLET = 'x64-mingw-static'

# ---- configure only when needed -----------------------------------------
$needConfigure = -not (Test-Path (Join-Path $BuildDir 'CMakeCache.txt'))
if ($Config -and -not $needConfigure) {
  $cur = (Select-String -Path (Join-Path $BuildDir 'CMakeCache.txt') -Pattern '^CMAKE_BUILD_TYPE:.*=(.*)$').Matches.Groups[1].Value
  if ($cur -ne $Config) { $needConfigure = $true }
}
if ($needConfigure) {
  Assert-ToolchainPinned $InspectionCore $VcpkgRoot
  $cfg = if ($Config) { $Config } else { 'Release' }
  Info "configure (one-time / config change) -> $cfg"
  $btArg = "-DCMAKE_BUILD_TYPE=$cfg"
  & cmake -S $InspectionCore --preset win-mingw $btArg
  if ($LASTEXITCODE -ne 0) { throw "configure failed ($LASTEXITCODE)" }
}

# ---- incremental build --------------------------------------------------
$buildCmd = @('--build', $BuildDir, '-j', "$Jobs")
if (-not $All) { $buildCmd += @('--target', 'visSele') }
Info ("cmake {0}" -f ($buildCmd -join ' '))
$sw = [System.Diagnostics.Stopwatch]::StartNew()
& cmake @buildCmd
if ($LASTEXITCODE -ne 0) { throw "build failed ($LASTEXITCODE)" }
$sw.Stop()
Info ("built in {0:n1}s" -f $sw.Elapsed.TotalSeconds)

# ---- bundle (delegate to build.sh --bundle-only) ------------------------
$Bash = (Get-Command bash.exe -ErrorAction SilentlyContinue).Source
if (-not $Bash) {
  foreach ($p in 'C:\Program Files\Git\bin\bash.exe', 'C:\Program Files\Git\usr\bin\bash.exe') {
    if (Test-Path $p) { $Bash = $p; break }
  }
}
if (-not $Bash) { throw "bash.exe not found (install Git for Windows)" }

function To-BashPath([string]$p) {
  $full = (Resolve-Path $p).Path
  '/' + $full.Substring(0, 1).ToLower() + ($full.Substring(2) -replace '\\', '/')
}
$bashCore  = To-BashPath $InspectionCore
$bashMingw = To-BashPath $MingwBin
$bashVcpkg = To-BashPath $VcpkgRoot
# $Out may not exist yet (build.sh mkdir -p's it) -> normalize to forward slashes,
# leave relative paths relative (build.sh resolves them under InspectionCore).
$outArg = ($Out -replace '\\', '/')
$forceRt = if ($ForceRuntime) { 'export FORCE_RUNTIME=1' } else { '' }

$cmd = @"
export PATH="${bashMingw}:`$PATH"
export VCPKG_ROOT="$bashVcpkg"
export VCPKG_DEFAULT_HOST_TRIPLET=x64-mingw-static
$forceRt
cd "$bashCore" || exit 1
./build.sh -p win-mingw --bundle-only -e '$outArg'
"@
Info "bundle -> $Out"
& $Bash -lc $cmd
if ($LASTEXITCODE -ne 0) { throw "bundle failed ($LASTEXITCODE)" }

# ---- sanity: confirm the folder is self-contained -----------------------
$outFull = if ([System.IO.Path]::IsPathRooted($Out)) { $Out } else { Join-Path $InspectionCore $Out }
$need = @('visSele.exe','libgcc_s_seh-1.dll','libstdc++-6.dll','libwinpthread-1.dll','MVCAMSDK_X64.DLL','MvCameraControl.dll')
$missing = $need | Where-Object { -not (Test-Path (Join-Path $outFull $_)) }
if ($missing) {
  Write-Warning ("bundle missing: {0}  (folder may need MVS installed on target, or run -ForceRuntime)" -f ($missing -join ', '))
} else {
  Info "bundle looks self-contained ($($need.Count)/$($need.Count) key files present)"
}
Write-Host "==> Export ready: $outFull" -ForegroundColor Green
