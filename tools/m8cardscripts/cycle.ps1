param([string]$Name, [string]$Label, [int]$Runs = 2)
# One driver iteration on the card, no owner: build, install under its own name, remote restart, bench at high priority.
$s = $PSScriptRoot
$root = 'C:\Users\lycet\RiderProjects\86Box-Inboard'
$o = "$env:TEMP\$($Name.ToLower())"
Push-Location $root
$null = & drivers\mach8_display\src\build.ps1 -Out $o 2>&1
Pop-Location
if (-not (Test-Path "$o\M8DISP.DRV")) { 'BUILD FAILED'; & "$root\drivers\mach8_display\src\build.ps1" -Out $o 2>&1 | Select-Object -Last 15; exit 1 }
foreach ($f in 'M8DISP.DRV','M8DD32.DLL') { $g = Get-Item "$o\$f"; "$f $($g.Length) md5 $((Get-FileHash $g.FullName -Algorithm MD5).Hash) $($g.LastWriteTime)" }
$W = 'C:\Users\lycet\tools\openwatcom'
$env:WATCOM = $W; $env:PATH = "$W\binnt64;$W\binnt;$env:PATH"; $env:INCLUDE = "$W\h;$W\h\nt"
Push-Location "$root\tools\m8d3dtest"
& wcl386 -q -bt=nt -l=nt_win -ox "-i=$W\h" "-i=$W\h\nt" "-i=$W\h\nt\directx" '-fe=M8D3DF.EXE' M8D3DF.C ddraw.lib 2>&1 | Select-String -Pattern 'Error' | ForEach-Object { $_.Line }
Pop-Location
& "$s\install_h.ps1" -Dir $o -Name $Name -Run $Label 2>&1 | Where-Object { "$_" -notmatch 'Transport error|Target was|No connection' }
$ctl = 'C:\Users\lycet\tools\v9x-remote-agent\host\v9xremote-0.12.0\scripts\v9xctl.ps1'
$h = @{ Host = '192.168.68.62'; ConnectTimeoutSeconds = 60 }
$c = "$root\docs\captures\2026-11-07_shape"
foreach ($n in 1..$Runs) {
  & $ctl exec -Application 'C:\M8IDLE.EXE' -Arguments '10 10 300' -TimeoutSeconds 360 @h 2>&1 | Out-Null
  & $ctl exec -Application 'C:\M8D3DF.EXE' -Arguments "$Label$n 3 high again" -TimeoutSeconds 240 @h 2>&1 | Select-String 'ExitCode|TimedOut' | ForEach-Object { $_.Line.Trim() }
}
& $ctl exec -Application 'C:\FB16CHK.EXE' -Arguments "$Label-after" -TimeoutSeconds 60 @h 2>&1 | Out-Null
& $ctl get -Source 'C:\M8D3DF.TXT' -Destination "$c\M8D3DF.TXT" @h 2>&1 | Out-Null
& $ctl get -Source 'C:\FB16CHK.TXT' -Destination "$c\FB16CHK.TXT" @h 2>&1 | Out-Null
Get-Content "$c\FB16CHK.TXT" -Tail 1
$t = Get-Content "$c\M8D3DF.TXT"
foreach ($n in 1..$Runs) {
  "--- $Label$n"
  $k = ($t | Select-String "^== M8D3DF $Label$n " | Select-Object -Last 1).LineNumber
  $t[($k - 1)..($t.Count - 1)] | Select-String 'hal_' | ForEach-Object { $_.Line -replace ' scene.*?flipcall','  flipcall' -replace '  blt .*?described \d+','' -replace 'flipcall.*?engine','engine' } | Select-Object -First 10
}
