# One boot of the LS-120 bed (vm_ls120win) with a named driver.
#
#   pwsh -File tools/ls120_bed_run.ps1 -Driver <path to LS120MP.MPD> -Tag name [-Seconds 600]
#
# Every run gets a fresh config, a fresh system image and a fresh MEDIUM, in
# that order, before anything is deployed. The medium matters as much as the
# system image here: ECP writes were found on 2026-09-13 to put bytes at LBA 0,
# so a run that reuses yesterday's rd.img is testing damage, not the driver.
#
# 86box.log is where pclog output lands, which is where [ECPDIAG] and the EPAT
# bridge trace live. It is rotated to 86box.log.<tag> at the end so a run can
# be cited later (technique 89 - an artefact nobody can name is not evidence).

param(
    [Parameter(Mandatory = $true)] [string] $Driver,
    [Parameter(Mandatory = $true)] [string] $Tag,
    [int]    $Seconds = 600,
    [string] $VmPath  = "C:\Users\lycet\RiderProjects\86Box-Inboard\vm_ls120win",
    [string] $ExePath = "C:\Users\lycet\RiderProjects\86Box-Inboard\86box_upstream\build\src\86Box.exe"
)

$ErrorActionPreference = 'Stop'
$repo = "C:\Users\lycet\RiderProjects\86Box-Inboard"
$img  = Join-Path $VmPath "ls120win.img"
$rd   = Join-Path $VmPath "rd.img"

foreach ($t in @($ExePath,
                 (Join-Path $VmPath "86box.cfg.master"),
                 (Join-Path $VmPath "ls120win_clean.img"),
                 (Join-Path $VmPath "rd_verified_good.img"),
                 $Driver)) {
    if (-not (Test-Path $t)) { Write-Output "MISSING: $t"; exit 1 }
}

# Technique 70: say which emulator binary is about to run, and when it was built.
$exeTime = (Get-Item $ExePath).LastWriteTime
Write-Output "86Box.exe built $exeTime"

Get-Process 86Box -EA SilentlyContinue | Stop-Process -Force
Start-Sleep 1

Copy-Item (Join-Path $VmPath "86box.cfg.master") (Join-Path $VmPath "86box.cfg") -Force
Copy-Item (Join-Path $VmPath "ls120win_clean.img") $img -Force
Copy-Item (Join-Path $VmPath "rd_verified_good.img") $rd -Force

$mach = (Select-String -Path (Join-Path $VmPath "86box.cfg") -Pattern '^machine = (.+)$').Matches[0].Groups[1].Value
if ($mach -ne "ibmxt_inboard386") { Write-Output "WRONG MACHINE: $mach"; exit 1 }

python "$repo\tools\fatcp.py" $img "WINDOWS/SYSTEM/IOSUBSYS/LS120MP.MPD" $Driver --yes
if ($LASTEXITCODE -ne 0) { Write-Output "DEPLOY FAILED"; exit 1 }

# Verify at the DESTINATION, never the staging copy (technique 75).
$uut = Join-Path $env:TEMP "ls120_under_test.mpd"
Remove-Item $uut -EA SilentlyContinue
python "$repo\tools\fatls.py" $img --get "C:\WINDOWS\SYSTEM\IOSUBSYS\LS120MP.MPD" $uut | Out-Null
if (-not (Test-Path $uut)) { Write-Output "driver NOT in the image after deploy"; exit 1 }
Write-Output ("driver in the image: md5 " + (Get-FileHash $uut -Algorithm MD5).Hash.ToLower())

python "$repo\tools\fatcp.py" $img --rm "C:\BOOTLOG.TXT" 2>&1 | Out-Null
python "$repo\tools\fatclean.py" $img | Out-Null

$log = Join-Path $VmPath "86box.log"
Remove-Item $log -EA SilentlyContinue

# -L is not optional: without it pclog output (the [ECPDIAG] census and the
# whole EPAT bridge trace) goes to a console nobody is reading and the run
# produces no evidence at all. One run was lost to this on 2026-09-13.
$p = Start-Process $ExePath -ArgumentList @("-P", $VmPath, "-L", $log) `
        -WorkingDirectory $VmPath -PassThru
Write-Output "86Box pid $($p.Id), running $Seconds s..."
$p | Wait-Process -Timeout $Seconds -EA SilentlyContinue
if (-not $p.HasExited) { $p | Stop-Process -Force }
Start-Sleep 2

if (Test-Path $log) {
    Copy-Item $log (Join-Path $VmPath "86box.log.$Tag") -Force
    Write-Output ("86box.log.$Tag  " + (Get-Item $log).Length + " bytes")
}
python "$repo\tools\fatls.py" $img --get "C:\BOOTLOG.TXT" (Join-Path $VmPath "bootlog_$Tag.txt") | Out-Null
if (Test-Path (Join-Path $VmPath "bootlog_$Tag.txt")) {
    Write-Output "--- ls120mp lines from BOOTLOG ---"
    Select-String -Path (Join-Path $VmPath "bootlog_$Tag.txt") -Pattern 'ls120' | ForEach-Object { $_.Line }
} else {
    Write-Output "NO BOOTLOG.TXT - the run may not have reached Windows. Read the screen."
}
