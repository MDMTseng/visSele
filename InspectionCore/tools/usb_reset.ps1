# usb_reset.ps1 -- power-cycle a USB device at the OS level (disable + re-enable).
#
# WHEN THIS HELPS:
#   - camera enumerates but every grab times out / SDK says "device busy"
#   - COM3 vanished or is stuck "in use" after a hard kill
#   - MVS Viewer cannot open the camera even though it is listed
#
# WHEN THIS DOES NOT HELP:
#   - camera grabs fine in free-run but hardware trigger produces no frames.
#     That is a signal path problem (wire / opto / connector), not USB.
#     Use Peripheral/uInspESP32/tools/pin17_toggle instead.
#
# Softer option first: tools/cam_device_reset.py (SDK-level DeviceReset, no admin,
# camera is back in ~12s). Only come here if that is not enough.
#
# REQUIRES ADMIN (Disable-PnpDevice is elevated-only).
#
# Usage:
#   powershell -ExecutionPolicy Bypass -File usb_reset.ps1 -Target cam
#   powershell -ExecutionPolicy Bypass -File usb_reset.ps1 -Target esp32
#   powershell -ExecutionPolicy Bypass -File usb_reset.ps1 -Target both
#   powershell -ExecutionPolicy Bypass -File usb_reset.ps1 -List

[CmdletBinding()]
param(
    [ValidateSet('cam','esp32','both')]
    [string]$Target = 'cam',
    [switch]$List,
    [int]$SettleSeconds = 3
)

$ErrorActionPreference = 'Stop'

# HikRobot USB3 camera = VID_2BDF ; ESP32 board is a CP210x bridge = VID_10C4
$PATTERNS = @{
    cam   = 'USB\VID_2BDF*'
    esp32 = 'USB\VID_10C4*'
}

function Get-Targets([string]$which) {
    $keys = if ($which -eq 'both') { @('cam','esp32') } else { @($which) }
    $found = @()
    foreach ($k in $keys) {
        $d = Get-PnpDevice -PresentOnly | Where-Object { $_.InstanceId -like $PATTERNS[$k] }
        if (-not $d) { Write-Warning "no device present matching $k ($($PATTERNS[$k]))" }
        $found += $d
    }
    return $found
}

if ($List) {
    Get-PnpDevice -PresentOnly |
        Where-Object { $_.InstanceId -like 'USB\*' } |
        Select-Object Status, Class, FriendlyName, InstanceId |
        Sort-Object Class, FriendlyName |
        Format-Table -AutoSize
    exit 0
}

$isAdmin = ([Security.Principal.WindowsPrincipal] `
    [Security.Principal.WindowsIdentity]::GetCurrent()
    ).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $isAdmin) {
    Write-Host "This needs Administrator. Right-click PowerShell -> Run as administrator." -ForegroundColor Red
    exit 1
}

# The core holds the camera open; yanking the device out from under it is exactly
# the wedge we keep having to recover from. Stop it gracefully first.
$core = Get-Process -Name 'Core0_1' -ErrorAction SilentlyContinue
if ($core -and $Target -ne 'esp32') {
    Write-Host "Core0_1 is running (pid $($core.Id)). Stop it first:" -ForegroundColor Yellow
    Write-Host "    powershell -ExecutionPolicy Bypass -File `"$PSScriptRoot\stop_core.ps1`"" -ForegroundColor Yellow
    Write-Host "  (never taskkill /F -- that is what wedges the camera)" -ForegroundColor Yellow
    exit 1
}

$devices = Get-Targets $Target
if (-not $devices) { Write-Host "nothing to reset." -ForegroundColor Red; exit 1 }

foreach ($d in $devices) {
    Write-Host "--- $($d.FriendlyName)" -ForegroundColor Cyan
    Write-Host "    $($d.InstanceId)  [$($d.Status)]"
    Write-Host "    disabling..."
    Disable-PnpDevice -InstanceId $d.InstanceId -Confirm:$false
    Start-Sleep -Seconds $SettleSeconds
    Write-Host "    enabling..."
    Enable-PnpDevice  -InstanceId $d.InstanceId -Confirm:$false
    Start-Sleep -Seconds $SettleSeconds
    $after = Get-PnpDevice -InstanceId $d.InstanceId
    $col = if ($after.Status -eq 'OK') { 'Green' } else { 'Red' }
    Write-Host "    now: $($after.Status)" -ForegroundColor $col
}

Write-Host ""
Write-Host "Verify:" -ForegroundColor Cyan
Write-Host "  camera : python `"$PSScriptRoot\cam_check.py`""
Write-Host "  esp32  : the port may come back as a different COMn -- re-run with -List"
