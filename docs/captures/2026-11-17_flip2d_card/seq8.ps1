# Desktop already 640x480x8: flip2d (no mode change), close, then PERFLOG started, three times.
# Stops at the first silent agent. Records the agent's screen mode first.
$root = 'C:\Users\lycet\RiderProjects\86Box-Inboard'
$ctl = 'C:\Users\lycet\tools\v9x-remote-agent\host\v9xremote-0.12.0\scripts\v9xctl.ps1'
$h = @{ Host = '192.168.68.62'; ConnectTimeoutSeconds = 30 }
$d = "$root\docs\captures\2026-11-17_flip2d_card"
Set-Location $root
function Info { $j = & $ctl info @h -Json 2>$null; if ($j) { $j | ConvertFrom-Json } }
$i = Info; "{0:HH:mm:ss} start boot $($i.BootCounter) screen $($i.ScreenWidth)x$($i.ScreenHeight) bpp $($i.BitsPerPixel)" -f (Get-Date)
foreach ($k in 1..3) {
  & tools\m8cardscripts\sample.ps1 -Exe flip2d.exe -Dir references\dx61samples\ddraw\bin -Target 192.168.68.62 -Seconds 30 -ShotDir $d 2>&1 | Out-Null
  Start-Sleep 5
  $i = Info; if (-not $i) { "{0:HH:mm:ss} run $k after flip2d: agent silent" -f (Get-Date); break }
  & $ctl exec -Application 'C:\PERFLOG.EXE' -Arguments "seq8_$k 20" -Detach @h 2>&1 | Out-Null
  Start-Sleep 25
  $i = Info; if (-not $i) { "{0:HH:mm:ss} run $k after PERFLOG: agent silent" -f (Get-Date); break }
  "{0:HH:mm:ss} run $k clean, boot $($i.BootCounter)" -f (Get-Date)
}
