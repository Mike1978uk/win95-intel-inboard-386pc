# One iteration of the .PDR driver-load loop, entirely in the emulator.
#
#   pwsh -File tools/pdr_loadtest.ps1 [-Pdr <file>] [-Seconds 150] [-Tag name]
#
# Drops the driver into the test image's IOSUBSYS, clears the old BOOTLOG.TXT,
# boots the VM (MSDOS.SYS BootGUI=0 + "WIN /B" in AUTOEXEC.BAT make the log
# unconditional - no F8 keystroke to time), kills 86Box, extracts BOOTLOG.TXT
# and prints the lines that decide the question.
#
# Whether a VxD image loads is machine-independent. Answering it on the real
# 5160 cost eleven boots on 2026-08-31; this costs about two minutes and no
# hardware. Go to the 5160 only once "Init Success port.pdr" appears here.

param(
    [string]$Pdr     = "C:\Users\lycet\RiderProjects\86Box-Inboard\drivers\xtide_pdr\build\PORT.pdr",
    [string]$VmPath  = "C:\Users\lycet\RiderProjects\86Box-Inboard\vm_xtide_pdr",
    [string]$Image   = "xtide_test.img",
    [string]$ExePath = "C:\Users\lycet\RiderProjects\86Box-Inboard\86box_upstream\build\src\86Box.exe",
    # How long to let the guest run. This is NOT slack - it is the measurement
    # window, and things that decide the result happen late in it: the volume is
    # published from an appy-time callback, and anything in the StartUp group
    # runs after that. 150 s hid a working volume on 2026-09-01; 260 s published
    # it on one run and not the next, with builds that differed only by a
    # counter. Treat a short window as a source of false negatives, and see
    # technique 84.
    [int]$Seconds    = 400,
    # Restore the image from xtide_base.img before deploying. The baseline has
    # the device node installed and the logged boot armed, so a run that
    # corrupts the volume costs one copy, not a reinstall. Use it for anything
    # that writes.
    [switch]$Restore,
    # How long to keep tapping F1 at the BIOS's configuration complaint. A hard
    # kill can leave the NVR inconsistent, and the next boot then stops at
    # "162-System Options Not Set" - which produces no BOOTLOG.TXT at all and
    # reads exactly like a driver that killed the boot. 30 s was not enough
    # margin; POST length varies with the Mach8 option ROM. Technique 71.
    [int]$F1Seconds  = 90,
    # And keep tapping ENTER for a while after that. Repeated forced kills set
    # Windows' own "did not finish loading last time" flag, so a later boot
    # stops at the Startup Menu and waits - forever, since nothing here answers
    # it, and the run then reports no BOOTLOG as though the driver had killed
    # the boot. The menu already highlights "2. Logged" because that is how
    # these runs boot, so a bare Enter picks the right item. Technique 23.
    [int]$EnterSeconds = 150,
    [string]$Tag     = "run"
)

$repo = "C:\Users\lycet\RiderProjects\86Box-Inboard"
$img  = Join-Path $VmPath $Image
$out  = Join-Path $VmPath "bootlog_$Tag.txt"

foreach ($t in @($Pdr, $img, $ExePath)) {
    if (-not (Test-Path $t)) { Write-Output "MISSING: $t"; exit 1 }
}
Get-Process 86Box -EA SilentlyContinue | Stop-Process -Force
Start-Sleep 1

$base = Join-Path $VmPath "xtide_base.img"
if ($Restore) {
    if (-not (Test-Path $base)) { Write-Output "MISSING baseline: $base"; exit 1 }
    Copy-Item $base $img -Force
    Write-Output "restored $Image from xtide_base.img"
}

$md5 = (Get-FileHash $Pdr -Algorithm MD5).Hash.ToLower()
Write-Output "PDR: $Pdr  $((Get-Item $Pdr).Length) bytes  md5 $md5"

# Deploy, and delete the previous log so a stale one cannot be mistaken for a result.
python "$repo\tools\fatcp.py" $img "C:\WINDOWS\SYSTEM\IOSUBSYS\PORT.PDR" $Pdr
if ($LASTEXITCODE -ne 0) { Write-Output "FAILED: deploy"; exit 1 }
python "$repo\tools\fatcp.py" $img --rm "C:\BOOTLOG.TXT" 2>&1 | Out-Null

# The loop kills 86Box rather than shutting the guest down, so the volume is left
# dirty every time. Without this the next boot runs real-mode ScanDisk and never
# reaches WIN /B - it cost the first run of the day. MSDOS.SYS also has AutoScan=0.
python "$repo\tools\fatclean.py" $img

