param([string]$Label)
# The driver already running: one bench, no restart.
$root = 'C:\Users\lycet\RiderProjects\86Box-Inboard'
$ctl = 'C:\Users\lycet\tools\v9x-remote-agent\host\v9xremote-0.12.0\scripts\v9xctl.ps1'
$h = @{ Host = '192.168.68.62'; ConnectTimeoutSeconds = 60 }
$c = "$root\docs\captures\2026-11-07_shape"
$i = & $ctl info @h -Json | ConvertFrom-Json
"boot $($i.BootCounter) uptime $([int]($i.UptimeMilliseconds/1000)) s desktop $($i.DesktopReady)"
$tmp = Join-Path $env:TEMP 'card_peek'
& $ctl exec -Application 'C:\WINDOWS\REGEDIT.EXE' -Arguments '/E C:\V9XREMOTE\DISP.TXT "HKEY_LOCAL_MACHINE\System\CurrentControlSet\Services\Class\Display\0000\DEFAULT"' -TimeoutSeconds 60 @h 2>&1 | Out-Null
& $ctl get -Source 'C:\V9XREMOTE\DISP.TXT' -Destination "$tmp\disp.txt" @h 2>&1 | Out-Null
(Select-String -Path "$tmp\disp.txt" -Pattern '"drv"').Line
& $ctl get -Source 'C:\WINDOWS\SYSTEM\M8DD32.DLL' -Destination "$tmp\now.dll" @h 2>&1 | Out-Null
"DLL md5 $((Get-FileHash "$tmp\now.dll" -Algorithm MD5).Hash)"
& $ctl exec -Application 'C:\M8IDLE.EXE' -Arguments '10 10 300' -TimeoutSeconds 360 @h 2>&1 | Out-Null
& $ctl exec -Application 'C:\M8D3DF.EXE' -Arguments "$Label 3 high again" -TimeoutSeconds 240 @h 2>&1 | Select-String 'ExitCode|TimedOut' | ForEach-Object { $_.Line.Trim() }
& $ctl exec -Application 'C:\FB16CHK.EXE' -Arguments "$Label-after" -TimeoutSeconds 60 @h 2>&1 | Out-Null
& $ctl get -Source 'C:\M8D3DF.TXT' -Destination "$c\M8D3DF.TXT" @h 2>&1 | Out-Null
& $ctl get -Source 'C:\FB16CHK.TXT' -Destination "$c\FB16CHK.TXT" @h 2>&1 | Out-Null
Get-Content "$c\FB16CHK.TXT" -Tail 1
$t = Get-Content "$c\M8D3DF.TXT"
$k = ($t | Select-String "^== M8D3DF $Label " | Select-Object -Last 1).LineNumber
$t[($k - 1)..($t.Count - 1)] | Select-String 'hal_' | ForEach-Object { $_.Line -replace ' scene.*?flipcall','  flipcall' -replace '  blt .*?described \d+','' -replace 'flipcall.*?engine','engine' } | Select-Object -First 10
