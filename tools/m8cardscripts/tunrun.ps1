param([string]$Exe, [string]$Dir, [string]$Target = '127.0.0.1', [int]$Seconds = 60, [string[]]$Files = @(), [string]$Label = 'run')
# A sample built to close itself and log its fps (scratch rebuild): start it, wait, read fps and census.
$ctl = 'C:\Users\lycet\tools\v9x-remote-agent\host\v9xremote-0.12.0\scripts\v9xctl.ps1'
$h = @{ Host = $Target; ConnectTimeoutSeconds = 60 }
$name = [IO.Path]::GetFileNameWithoutExtension($Exe)
$tmp = Join-Path $env:TEMP 'sample_run'
New-Item -ItemType Directory -Force $tmp | Out-Null
& $ctl mkdir -Path 'C:\D3DS' @h 2>&1 | Out-Null
& $ctl put -Source "$Dir\$Exe" -Destination "C:\D3DS\$Exe" @h 2>&1 | Out-Null
"put exit $LASTEXITCODE"
foreach ($f in $Files) { & $ctl put -Source $f -Destination "C:\D3DS\$([IO.Path]::GetFileName($f))" @h 2>&1 | Out-Null }
"cd \D3DS`r`n$Exe`r`n" | Set-Content -NoNewline -Encoding ascii "$tmp\RUN.BAT"
& $ctl put -Source "$tmp\RUN.BAT" -Destination 'C:\D3DS\RUN.BAT' @h 2>&1 | Out-Null
$null = & $ctl exec -Application 'C:\M8CENSUS.EXE' -Arguments "before-$Label" -TimeoutSeconds 30 @h 2>&1
$null = & $ctl shell -Command 'C:\D3DS\RUN.BAT' -Detach @h 2>&1
"start exit $LASTEXITCODE"
Start-Sleep -Seconds $Seconds
$r = & $ctl info @h -Json 2>$null 6>$null
"agent exit $LASTEXITCODE"
if ($LASTEXITCODE -ne 0) { 'AGENT DOWN'; exit 1 }
$i = $r | ConvertFrom-Json
"boot $($i.BootCounter) up $([int]($i.UptimeMilliseconds/1000)) desktop $($i.DesktopReady)"
$null = & $ctl exec -Application 'C:\M8CENSUS.EXE' -Arguments "after-$Label" -TimeoutSeconds 30 @h 2>&1
& $ctl get -Source 'C:\M8SAMPLE.TXT' -Destination "$tmp\M8SAMPLE.TXT" @h 2>&1 | Out-Null
"fps log exit $LASTEXITCODE"
if ($LASTEXITCODE -eq 0) { Get-Content "$tmp\M8SAMPLE.TXT" | Select-Object -Last 26 | Out-String }
& $ctl get -Source 'C:\M8CENSUS.TXT' -Destination "$tmp\M8CENSUS.TXT" @h 2>&1 | Out-Null
$t = Get-Content "$tmp\M8CENSUS.TXT"
$k = ($t | Select-String "^== M8CENSUS after-$Label" | Select-Object -Last 1).LineNumber
if ($k) { $t[($k-1)..($t.Count-1)] | ForEach-Object { $_.Substring(0, [Math]::Min(460, $_.Length)) } }