$p = Start-Process $ExePath -ArgumentList @("-P", $VmPath) -WorkingDirectory $VmPath -PassThru `
        -RedirectStandardOutput (Join-Path $VmPath "stdout_$Tag.txt") `
        -RedirectStandardError  (Join-Path $VmPath "stderr_$Tag.txt")
Write-Output "86Box pid $($p.Id), running $Seconds s..."

# The Deskpro 386 BIOS stops on "press F1" for its configuration complaint. Tap F1
# for the first half-minute so nobody has to sit and watch the window; after POST
# the guest ignores it. Same technique as tools/quiet_run.ps1.
Add-Type @"
using System; using System.Runtime.InteropServices;
public class PT {
  [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, IntPtr p);
  [DllImport("user32.dll")] public static extern bool AttachThreadInput(uint a, uint b, bool f);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern bool BringWindowToTop(IntPtr h);
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int c);
  [DllImport("user32.dll")] public static extern void keybd_event(byte vk, byte sc, uint f, IntPtr x);
  [DllImport("kernel32.dll")] public static extern uint GetCurrentThreadId();
  public static void Tap(IntPtr h, byte vk, byte sc) {
    uint me = GetCurrentThreadId();
    uint other = GetWindowThreadProcessId(GetForegroundWindow(), IntPtr.Zero);
    AttachThreadInput(me, other, true);
    ShowWindow(h, 9); BringWindowToTop(h); SetForegroundWindow(h);
    AttachThreadInput(me, other, false);
    System.Threading.Thread.Sleep(120);
    keybd_event(vk, sc, 0, IntPtr.Zero); System.Threading.Thread.Sleep(60);
    keybd_event(vk, sc, 2, IntPtr.Zero);
  }
}
"@
$deadline = (Get-Date).AddSeconds($Seconds)
$f1until  = (Get-Date).AddSeconds($F1Seconds)
$enteruntil = (Get-Date).AddSeconds($EnterSeconds)
while ((Get-Date) -lt $deadline -and -not $p.HasExited) {
    if ((Get-Date) -lt $f1until) {
        $p.Refresh()
        if ($p.MainWindowHandle -ne 0) { [PT]::Tap($p.MainWindowHandle, 0x70, 0x3B) }
    }
    elseif ((Get-Date) -lt $enteruntil) {
        $p.Refresh()
        if ($p.MainWindowHandle -ne 0) { [PT]::Tap($p.MainWindowHandle, 0x0D, 0x1C) }
    }
    Start-Sleep 3
}
$shot = Join-Path $VmPath "screen_$Tag.png"
if (-not $p.HasExited) {
    & pwsh -File "$repo\tools\shot.ps1" $shot 2>&1 | Out-Null
    $p | Stop-Process -Force
}
Start-Sleep 2

python "$repo\tools\fatls.py" $img --get "C:\BOOTLOG.TXT" $out
if (-not (Test-Path $out)) {
    # No log is not a result about the driver. It is far more often POST: the
    # BIOS stopping for its configuration complaint, or a stale NVR, neither of
    # which ever starts Windows. LOOK AT THE SCREEN before drawing any
    # conclusion from this - a harness that clears prompts must show what it
    # was clearing (technique 71), and this one now says where to look.
    Write-Output "NO BOOTLOG - Windows never wrote one."
    Write-Output "  This is usually POST or the Startup Menu, not the driver."
    Write-Output "  Read $shot before concluding anything:"
    Write-Output "  '162-System Options Not Set', an F1 prompt, or 'Enter a choice:'"
    Write-Output "  all mean the run is VOID, not negative."
    exit 1
}

Write-Output ""
Write-Output "---- IOSUBSYS / port.pdr lines ----"
Select-String -Path $out -Pattern "PORT\.PDR|port\.pdr|hsflop|Initing|Init Success|Init Failure|LoadFailed|DEVICEINIT   = PORT" |
    ForEach-Object { $_.Line }
Write-Output ""
Write-Output "VERDICT:"
$log = Get-Content $out -Raw
if     ($log -match "Init Success port\.pdr") { Write-Output "  LOADS - Init Success port.pdr" }
elseif ($log -match "Init Failure port\.pdr") { Write-Output "  REFUSED - Init Failure port.pdr" }
elseif ($log -match "port\.pdr")              { Write-Output "  seen but neither Success nor Failure - read $out" }
else                                          { Write-Output "  IOS never mentioned port.pdr - read $out" }
