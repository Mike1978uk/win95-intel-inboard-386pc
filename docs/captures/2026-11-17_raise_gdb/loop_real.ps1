# The card's sequence on the debug bed: wormhole at 8 bpp, then flip2d, four times; stop at a raise or a silent agent.
$root = 'C:\Users\lycet\RiderProjects\86Box-Inboard'
$ctl = 'C:\Users\lycet\tools\v9x-remote-agent\host\v9xremote-0.12.0\scripts\v9xctl.ps1'
$c = "$root\docs\captures\2026-11-17_raise_gdb"
Set-Location $root
function Raises { (Select-String -Path "$c\raise_log.txt" -Pattern RAISE).Count }
$base = Raises
foreach ($k in 1..8) {
  $exe = @('wormhole.exe', 'flip2d.exe')[($k - 1) % 2]
  $d = Join-Path $c "real$k"; New-Item -ItemType Directory -Force $d | Out-Null
  & tools\m8cardscripts\sample.ps1 -Exe $exe -Dir references\dx61samples\ddraw\bin -Seconds 25 -ShotPort 9891 -VmPath vm_5160_dbg -ShotDir $d 2>&1 | Out-Null
  Start-Sleep 5
  & tools\bed_shot.ps1 -VmPath vm_5160_dbg -Port 9891 -Out "$d\after.png" 2>&1 | Out-Null
  $i = & $ctl info -Host 127.0.0.1 -ConnectTimeoutSeconds 20 -Json 2>$null
  "run $k $exe raises $((Raises) - $base) agent $([bool]$i)"
  if ((Raises) -gt $base -or -not $i) { break }
}
