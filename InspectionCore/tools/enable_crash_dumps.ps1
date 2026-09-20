<#
  enable_crash_dumps.ps1 -- make visSele.exe's heap-corruption crashes leave a dump.

  WHY THIS EXISTS

  visSele.exe already installs its own minidump handler (SetUnhandledExceptionFilter
  + std::set_terminate, see Core0_1/BUILD.md section 8), and it works: ordinary
  access violations do produce insp_crash_<pid>_<ts>.dmp.

  It does NOT fire for the crash that has been killing the core roughly weekly.
  That one is 0xC0000374 (STATUS_HEAP_CORRUPTION), which the Windows heap raises
  as a FAIL-FAST exception -- by design that skips unhandled-exception filters
  and goes straight to Windows Error Reporting. So our handler never runs, and
  crash_reports/<date>/ (written by the drainer, after the fact) can only ever
  say "Stack trace (0 frames)".

  WER LocalDumps is the only thing that catches it. It is read from HKLM only --
  HKCU is ignored -- so this needs an elevated shell. The script re-launches
  itself elevated if it is not already.

  USAGE

      pwsh -File InspectionCore\tools\enable_crash_dumps.ps1
      pwsh -File InspectionCore\tools\enable_crash_dumps.ps1 -Status
      pwsh -File InspectionCore\tools\enable_crash_dumps.ps1 -Disable

  AFTER A CRASH

      The dump lands in -DumpFolder (default C:\CrashDumps). To get file:line,
      build with debug info and use the addr2line bundled next to the exe:

          ./InspectionCore/build.sh -p win-mingw-msys -c RelWithDebInfo -e dist
          addr2line -e visSele.exe -f -C <address>

      or open the .dmp in WinDbg / Visual Studio.

  NOTE: DumpType 2 is a FULL memory dump. For a core holding a 16 MB log ring,
  frame buffers and OpenCV scratch, expect roughly 1-3 GB per dump. DumpCount
  caps how many are kept (oldest is discarded). Mini dumps (DumpType 1) are far
  smaller but frequently useless for heap corruption, because the point is to
  inspect heap memory that a minidump omits -- so full is the default here.
#>

[CmdletBinding()]
param(
    [string] $DumpFolder = 'C:\CrashDumps',
    [ValidateRange(1, 100)]
    [int]    $DumpCount  = 10,
    [ValidateSet(1, 2)]
    [int]    $DumpType   = 2,     # 1 = mini, 2 = full
    [string] $Exe        = 'visSele.exe',
    [switch] $Disable,
    [switch] $Status
)

$ErrorActionPreference = 'Stop'

$root = 'HKLM:\SOFTWARE\Microsoft\Windows\Windows Error Reporting\LocalDumps'
$key  = Join-Path $root $Exe

function Test-Admin {
    $id = [Security.Principal.WindowsIdentity]::GetCurrent()
    (New-Object Security.Principal.WindowsPrincipal $id).IsInRole(
        [Security.Principal.WindowsBuiltInRole]::Administrator)
}

function Show-Status {
    if (-not (Test-Path $key)) {
        Write-Host "not configured -- $Exe crashes will NOT leave a dump" -ForegroundColor Yellow
        Write-Host "  key absent: $key"
        return
    }
    $p = Get-ItemProperty -Path $key
    $typeName = if ($p.DumpType -eq 2) { 'full' } elseif ($p.DumpType -eq 1) { 'mini' } else { "unknown($($p.DumpType))" }
    Write-Host "enabled for $Exe" -ForegroundColor Green
    Write-Host "  folder : $($p.DumpFolder)"
    Write-Host "  count  : $($p.DumpCount)"
    Write-Host "  type   : $($p.DumpType) ($typeName)"
    if (Test-Path $p.DumpFolder) {
        $dumps = @(Get-ChildItem -Path $p.DumpFolder -Filter '*.dmp' -ErrorAction SilentlyContinue |
                   Sort-Object LastWriteTime -Descending)
        if ($dumps.Count -eq 0) {
            Write-Host "  dumps  : none yet"
        } else {
            Write-Host "  dumps  : $($dumps.Count) present, newest:"
            $dumps | Select-Object -First 5 | ForEach-Object {
                '{0}  {1,8:N0} MB  {2}' -f $_.LastWriteTime.ToString('yyyy-MM-dd HH:mm'),
                                           ($_.Length / 1MB), $_.Name | Write-Host "           $_"
            }
        }
    } else {
        Write-Host "  dumps  : folder does not exist yet (created on first crash)"
    }
}

# -Status is read-only: answer without demanding elevation.
if ($Status) {
    if (-not (Test-Path $root)) {
        Write-Host "LocalDumps is not configured for anything on this machine." -ForegroundColor Yellow
    }
    Show-Status
    return
}

