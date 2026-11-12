param([string]$Name, [string]$Shot)
$ctl = 'C:\Users\lycet\tools\v9x-remote-agent\host\v9xremote-0.12.0\scripts\v9xctl.ps1'
$h = @{ Host = '192.168.68.62'; ConnectTimeoutSeconds = 60 }
$tmp = Join-Path $env:TEMP 'card_peek'
$o = "$env:TEMP\$($Name.ToLower())"
if ($Shot) { & $ctl screenshot -Destination $Shot @h 2>&1 | Out-Null; "screenshot: $(Test-Path $Shot)" }
. "$PSScriptRoot\card_restart.ps1"
$r = Restart-Card -Ctl $ctl -Target '192.168.68.62' -Name $Name
"boot before: $($r.Before)"
if (-not $r.Ok) { $(if ($r.WentDown) { 'did not come back' } else { 'never went down: the Shut Down dialog may be waiting' }); exit 1 }
"back: boot $($r.Boot)"
& $ctl get -Source 'C:\WINDOWS\SYSTEM\M8DD32.DLL' -Destination "$tmp\now.dll" @h 2>&1 | Out-Null
$m = (Get-FileHash "$tmp\now.dll" -Algorithm MD5).Hash
$w = (Get-FileHash "$o\M8DD32.DLL" -Algorithm MD5).Hash
"DLL on card $m, built $w, match $($m -eq $w)"
