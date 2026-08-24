<#
.SYNOPSIS
  Turn Windows auto-logon on (or off) so an unattended bench comes back by itself
  after an unexpected restart.

.DESCRIPTION
  A pending Windows Update reboot cannot be cleared without rebooting, and this
  machine runs unattended for days. If it does restart, it stops at the logon
  screen: the core, the WebUI and the browser all die with the session and
  nothing returns until a human logs in. Auto-logon plus the Startup shortcut
  (Startup\visSele bench.lnk) closes that gap end to end.

  READ THIS: AutoAdminLogon stores the password in the registry AS PLAIN TEXT at
  HKLM\...\Winlogon\DefaultPassword, readable by anything running as admin on
  this box. That is how the feature works -- there is no encrypted variant short
  of Sysinternals AutoLogon (which stores it as an LSA secret instead). Turn it
  off with -Undo when the unattended run is over.

  The password is NOT stored in this script; it is passed in by the caller.

.EXAMPLE
  .\set_autologon.ps1 -Password 'xxxx'
  .\set_autologon.ps1 -Undo
#>
param(
  [string]$Password,
  [string]$User = $env:USERNAME,
  [switch]$Undo
)
$ErrorActionPreference = 'Stop'

$isAdmin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()
           ).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $isAdmin) {
  Write-Host "not elevated -- re-launching with UAC..." -ForegroundColor Yellow
  $argl = @('-ExecutionPolicy','Bypass','-NoProfile','-File',"`"$PSCommandPath`"")
  if ($Undo) { $argl += '-Undo' } else { $argl += @('-Password',"`"$Password`"",'-User',"`"$User`"") }
  Start-Process powershell -Verb RunAs -ArgumentList $argl
  return
}

$WL = 'HKLM:\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Winlogon'

if ($Undo) {
  Set-ItemProperty $WL -Name AutoAdminLogon -Value "0" -Type String
  Remove-ItemProperty $WL -Name DefaultPassword -ErrorAction SilentlyContinue
  Write-Host "auto-logon DISABLED, stored password removed." -ForegroundColor Green
  Get-ItemProperty $WL | Select-Object AutoAdminLogon, DefaultUserName,
    @{n='HasStoredPassword';e={[bool]$_.DefaultPassword}} | Format-List
  return
}

if (-not $Password) { Write-Host "no -Password given; nothing done." -ForegroundColor Red; return }

Set-ItemProperty $WL -Name AutoAdminLogon  -Value "1"   -Type String
Set-ItemProperty $WL -Name DefaultUserName -Value $User  -Type String
Set-ItemProperty $WL -Name DefaultPassword -Value $Password -Type String
# Domain is required by some builds; empty means local account.
if (-not (Get-ItemProperty $WL).DefaultDomainName) {
  Set-ItemProperty $WL -Name DefaultDomainName -Value $env:COMPUTERNAME -Type String
}
Write-Host "auto-logon ENABLED for '$User'." -ForegroundColor Green
Get-ItemProperty $WL | Select-Object AutoAdminLogon, DefaultUserName, DefaultDomainName,
  @{n='HasStoredPassword';e={[bool]$_.DefaultPassword}} | Format-List
Write-Host "The password is now in the registry in PLAIN TEXT. Undo with: .\set_autologon.ps1 -Undo" -ForegroundColor Yellow