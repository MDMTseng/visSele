<#
.SYNOPSIS
  Stop the core the way it is meant to be stopped: Ctrl-C, not taskkill /F.

.DESCRIPTION
  docs/README.md: "絕不 kill -9 core。相機會被卡住,之後每個行程都拿不到,要
  DeviceReset 才救得回。" A force-kill leaves the HikRobot camera in a state
  where it still enumerates and still accepts settings but delivers no frames on
  hardware trigger -- which then looks exactly like a dead trigger wire, and
  costs hours before anyone suspects the previous shutdown.

  Ctrl-C is easy when the core owns a console you can click. It is NOT easy for a
  core started from a script or a background job, and "there is no window to
  press Ctrl-C in" is precisely when people reach for /F. This attaches to the
  core's console and raises the same CTRL_C event the keyboard would.

  Falls back to /F only if you ask for it, and says what that costs.

    .\stop_core.ps1              # graceful, wait up to 20s
    .\stop_core.ps1 -Force       # graceful first, then /F (and warn)
#>
param(
  [int]$TimeoutSec = 20,
  [switch]$Force
)

# The console dance runs in a CHILD process on purpose.
#
# Sending CTRL_C to another process means detaching from our own console
# (FreeConsole) and attaching to theirs -- after which this script has no
# console at all, and the next Write-Host dies with a HostException. Doing it in
# a child keeps our own output alive and leaves no state to undo.
$signaller = {
  param($targetPid)
  $sig = @"
using System;
using System.Runtime.InteropServices;
public static class ConsoleCtrl {
  [DllImport("kernel32.dll", SetLastError=true)] public static extern bool AttachConsole(uint dwProcessId);
  [DllImport("kernel32.dll", SetLastError=true)] public static extern bool FreeConsole();
  [DllImport("kernel32.dll")] public static extern bool SetConsoleCtrlHandler(IntPtr handler, bool add);
  [DllImport("kernel32.dll")] public static extern bool GenerateConsoleCtrlEvent(uint dwCtrlEvent, uint dwProcessGroupId);
}
"@
  Add-Type -TypeDefinition $sig
  [void][ConsoleCtrl]::FreeConsole()
  if ([ConsoleCtrl]::AttachConsole([uint32]$targetPid)) {
    [void][ConsoleCtrl]::SetConsoleCtrlHandler([IntPtr]::Zero, $true)
    [void][ConsoleCtrl]::GenerateConsoleCtrlEvent(0, 0)   # CTRL_C_EVENT to the attached group
    Start-Sleep -Milliseconds 800
    exit 0
  }
  exit 1
}

$proc = Get-Process visSele -ErrorAction SilentlyContinue
if (-not $proc) { Write-Host "core is not running." -ForegroundColor Yellow; return }
foreach ($p in $proc) {
  Write-Host ("core pid {0} -- sending CTRL_C to its console" -f $p.Id) -ForegroundColor Green
  $enc = [Convert]::ToBase64String([Text.Encoding]::Unicode.GetBytes(
    "& { $($signaller.ToString()) } " + $p.Id))
  $child = Start-Process powershell -ArgumentList '-NoProfile','-EncodedCommand',$enc -Wait -PassThru -WindowStyle Hidden
  if ($child.ExitCode -ne 0) {
    Write-Host "  could not attach to that console -- a core started with no console cannot be signalled." -ForegroundColor Yellow
    Write-Host "  start it via tools\start_bench.ps1 (own window) and this works." -ForegroundColor Yellow
  }
}

$deadline = (Get-Date).AddSeconds($TimeoutSec)
while ((Get-Date) -lt $deadline) {
  if (-not (Get-Process visSele -ErrorAction SilentlyContinue)) {
    Write-Host "core exited cleanly -- camera released." -ForegroundColor Green
    return
  }
  Start-Sleep -Milliseconds 500
}

Write-Host ("core still running after {0}s." -f $TimeoutSec) -ForegroundColor Yellow
if ($Force) {
  Write-Host "forcing (/F). This can wedge the camera: it will still enumerate and" -ForegroundColor Red
  Write-Host "accept settings, but deliver NO frames on hardware trigger. If that" -ForegroundColor Red
  Write-Host "happens: python tools\cam_device_reset.py (with the core stopped)." -ForegroundColor Red
  Stop-Process -Name visSele -Force
} else {
  Write-Host "left it running. Use -Force only if you accept the camera risk above." -ForegroundColor Yellow
}
