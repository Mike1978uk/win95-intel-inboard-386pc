# Stage the LS-120 Windows bed and run it through bed_launch.ps1.
# Same staging as ls120_bed_run.ps1 - master config, clean image, verified
# LS-120 media, driver checked at its destination - but it launches through
# bed_launch.ps1, so it never stops an 86Box it did not start.
#
#   tools\ls120_bed_stage.ps1 -Exe <86Box.exe> -Tag <name> [-Seconds 420]
#
# Result: vm_ls120win\LSPROBE_<tag>.TXT (written by the guest) and
# vm_ls120win\86box.log.<tag>.

param(
  [Parameter(Mandatory)] [string]$Exe,
  [Parameter(Mandatory)] [string]$Tag,
  [int]$Seconds = 420,
  [string]$Driver = "",
  [string]$Startup = ""
)
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot
$vm   = Join-Path $repo 'vm_ls120win'
$img  = Join-Path $vm 'ls120win.img'
if (-not $Driver)  { $Driver  = Join-Path $repo 'dist\ls120_mpd\LS120MP.MPD' }
if (-not $Startup) { $Startup = Join-Path $repo 'tools\fixtures\LSENUMJW.BAT' }

Copy-Item (Join-Path $vm '86box.cfg.master')    (Join-Path $vm '86box.cfg') -Force
Copy-Item (Join-Path $vm 'ls120win_clean.img')  $img -Force
Copy-Item (Join-Path $vm 'rd_verified_good.img') (Join-Path $vm 'rd.img') -Force

python "$repo\tools\fatcp.py" $img 'WINDOWS/SYSTEM/IOSUBSYS/LS120MP.MPD' $Driver --yes | Out-Null
if ($LASTEXITCODE -ne 0) { Write-Host "DEPLOY FAILED"; exit 1 }
$chk = Join-Path $env:TEMP "ls120_stage_check.mpd"
python "$repo\tools\fatls.py" $img --get 'C:\WINDOWS\SYSTEM\IOSUBSYS\LS120MP.MPD' $chk | Out-Null
$want = (Get-FileHash $Driver -Algorithm MD5).Hash.ToLower()
$got  = (Get-FileHash $chk -Algorithm MD5).Hash.ToLower()
if ($want -ne $got) { Write-Host "DRIVER MISMATCH at destination: $got, wanted $want"; exit 1 }
Write-Host "driver    $got (checked at its destination)"

$sname = [IO.Path]::GetFileName($Startup)
python "$repo\tools\fatcp.py" $img "WINDOWS/STARTM~1/PROGRAMS/STARTUP/$sname" $Startup --yes | Out-Null
if ($LASTEXITCODE -ne 0) { Write-Host "STARTUP DEPLOY FAILED"; exit 1 }
foreach ($f in 'C:\LSPROBE.TXT', 'C:\BOOTLOG.TXT') {
  try { python "$repo\tools\fatcp.py" $img --rm $f --yes 2>&1 | Out-Null } catch {}
}
Write-Host "startup   $sname"

# This bed keeps msserial: without a mouse Windows 95 stops on a modal dialog
# and nothing in StartUp runs (inboard-hw-debug technique 87).
$log = Join-Path $vm "86box.log.$Tag"
& "$repo\tools\bed_launch.ps1" -Exe $Exe -VmPath $vm -Seconds $Seconds -Log $log -AllowMouse
$out = Join-Path $vm "LSPROBE_$Tag.TXT"
if (Test-Path $out) { Remove-Item $out }
python "$repo\tools\fatls.py" $img --get 'C:\LSPROBE.TXT' $out 2>&1 | Out-Null
if (Test-Path $out) { Write-Host "--- guest's LSPROBE.TXT"; Get-Content $out } else { Write-Host "NO LSPROBE.TXT - the probe never ran" }
