<#
.SYNOPSIS
  Compile-time profiling for the C++ core. Does a clean Ninja build with ccache
  DISABLED (so real per-file compile times are measured, not cache hits), then
  parses Ninja's .ninja_log into a ranked report (slowest TUs + per-target totals)
  and writes it to a log file.

.DESCRIPTION
  Uses a dedicated build dir (build\win-mingw-ninja, preset win-mingw-ninja) so
  it never disturbs your normal make-based dev build. The vcpkg deps (OpenCV etc.)
  are reused from the binary cache; only the project's own TUs are profiled.

.EXAMPLE
  .\profile.ps1                 # clean profile, Release, top 25 -> console + log file
  .\profile.ps1 -Top 40         # show 40 slowest TUs
  .\profile.ps1 -Config Debug   # profile a Debug build
  .\profile.ps1 -NoClean        # profile only what's out of date (incremental)
#>

param(
  [ValidateSet('Release', 'Debug', 'RelWithDebInfo')]
  [string]$Config = 'Release',
  [int]$Top = 25,
  [switch]$NoClean,
  [int]$Jobs = [int]$env:NUMBER_OF_PROCESSORS,
  [string]$MingwBin  = $(if ($env:MINGW_BIN)  { $env:MINGW_BIN }  else { 'C:\msys64\mingw64\bin' }),
  [string]$VcpkgRoot = $(if ($env:VCPKG_ROOT) { $env:VCPKG_ROOT } else { 'C:\vcpkg' })
)

$ErrorActionPreference = 'Stop'
function Info($m) { Write-Host "==> $m" -ForegroundColor Cyan }

$InspectionCore = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$Preset   = 'win-mingw-ninja'
$BuildDir = Join-Path $InspectionCore "build\$Preset"
$LogFile  = Join-Path $BuildDir 'compile_profile.txt'
$NinjaLog = Join-Path $BuildDir '.ninja_log'

if (-not (Test-Path "$MingwBin\gcc.exe"))    { throw "MinGW gcc not found at $MingwBin" }
if (-not (Test-Path "$MingwBin\ninja.exe"))  { throw "ninja not found at $MingwBin (pacman -S mingw-w64-x86_64-ninja)" }
if (-not (Test-Path "$VcpkgRoot\vcpkg.exe")) { throw "vcpkg not found at $VcpkgRoot" }

# ---- env: toolchain + DISABLE ccache (real compile times, not cache hits) ----
$env:Path = "$MingwBin;$env:Path"
$env:VCPKG_ROOT = $VcpkgRoot
$env:VCPKG_DEFAULT_HOST_TRIPLET = 'x64-mingw-static'
$env:CCACHE_DISABLE = '1'

# ---- configure (one-time for this dir) ----------------------------------
if (-not (Test-Path (Join-Path $BuildDir 'CMakeCache.txt'))) {
  Info "configure ($Preset, ccache disabled) -> $Config"
  # NB: build the -D arg as a quoted string -- a bare `-DKEY=$var` native arg does
  # NOT expand $var in Windows PowerShell 5.1 (it passes the literal "$var").
  $btArg = "-DCMAKE_BUILD_TYPE=$Config"
  & cmake -S $InspectionCore --preset $Preset $btArg
  if ($LASTEXITCODE -ne 0) { throw "configure failed ($LASTEXITCODE)" }
}

# ---- force a clean compile so every TU is timed -------------------------
if (-not $NoClean) {
  Info "ninja -t clean (force full recompile for a complete profile)"
  & ninja -C $BuildDir -t clean | Out-Null
  if (Test-Path $NinjaLog) { Remove-Item $NinjaLog -Force }   # start the timing log fresh
}

# ---- timed build --------------------------------------------------------
Info "building (j=$Jobs, ccache OFF) -- this is the real compile, be patient..."
$sw = [System.Diagnostics.Stopwatch]::StartNew()
& cmake --build $BuildDir -j $Jobs
$rc = $LASTEXITCODE
$sw.Stop()
if ($rc -ne 0) { throw "build failed ($rc)" }
$wall = $sw.Elapsed.TotalSeconds

