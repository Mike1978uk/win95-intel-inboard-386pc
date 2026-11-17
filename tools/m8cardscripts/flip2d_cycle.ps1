param([string]$Name, [Parameter(Mandatory)][string]$Out, [string]$Build = '', [int]$Seconds = 30,
      [string]$Census = '', [string]$Exe = 'flip2d.exe', [string]$Dir = 'references\dx61samples\ddraw\bin')
# One 8 bpp DirectDraw iteration on the 5160: install the build folder's DRV as <Name>.DRV and its
# M8DD32.DLL by WININIT, restart, then run the program for -Seconds with PERFLOG beside it.
# Without -Name the driver already running is measured. -Census puts a new M8CENSUS.EXE first.
# Writes the census section and PERFLOG.CSV to -Out and prints the frame and paging summary.
$ErrorActionPreference = 'Stop'
$root = 'C:\Users\lycet\RiderProjects\86Box-Inboard'
$ctl = 'C:\Users\lycet\tools\v9x-remote-agent\host\v9xremote-0.12.0\scripts\v9xctl.ps1'
$h = @{ Host = '192.168.68.62'; ConnectTimeoutSeconds = 60 }
$key = 'HKEY_LOCAL_MACHINE\System\CurrentControlSet\Services\Class\Display\0000\DEFAULT'
$t = Join-Path $env:TEMP 'card_peek'
New-Item -ItemType Directory -Force $t, $Out | Out-Null
Set-Location $root
if (-not $Build) { $Build = "$root\drivers\mach8_display\src\build" }
if ($Census) { & $ctl put -Source $Census -Destination 'C:\M8CENSUS.EXE' @h 2>&1 | Out-Null }
if ($Name) {
  if ((& $ctl stat -Path 'C:\WINDOWS\WININIT.INI' @h -Json | ConvertFrom-Json).Exists) { throw 'WININIT.INI exists: a rename is pending' }
  Copy-Item "$Build\M8DISP.DRV", "$Build\M8DD32.DLL" $Out
  & $ctl put -Source "$Out\M8DISP.DRV" -Destination "C:\WINDOWS\SYSTEM\$Name.DRV" @h 2>&1 | Out-Null
  & $ctl put -Source "$Out\M8DD32.DLL" -Destination 'C:\WINDOWS\SYSTEM\M8DD32.NEW' @h 2>&1 | Out-Null
  "[rename]`r`nC:\WINDOWS\SYSTEM\M8DD32.DLL=C:\WINDOWS\SYSTEM\M8DD32.NEW`r`n" | Set-Content -NoNewline -Encoding ascii "$t\WININIT.INI"
  & $ctl put -Source "$t\WININIT.INI" -Destination 'C:\WINDOWS\WININIT.INI' @h 2>&1 | Out-Null
  "REGEDIT4`r`n`r`n[$key]`r`n`"drv`"=`"$($Name.ToLower()).drv`"`r`n" | Set-Content -NoNewline -Encoding ascii "$t\$Name.REG"
  & $ctl put -Source "$t\$Name.REG" -Destination "C:\V9XREMOTE\REG\$Name.REG" @h 2>&1 | Out-Null
  & $ctl exec -Application 'C:\WINDOWS\REGEDIT.EXE' -Arguments "/S C:\V9XREMOTE\REG\$Name.REG" -TimeoutSeconds 60 @h 2>&1 | Out-Null
  . "$root\tools\m8cardscripts\card_restart.ps1"
  $r = Restart-Card -Ctl $ctl -Target '192.168.68.62' -Name $Name 6>$null 2>$null
  if (-not $r.Ok) { throw 'the card did not come back: ask the owner' }
  & $ctl get -Source 'C:\WINDOWS\SYSTEM\M8DD32.DLL' -Destination "$t\now.dll" @h 2>&1 | Out-Null
  $m = (Get-FileHash "$t\now.dll" -A MD5).Hash -eq (Get-FileHash "$Out\M8DD32.DLL" -A MD5).Hash
  "boot $($r.Boot), DLL on card matches build: $m"
  if (-not $m) { throw 'DLL mismatch' }
}
& $ctl exec -Application 'C:\M8IDLE.EXE' -Arguments '10 10 200' -TimeoutSeconds 260 @h 2>&1 | Out-Null
$lab = 'f' + (Get-Date -Format 'HHmmss')
& $ctl exec -Application 'C:\PERFLOG.EXE' -Arguments "$lab $($Seconds + 20)" -Detach @h 2>&1 | Out-Null
Start-Sleep 5
$s = & "$root\tools\m8cardscripts\sample.ps1" -Exe $Exe -Dir $Dir -Target 192.168.68.62 -Seconds $Seconds 2>&1 | Out-String
$k = $s.IndexOf('== M8CENSUS during')
$s.Substring([Math]::Max(0, $k)) | Set-Content "$Out\census_during.txt"
Start-Sleep 15
& $ctl get -Source 'C:\PERFLOG.CSV' -Destination "$Out\PERFLOG.CSV" @h 2>&1 | Out-Null
$rows = @(Import-Csv "$Out\PERFLOG.CSV" | Where-Object label -like "$lab*")
$f = [int]$rows[1].'VMM\cPageIns'; $l = [int]$rows[-1].'VMM\cPageIns'
$fo = [int]$rows[1].'VMM\cPageOuts'; $lo = [int]$rows[-1].'VMM\cPageOuts'
"paging over the run: page-ins $($l - $f), page-outs $($lo - $fo), locked $($rows[-2].'VMM\cpgLocked')"
Get-Content "$Out\census_during.txt" | Select-String '^8bpp'