# Everything below writes to HKLM. Re-launch elevated rather than failing with
# a bare "Access to the registry key is denied".
if (-not (Test-Admin)) {
    Write-Host 'This needs elevation (HKLM). Re-launching as administrator...' -ForegroundColor Cyan
    $psExe = (Get-Process -Id $PID).Path            # pwsh.exe or powershell.exe, whichever is running
    $argList = @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', $PSCommandPath)
    foreach ($kv in $PSBoundParameters.GetEnumerator()) {
        if ($kv.Value -is [switch]) {
            if ($kv.Value.IsPresent) { $argList += "-$($kv.Key)" }
        } else {
            $argList += @("-$($kv.Key)", [string]$kv.Value)
        }
    }
    $argList += '-NoExit'   # keep the elevated window open so you can read the result
    try {
        Start-Process -FilePath $psExe -Verb RunAs -ArgumentList $argList
    } catch {
        Write-Error "Elevation was declined or failed: $($_.Exception.Message)"
        exit 1
    }
    Write-Host 'Continue in the elevated window.' -ForegroundColor Cyan
    return
}

if ($Disable) {
    if (Test-Path $key) {
        Remove-Item -Path $key -Recurse -Force
        Write-Host "removed LocalDumps config for $Exe" -ForegroundColor Green
        Write-Host "(dumps already written in a dump folder are left alone)"
    } else {
        Write-Host "nothing to remove -- $Exe was not configured"
    }
    return
}

# ---- enable ----------------------------------------------------------------

if (-not (Test-Path $DumpFolder)) {
    New-Item -ItemType Directory -Path $DumpFolder -Force | Out-Null
    Write-Host "created $DumpFolder"
}

if (-not (Test-Path $key)) { New-Item -Path $key -Force | Out-Null }

# DumpFolder is ExpandString so a path with %VAR% still resolves.
New-ItemProperty -Path $key -Name DumpFolder -Value $DumpFolder -PropertyType ExpandString -Force | Out-Null
New-ItemProperty -Path $key -Name DumpCount  -Value $DumpCount  -PropertyType DWord        -Force | Out-Null
New-ItemProperty -Path $key -Name DumpType   -Value $DumpType   -PropertyType DWord        -Force | Out-Null

Write-Host ''
Show-Status
Write-Host ''
Write-Host 'Takes effect for the NEXT crash -- a core already running is covered,' -ForegroundColor Cyan
Write-Host 'no restart needed. WER reads this at crash time.' -ForegroundColor Cyan

# Disk-space sanity.
#
# The reserve is the point. Filling the system drive on a machine that runs
# product is a worse outcome than losing a crash dump -- and DumpCount full
# dumps that merely FIT leave nothing for Windows, the log ring or an update
# package. So require the dumps to fit AND leave RESERVE_GB behind, and say what
# count would actually be safe rather than only complaining.
#
# One good dump solves a reproducible crash. DumpCount above ~3 buys very little
# here and costs tens of GB.
$DUMP_GB_EACH = 3      # pessimistic: full dump of the core, 16 MB ring + frame buffers + OpenCV
$RESERVE_GB   = 15

$free = (Get-PSDrive -Name ($DumpFolder.Substring(0,1)) -ErrorAction SilentlyContinue).Free
if ($free) {
    $freeGB = [math]::Round($free / 1GB, 1)
    $needGB = $DumpCount * $DUMP_GB_EACH
    Write-Host ("Free space on {0} {1} GB. A full dump of the core runs roughly {2} GB, x{3} kept = up to {4} GB." `
                -f $DumpFolder.Substring(0,2), $freeGB, $DUMP_GB_EACH, $DumpCount, $needGB) -ForegroundColor DarkGray

    $safeCount = [math]::Floor(($freeGB - $RESERVE_GB) / $DUMP_GB_EACH)
    if ($safeCount -lt 1) {
        Write-Host ''
        Write-Host ("WARNING: only {0} GB free. Even one full dump plus a {1} GB reserve does not fit." `
                    -f $freeGB, $RESERVE_GB) -ForegroundColor Red
        Write-Host 'Free some space, or use -DumpType 1 (mini -- much smaller, but often useless for heap corruption).' -ForegroundColor Red
    }
    elseif ($needGB -gt ($freeGB - $RESERVE_GB)) {
        Write-Host ''
        Write-Host ("WARNING: {0} dumps could use {1} GB of {2} GB free, leaving under the {3} GB reserve." `
                    -f $DumpCount, $needGB, $freeGB, $RESERVE_GB) -ForegroundColor Yellow
        Write-Host ("Filling {0} would hurt more than a missed dump. Re-run with:" -f $DumpFolder.Substring(0,2)) -ForegroundColor Yellow
        Write-Host ("    pwsh -File `"$PSCommandPath`" -DumpCount {0}" -f $safeCount) -ForegroundColor Yellow
        Write-Host 'One good dump is enough to solve a reproducible crash.' -ForegroundColor DarkGray
    }
}
