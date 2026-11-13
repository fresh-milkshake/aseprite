$ErrorActionPreference = 'Stop'
$root = Join-Path $env:RUNNER_TEMP ('aseprite-concurrent-recents-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Force $root | Out-Null
$env:ASEPRITE_USER_FOLDER = $root
@'
[general]
recent_items = 100
'@ | Set-Content (Join-Path $root 'aseprite.ini') -Encoding utf8
$probe = (Resolve-Path 'build/bin/recent_store_process_check.exe').Path
$workers = @()
try {
  foreach ($name in @('worker-a', 'worker-b')) {
    $workers += Start-Process $probe -ArgumentList @('worker', $name) -PassThru -NoNewWindow -WorkingDirectory $PWD.Path `
      -RedirectStandardOutput "$root/$name.log" -RedirectStandardError "$root/$name-error.log"
  }
  foreach ($worker in $workers) {
    if (-not $worker.WaitForExit(30000)) { throw 'Concurrent recent writer timed out' }
    if ($worker.ExitCode -ne 0) { throw 'Concurrent recent writer failed' }
  }
  & $probe verify-full
  if ($LASTEXITCODE -ne 0) { throw 'Concurrent history verification failed' }
  $gate = Join-Path $root 'exit-stale-process'
  $ready = Join-Path $root 'stale-snapshot-ready'
  $held = Start-Process $probe -ArgumentList @('hold', ('"' + $ready + '"'), ('"' + $gate + '"')) -PassThru -NoNewWindow -WorkingDirectory $PWD.Path `
    -RedirectStandardOutput "$root/stale.log" -RedirectStandardError "$root/stale-error.log"
  $workers += $held
  $deadline = [DateTime]::UtcNow.AddSeconds(10)
  while (-not (Test-Path $ready)) {
    if ($held.HasExited -or [DateTime]::UtcNow -gt $deadline) { throw 'Stale reader did not become ready' }
    Start-Sleep -Milliseconds 50
  }
  & $probe clear
  if ($LASTEXITCODE -ne 0) { throw 'Concurrent history clear failed' }
  Set-Content $gate 'exit'
  if (-not $held.WaitForExit(10000) -or $held.ExitCode -ne 0) { throw 'Stale process exit failed' }
  & $probe verify-empty
  if ($LASTEXITCODE -ne 0) { throw 'Cleared history was resurrected after stale exit' }
} finally {
  foreach ($worker in $workers) { if (-not $worker.HasExited) { $worker.Kill() } }
  Get-Content "$root/*.log" -ErrorAction SilentlyContinue
}
