# One boot of the REAL card image on the Inboard machine profile.
#
#   pwsh -File tools/pdr_inboard_run.ps1 [-Seconds 900] [-Tag name]
#
# The other harness (pdr_loadtest.ps1) runs a Deskpro 386 because "whether a VxD
# loads is machine-independent". That turned out to be false: on 2026-09-01 the
# same claim-nothing build gave Init Success on the Deskpro bed and Init Failure
# on the 5160. So the machine is a variable, and this bed removes it - the real
# card's own install, the Inboard machine, the 1986 5160 BIOS.
#
# It does NOT deploy anything. Put the driver in with tools/fatcp.py first, so
# the binary under test is chosen deliberately and its md5 is on the record.
#
# MSDOS.SYS in the image is armed with BootMenu=1 / BootMenuDefault=2, which is
# what "F8 -> Logged" picks by hand on the real machine - so BOOTLOG.TXT is
# written without anyone having to time a keystroke.

param(
    [string]$VmPath  = "C:\Users\lycet\RiderProjects\86Box-Inboard\vm_xtide_inboard",
    [string]$Image   = "prextide.img",
    [string]$ExePath = "C:\Users\lycet\RiderProjects\86Box-Inboard\86box_upstream\build\src\86Box.exe",
    # This machine POSTs far more slowly than the Deskpro bed - the Mach8 option
    # ROM alone takes tens of seconds - and Windows 95 on top of it is slower
    # again. A short window here is a false negative, not a result.
    [int]$Seconds    = 900,
    # How long to keep tapping Enter through the DOS phase.
    [int]$EnterSeconds = 300,
    [string]$Tag     = "inboard"
)

$repo = "C:\Users\lycet\RiderProjects\86Box-Inboard"
$img  = Join-Path $VmPath $Image
$out  = Join-Path $VmPath "bootlog_$Tag.txt"
$shot = Join-Path $VmPath "screen_$Tag.png"

foreach ($t in @($img, $ExePath)) {
    if (-not (Test-Path $t)) { Write-Output "MISSING: $t"; exit 1 }
}
Get-Process 86Box -EA SilentlyContinue | Stop-Process -Force
Start-Sleep 1

# Restore the config from the master copy EVERY run. 86Box rewrites .cfg on
# every load, and a run that fails early - a missing ROM, say - gets normalised
# down to a plain 8088 ibmxt with 256K and that is what it writes back. The next
# run then boots a stock XT trying to start Windows 95, shows a black screen,
# and looks like a completely different problem. Technique 43.
$master = Join-Path $VmPath "86box.cfg.master"
if (-not (Test-Path $master)) { Write-Output "MISSING master config: $master"; exit 1 }
Copy-Item $master (Join-Path $VmPath "86box.cfg") -Force

# And say out loud what machine is about to boot, so a silent downgrade cannot
# be mistaken for a driver result.
$mach = (Select-String -Path $master -Pattern '^machine = (.+)$').Matches[0].Groups[1].Value
$mem  = (Select-String -Path $master -Pattern '^mem_size = (.+)$').Matches[0].Groups[1].Value
Write-Output "machine: $mach   mem: $mem KB"
if ($mach -ne "ibmxt_inboard386") { Write-Output "WRONG MACHINE - this bed exists to test the Inboard"; exit 1 }

# Say which driver is actually in the image. A run whose binary nobody recorded
# is a run nobody can cite (technique 70).
python "$repo\tools\fatls.py" $img --get "C:\WINDOWS\SYSTEM\IOSUBSYS\PORT.PDR" `
    (Join-Path $env:TEMP "pdr_under_test.pdr") | Out-Null
if (Test-Path (Join-Path $env:TEMP "pdr_under_test.pdr")) {
    $m = (Get-FileHash (Join-Path $env:TEMP "pdr_under_test.pdr") -Algorithm MD5).Hash.ToLower()
    Write-Output "driver in the image: md5 $m"
}
python "$repo\tools\fatcp.py" $img --rm "C:\BOOTLOG.TXT" 2>&1 | Out-Null
python "$repo\tools\fatclean.py" $img

$p = Start-Process $ExePath -ArgumentList @("-P", $VmPath) -WorkingDirectory $VmPath -PassThru `
        -RedirectStandardOutput (Join-Path $VmPath "stdout_$Tag.txt") `
        -RedirectStandardError  (Join-Path $VmPath "stderr_$Tag.txt")
Write-Output "86Box pid $($p.Id), running $Seconds s..."

# The real card's CONFIG.SYS stops for keypresses this bed has nobody to give
# it - TSLCD prints "Driver aborting... Press [return]" when no CD answers, and
# the Startup Menu wants a choice. On the 5160 a human clears both. Tap Enter
# through the DOS phase; it also accepts the menu's own default, item 2/Logged.
Add-Type @"
using System; using System.Runtime.InteropServices;
public class PT2 {
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
$enteruntil = (Get-Date).AddSeconds($EnterSeconds)
while ((Get-Date) -lt $deadline -and -not $p.HasExited) {
    if ((Get-Date) -lt $enteruntil) {
        $p.Refresh()
        if ($p.MainWindowHandle -ne 0) { [PT2]::Tap($p.MainWindowHandle, 0x0D, 0x1C) }
    }
    Start-Sleep 5
}

if (-not $p.HasExited) {
    & pwsh -File "$repo\tools\shot.ps1" $shot 2>&1 | Out-Null
    $p | Stop-Process -Force
}
Start-Sleep 2

python "$repo\tools\fatls.py" $img --get "C:\BOOTLOG.TXT" $out
if (-not (Test-Path $out)) {
    Write-Output "NO BOOTLOG - Windows never wrote one."
    Write-Output "  Read $shot before concluding anything. On this machine the"
    Write-Output "  usual causes are POST stopping for a prompt, or simply not"
    Write-Output "  having had long enough - it is much slower than the Deskpro bed."
    exit 1
}

Write-Output ""
Write-Output "---- port.pdr ----"
Select-String -Path $out -Pattern "port\.pdr|hsflop|Init Success|Init Failure|LoadFailed" |
    ForEach-Object { $_.Line }
