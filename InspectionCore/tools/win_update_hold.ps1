<#
.SYNOPSIS
  Hold Windows Update off the bench for an unattended run -- and put it back.

.DESCRIPTION
  This machine had a reboot ALREADY PENDING (RebootRequired + CBS RebootPending
  both set on 2026-08-20) with active hours 08:00-17:00, i.e. Windows was free to
  restart it any evening. A restart mid-run kills the core, the WebUI and any
  measurement in progress, and nobody is there to bring them back.

  Everything here needs administrator rights, which is the whole reason it is a
  script you click rather than something the agent did: the unelevated shell can
  write ActiveHours (already widened to 0-23) but not the policy keys or the
  UpdateOrchestrator tasks.

  -Days N   pause updates for N days (default 7, max 35 -- Windows' own ceiling)
  -Undo     put everything back: resume updates, drop the policy, re-enable tasks

.EXAMPLE
  Right-click -> Run with PowerShell (accept the UAC prompt), or:
    powershell -ExecutionPolicy Bypass -File win_update_hold.ps1 -Days 7
    powershell -ExecutionPolicy Bypass -File win_update_hold.ps1 -Undo

.NOTES
  A pause is a pause, not a fix: the pending update is still pending and the
  machine still wants a restart. Once the run is done, -Undo and reboot on
  purpose, at a time you choose.
#>
param(
  [int]$Days = 7,
  [switch]$Undo
)

$ErrorActionPreference = 'Stop'

# Self-elevate: re-launch this same script with the same arguments under UAC so
# a double-click is enough. Without this the writes below fail one by one and
# the machine looks protected when it is not.
$isAdmin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()
           ).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $isAdmin) {
  Write-Host "not elevated -- re-launching with UAC..." -ForegroundColor Yellow
  $argl = @('-ExecutionPolicy','Bypass','-NoProfile','-File',"`"$PSCommandPath`"")
  if ($Undo) { $argl += '-Undo' } else { $argl += @('-Days',"$Days") }
  Start-Process powershell -Verb RunAs -ArgumentList $argl
  return
}

$UX  = 'HKLM:\SOFTWARE\Microsoft\WindowsUpdate\UX\Settings'
$AU  = 'HKLM:\SOFTWARE\Policies\Microsoft\Windows\WindowsUpdate\AU'
$WU  = 'HKLM:\SOFTWARE\Policies\Microsoft\Windows\WindowsUpdate'
$TASKPATH = '\Microsoft\Windows\UpdateOrchestrator\'

function Show-State($label) {
  Write-Host "`n=== $label ===" -ForegroundColor Cyan
  $reboot = (Test-Path 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\WindowsUpdate\Auto Update\RebootRequired') -or
            (Test-Path 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Component Based Servicing\RebootPending')
  Write-Host ("reboot pending   : {0}" -f $reboot)
  $u = Get-ItemProperty $UX -ErrorAction SilentlyContinue
  Write-Host ("active hours     : {0}..{1}" -f $u.ActiveHoursStart, $u.ActiveHoursEnd)
  Write-Host ("paused until     : {0}" -f $(if ($u.PauseUpdatesExpiryTime) { $u.PauseUpdatesExpiryTime } else { '(not paused)' }))
  $a = Get-ItemProperty $AU -ErrorAction SilentlyContinue
  Write-Host ("no-auto-reboot   : {0}" -f $(if ($a.NoAutoRebootWithLoggedOnUsers) { $a.NoAutoRebootWithLoggedOnUsers } else { '(unset)' }))
  Get-ScheduledTask -TaskPath $TASKPATH -ErrorAction SilentlyContinue |
    Where-Object { $_.TaskName -match 'Reboot' } |
    ForEach-Object { Write-Host ("task {0,-24}: {1}" -f $_.TaskName, $_.State) }
}

Show-State 'BEFORE'

if ($Undo) {
  Write-Host "`nrestoring Windows Update to normal..." -ForegroundColor Green
  foreach ($n in 'PauseUpdatesExpiryTime','PauseFeatureUpdatesStartTime','PauseFeatureUpdatesEndTime',
                 'PauseQualityUpdatesStartTime','PauseQualityUpdatesEndTime','PauseUpdatesStartTime') {
    Remove-ItemProperty $UX -Name $n -ErrorAction SilentlyContinue
  }
  # Active hours back to a normal working day.
  Set-ItemProperty $UX -Name ActiveHoursStart -Value 8  -Type DWord
  Set-ItemProperty $UX -Name ActiveHoursEnd   -Value 17 -Type DWord
  if (Test-Path $AU) { Remove-ItemProperty $AU -Name NoAutoRebootWithLoggedOnUsers -ErrorAction SilentlyContinue }
  Get-ScheduledTask -TaskPath $TASKPATH -ErrorAction SilentlyContinue |
    Where-Object { $_.TaskName -match 'Reboot' } |
    ForEach-Object { try { Enable-ScheduledTask -TaskPath $TASKPATH -TaskName $_.TaskName | Out-Null } catch {} }
  Show-State 'AFTER (restored)'
  Write-Host "`nReboot deliberately when convenient -- the pending update is still pending." -ForegroundColor Yellow
  return
}

if ($Days -lt 1) { $Days = 1 }
if ($Days -gt 35) { $Days = 35 }   # Windows refuses longer than 35 days
$now = (Get-Date).ToUniversalTime()
$end = $now.AddDays($Days)
$fmt = 'yyyy-MM-ddTHH:mm:ssZ'

Write-Host "`nholding updates for $Days day(s)..." -ForegroundColor Green
New-Item -Path $UX -Force | Out-Null
Set-ItemProperty $UX -Name PauseUpdatesExpiryTime         -Value $end.ToString($fmt) -Type String
Set-ItemProperty $UX -Name PauseFeatureUpdatesStartTime   -Value $now.ToString($fmt) -Type String
Set-ItemProperty $UX -Name PauseFeatureUpdatesEndTime     -Value $end.ToString($fmt) -Type String
Set-ItemProperty $UX -Name PauseQualityUpdatesStartTime   -Value $now.ToString($fmt) -Type String
Set-ItemProperty $UX -Name PauseQualityUpdatesEndTime     -Value $end.ToString($fmt) -Type String

# The one that matters for a machine left running: no automatic restart while a
# user is logged on. Windows still installs, still asks -- it just stops taking
# the decision away.
New-Item -Path $AU -Force | Out-Null
Set-ItemProperty $AU -Name NoAutoRebootWithLoggedOnUsers -Value 1 -Type DWord
Set-ItemProperty $AU -Name AUOptions                     -Value 2 -Type DWord   # notify, do not auto-install

# Belt and braces: the orchestrator's own reboot tasks. Best-effort -- some
# builds protect these even from admin, hence the try/catch and the report.
Get-ScheduledTask -TaskPath $TASKPATH -ErrorAction SilentlyContinue |
  Where-Object { $_.TaskName -match 'Reboot' } |
  ForEach-Object {
    try { Disable-ScheduledTask -TaskPath $TASKPATH -TaskName $_.TaskName | Out-Null }
    catch { Write-Host ("could not disable task {0}: {1}" -f $_.TaskName, $_.Exception.Message) -ForegroundColor Yellow }
  }

Show-State 'AFTER (held)'
Write-Host "`nDone. Undo with:  .\win_update_hold.ps1 -Undo" -ForegroundColor Cyan
