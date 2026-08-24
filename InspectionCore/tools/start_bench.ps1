<#
.SYNOPSIS
  Bring the bench back after a restart: core + WebUI + browser + Claude Code.

.DESCRIPTION
  Four things have to be running, and the fourth is the one that is easy to
  forget: without the agent session there is nobody to read the results, react
  to a crash, or carry on the work until a human comes back.

    1. inspection core        (InspectionCore/run_core.sh, own window)
    2. WebUI preview server   (UI/WebUI `npm run preview` -> :8082)
    3. browser at :8082
    4. Claude Code, resumed into THIS conversation, cwd = the workspace root

  Started from Startup\visSele bench.lnk on logon, or by hand from the desktop.
  Everything is skip-if-already-running, so running it twice is harmless.

  The core gets its own window ON PURPOSE: it handles SIGINT through mainLoop,
  so Ctrl-C there is a clean shutdown. A core with no console can only be killed
  with taskkill /F, which throws the graceful path away.

  -NoBrowser   core + preview + claude only
  -NoClaude    skip the agent
  -Dev         browser at the vite dev server (:8081). ~9x the memory of the
               production bundle on an idle page (131 MB vs 14.4 MB JS heap,
               measured 2026-08-20) -- not what to leave running for days.
  -Session     resume a different Claude Code session id
#>
param(
  [switch]$NoBrowser,
  [switch]$NoClaude,
  [switch]$Dev,
  [string]$Session = '325bff37-610d-48da-bd59-dc6b3c23c835'
)

$ErrorActionPreference = 'Continue'
$repo  = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)   # ...\visSele
$ws    = Split-Path -Parent $repo                                # ...\workspace
$core  = Join-Path $repo 'InspectionCore'
$webui = Join-Path $repo 'UI\WebUI'
$port  = if ($Dev) { 8081 } else { 8082 }

function Test-Port($p) {
  $null -ne (Get-NetTCPConnection -LocalPort $p -State Listen -ErrorAction SilentlyContinue)
}

# --- 1. core -----------------------------------------------------------------
if (Get-Process visSele -ErrorAction SilentlyContinue) {
  Write-Host "core   : already running" -ForegroundColor Yellow
} else {
  $bash = @('C:\Program Files\Git\bin\bash.exe','C:\Program Files\Git\usr\bin\bash.exe') |
          Where-Object { Test-Path $_ } | Select-Object -First 1
  if (-not $bash) { Write-Host "core   : bash.exe not found (install Git for Windows)" -ForegroundColor Red }
  else {
    Write-Host "core   : starting (own window; Ctrl-C there = clean shutdown)" -ForegroundColor Green
    # Quote the -c command as ONE argument. Start-Process joins ArgumentList with
    # spaces, so an unquoted "cd X && ./run_core.sh" reaches bash as -c "cd" plus
    # loose words: bash cd's, exits, and the core never starts -- silently, with
    # no window left to show why. Measured 2026-08-20: launched fine by hand,
    # never once from this script.
    $inner = "cd '$($core -replace '\\','/')' && ./run_core.sh"
    Start-Process $bash -ArgumentList @('-lc', ('"' + $inner + '"'))
    Start-Sleep -Seconds 8
  }
}

# --- 2. WebUI ----------------------------------------------------------------
if (Test-Port $port) {
  Write-Host "webui  : already serving :$port" -ForegroundColor Yellow
} else {
  $cmd = if ($Dev) { 'npm run dev' } else { 'npm run preview' }
  Write-Host "webui  : starting ($cmd) on :$port" -ForegroundColor Green
  Start-Process cmd -ArgumentList '/c', "cd /d `"$webui`" && $cmd" -WindowStyle Minimized
  Start-Sleep -Seconds 6
}

# --- 3. browser --------------------------------------------------------------
if (-not $NoBrowser) {
  Write-Host "browser: opening http://localhost:$port/" -ForegroundColor Green
  Start-Process "http://localhost:$port/"
}

# --- 4. Claude Code ----------------------------------------------------------
# Resumed, not fresh: --resume <id> restores the conversation that was doing the
# work, so it comes back knowing what it was in the middle of. Falls back to
# --continue (most recent conversation in this directory) if the id is gone.
if (-not $NoClaude) {
  if (Get-Process claude -ErrorAction SilentlyContinue) {
    Write-Host "claude : already running" -ForegroundColor Yellow
  } else {
    $exe = "$env:USERPROFILE\.local\bin\claude.exe"
    if (-not (Test-Path $exe)) { $exe = (Get-Command claude -ErrorAction SilentlyContinue).Source }
    if (-not $exe) { Write-Host "claude : claude.exe not found" -ForegroundColor Red }
    else {
      $sessFile = Join-Path $env:USERPROFILE ".claude\projects\C--Users-w2110-Documents-workspace\$Session.jsonl"
      $arg = if (Test-Path $sessFile) { @('--resume', $Session) } else { @('--continue') }
      Write-Host ("claude : starting in {0} ({1})" -f $ws, ($arg -join ' ')) -ForegroundColor Green
      Start-Process $exe -ArgumentList $arg -WorkingDirectory $ws
    }
  }
}

Write-Host "`n--- state ---" -ForegroundColor Cyan
Write-Host ("core    : {0}" -f $(if (Get-Process visSele -ErrorAction SilentlyContinue) { 'running' } else { 'NOT running' }))
Write-Host ("core ws : {0}" -f $(if (Test-Port 4090) { '4090 open' } else { '4090 CLOSED' }))
Write-Host ("webui   : {0}" -f $(if (Test-Port $port) { "listening on :$port" } else { "NOT listening on :$port" }))
Write-Host ("claude  : {0}" -f $(if (Get-Process claude -ErrorAction SilentlyContinue) { 'running' } else { 'NOT running' }))