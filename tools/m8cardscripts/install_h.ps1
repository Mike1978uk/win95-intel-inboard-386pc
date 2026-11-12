param([string]$Dir, [string]$Name, [string]$Run, [string]$Target = '192.168.68.62')
$ErrorActionPreference = 'Stop'
$ctl = 'C:\Users\lycet\tools\v9x-remote-agent\host\v9xremote-0.12.0\scripts\v9xctl.ps1'
$root = 'C:\Users\lycet\RiderProjects\86Box-Inboard'
$key = 'HKEY_LOCAL_MACHINE\System\CurrentControlSet\Services\Class\Display\0000\DEFAULT'
function Agent([string]$verb, [hashtable]$a = @{}) {
  $tries = if ($verb -eq 'put') { 3 } else { 1 }
  for ($t = 1; $t -le $tries; $t++) {
    $out = & $ctl $verb @a -Host $Target -ConnectTimeoutSeconds 60 -Json
    if ($LASTEXITCODE -eq 0) { return $out }
    if ($t -lt $tries) { Start-Sleep -Seconds 10 }
  }
  throw "v9xctl $verb failed ($LASTEXITCODE): $out"
}
$tmp = Join-Path $env:TEMP 'card_peek'
New-Item -ItemType Directory -Force $tmp | Out-Null

# The way back: M8D3DD's DLL, once.
$card = $Target -eq '192.168.68.62'
$back = Agent stat @{ Path = 'C:\V9XREMOTE\REG\M8DD32.DD' } | ConvertFrom-Json
if ($card -and -not $back.Exists) {
  Agent get @{ Source = 'C:\WINDOWS\SYSTEM\M8DD32.DLL'; Destination = "$tmp\M8DD32.DD" } | Out-Null
  $m = (Get-FileHash "$tmp\M8DD32.DD" -Algorithm MD5).Hash
  if ($m -ne '69FF48E5F3946A4BCDA63E5408DDAB1C') { throw "card DLL is $m, not M8D3DD's" }
  Agent put @{ Source = "$tmp\M8DD32.DD"; Destination = 'C:\V9XREMOTE\REG\M8DD32.DD' } | Out-Null
  "backup of M8D3DD's DLL: C:\V9XREMOTE\REG\M8DD32.DD"
}
$wayback = Agent stat @{ Path = 'C:\V9XREMOTE\REG\M8D3DD.REG' } | ConvertFrom-Json
"M8D3DD.REG on card: $($wayback.Exists)"

# Tools and list. The name a desktop copy of M8NOAG runs under picks the list.
Agent put @{ Source = "$root\tools\fb16test\FB16CHK.EXE"; Destination = 'C:\FB16CHK.EXE' } | Out-Null
Agent put @{ Source = "$root\tools\m8probe\M8SHAPE.EXE"; Destination = 'C:\M8SHAPE.EXE' } | Out-Null
Agent put @{ Source = "$root\tools\m8d3dtest\M8D3DF.EXE"; Destination = 'C:\M8D3DF.EXE' } | Out-Null
$lst = "$tmp\NOAGPAGE.LST"
@(
  "# Paging only, about 6 minutes, hands off, first thing after a restart. Card against frame buffer before and after.",
  "C:\FB16CHK.EXE $Run-start",
  "&C:\PERFLOG.EXE $Run 300",
  "C:\SLEEP.EXE 10",
  "C:\PAGETIME.EXE $Run",
  "C:\WINCLOSE.EXE WAIT PERFLOG",
  "C:\FB16CHK.EXE $Run-end"
) -join "`r`n" | Set-Content -NoNewline -Encoding ascii $lst
Agent put @{ Source = $lst; Destination = 'C:\V9XREMOTE\NOAGPAGE.LST' } | Out-Null
$desk = Agent stat @{ Path = 'C:\WINDOWS\DESKTOP\NOAGPAGE.EXE' } | ConvertFrom-Json
if ($card -and -not $desk.Exists) {
  Agent get @{ Source = 'C:\WINDOWS\DESKTOP\NOAGDISK.EXE'; Destination = "$tmp\NOAGPAGE.EXE" } | Out-Null
  Agent put @{ Source = "$tmp\NOAGPAGE.EXE"; Destination = 'C:\WINDOWS\DESKTOP\NOAGPAGE.EXE' } | Out-Null
}

# The driver, under its own name; the DLL by WININIT at the restart.
$drvName = $Name.ToLower() + '.drv'
$wi = Agent stat @{ Path = 'C:\WINDOWS\WININIT.INI' } | ConvertFrom-Json
if ($wi.Exists) { throw 'WININIT.INI exists: a rename is pending, look first' }
Agent put @{ Source = "$Dir\M8DISP.DRV"; Destination = "C:\WINDOWS\SYSTEM\$Name.DRV" } | Out-Null
Agent put @{ Source = "$Dir\M8DD32.DLL"; Destination = 'C:\WINDOWS\SYSTEM\M8DD32.NEW' } | Out-Null
"[rename]`r`nC:\WINDOWS\SYSTEM\M8DD32.DLL=C:\WINDOWS\SYSTEM\M8DD32.NEW`r`n" | Set-Content -NoNewline -Encoding ascii "$tmp\WININIT.INI"
Agent put @{ Source = "$tmp\WININIT.INI"; Destination = 'C:\WINDOWS\WININIT.INI' } | Out-Null
"REGEDIT4`r`n`r`n[$key]`r`n`"drv`"=`"$drvName`"`r`n" | Set-Content -NoNewline -Encoding ascii "$tmp\$Name.REG"
Agent put @{ Source = "$tmp\$Name.REG"; Destination = "C:\V9XREMOTE\REG\$Name.REG" } | Out-Null
Agent exec @{ Application = 'C:\WINDOWS\REGEDIT.EXE'; Arguments = "/S C:\V9XREMOTE\REG\$Name.REG"; TimeoutSeconds = 60 } | Out-Null
Agent exec @{ Application = 'C:\WINDOWS\REGEDIT.EXE'; Arguments = "/E C:\V9XREMOTE\DISP.TXT `"$key`""; TimeoutSeconds = 60 } | Out-Null
Agent get @{ Source = 'C:\V9XREMOTE\DISP.TXT'; Destination = "$tmp\disp.txt" } | Out-Null
if (-not (Select-String -Path "$tmp\disp.txt" -SimpleMatch "`"drv`"=`"$drvName`"" -Quiet)) { throw "registry does not select $drvName" }
"registry selects $drvName"

# Restart the way a person does: the agent's own reboot can end in a fatal exception here.
. "$PSScriptRoot\card_restart.ps1"
$r = Restart-Card -Ctl $ctl -Target $Target -Name $Name
if (-not $r.Ok) { throw $(if ($r.WentDown) { 'the card did not come back within 5 minutes' } else { 'the card never went down: the Shut Down dialog may be waiting' }) }
"back: boot $($r.Boot)"
Agent get @{ Source = 'C:\WINDOWS\SYSTEM\M8DD32.DLL'; Destination = "$tmp\now.dll" } | Out-Null
"DLL on card md5 $((Get-FileHash "$tmp\now.dll" -Algorithm MD5).Hash)"
Agent get @{ Source = "C:\WINDOWS\SYSTEM\$Name.DRV"; Destination = "$tmp\now.drv" } | Out-Null
"DRV on card md5 $((Get-FileHash "$tmp\now.drv" -Algorithm MD5).Hash)"
