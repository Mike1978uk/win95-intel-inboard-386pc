param([string]$Label)
# M8CURVE on the card: one run after an idle wait, log to the session's capture folder.
$root = 'C:\Users\lycet\RiderProjects\86Box-Inboard'
$ctl = 'C:\Users\lycet\tools\v9x-remote-agent\host\v9xremote-0.12.0\scripts\v9xctl.ps1'
$h = @{ Host = '192.168.68.62'; ConnectTimeoutSeconds = 60 }
$d = "$root\docs\captures\2026-11-07_curves"
New-Item -ItemType Directory -Force $d | Out-Null
& $ctl put -Source "$root\tools\m8probe\M8CURVE.EXE" -Destination 'C:\M8CURVE.EXE' @h 2>&1 | Out-Null
"put exit $LASTEXITCODE"
$null = & $ctl exec -Application 'C:\M8IDLE.EXE' -Arguments '10 10 120' -TimeoutSeconds 180 @h 2>&1
"idle exit $LASTEXITCODE"
$null = & $ctl exec -Application 'C:\M8CURVE.EXE' -Arguments $Label -TimeoutSeconds 200 @h 2>&1
"curve exit $LASTEXITCODE"
& $ctl get -Source 'C:\M8CURVE.TXT' -Destination "$d\M8CURVE.TXT" @h 2>&1 | Out-Null
Get-Content "$d\M8CURVE.TXT" | Out-String
