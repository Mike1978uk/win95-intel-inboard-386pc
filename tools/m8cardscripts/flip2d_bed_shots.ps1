param([Parameter(Mandatory)][string]$Name, [Parameter(Mandatory)][string]$Out, [string]$Build = '',
      [string]$VmPath = 'C:\Users\lycet\RiderProjects\86Box-Inboard\vm_5160_dbg', [int]$Port = 9891)
# Pixels for an 8 bpp DirectDraw build on the bed (bed vm_5160_dbg, started with -Automation 9891, desktop
# 640x480x8, flip2d staged in C:\D3DS by sample.ps1): install as <Name>.DRV with M8DD32.DLL by WININIT,
# restart, run flip2d, take four full-frame pictures 8 s apart, write grid.png and the census lines.
# The bed never flips 8 bpp pages (worklist X22), so the pictures judge pixels, not frame rate.
$ErrorActionPreference = 'Stop'
$root = 'C:\Users\lycet\RiderProjects\86Box-Inboard'
$ctl = 'C:\Users\lycet\tools\v9x-remote-agent\host\v9xremote-0.12.0\scripts\v9xctl.ps1'
$h = @{ Host = '127.0.0.1'; ConnectTimeoutSeconds = 60 }
$key = 'HKEY_LOCAL_MACHINE\System\CurrentControlSet\Services\Class\Display\0000\DEFAULT'
$t = Join-Path $env:TEMP 'bed_peek'
New-Item -ItemType Directory -Force $t, $Out | Out-Null
Set-Location $root
if (-not $Build) { $Build = "$root\drivers\mach8_display\src\build" }
if ((& $ctl stat -Path 'C:\WINDOWS\WININIT.INI' @h -Json | ConvertFrom-Json).Exists) { throw 'WININIT.INI exists: a rename is pending' }
& $ctl put -Source "$root\tools\m8d3dtest\M8CENSUS.EXE" -Destination 'C:\M8CENSUS.EXE' @h 2>&1 | Out-Null
& $ctl put -Source "$Build\M8DISP.DRV" -Destination "C:\WINDOWS\SYSTEM\$Name.DRV" @h 2>&1 | Out-Null
& $ctl put -Source "$Build\M8DD32.DLL" -Destination 'C:\WINDOWS\SYSTEM\M8DD32.NEW' @h 2>&1 | Out-Null
"[rename]`r`nC:\WINDOWS\SYSTEM\M8DD32.DLL=C:\WINDOWS\SYSTEM\M8DD32.NEW`r`n" | Set-Content -NoNewline -Encoding ascii "$t\WININIT.INI"
& $ctl put -Source "$t\WININIT.INI" -Destination 'C:\WINDOWS\WININIT.INI' @h 2>&1 | Out-Null
"REGEDIT4`r`n`r`n[$key]`r`n`"drv`"=`"$($Name.ToLower()).drv`"`r`n" | Set-Content -NoNewline -Encoding ascii "$t\$Name.REG"
& $ctl put -Source "$t\$Name.REG" -Destination "C:\V9XREMOTE\REG\$Name.REG" @h 2>&1 | Out-Null
& $ctl exec -Application 'C:\WINDOWS\REGEDIT.EXE' -Arguments "/S C:\V9XREMOTE\REG\$Name.REG" -TimeoutSeconds 60 @h 2>&1 | Out-Null
. "$root\tools\m8cardscripts\card_restart.ps1"
$r = Restart-Card -Ctl $ctl -Target '127.0.0.1' -Name $Name 6>$null 2>$null
if (-not $r.Ok) { throw 'the bed did not come back' }
"bed boot $($r.Boot)"
& $ctl exec -Application 'C:\M8CENSUS.EXE' -Arguments 'before-bed' -TimeoutSeconds 30 @h 2>&1 | Out-Null
& $ctl shell -Command 'C:\D3DS\RUN.BAT' -Detach @h 2>&1 | Out-Null
Start-Sleep 25
foreach ($k in 1..4) { & "$root\tools\bed_shot.ps1" -VmPath $VmPath -Port $Port -Out "$Out\s$k.png" 2>&1 | Out-Null; Start-Sleep 8 }
& $ctl exec -Application 'C:\M8CENSUS.EXE' -Arguments 'during-bed' -TimeoutSeconds 30 @h 2>&1 | Out-Null
& $ctl input -Sequence 'key ALT+F4; delay 3000' @h 2>&1 | Out-Null
& $ctl get -Source 'C:\M8CENSUS.TXT' -Destination "$t\M8CENSUS.TXT" @h 2>&1 | Out-Null
$c = Get-Content "$t\M8CENSUS.TXT"
$k = ($c | Select-String '^== M8CENSUS during-bed' | Select-Object -Last 1).LineNumber
$c[($k - 1)..($c.Count - 1)] | Set-Content "$Out\census_bed.txt"
Get-Content "$Out\census_bed.txt" | Select-String '^8bpp'
python -I -c "from PIL import Image; import sys; o=sys.argv[1]; ims=[Image.open(o+'/s%d.png'%k).convert('RGB').resize((320,240)) for k in range(1,5)]; g=Image.new('RGB',(1280,240)); [g.paste(im,(320*i,0)) for i,im in enumerate(ims)]; g.save(o+'/grid.png')" $Out
"grid: $Out\grid.png"
