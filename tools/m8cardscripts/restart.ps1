param([string]$Name, [string]$Shot)
$ctl = 'C:\Users\lycet\tools\v9x-remote-agent\host\v9xremote-0.12.0\scripts\v9xctl.ps1'
$h = @{ Host = '192.168.68.62'; ConnectTimeoutSeconds = 60 }
$tmp = Join-Path $env:TEMP 'card_peek'
$o = "$env:TEMP\$($Name.ToLower())"
if ($Shot) { & $ctl screenshot -Destination $Shot @h 2>&1 | Out-Null; "screenshot: $(Test-Path $Shot)" }
$boot = (& $ctl info @h -Json | ConvertFrom-Json).BootCounter
"boot before: $boot"
& $ctl input -Sequence 'key CTRL+ESC; delay 1500; key U' @h 2>&1 | Out-Null
Start-Sleep -Seconds 25
& $ctl input -Sequence 'key R; delay 500; key ENTER' @h 2>&1 | Out-Null
$ok = $false
for ($t = 0; $t -lt 30 -and -not $ok; $t++) {
  Start-Sleep -Seconds 10
  $i = & $ctl info -Host '192.168.68.62' -ConnectTimeoutSeconds 5 -Json 2>$null 6>$null
  if ($LASTEXITCODE -eq 0) { $i = $i | ConvertFrom-Json; $ok = $i.BootCounter -ne $boot -and $i.DesktopReady }
}
Add-Content -Path (Join-Path $env:TEMP 'm8_card_restarts.log') -Value ('{0}  {1}  boot {2} -> {3}  {4}' -f (Get-Date -Format 'yyyy-MM-dd HH:mm'), $Name, $boot, $(if ($ok) { $i.BootCounter } else { '-' }), $(if ($ok) { 'ok' } else { 'did not come back' }))
if (-not $ok) { 'did not come back'; exit 1 }
"back: boot $($i.BootCounter)"
& $ctl get -Source 'C:\WINDOWS\SYSTEM\M8DD32.DLL' -Destination "$tmp\now.dll" @h 2>&1 | Out-Null
$m = (Get-FileHash "$tmp\now.dll" -Algorithm MD5).Hash
$w = (Get-FileHash "$o\M8DD32.DLL" -Algorithm MD5).Hash
"DLL on card $m, built $w, match $($m -eq $w)"
