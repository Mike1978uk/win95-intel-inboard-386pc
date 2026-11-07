param([string]$OutDir, [string]$Vxd, [int]$Loops = 3, [string]$Label = 'bz')
$ctl = 'C:\Users\lycet\tools\v9x-remote-agent\host\v9xremote-0.12.0\scripts\v9xctl.ps1'
$h = @{ Host = '127.0.0.1'; ConnectTimeoutSeconds = 30 }
New-Item -ItemType Directory -Force $OutDir | Out-Null
function Say($m) { "{0:HH:mm:ss} {1}" -f (Get-Date), $m | Tee-Object -FilePath "$OutDir\run.log" -Append }
function Enter-AtMenu {
  $w = New-Object -ComObject WScript.Shell
  $p = Get-Process 86Box | Select-Object -First 1
  if ($w.AppActivate($p.Id)) { Start-Sleep -Milliseconds 400; $w.SendKeys('{ENTER}'); Say 'enter sent' }
}
Say "--- bed swap test $Vxd"
$wi = & $ctl stat -Path 'C:\WINDOWS\WININIT.INI' @h -Json 2>&1 | ConvertFrom-Json
if ($wi.Exists) { Say 'WININIT pending: stop'; exit 1 }
$bak = & $ctl stat -Path 'C:\SWAPCOMP.BAK' @h -Json 2>&1 | ConvertFrom-Json
if (-not $bak.Exists) {
  & $ctl get -Source 'C:\SWAPCOMP.VXD' -Destination "$OutDir\orig.vxd" @h 2>&1 | Out-Null
  & $ctl put -Source "$OutDir\orig.vxd" -Destination 'C:\SWAPCOMP.BAK' @h 2>&1 | Out-Null
  Say "kept original as C:\SWAPCOMP.BAK, md5 $((Get-FileHash "$OutDir\orig.vxd" -Algorithm MD5).Hash)"
}
& $ctl put -Source $Vxd -Destination 'C:\SWAPCOMP.NEW' @h 2>&1 | Out-Null
"[rename]`r`nC:\SWAPCOMP.VXD=C:\SWAPCOMP.NEW`r`n" | Set-Content -NoNewline -Encoding ascii "$OutDir\WININIT.INI"
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
    $null = & $ctl info -Host 127.0.0.1 -ConnectTimeoutSeconds 5 -Json 2>$null 6>$null
    if ($LASTEXITCODE -ne 0) { Enter-AtMenu }
  }
  $r = & $ctl info -Host 127.0.0.1 -ConnectTimeoutSeconds 5 -Json 2>$null 6>$null
  if ($LASTEXITCODE -eq 0) { $r = $r | ConvertFrom-Json; if ($r.BootCounter -ne $boot -and $r.DesktopReady) { $i = $r } }
}
if (-not $i) { Say 'NOT RESTARTED: stop'; exit 1 }
Say "restarted: boot $($i.BootCounter)"
& $ctl get -Source 'C:\SWAPCOMP.VXD' -Destination "$OutDir\now.vxd" @h 2>&1 | Out-Null
$m = (Get-FileHash "$OutDir\now.vxd" -Algorithm MD5).Hash
$w = (Get-FileHash $Vxd -Algorithm MD5).Hash
Say "VxD md5 $m, wanted $w"
if ($m -ne $w) { Say 'WRONG VXD: stop'; exit 1 }
& $ctl exec -Application 'C:\M8IDLE.EXE' -Arguments '10 10 400' -TimeoutSeconds 460 @h 2>&1 | Out-Null
& $ctl exec -Application 'C:\PERFLOG.EXE' -Arguments "$Label 600" -Detach @h 2>&1 | Out-Null
Start-Sleep -Seconds 10
for ($n = 1; $n -le $Loops; $n++) {
  Say "loop ${n}, PAGETIME"
  $r = & $ctl exec -Application 'C:\PAGETIME.EXE' -Arguments "$Label$n" -TimeoutSeconds 1500 @h -Json 2>&1 | Out-String
  try { $j = $r | ConvertFrom-Json; Say "  exit $($j.ExitCode) timedout $($j.TimedOut) ms $($j.ElapsedMilliseconds)" } catch { Say "  $r" }
}
Start-Sleep -Seconds 20
& $ctl get -Source 'C:\PERFLOG.CSV' -Destination "$OutDir\PERFLOG.CSV" @h 2>&1 | Out-Null
$l = Get-Content "$OutDir\PERFLOG.CSV"
$head = $l[0] -split ','
$last = ($l | Where-Object { $_ -like "$Label*" } | Select-Object -Last 1)
Say "header fields $($head.Count)"
if ($last) {
  $f = $last -split ','
  Say "last row fields $($f.Count)"
  $names = $head | Where-Object { $_ -like 'SWAPCOMP*' -or $_ -like 'VMM\cPage*' }
  foreach ($n in $names) { $k = [array]::IndexOf($head, $n); Say ("  by header {0} = {1}" -f $n, $f[$k]) }
  Say "  tail: $(($f | Select-Object -Last 16) -join ',')"
  Say "  head tail: $(($head | Select-Object -Last 16) -join ',')"
} else { Say 'no PERFLOG row' }
Say 'finished'
