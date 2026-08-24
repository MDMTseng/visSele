<#
.SYNOPSIS
  Panic button: collect what the machine is doing and wake Claude Code with it.

.DESCRIPTION
  For someone who is not going to debug anything -- they press one thing, and the
  agent comes back already knowing the state. It:

    1. writes a snapshot (processes, ports, recent crash dumps, disk, uptime)
       to %TEMP%\bench_state.txt, plus whatever the person typed as -Note
    2. restarts core / WebUI if they are down (start_bench.ps1 -NoClaude)
    3. opens Claude Code resumed into the working conversation, with an opening
       prompt that points at the snapshot file

  Needs no admin. Safe to run twice.
#>
param(
  [string]$Note = '',
  [string]$Session = '325bff37-610d-48da-bd59-dc6b3c23c835'
)
$ErrorActionPreference = 'Continue'

$repo  = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)   # ...\visSele
$ws    = Split-Path -Parent $repo                                # ...\workspace
$snap  = Join-Path $env:TEMP 'bench_state.txt'

function P($n) { $null -ne (Get-NetTCPConnection -LocalPort $n -State Listen -ErrorAction SilentlyContinue) }

$lines = @()
$lines += "bench state @ " + (Get-Date -Format 'yyyy-MM-dd HH:mm:ss')
$lines += "note from operator: " + $(if ($Note) { $Note } else { '(none given)' })
$lines += ""
$lines += "-- processes --"
foreach ($n in 'visSele','inspd_log','claude','node','chrome') {
  $p = Get-Process $n -ErrorAction SilentlyContinue
  $lines += ("{0,-12}: {1}" -f $n, $(if ($p) { "$($p.Count) running, RSS " + [int](($p | Measure-Object WorkingSet64 -Sum).Sum/1MB) + " MB" } else { 'NOT running' }))
}
$lines += ""
$lines += "-- ports --"
foreach ($n in 4090,4091,4099,8081,8082) { $lines += ("{0}: {1}" -f $n, $(if (P $n) { 'listening' } else { 'closed' })) }
$lines += ""
$lines += "-- recent crash dumps (newest 5) --"
$dumps = Get-ChildItem (Join-Path $repo 'InspectionCore\Core0_1') -Filter 'crash_*.dump' -ErrorAction SilentlyContinue |
         Sort-Object LastWriteTime -Descending | Select-Object -First 5
if ($dumps) { foreach ($d in $dumps) { $lines += ("{0}  {1}" -f $d.LastWriteTime.ToString('MM-dd HH:mm'), $d.Name) } }
else { $lines += "(none)" }
$lines += ""
$lines += "-- machine --"
$os = Get-CimInstance Win32_OperatingSystem
$lines += "last boot : " + $os.LastBootUpTime
$lines += "free RAM  : " + [int]($os.FreePhysicalMemory/1024) + " MB"
$d = Get-PSDrive C
$lines += "free disk : " + [int]($d.Free/1GB) + " GB"
$lines += "reboot pending: " + ((Test-Path 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\WindowsUpdate\Auto Update\RebootRequired') -or
                                (Test-Path 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Component Based Servicing\RebootPending'))

$lines -join "`r`n" | Set-Content -Path $snap -Encoding UTF8
Write-Host "state written to $snap" -ForegroundColor Cyan
$lines | ForEach-Object { Write-Host "  $_" }

# --- put the bench back if it fell over -------------------------------------
Write-Host "`nchecking core / WebUI..." -ForegroundColor Cyan
& (Join-Path $PSScriptRoot 'start_bench.ps1') -NoBrowser -NoClaude

# --- wake the agent ----------------------------------------------------------
$exe = "$env:USERPROFILE\.local\bin\claude.exe"
if (-not (Test-Path $exe)) { $exe = (Get-Command claude -ErrorAction SilentlyContinue).Source }
if (-not $exe) { Write-Host "claude.exe not found -- cannot wake the agent" -ForegroundColor Red; return }

$prompt = "WAKE: 現場同事按了桌面的緊急按鈕。機器狀態快照在 $snap 請先讀這個檔案 判斷發生什麼事並接手處理。操作者留言: " +
          $(if ($Note) { $Note } else { '(未填寫)' })

$sessFile = Join-Path $env:USERPROFILE ".claude\projects\C--Users-w2110-Documents-workspace\$Session.jsonl"
# NOT $args -- that is PowerShell's automatic argument variable, and assigning to
# it makes Start-Process receive nothing: the window never opened, with no error
# and no exit code to notice. Cost one silent failure of this exact script.
# TWO traps here, and fixing one re-introduced the other once already:
#
#   1. NOT $args. That is PowerShell's automatic argument variable; assigning to
#      it makes Start-Process receive nothing, so the window never opens -- no
#      error, no exit code, nothing to notice.
#   2. The prompt MUST be quoted as one argument. Start-Process joins
#      ArgumentList with spaces, so a bare multi-word prompt arrives truncated at
#      the first space: the agent woke up seeing "WAKE:" and nothing else.
#      Verified 2026-08-20 with -p: bare -> agent never saw the words; quoted ->
#      full text arrived.
$claudeArgs = if (Test-Path $sessFile) { @('--resume', $Session, ('"' + $prompt + '"')) }
              else                     { @('--continue', ('"' + $prompt + '"')) }

Write-Host "`nwaking Claude Code in $ws ..." -ForegroundColor Green
Start-Process $exe -ArgumentList $claudeArgs -WorkingDirectory $ws
Write-Host "done -- a Claude Code window should open. Leave it open." -ForegroundColor Green