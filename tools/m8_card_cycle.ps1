# One driver run on the real 5160 through the V9x Remote Agent: put the driver on the card under
# its own name, select it, reboot unattended, run M8BENCH twice (the first run after a boot is
# unreliable), fetch C:\M8BENCH.TXT and compare the second run. Prints the comparison.
#
#   tools\m8_card_cycle.ps1 -Drv <M8DISP.DRV> -Name M8DASH -Label card_dash [-Against card_ati1,card_ati2]
#
# -Name is the 8.3 name the driver is installed as (C:\WINDOWS\SYSTEM\<Name>.DRV), so the
# driver in use stays where it is until the reboot. Runs are labelled <Label>1 and <Label>2.
# A wedged card stops the script at the bench: ask the owner to restart, then read the log
# with -FetchOnly. -BenchOnly benches the driver already running (the card booted into it).

param(
  [Parameter(Mandatory)][string]$Label,
  [string]$Drv,
  [string]$Name,
  [string]$Against = 'card_ati1,card_ati2',
  [string]$CardHost = '192.168.68.62',
  [string]$Log = (Join-Path $env:TEMP 'm8_card_bench.txt'),
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
  Agent reboot @{ WaitSeconds = 600 } | Out-Null
  Agent wait-desktop @{ WaitSeconds = 300 } | Out-Null
}
if (-not $FetchOnly) {
  foreach ($n in 1, 2) {
    Agent exec @{ Application = 'C:\M8BENCH.EXE'; Arguments = "$Label$n"; TimeoutSeconds = 300 } | Out-Null
  }
}
Agent get @{ Source = 'C:\M8BENCH.TXT'; Destination = $Log } | Out-Null
python -I (Join-Path $root 'tools\m8_bench_compare.py') $Log $Against "$($Label)2"
