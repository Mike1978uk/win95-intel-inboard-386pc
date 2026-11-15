param([Parameter(Mandatory)][string]$Build, [int]$Feed = 0, [int]$Trace = 0, [string]$Tag = 'roundtrip')
# A Mach8 build folder (-Build, holding M8DISP.DRV and M8DD32.DLL) on the card: the DRV as M8SPAN.DRV,
# M8DD32.DLL by WININIT,
# C:\M8D3D.INI [feed] on / [diag] trace as given (original kept), then bench, wormhole,
# 5 minutes of pings, bench.
$root = 'C:\Users\lycet\RiderProjects\86Box-Inboard'
$ctl = 'C:\Users\lycet\tools\v9x-remote-agent\host\v9xremote-0.12.0\scripts\v9xctl.ps1'
$bld = $Build
$h = @{ Host = '192.168.68.62'; ConnectTimeoutSeconds = 60 }
$out = "$root\docs\captures\2026-11-15_d8_roundtrip_card"
function Say($m) { "{0:HH:mm:ss} {1}" -f (Get-Date), $m | Tee-Object -FilePath "$out\run.log" -Append }
Set-Location $root
Say "=== $Tag : span build (DRV + DLL), feed=$Feed trace=$Trace"

if (-not (Test-Path "$out\card_M8D3D.INI.orig")) {
  $null = & $ctl get -Source 'C:\M8D3D.INI' -Destination "$out\card_M8D3D.INI.orig" @h 2>&1
}
$ini = Get-Content "$out\card_M8D3D.INI.orig" -Raw
$ini = $ini -replace '(?ms)(\[feed\][^\[]*?)\bon\s*=\s*\d', "`${1}on=$Feed"
$ini = $ini -replace '(?m)^\s*trace\s*=\s*\d', "trace=$Trace"
Set-Content -NoNewline -Encoding ascii "$out\card_M8D3D.INI" $ini
$null = & $ctl put -Source "$out\card_M8D3D.INI" -Destination 'C:\M8D3D.INI' @h 2>&1
Say "  INI: $(($ini -split "`r?`n" | Where-Object { $_ -match 'on=|trace=' }) -join ' ')"

# The DLL is renamed into place at the restart the card cycle makes.
$null = & $ctl put -Source "$bld\M8DD32.DLL" -Destination 'C:\WINDOWS\SYSTEM\M8DD32.NEW' @h 2>&1
"[rename]`r`nC:\WINDOWS\SYSTEM\M8DD32.DLL=C:\WINDOWS\SYSTEM\M8DD32.NEW`r`n" | Set-Content -NoNewline -Encoding ascii "$out\WININIT.INI"
$null = & $ctl put -Source "$out\WININIT.INI" -Destination 'C:\WINDOWS\WININIT.INI' @h 2>&1

& .\tools\m8_card_cycle.ps1 -Drv "$bld\M8DISP.DRV" -Name M8SPAN -Label card_spn -Against 'card_shf1,card_shf2' -Runs 1 2>&1 |
  Select-String -Pattern 'geometric|differ|^== card_spn|idle:' | ForEach-Object { Say "  $_" }
$null = & $ctl get -Source 'C:\WINDOWS\SYSTEM\M8DD32.DLL' -Destination "$out\now_dll.bin" @h 2>&1
$m = (Get-FileHash "$out\now_dll.bin" -Algorithm MD5).Hash; $w = (Get-FileHash "$bld\M8DD32.DLL" -Algorithm MD5).Hash
Say "  DLL md5 $m (want $w)"
Remove-Item "$out\now_dll.bin"
if ($m -ne $w) { Say 'WRONG DLL: stop'; exit 1 }

& "$root\tools\m8cardscripts\card_wh_probe.ps1" -Tag $Tag
if ($LASTEXITCODE -ne 0) { Say 'stopped after wormhole'; exit 1 }
& .\tools\m8_card_cycle.ps1 -BenchOnly -Label card_spnw -Against 'card_spn1' -Runs 1 2>&1 |
  Select-String -Pattern 'geometric|differ|^== card_spnw|idle:' | ForEach-Object { Say "  $_" }
Say "bench after wormhole exit $LASTEXITCODE"
