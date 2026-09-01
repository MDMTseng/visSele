# Start a long soak as processes this session does not own.
#
#   powershell -ExecutionPolicy Bypass -File soak_start.ps1 [-Minutes 480]
#
# Everything here was previously launched as tracked background tasks and was
# killed four times in one afternoon, always in the gap between hourly
# stretches, so eight hours only ever accumulated one. Start-Process detaches:
# these keep running whatever happens to the session that started them, and the
# way back in is the peripheral console on 4099 plus the JSONL on disk.
#
# WHY webctld STAYS UP FOR THE WHOLE RUN and is not just a setup step: the
# core's perif channel is created and destroyed with its WS clients
# (sendcJsonTo_perifCH says so, and this session spent several minutes on "no
# perif channel" before finding it). webctld holds the page, the page holds the
# socket, the socket holds the channel the board is reached through. Close it
# and the board is on its own -- which is safe, it faults on host timeout, but
# it is the end of the run.
param([int]$Minutes = 480)

$ErrorActionPreference = 'Stop'
$webctl = $PSScriptRoot
$repo   = Resolve-Path (Join-Path $webctl '..\..\..\..')
$core   = Join-Path $repo 'InspectionCore\Core0_1'
$exe    = Join-Path $repo 'export_v2\app\2.0.0-rc2\Core\visSele.exe'
$logs   = Join-Path $webctl 'soak_logs'
New-Item -ItemType Directory -Force -Path $logs | Out-Null

function Start-Bg($name, $file, $argline, $wd) {
  # -ArgumentList refuses an empty string, and the core takes no arguments, so
  # the parameter has to be absent rather than blank. Splatting is the only way
  # to make a parameter conditionally not exist.
  $opt = @{
    FilePath = $file; WorkingDirectory = $wd; WindowStyle = 'Hidden'; PassThru = $true
    RedirectStandardOutput = (Join-Path $logs "$name.out")
    RedirectStandardError  = (Join-Path $logs "$name.err")
  }
  if ($argline) { $opt.ArgumentList = $argline }
  $p = Start-Process @opt
  "  $name pid $($p.Id)"
  return $p
}

# Nothing is assumed to be free: a half-dead predecessor holding 8081 or COM3 is
# the failure that looks like a bug in whatever starts next.
foreach ($port in 8081, 8765) {
  $c = Get-NetTCPConnection -State Listen -LocalPort $port -ErrorAction SilentlyContinue | Select -First 1
  if ($c) { Stop-Process -Id $c.OwningProcess -Force -ErrorAction SilentlyContinue; "  freed port $port" }
}
Get-Process visSele -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Seconds 2

"starting detached:"
$env:INSP_PERIF_CONSOLE = '4099'
Start-Bg 'core'  $exe '' $core | Out-Null
Start-Bg 'serve' 'node' 'serve_dist.mjs 8081 ../../dist-soak' $webctl | Out-Null
Start-Sleep -Seconds 3
Start-Bg 'webctld' 'node' 'webctld.mjs' $webctl | Out-Null
Start-Sleep -Seconds 12

"setup (loads the recipe, enters 全檢, sets both layers to auto):"
& node (Join-Path $webctl 'soak_auto.mjs') setup
if ($LASTEXITCODE -ne 0) { "SETUP FAILED - not starting the sampler"; exit 1 }

Start-Bg 'sampler' 'node' "soak_auto.mjs run $Minutes" $webctl | Out-Null
"sampling for $Minutes minutes; soak_auto.jsonl grows one line a minute"
