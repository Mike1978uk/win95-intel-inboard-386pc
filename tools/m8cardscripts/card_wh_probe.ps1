param([string]$Tag)
$root = 'C:\Users\lycet\RiderProjects\86Box-Inboard'
$ctl = 'C:\Users\lycet\tools\v9x-remote-agent\host\v9xremote-0.12.0\scripts\v9xctl.ps1'
$h = @{ Host = '192.168.68.62'; ConnectTimeoutSeconds = 15 }
$out = "$root\docs\captures\2026-11-15_d8_roundtrip_card"
New-Item -ItemType Directory -Force $out | Out-Null
function Say($m) { "{0:HH:mm:ss} {1}" -f (Get-Date), $m | Tee-Object -FilePath "$out\run.log" -Append }
$i = & $ctl info @h -Json | ConvertFrom-Json
Say "--- ${Tag}: boot $($i.BootCounter), up $([int]($i.UptimeMilliseconds/1000)) s"
$null = & $ctl exec -Application 'C:\M8IDLE.EXE' -Arguments '10 10 400' -TimeoutSeconds 460 -Host 192.168.68.62 -ConnectTimeoutSeconds 60 2>&1
Say "idle exit $LASTEXITCODE"
& "$root\tools\m8cardscripts\sample.ps1" -Exe 'wormhole.exe' -Dir "$root\references\dx61samples\ddraw\bin" -Target 192.168.68.62 -Seconds 25 2>&1 |
  Where-Object { $_ -match 'exit|boot' } | ForEach-Object { Say "  $_" }
$t0 = Get-Date
for ($k = 0; $k -lt 30; $k++) {
  $r = & $ctl ping @h -Json 2>$null
  $ok = $LASTEXITCODE -eq 0
  Say ("  +{0,3} s agent {1}" -f [int]((Get-Date) - $t0).TotalSeconds, $(if ($ok) { 'ok' } else { 'NO ANSWER' }))
  if (-not $ok) { $miss++ ; if ($miss -ge 3) { Say 'agent gone 3 times: card faulted or hung'; exit 1 } } else { $miss = 0 }
  Start-Sleep -Seconds 10
}
Say 'agent answered for 5 minutes after wormhole'
