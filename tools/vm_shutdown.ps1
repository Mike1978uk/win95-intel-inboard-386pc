# Drive a Windows 95 shutdown in a running 86Box, keyboard only, and capture it.
#
#   pwsh -File tools/vm_shutdown.ps1 -VmPath <dir> [-Tag name] [-WaitSeconds 300]
#
# Separate from pdr_inboard_run.ps1 on purpose. Timing a shutdown off a fixed
# boot window means sitting on an idle desktop for minutes, and every focus
# grab in that window is stolen from whoever is using the machine. Fire this
# when the desktop is actually up instead.
#
# WHY A SHUTDOWN IS WORTH DRIVING AT ALL: IOS broadcasts AEP_PEND_UNCONFIG_DCB,
# AEP_UNCONFIG_DCB, AEP_SYSTEM_SHUTDOWN and AEP_SYSTEM_CRIT_SHUTDOWN at
# System_Exit and nowhere else. A driver that never sees a shutdown has never
# had those code paths executed - and the DDK sample answers AEP_FAILURE to
# every function code it does not recognise, which is all four of them.
#
# The readback is BOOTLOG.TXT's own Terminate=/EndTerminate= pairs. A stage that
# starts and never ends NAMES the hang; a screenshot of a frozen desktop does
# not. Read it afterwards with tools/fatls.py --get.

param(
    [string]$VmPath = "C:\Users\lycet\RiderProjects\86Box-Inboard\vm_xtide_inboard",
    [string]$Tag = "shutdown",
    [int]$WaitSeconds = 300,
    # Take a screenshot at each step. Each one grabs focus for a moment, so
    # -Quiet skips the intermediates and keeps only the final frame.
    [switch]$Quiet
)

$repo = "C:\Users\lycet\RiderProjects\86Box-Inboard"
$p = Get-Process 86Box -EA SilentlyContinue | Select-Object -First 1
if (-not $p) { Write-Output "No 86Box running - nothing to shut down."; exit 1 }
$p.Refresh()
if ($p.MainWindowHandle -eq 0) { Write-Output "86Box has no window handle yet."; exit 1 }

Add-Type @"
using System; using System.Runtime.InteropServices;
public class VMSD {
  [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, IntPtr p);
  [DllImport("user32.dll")] public static extern bool AttachThreadInput(uint a, uint b, bool f);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern bool BringWindowToTop(IntPtr h);
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int c);
  [DllImport("user32.dll")] public static extern void keybd_event(byte vk, byte sc, uint f, IntPtr x);
  [DllImport("kernel32.dll")] public static extern uint GetCurrentThreadId();
  // SetForegroundWindow alone is refused across processes; the guest then never
  // sees the key and the run looks like a driver result. Technique 71.
  public static void Focus(IntPtr h) {
    uint me = GetCurrentThreadId();
    uint other = GetWindowThreadProcessId(GetForegroundWindow(), IntPtr.Zero);
    AttachThreadInput(me, other, true);
    ShowWindow(h, 9); BringWindowToTop(h); SetForegroundWindow(h);
    AttachThreadInput(me, other, false);
    System.Threading.Thread.Sleep(150);
  }
  // keybd_event is GLOBAL input injection - it lands wherever focus is. So the
  // sequence takes focus ONCE, up front and announced, and every key after
  // that is sent only if the VM still has it. Never a grab per keystroke: this
  // runs on a machine somebody is using.
  public static void Tap(IntPtr h, byte vk, byte sc) {
    if (GetForegroundWindow() != h) { return; }
    keybd_event(vk, sc, 0, IntPtr.Zero); System.Threading.Thread.Sleep(60);
    keybd_event(vk, sc, 2, IntPtr.Zero);
  }
  // Held as a real chord. The guest reads make/break codes, so Ctrl must still
  // be down when Esc arrives or Win95 just eats an Escape.
  public static void CtrlEsc(IntPtr h) {
    if (GetForegroundWindow() != h) { return; }
    keybd_event(0x11, 0x1D, 0, IntPtr.Zero); System.Threading.Thread.Sleep(80);
    keybd_event(0x1B, 0x01, 0, IntPtr.Zero); System.Threading.Thread.Sleep(80);
    keybd_event(0x1B, 0x01, 2, IntPtr.Zero); System.Threading.Thread.Sleep(80);
    keybd_event(0x11, 0x1D, 2, IntPtr.Zero);
  }
}
"@

function Shot($name) {
    if ($Quiet) { return }
    & pwsh -File "$repo\tools\shot.ps1" (Join-Path $VmPath "screen_${Tag}_$name.png") 2>&1 | Out-Null
}

$h = $p.MainWindowHandle
Shot "desktop"
Write-Output "taking focus ONCE for a 3-key sequence (~10 s), then leaving it alone."
[VMSD]::Focus($h)
Write-Output "Ctrl+Esc (Start menu)"
[VMSD]::CtrlEsc($h);          Start-Sleep 4; Shot "startmenu"
Write-Output "U (Shut Down...)"
[VMSD]::Tap($h, 0x55, 0x16);  Start-Sleep 4; Shot "dialog"
Write-Output "Enter (OK)"
[VMSD]::Tap($h, 0x0D, 0x1C)

# From here on, do NOT touch focus. Windows is tearing down; a foreground grab
# during shutdown is one more variable in a run whose whole point is what the
# guest does on its own.
Write-Output "waiting $WaitSeconds s for teardown (no further focus grabs)..."
$end = (Get-Date).AddSeconds($WaitSeconds)
while ((Get-Date) -lt $end -and -not $p.HasExited) { Start-Sleep 5 }

& pwsh -File "$repo\tools\shot.ps1" (Join-Path $VmPath "screen_${Tag}_final.png") 2>&1 | Out-Null
Write-Output "final frame: $VmPath\screen_${Tag}_final.png"
Write-Output "86Box exited: $($p.HasExited)"
