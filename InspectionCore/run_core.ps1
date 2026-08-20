<#
Launch the inspection core on the Windows bench. PowerShell twin of run_core.sh.

    .\run_core.ps1                 real camera (HikRobot build)
    .\run_core.ps1 -Bench          no camera: synth cam_ts + dev console on 4099
    .\run_core.ps1 -Build NAME     force a build under InspectionCore\build\
    .\run_core.ps1 -Check          run the preflight checks and exit

IN THE FOREGROUND, ON PURPOSE. Ctrl-C is the only working shutdown: the core
handles SIGINT/SIGTERM and tears down through mainLoop, but a core started
detached has no console and no window, so Windows refuses anything but
`taskkill /F` ("this process can only be forcibly terminated"). Launch it in
the background and you have thrown the graceful path away.

That matters more than it sounds, because /F is not always survivable. Measured
2026-08-20: a core killed while the camera sat in TriggerMode(2) having never
grabbed a frame left the camera fine, but /F while it was free-running at 10fps
WEDGED it -- Windows PnP still reported OK while MV_CC_EnumDevices returned 0
devices and the next core spun in discovery forever. Recovery was a physical
USB replug. Ctrl-C in the foreground avoids the whole question.

The preflight checks are not ceremony. Every one is a failure that already cost
bench time, and all of them fail QUIETLY -- the core starts, the UI looks
normal, and the numbers are wrong.
#>
[CmdletBinding()]
param(
  [switch]$Bench,
  [string]$Build = "",
  [switch]$Check
)

$ErrorActionPreference = 'Continue'

$Here     = Split-Path -Parent $MyInvocation.MyCommand.Path      # InspectionCore\
$CoreDir  = Join-Path $Here 'Core0_1'
$BuildDir = Join-Path $Here 'build'
$Mingw    = 'C:\msys64\mingw64\bin'
$MvsRt    = 'C:\Program Files (x86)\Common Files\MVS\Runtime\Win64_x64'

function Say  ($m) { Write-Host "  $m" }
function OK   ($m) { Write-Host "  ok    $m" -ForegroundColor Green }
function Warn ($m) { Write-Host "  WARN  $m" -ForegroundColor Yellow }
function Die  ($m) { Write-Host "  STOP  $m" -ForegroundColor Red; exit 1 }

# objdump is needed by the build check below, so mingw goes on PATH first.
$env:PATH = "$Mingw;$env:PATH"

# --- which build -------------------------------------------------------------
# nohik-cv4 is built with FEATURE_HIKROBOT=OFF. With a real camera attached it
# does not fail -- it enumerates the BMP carousel and hands you a FAKE camera,
# with nothing on screen looking wrong. So the build is asked what it actually
# links, rather than trusted for its name.
function Test-HikCapable ($name) {
  $exe = Join-Path (Join-Path $BuildDir $name) 'visSele.exe'
  if (-not (Test-Path $exe)) { return $false }
  $out = & objdump -p $exe 2>$null | Select-String -Pattern 'MVCameraControl.dll' -Quiet
  return [bool]$out
}

if ([string]::IsNullOrEmpty($Build)) {
  if ($Bench) {
    $Build = 'nohik-cv4'
  } else {
    foreach ($b in @('win-mingw-msys', 'nohik-cv4')) {
      if (Test-HikCapable $b) { $Build = $b; break }
    }
    if ([string]::IsNullOrEmpty($Build)) {
      Die "no build under $BuildDir links MVCameraControl.dll -- rebuild with FEATURE_HIKROBOT=ON"
    }
  }
}
$Exe = Join-Path (Join-Path $BuildDir $Build) 'visSele.exe'
if (-not (Test-Path $Exe)) { Die "no such build: $Exe" }

Write-Host "core launcher"
Say "build   $Build"
if ($Bench) { Say "mode    bench (synth cam_ts, no camera)" } else { Say "mode    real camera" }