# ---- parse .ninja_log ---------------------------------------------------
# format: <start_ms>\t<end_ms>\t<mtime>\t<output>\t<hash>   (one line per build edge)
if (-not (Test-Path $NinjaLog)) { throw "no .ninja_log produced at $NinjaLog" }

$latest = @{}   # keep the last entry per output (ninja appends)
$minStart = [long]::MaxValue; $maxEnd = [long]0
foreach ($line in Get-Content $NinjaLog) {
  if ($line.StartsWith('#')) { continue }
  $f = $line -split "`t"
  if ($f.Count -lt 5) { continue }
  $s = [long]$f[0]; $e = [long]$f[1]
  $latest[$f[3]] = $e - $s                   # duration ms
  if ($s -lt $minStart) { $minStart = $s }
  if ($e -gt $maxEnd)   { $maxEnd = $e }
}
# real wall-clock of the recorded build (independent of this run's stopwatch,
# so a -NoClean re-parse still reports the original full build's wall time)
if ($maxEnd -gt $minStart) { $wall = ($maxEnd - $minStart) / 1000 }

$rows = foreach ($kv in $latest.GetEnumerator()) {
  $out = $kv.Key
  $kind = if ($out -match '\.obj$') { 'compile' } elseif ($out -match '\.(exe|a|dll)$') { 'link' } else { 'other' }
  $target = if ($out -match 'CMakeFiles/(.+?)\.dir/') { $Matches[1] } else { '-' }
  $src = ($out -replace '^CMakeFiles/.+?\.dir/', '') -replace '\.obj$', ''
  [pscustomobject]@{ ms = $kv.Value; sec = [math]::Round($kv.Value / 1000, 2); kind = $kind; target = $target; file = $src; out = $out }
}

$compiles = $rows | Where-Object kind -eq 'compile'
$links    = $rows | Where-Object kind -eq 'link'
$cpuSec   = [math]::Round((($compiles | Measure-Object ms -Sum).Sum) / 1000, 1)

# ---- report -------------------------------------------------------------
$report = New-Object System.Collections.Generic.List[string]
$report.Add("visSele compile-time profile  ($Config, ccache OFF, j=$Jobs)")
$report.Add(("wall-clock: {0:n1}s   |   total CPU compile: {1:n1}s   |   TUs: {2}" -f $wall, $cpuSec, $compiles.Count))
$report.Add(("speedup from parallelism: {0:n1}x" -f $(if ($wall -gt 0) { $cpuSec / $wall } else { 0 })))
$report.Add('')
$report.Add("--- top $Top slowest translation units ---")
$report.Add(("{0,7}  {1,-18} {2}" -f 'sec', 'target', 'file'))
foreach ($r in ($compiles | Sort-Object ms -Descending | Select-Object -First $Top)) {
  $report.Add(("{0,7:n2}  {1,-18} {2}" -f $r.sec, $r.target, $r.file))
}
$report.Add('')
$report.Add('--- per-target compile total (sum of its TUs) ---')
$report.Add(("{0,7}  {1,-18} {2}" -f 'sec', 'target', 'TUs'))
foreach ($g in ($compiles | Group-Object target | Sort-Object { ($_.Group | Measure-Object ms -Sum).Sum } -Descending)) {
  $tsec = [math]::Round((($g.Group | Measure-Object ms -Sum).Sum) / 1000, 1)
  $report.Add(("{0,7:n1}  {1,-18} {2}" -f $tsec, $g.Name, $g.Count))
}
if ($links) {
  $report.Add('')
  $report.Add('--- link steps ---')
  foreach ($r in ($links | Sort-Object ms -Descending)) {
    $report.Add(("{0,7:n2}  {1}" -f $r.sec, $r.out))
  }
}

$text = $report -join "`r`n"
$text | Out-File -FilePath $LogFile -Encoding utf8
Write-Host ''
Write-Host $text
Write-Host ''
Info "profile log written to: $LogFile"
