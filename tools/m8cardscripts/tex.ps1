param([string]$Label)
# M8TEX on the card: one run after an idle wait, log to the silicon capture folder.
$root = 'C:\Users\lycet\RiderProjects\86Box-Inboard'
$ctl = 'C:\Users\lycet\tools\v9x-remote-agent\host\v9xremote-0.12.0\scripts\v9xctl.ps1'
$h = @{ Host = '192.168.68.62'; ConnectTimeoutSeconds = 60 }
$d = "$root\docs\captures\2026-11-07_silicon"
New-Item -ItemType Directory -Force $d | Out-Null
& $ctl put -Source "$root\tools\m8probe\M8TEX.EXE" -Destination 'C:\M8TEX.EXE' @h 2>&1 | Out-Null
"put exit $LASTEXITCODE"
$null = & $ctl exec -Application 'C:\M8IDLE.EXE' -Arguments '10 10 120' -TimeoutSeconds 180 @h 2>&1
"idle exit $LASTEXITCODE"
$null = & $ctl exec -Application 'C:\M8TEX.EXE' -Arguments $Label -TimeoutSeconds 120 @h 2>&1
"tex exit $LASTEXITCODE"
& $ctl get -Source 'C:\M8TEX.TXT' -Destination "$d\M8TEX.TXT" @h 2>&1 | Out-Null
Get-Content "$d\M8TEX.TXT" | Out-String