# --- PATH --------------------------------------------------------------------
# Four directories. The exe resolves DLLs through PATH, not through its own
# directory:
#   mingw64\bin   OpenCV 4.13 + libgomp
#   <build>       the build's own DLLs
#   nohik-cv4     MVCAMSDK_X64.DLL lives ONLY here -- a MindVision link-time
#                 dependency, required even though there is no MindVision camera
#   MVS runtime   MvCameraControl.dll (HikRobot)
$env:PATH = "$Mingw;$(Join-Path $BuildDir $Build);$(Join-Path $BuildDir 'nohik-cv4');$MvsRt;$env:PATH"

# --- preflight ---------------------------------------------------------------
Write-Host "preflight"
$fail = 0

# An orphan core holds 4090/4099 and COM3. The new one fails to bind and exits
# QUIETLY, so every command you then send reaches the OLD process -- and the run
# looks completely normal. This has happened.
$running = Get-Process visSele -ErrorAction SilentlyContinue
if ($running) {
  Warn "a core is ALREADY RUNNING (PID $($running.Id -join ', ')) -- it holds 4090/4099 and COM3."
  Say  "      this one would exit quietly and your commands would reach the old one."
  Say  "      stop it first:  Stop-Process -Name visSele -Force"
  $fail = 1
} else {
  OK "no other core running"
}

# A leftover Playwright browser does the PD CONNECT for you. Headless tooling
# then appears to work while actually riding on the browser's channel; kill the
# browser and the same tooling stops working.
if (Get-Process chrome-headless-shell -ErrorAction SilentlyContinue) {
  Warn "a headless Chromium is attached -- it will PD CONNECT behind your back"
  Say  "      Stop-Process -Name chrome-headless-shell -Force"
} else {
  OK "no orphan headless browser"
}

foreach ($dll in @('libopencv_core-413.dll', 'MVCAMSDK_X64.DLL')) {
  $hit = $null
  foreach ($d in ($env:PATH -split ';')) {
    if ([string]::IsNullOrWhiteSpace($d)) { continue }
    if (Test-Path (Join-Path $d $dll)) { $hit = $d; break }
  }
  if ($hit) { OK $dll } else { Warn "$dll NOT on PATH -- the exe will refuse to start"; $fail = 1 }
}

if (-not $Bench) {
  if (Test-Path (Join-Path $MvsRt 'MvCameraControl.dll')) {
    OK "MvCameraControl.dll (HikRobot)"
  } else {
    Warn "MvCameraControl.dll not found at $MvsRt -- is the MVS runtime installed?"; $fail = 1
  }
  if (Test-HikCapable $Build) {
    OK "$Build links the HikRobot SDK"
  } else {
    Warn "$Build does NOT link HikRobot -- you will get the BMP carousel, silently"
  }
}

if (Test-Path (Join-Path $CoreDir 'data')) {
  OK "data\ present (cwd will be Core0_1)"
} else {
  Warn "no $CoreDir\data -- defs and camera settings resolve relative to cwd"; $fail = 1
}

if ($Check) { exit $fail }
if ($fail -ne 0) { Write-Host ""; Die "preflight found problems above -- fix them, or use -Check to review" }

# --- go ----------------------------------------------------------------------
Set-Location $CoreDir
if ($Bench) {
  # Without the synth there is no camera for the clock calibration to learn
  # from: the board sits in 102 and lands in 112 with error 14
  # (CAM_CLOCK_CAL_FAILED). That is what "no camera" means to the state machine,
  # not a fault.
  $env:INSP_PERIF_CONSOLE   = '4099'
  $env:INSP_CAM_TS_SYNTH    = '1'
  $env:INSP_CAM_TS_OFFSET_US= '800'
  $env:INSP_CAM_TS_MULT     = '1.0'
  Say "console 4099, synth cam_ts = t_us*1.0 + 800us"
}
Write-Host ""
Write-Host "starting -- Ctrl-C for a clean shutdown (do NOT background this)"
Write-Host "----------------------------------------------------------------"
& $Exe
