# One driver run on the real 5160 through the V9x Remote Agent: put the driver on the card under
# its own name, select it, reboot unattended, wait for the start-up programs to finish, run
# M8BENCH, fetch C:\M8BENCH.TXT and compare the last run. Prints the comparison.
#
#   tools\m8_card_cycle.ps1 -Drv <M8DISP.DRV> -Name M8DASH -Label card_dash [-Against card_ati1,card_ati2]
#
# -Name is the 8.3 name the driver is installed as (C:\WINDOWS\SYSTEM\<Name>.DRV), so the
# driver in use stays where it is until the reboot. Runs are labelled <Label>1..<Label><Runs>.
# A bench while the start-up programs still run is slow in every test, so the first run waits
# for C:\M8IDLE.EXE (tools/m8probe): CPU use at most 10 % for 10 s, at most -Settle seconds.
# A wedged card stops the script at the bench: ask the owner to restart, then read the log
# with -FetchOnly. -BenchOnly benches the driver already running (the card booted into it).

param(
  [Parameter(Mandatory)][string]$Label,
  [string]$Drv,
  [string]$Name,
  [string]$Against = 'card_ati1,card_ati2',
  [string]$CardHost = '192.168.68.62',
  [string]$Log = (Join-Path $env:TEMP 'm8_card_bench.txt'),
  [int]$Runs = 1,
  [int]$Settle = 600,
  [switch]$FetchOnly,
  [switch]$BenchOnly
)
$ErrorActionPreference = 'Stop'
$ctl = 'C:\Users\lycet\tools\v9x-remote-agent\host\v9xremote-0.12.0\scripts\v9xctl.ps1'
$root = Split-Path $PSScriptRoot
$key = 'HKEY_LOCAL_MACHINE\System\CurrentControlSet\Services\Class\Display\0000\DEFAULT'

function Agent([string]$verb, [hashtable]$a = @{}) {
  $out = & $ctl $verb @a -Host $CardHost -ConnectTimeoutSeconds 60 -Json
  if ($LASTEXITCODE -ne 0) { throw "v9xctl $verb failed ($LASTEXITCODE): $out" }
  return $out
}

if (-not $FetchOnly -and -not $BenchOnly) {
  if (-not $Drv -or -not $Name) { throw '-Drv and -Name are required unless -FetchOnly' }
  $drvName = $Name.ToLower() + '.drv'
  $reg = Join-Path $env:TEMP "$Name.REG"
  "REGEDIT4`r`n`r`n[$key]`r`n`"drv`"=`"$drvName`"`r`n" | Set-Content -NoNewline -Encoding ascii $reg
  Agent put @{ Source = $Drv; Destination = "C:\WINDOWS\SYSTEM\$Name.DRV" } | Out-Null
  Agent put @{ Source = $reg; Destination = "C:\$Name.REG" } | Out-Null
  Agent exec @{ Application = 'C:\WINDOWS\REGEDIT.EXE'; Arguments = "/S C:\$Name.REG"; TimeoutSeconds = 60 } | Out-Null
  # Read the key back: the boot must load this driver.
  Agent exec @{ Application = 'C:\WINDOWS\REGEDIT.EXE'; Arguments = "/E C:\V9XREMOTE\DISP.TXT `"$key`""; TimeoutSeconds = 60 } | Out-Null
  $disp = Join-Path $env:TEMP 'm8_disp.txt'
  Agent get @{ Source = 'C:\V9XREMOTE\DISP.TXT'; Destination = $disp } | Out-Null
  if (-not (Select-String -Path $disp -SimpleMatch "`"drv`"=`"$drvName`"" -Quiet)) {
    throw "registry does not select $drvName"
  }
  # Restart the way a person does, through Start > Shut Down > Restart: on this card the
  # agent's own reboot can end in a fatal exception 0D. A normal restart is up well inside
  # 4 minutes; a stuck one fails fast. The start-up programs are M8IDLE's to wait for.
  $boot = (Agent info | ConvertFrom-Json).BootCounter
  Agent input @{ Sequence = 'key CTRL+ESC; delay 1500; key U; delay 2500; key R; delay 500; key ENTER' } | Out-Null
  $back = $false
  for ($t = 0; $t -lt 24 -and -not $back; $t++) {
    Start-Sleep -Seconds 10
    $i = & $ctl info -Host $CardHost -ConnectTimeoutSeconds 5 -Json 2>$null 6>$null
    if ($LASTEXITCODE -eq 0) {
      $i = $i | ConvertFrom-Json
      $back = $i.BootCounter -ne $boot -and $i.DesktopReady
    }
  }
  # One line per restart: Windows 95 keeps no record of a fatal exception, so a restart
  # that never came back is the only trace, and this log is how they are counted.
  $after = if ($back) { $i.BootCounter } else { '-' }
  Add-Content -Path (Join-Path $env:TEMP 'm8_card_restarts.log') -Value (
    '{0}  {1}  boot {2} -> {3}  {4}' -f (Get-Date -Format 'yyyy-MM-dd HH:mm'), $Name, $boot, $after,
    $(if ($back) { 'ok' } else { 'did not come back' }))
  if (-not $back) { throw 'the card did not come back from the restart within 4 minutes' }
}
if (-not $FetchOnly) {
  if (-not $BenchOnly) {
    Agent put @{ Source = (Join-Path $root 'tools\m8probe\M8IDLE.EXE'); Destination = 'C:\M8IDLE.EXE' } | Out-Null
    Agent exec @{ Application = 'C:\M8IDLE.EXE'; Arguments = "10 10 $Settle"; TimeoutSeconds = $Settle + 60 } | Out-Null
  }
  foreach ($n in 1..$Runs) {
    Agent exec @{ Application = 'C:\M8BENCH.EXE'; Arguments = "$Label$n"; TimeoutSeconds = 300 } | Out-Null
  }
}
Agent get @{ Source = 'C:\M8BENCH.TXT'; Destination = $Log } | Out-Null
python -I (Join-Path $root 'tools\m8_bench_compare.py') $Log $Against "$Label$Runs"
