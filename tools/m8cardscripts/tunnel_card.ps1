param([string]$Exe, [string]$Label, [string]$Target = '192.168.68.62', [int]$Settle = 30, [int]$Gap = 30)
# A self-starting full-screen sample on the card: fps from the census, one big indexed call a frame.
# Normal priority, agent on: the agent has to read the census and close it. Nothing else runs.
$root = 'C:\Users\lycet\RiderProjects\86Box-Inboard'
$ctl = 'C:\Users\lycet\tools\v9x-remote-agent\host\v9xremote-0.12.0\scripts\v9xctl.ps1'
$h = @{ Host = $Target; ConnectTimeoutSeconds = 60 }
$tmp = Join-Path $env:TEMP 'card_tunnel'
New-Item -ItemType Directory -Force $tmp | Out-Null
$d = "$root\references\dx61samples\d3dim"
& $ctl mkdir -Path 'C:\D3DS' @h 2>&1 | Out-Null
& $ctl put -Source $Exe -Destination 'C:\D3DS\M8TUNF.EXE' @h 2>&1 | Out-Null
& $ctl put -Source "$d\src\tunneldp\tex2.bmp" -Destination 'C:\D3DS\tex2.bmp' @h 2>&1 | Out-Null
& $ctl put -Source "$root\tools\m8d3dtest\M8CENSUS.EXE" -Destination 'C:\M8CENSUS.EXE' @h 2>&1 | Out-Null
& $ctl put -Source "$root\tools\m8d3dtest\M8HIGH.EXE" -Destination 'C:\M8HIGH.EXE' @h 2>&1 | Out-Null
"files $LASTEXITCODE"
$null = & $ctl exec -Application 'C:\M8IDLE.EXE' -Arguments '10 10 120' -TimeoutSeconds 180 @h 2>&1
"cd \D3DS`r`nM8TUNF.EXE`r`n" | Set-Content -NoNewline -Encoding ascii "$tmp\RUN.BAT"
& $ctl put -Source "$tmp\RUN.BAT" -Destination 'C:\D3DS\RUN.BAT' @h 2>&1 | Out-Null
$null = & $ctl shell -Command 'C:\D3DS\RUN.BAT' -Detach @h 2>&1
"start $LASTEXITCODE $(Get-Date -Format HH:mm:ss)"
Start-Sleep -Seconds $Settle
$null = & $ctl exec -Application 'C:\M8CENSUS.EXE' -Arguments "$Label-a" -TimeoutSeconds 60 @h 2>&1
"census a $LASTEXITCODE"
$t0 = Get-Date
Start-Sleep -Seconds $Gap
$null = & $ctl exec -Application 'C:\M8CENSUS.EXE' -Arguments "$Label-b" -TimeoutSeconds 60 @h 2>&1
"census b $LASTEXITCODE"
$el = ((Get-Date) - $t0).TotalSeconds
$null = & $ctl input -Sequence 'key ALT+F4; delay 3000' @h 2>&1
"close $LASTEXITCODE"
& $ctl get -Source 'C:\M8CENSUS.TXT' -Destination "$tmp\M8CENSUS.TXT" @h 2>&1 | Out-Null
$t = Get-Content "$tmp\M8CENSUS.TXT"
$k = ($t | Select-String "^== M8CENSUS $Label-a" | Select-Object -Last 1).LineNumber
if (-not $k) { 'no census'; exit 1 }
$s = $t[($k-1)..($t.Count-1)]
$s | Select-String '^contexts|^calls|^tris' | ForEach-Object { $_.Line.Substring(0, [Math]::Min(120, $_.Line.Length)) }
$c = $s | Select-String '^calls exec \d+ one \d+ indexed (\d+)' | ForEach-Object { [int]$_.Matches[0].Groups[1].Value }
$u = $s | Select-String '^tris (\d+)' | ForEach-Object { [int]$_.Matches[0].Groups[1].Value }
if ($c.Count -ge 2) { $n = $c[1] - $c[0]; "elapsed $([int]$el) s  big calls $n  tris $($u[1]-$u[0])  fps about $([math]::Round($n/$el,2))  tris a second $([int](($u[1]-$u[0])/$el))" }
