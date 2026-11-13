param([int]$Port = 9869, [string]$OutDir, [string]$Mpd, [string]$Label = 'bx')
$ctl = 'C:\Users\lycet\tools\v9x-remote-agent\host\v9xremote-0.12.0\scripts\v9xctl.ps1'
$h = @{ Host = '127.0.0.1'; Port = $Port; ConnectTimeoutSeconds = 30 }
$dest = 'C:\WINDOWS\SYSTEM\IOSUBSYS\XTIDEMP.MPD'
New-Item -ItemType Directory -Force $OutDir | Out-Null
function Say($m) { "{0:HH:mm:ss} {1}" -f (Get-Date), $m | Tee-Object -FilePath "$OutDir\run.log" -Append }
function Enter-AtMenu {
  $w = New-Object -ComObject WScript.Shell
  $p = Get-Process 86Box | Select-Object -First 1
  if ($w.AppActivate($p.Id)) { Start-Sleep -Milliseconds 400; $w.SendKeys('{ENTER}'); Say 'enter sent' }
}
Say "--- bed XT-CF test $Mpd"
$wi = & $ctl stat -Path 'C:\WINDOWS\WININIT.INI' @h -Json 2>&1 | ConvertFrom-Json
if ($wi.Exists) { Say 'WININIT pending: stop'; exit 1 }
& $ctl get -Source $dest -Destination "$OutDir\orig.mpd" @h 2>&1 | Out-Null
Say "bed had md5 $((Get-FileHash "$OutDir\orig.mpd" -Algorithm MD5).Hash)"
& $ctl put -Source "$OutDir\orig.mpd" -Destination 'C:\XTIDEMP.BAK' @h 2>&1 | Out-Null
& $ctl put -Source $Mpd -Destination 'C:\WINDOWS\SYSTEM\IOSUBSYS\XTIDEMP.NEW' @h 2>&1 | Out-Null
"[rename]`r`n$dest=C:\WINDOWS\SYSTEM\IOSUBSYS\XTIDEMP.NEW`r`n" | Set-Content -NoNewline -Encoding ascii "$OutDir\WININIT.INI"
& $ctl put -Source "$OutDir\WININIT.INI" -Destination 'C:\WINDOWS\WININIT.INI' @h 2>&1 | Out-Null
$boot = (& $ctl info @h -Json | ConvertFrom-Json).BootCounter
Say "boot before: $boot"
& $ctl input -Sequence 'key CTRL+ESC; delay 3000; key U' @h 2>&1 | Out-Null
Start-Sleep -Seconds 45
& $ctl input -Sequence 'key R; delay 1500; key ENTER' @h 2>&1 | Out-Null
$i = $null
for ($t = 0; $t -lt 60 -and -not $i; $t++) {
  Start-Sleep -Seconds 10
  if ($t -eq 15 -or $t -eq 30) {
    $null = & $ctl info -Host 127.0.0.1 -Port $Port -ConnectTimeoutSeconds 5 -Json 2>$null 6>$null
    if ($LASTEXITCODE -ne 0) { Enter-AtMenu }
  }
  $r = & $ctl info -Host 127.0.0.1 -Port $Port -ConnectTimeoutSeconds 5 -Json 2>$null 6>$null
  if ($LASTEXITCODE -eq 0) { $r = $r | ConvertFrom-Json; if ($r.BootCounter -ne $boot -and $r.DesktopReady) { $i = $r } }
}
if (-not $i) { Say 'NOT RESTARTED: stop'; exit 1 }
Say "restarted: boot $($i.BootCounter)"
& $ctl get -Source $dest -Destination "$OutDir\now.mpd" @h 2>&1 | Out-Null
$m = (Get-FileHash "$OutDir\now.mpd" -Algorithm MD5).Hash
$w = (Get-FileHash $Mpd -Algorithm MD5).Hash
Say "MPD md5 $m, wanted $w"
if ($m -ne $w) { Say 'WRONG MPD: stop'; exit 1 }
& $ctl exec -Application 'C:\M8IDLE.EXE' -Arguments '10 10 300' -TimeoutSeconds 360 @h 2>&1 | Out-Null
$r = & $ctl exec -Application 'C:\PAGETIME.EXE' -Arguments "$Label" -TimeoutSeconds 1500 @h -Json 2>&1 | Out-String
try { $j = $r | ConvertFrom-Json; Say "PAGETIME exit $($j.ExitCode) timedout $($j.TimedOut) ms $($j.ElapsedMilliseconds)" } catch { Say "$r" }
$r = & $ctl exec -Application 'C:\XTSECT.EXE' -Arguments "$Label" -TimeoutSeconds 400 @h -Json 2>&1 | Out-String
try { $j = $r | ConvertFrom-Json; Say "XTSECT exit $($j.ExitCode) timedout $($j.TimedOut) ms $($j.ElapsedMilliseconds)" } catch { Say "$r" }
# Every file read back through the driver: a wrong sector would show in the hash.
foreach ($f in 'C:\WINDOWS\SYSTEM\KERNEL32.DLL', 'C:\WINDOWS\SYSTEM\GDI32.DLL', 'C:\WINDOWS\SYSTEM\VMM32.VXD', 'C:\WINDOWS\EXPLORER.EXE') {
  & $ctl get -Source $f -Destination "$OutDir\chk.bin" @h 2>&1 | Out-Null
  Say "$f $((Get-Item "$OutDir\chk.bin").Length) $((Get-FileHash "$OutDir\chk.bin" -Algorithm MD5).Hash)"
}
Say 'finished'
