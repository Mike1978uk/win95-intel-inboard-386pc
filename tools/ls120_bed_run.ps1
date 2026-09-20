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
    # Not mandatory: a -VendorDriver or -NoDriver run has no driver of ours
    # under test. Exactly one of the three must be given, checked below.
    [string] $Driver = "",
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
    # Run the VENDOR miniport instead of ours: removes LS120MP.MPD and puts
    # the given sd120ppd.mpd into IOSUBSYS.
    #
    # ⛔ THIS DOES NOT WORK ON ls120win_clean.img, and the reason is worth
    # keeping. A Win9x miniport is bound to a device node by the registry's
    # PortDriver value; dropping the file into IOSUBSYS installs nothing. On
    # that image the node points at LS120MP.MPD, so this switch produces:
    #
    #     Initing ls120mp.mpd / Init Failure ls120mp.mpd
    #
    # - our driver referenced, its file removed - while the vendor binary sits
    # there unreferenced. The run looks like "the drive did not enumerate" and
    # means nothing at all.
    #
    # C:\LS120FIX\APPLY.BAT was read as proof the vendor SETUP had run here.
    # It is not: it proves the vendor was installed on SOME image once. Check
    # the binding, not a leftover batch file.
    #
    # Use this only on an image whose registry already binds sd120ppd.mpd -
    # i.e. one taken from a machine where the vendor install actually works.
    [string] $VendorDriver = "",
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

if (($Driver -eq "") -and ($VendorDriver -eq "") -and (-not $NoDriver)) {
    Write-Output "Give one of -Driver <path>, -VendorDriver <path>, or -NoDriver"
    exit 1
}
if (($Driver -ne "") -and ($VendorDriver -ne "")) {
    Write-Output "-Driver and -VendorDriver are mutually exclusive"
    exit 1
}
$repo = "C:\Users\lycet\RiderProjects\86Box-Inboard"
$img  = Join-Path $VmPath "ls120win.img"
$rd   = Join-Path $VmPath "rd.img"

$needed = @($ExePath,
            (Join-Path $VmPath "86box.cfg.master"),
            (Join-Path $VmPath "ls120win_clean.img"),
            (Join-Path $VmPath "rd_verified_good.img"))
# Only one of these exists on any given run; an empty path fails Test-Path.
if ($Driver -ne "")       { $needed += $Driver }
if ($VendorDriver -ne "") { $needed += $VendorDriver }
foreach ($t in $needed) {
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

if ($VendorDriver -ne "") {
    if (-not (Test-Path $VendorDriver)) { Write-Output "MISSING vendor driver: $VendorDriver"; exit 1 }
    python "$repo\tools\fatcp.py" $img --rm "C:\WINDOWS\SYSTEM\IOSUBSYS\LS120MP.MPD" 2>&1 | Out-Null
    python "$repo\tools\fatcp.py" $img "WINDOWS/SYSTEM/IOSUBSYS/SD120PPD.MPD" $VendorDriver --yes
    if ($LASTEXITCODE -ne 0) { Write-Output "VENDOR DEPLOY FAILED"; exit 1 }
    Write-Output "VENDOR sd120ppd.mpd deployed, LS120MP.MPD removed"
} elseif ($NoDriver) {
    python "$repo\tools\fatcp.py" $img --rm "C:\WINDOWS\SYSTEM\IOSUBSYS\LS120MP.MPD" 2>&1 | Out-Null
    Write-Output "LS120MP.MPD REMOVED from IOSUBSYS - our driver is not under test"
} else {
    python "$repo\tools\fatcp.py" $img "WINDOWS/SYSTEM/IOSUBSYS/LS120MP.MPD" $Driver --yes
    if ($LASTEXITCODE -ne 0) { Write-Output "DEPLOY FAILED"; exit 1 }
}

# Verify at the DESTINATION, never the staging copy (technique 75).
$uut = Join-Path $env:TEMP "ls120_under_test.mpd"
Remove-Item $uut -EA SilentlyContinue
# Verify whichever driver this run is actually meant to be exercising.
$uutPath = if ($VendorDriver -ne "") { "C:\WINDOWS\SYSTEM\IOSUBSYS\SD120PPD.MPD" }
           else { "C:\WINDOWS\SYSTEM\IOSUBSYS\LS120MP.MPD" }
python "$repo\tools\fatls.py" $img --get $uutPath $uut | Out-Null
$uutMd5 = "none"
if (Test-Path $uut) {
    $uutMd5 = (Get-FileHash $uut -Algorithm MD5).Hash.ToLower()
    Write-Output ("driver in the image: " + $uutPath + "  md5 " + $uutMd5)
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
# These three may legitimately not exist - a fresh master image has never run.
# fatcp.py writes "not found" to stderr and PowerShell turns a native command's
# stderr into a terminating error under ErrorActionPreference=Stop, which aborted
# the whole run before 86Box was ever launched. Relax it for the removals only.
$oldEAP = $ErrorActionPreference
$ErrorActionPreference = 'Continue'
python "$repo\tools\fatcp.py" $img --rm "C:\DOSPROBE.TXT" 2>&1 | Out-Null
python "$repo\tools\fatcp.py" $img --rm "C:\LSPROBE.TXT" 2>&1 | Out-Null
python "$repo\tools\fatcp.py" $img --rm "C:\BOOTLOG.TXT" 2>&1 | Out-Null
$ErrorActionPreference = $oldEAP
$global:LASTEXITCODE = 0
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
# -WindowStyle Hidden suppresses the console box. 86Box's CMake FORCEs the
# console subsystem whenever the SDL frontend is built (WIN32 AND NOT QT), so
# the window exists whether or not anyone reads it; the emulator's own window
# still appears and -L still writes the log.
$p = Start-Process $ExePath -ArgumentList @("-P", $VmPath, "-L", $log) `
        -WorkingDirectory $VmPath -WindowStyle Hidden -PassThru
Write-Output "86Box pid $($p.Id), running $Seconds s..."
$p | Wait-Process -Timeout $Seconds -EA SilentlyContinue
if (-not $p.HasExited) { $p | Stop-Process -Force }
Start-Sleep 2

if (Test-Path $log) {
    Copy-Item $log (Join-Path $VmPath "86box.log.$Tag") -Force
    # A rotated log that does not say which driver produced it cannot be cited.
    # The md5 was only ever on the harness's stdout, so fifty archived runs have
    # no attribution at all - keep it beside the log it describes.
    # The workload switches belong here too. Without them a log showing two
    # READ(10)s and one showing seventy are not comparable, and nothing in the
    # archive says which of them had a batch driving the drive.
    @("tag        $Tag",
      "driver_md5 $uutMd5",
      "driver_src $Driver",
      "startup    $(if ($Startup)  { $Startup }  else { '(none)' })",
      "configsys  $(if ($ConfigSys){ $ConfigSys} else { '(image default)' })",
      "autoexec   $(if ($Autoexec) { $Autoexec } else { '(image default)' })",
      "nodriver   $NoDriver",
      "seconds    $Seconds",
      "86box_exe  $ExePath",
      "86box_built $exeTime",
      "finished   $(Get-Date -Format s)"
    ) | Set-Content -Path (Join-Path $VmPath "86box.log.$Tag.provenance") -Encoding ascii
    Write-Output ("86box.log.$Tag  " + (Get-Item $log).Length + " bytes  driver md5 $uutMd5")
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
