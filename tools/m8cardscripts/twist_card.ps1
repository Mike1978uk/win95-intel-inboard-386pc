param([string]$Label, [string]$Target = '192.168.68.62', [int]$Start = 20, [int]$Settle = 60, [int]$Gap = 60, [string]$Keys = '',
      [string]$Sdk = (Join-Path $env:TEMP 'dx5sdk_x\bin'))
# DX5 SDK twist on the card, full screen from the start: M8TWIST.EXE, made from twist.exe by
# tools/m8d3dtest/twist_full_patch.py; two census reads and Alt+F4, as tunnel_card.ps1. twist.exe
# and its four textures come from the DX5 SDK (references/d3d_programs/idx5sdk.exe -> DX5SDK.EXE
# -> cdrom\sdk\bin), extracted into -Sdk.
$root = 'C:\Users\lycet\RiderProjects\86Box-Inboard'
$ctl = 'C:\Users\lycet\tools\v9x-remote-agent\host\v9xremote-0.12.0\scripts\v9xctl.ps1'
$h = @{ Host = $Target; ConnectTimeoutSeconds = 60 }
$tmp = Join-Path $env:TEMP 'card_twist'
New-Item -ItemType Directory -Force $tmp | Out-Null
& $ctl mkdir -Path 'C:\TWIST' @h 2>&1 | Out-Null
foreach ($f in 'M8TWIST.EXE','checker.ppm', 'tex2.ppm', 'tex7.ppm', 'win95.ppm') {
  & $ctl put -Source "$Sdk\$f" -Destination "C:\TWIST\$f" @h 2>&1 | Out-Null
  if ($LASTEXITCODE -ne 0) { "put $f failed"; exit 1 }
}
& $ctl put -Source "$root\tools\m8d3dtest\M8CENSUS.EXE" -Destination 'C:\M8CENSUS.EXE' @h 2>&1 | Out-Null
$null = & $ctl exec -Application 'C:\M8IDLE.EXE' -Arguments '10 10 120' -TimeoutSeconds 180 @h 2>&1
"cd \TWIST`r`nM8TWIST.EXE`r`n" | Set-Content -NoNewline -Encoding ascii "$tmp\RUN.BAT"
& $ctl put -Source "$tmp\RUN.BAT" -Destination 'C:\TWIST\RUN.BAT' @h 2>&1 | Out-Null
$null = & $ctl shell -Command 'C:\TWIST\RUN.BAT' -Detach @h 2>&1
"start $LASTEXITCODE $(Get-Date -Format HH:mm:ss)"
Start-Sleep -Seconds $Start
# -Keys 'key TAB; delay 2000; key TAB': the framework's stats text, drawn by GDI into the back
# buffer in RAM, is sent to the card at every Flip; Tab toggles it.
if ($Keys) { $null = & $ctl input -Sequence $Keys @h 2>&1; "keys $LASTEXITCODE" }
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
$s | Select-String '^flipwhy|^contexts|^calls|^tris|^feed|^mode' | ForEach-Object { $_.Line.Substring(0, [Math]::Min(140, $_.Line.Length)) }
$ck = $s | Select-String '^clock (\d+) ours (\d+) flip (\d+) engine (\d+) flips (\d+)' | ForEach-Object { ,($_.Matches[0].Groups[1..5] | ForEach-Object { [double]$_.Value }) }
if ($ck.Count -ge 2) { $f = $ck[0][0]; $n = $ck[1][4] - $ck[0][4]; if ($n -gt 0) { "per flip, ms: ours $([math]::Round(($ck[1][1]-$ck[0][1])*1000/$f/$n,1))  flip $([math]::Round(($ck[1][2]-$ck[0][2])*1000/$f/$n,1))  flips $n  whole frame $([math]::Round($el*1000/$n,0))  fps $([math]::Round($n/$el,2))" } else { 'no flips between the reads' } }
