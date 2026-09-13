# One boot of the LS-120 bed (vm_ls120win) with a named driver.
#
#   pwsh -File tools/ls120_bed_run.ps1 -Driver <path to LS120MP.MPD> -Tag name [-Seconds 600]
#
# Every run gets a fresh config, a fresh system image and a fresh MEDIUM, in
# that order, before anything is deployed. The medium matters as much as the
# system image: a driver under test writes to it, so reusing the previous
# run's rd.img tests whatever that run left behind.
#
# 86box.log is where pclog output lands, which is where [ECPDIAG] and the EPAT
# bridge trace live. It is rotated to 86box.log.<tag> at the end so a run can
# be cited later - an artefact nobody can name is not evidence.
#
# A force-killed run loses whatever is still in the guest's write cache, so a
# file the guest lists is not necessarily on the medium. Verify from the host.

param(
    [Parameter(Mandatory = $true)] [string] $Driver,
    [Parameter(Mandatory = $true)] [string] $Tag,
    [int]    $Seconds = 600,
    # A batch dropped into the guest's StartUp folder after the image is
    # restored, so the bed can exercise the drive with nobody at the keyboard
    # (technique 87 - drive it from inside the guest, not with host keystrokes).
    [string] $Startup = "",   # e.g. tools/fixtures/LSWRITE.BAT
    # Arm the CS:EIP heartbeat and the VMM write watch. Both are off by
    # default because they are expensive; use them to find where a guest that
    # has stopped talking to the device is actually spinning.
    [switch] $Heartbeat,
    # Take our miniport OUT of IOSUBSYS entirely, so a run exercises
    # whatever else is installed. Renaming a driver out is the cheapest
    # bisect there is and it needs no reinstall.
    [switch] $NoDriver,
    # Replace the guest's CONFIG.SYS / AUTOEXEC.BAT for this run, to boot the
    # vendor's real-mode stack instead. It is the one implementation of this
    # transport known to work on the owner's machine, so it is the reference
    # to compare against when ours misbehaves.
    [string] $ConfigSys = "",
    [string] $Autoexec  = "",
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

if ($NoDriver) {
    python "$repo\tools\fatcp.py" $img --rm "C:\WINDOWS\SYSTEM\IOSUBSYS\LS120MP.MPD" 2>&1 | Out-Null
    Write-Output "LS120MP.MPD REMOVED from IOSUBSYS - our driver is not under test"
} else {
    python "$repo\tools\fatcp.py" $img "WINDOWS/SYSTEM/IOSUBSYS/LS120MP.MPD" $Driver --yes
    if ($LASTEXITCODE -ne 0) { Write-Output "DEPLOY FAILED"; exit 1 }
}

# Verify at the DESTINATION, never the staging copy (technique 75).
$uut = Join-Path $env:TEMP "ls120_under_test.mpd"
Remove-Item $uut -EA SilentlyContinue
python "$repo\tools\fatls.py" $img --get "C:\WINDOWS\SYSTEM\IOSUBSYS\LS120MP.MPD" $uut | Out-Null
if (Test-Path $uut) {
    Write-Output ("driver in the image: md5 " + (Get-FileHash $uut -Algorithm MD5).Hash.ToLower())
} elseif ($NoDriver) {
    Write-Output "driver in the image: NONE, as intended"
} else {
    Write-Output "driver NOT in the image after deploy"
    exit 1
}

if ($Startup -ne "") {
    if (-not (Test-Path $Startup)) { Write-Output "MISSING startup batch: $Startup"; exit 1 }
    $sname = [IO.Path]::GetFileName($Startup)
    python "$repo\tools\fatcp.py" $img "WINDOWS/STARTM~1/PROGRAMS/STARTUP/$sname" $Startup --yes
    if ($LASTEXITCODE -ne 0) { Write-Output "STARTUP DEPLOY FAILED"; exit 1 }
}
if ($ConfigSys -ne "") {
    python "$repo\tools\fatcp.py" $img "CONFIG.SYS" $ConfigSys --yes
    if ($LASTEXITCODE -ne 0) { Write-Output "CONFIG.SYS DEPLOY FAILED"; exit 1 }
}
if ($Autoexec -ne "") {
    python "$repo\tools\fatcp.py" $img "AUTOEXEC.BAT" $Autoexec --yes
    if ($LASTEXITCODE -ne 0) { Write-Output "AUTOEXEC.BAT DEPLOY FAILED"; exit 1 }
}
python "$repo\tools\fatcp.py" $img --rm "C:\DOSPROBE.TXT" 2>&1 | Out-Null
python "$repo\tools\fatcp.py" $img --rm "C:\LSPROBE.TXT" 2>&1 | Out-Null
python "$repo\tools\fatcp.py" $img --rm "C:\BOOTLOG.TXT" 2>&1 | Out-Null
python "$repo\tools\fatclean.py" $img | Out-Null

$log = Join-Path $VmPath "86box.log"
Remove-Item $log -EA SilentlyContinue

# -L is not optional: without it pclog output (the [ECPDIAG] census and the
# whole EPAT bridge trace) goes to a console nobody is reading and the run
# produces no evidence at all. One run was lost to this on 2026-09-13.
if ($Heartbeat) {
    $env:INBOARD_HEARTBEAT = "1"
    Write-Output "heartbeat armed"
} else {
    Remove-Item Env:INBOARD_HEARTBEAT -EA SilentlyContinue
}
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

python "$repo\tools\fatls.py" $img --get "C:\DOSPROBE.TXT" (Join-Path $VmPath "dosprobe_$Tag.txt") | Out-Null
if (Test-Path (Join-Path $VmPath "dosprobe_$Tag.txt")) {
    Write-Output "--- DOSPROBE.TXT ---"
    Get-Content (Join-Path $VmPath "dosprobe_$Tag.txt")
}
python "$repo\tools\fatls.py" $img --get "C:\LSPROBE.TXT" (Join-Path $VmPath "lsprobe_$Tag.txt") | Out-Null
if (Test-Path (Join-Path $VmPath "lsprobe_$Tag.txt")) {
    Write-Output "--- LSPROBE.TXT ---"
    Get-Content (Join-Path $VmPath "lsprobe_$Tag.txt")
}
