param([string]$Exe, [string]$Dir, [string]$Target = '127.0.0.1', [int]$Seconds = 30, [string[]]$Files = @(), [string]$Keys = '')
# A program under test, on the bed by default: start it detached, let it run, read the census,
# take a picture, close it. -Dir holds the EXE; -Files its data, which land beside it.
$root = 'C:\Users\lycet\RiderProjects\86Box-Inboard'
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
& $ctl exec -Application 'C:\M8CENSUS.EXE' -Arguments "before-$name" -TimeoutSeconds 30 @h 2>&1 | Out-Null
& $ctl shell -Command 'C:\D3DS\RUN.BAT' -Detach @h 2>&1 | Out-Null
"start exit $LASTEXITCODE"
Start-Sleep -Seconds $Seconds
if ($Keys) { & $ctl input -Sequence $Keys @h 2>&1 | Out-Null; Start-Sleep -Seconds 10 }
& $ctl screenshot -OutFile "$tmp\$name.png" @h 2>&1 | Out-Null
"shot exit $LASTEXITCODE"
& $ctl exec -Application 'C:\M8CENSUS.EXE' -Arguments "during-$name" -TimeoutSeconds 30 @h 2>&1 | Out-Null
"census exit $LASTEXITCODE"
& $ctl input -Sequence 'key ALT+F4; delay 3000' @h 2>&1 | Out-Null
Start-Sleep -Seconds 5
& $ctl get -Source 'C:\M8CENSUS.TXT' -Destination "$tmp\M8CENSUS.TXT" @h 2>&1 | Out-Null
$t = Get-Content "$tmp\M8CENSUS.TXT"
$k = ($t | Select-String "^== M8CENSUS before-$name" | Select-Object -Last 1).LineNumber
if ($k) { $t[($k-1)..($t.Count-1)] | Out-String }
"picture: $tmp\$name.png"
$i = & $ctl info @h -Json 2>$null 6>$null | ConvertFrom-Json
"boot $($i.BootCounter) up $([int]($i.UptimeMilliseconds/1000)) desktop $($i.DesktopReady)"
