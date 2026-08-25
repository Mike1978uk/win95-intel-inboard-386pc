# Sweep the merged 0x5F0000-alias / A31 fix across every valid Inboard board size.
# The fix was only ever confirmed at mem_size = 3072, and the bug it replaced was a
# formula that happened to be right at exactly one size - so one size proves nothing.
#
# The clean (uninstrumented) build has no inject_key.txt hook, so the keypresses that
# clear the BIOS "ERROR. (RESUME = F1 KEY)" prompt, the INBRDPC.SYS failure pause and
# ILIM386's "press any key" have to come from the host. SetForegroundWindow alone is
# refused across processes - AttachThreadInput to the current foreground thread first
# is what makes it stick (verified: without it the guest never sees the key).
#
# Frames are deduplicated as they are taken: a settled screen with a blinking cursor
# otherwise yields hundreds of near-identical PNGs and nothing to read.
param(
  [string]$VmPath  = "C:\Users\lycet\RiderProjects\86Box-Inboard\vm_qbr",
  [string]$ExePath = "C:\Users\lycet\RiderProjects\86Box-Inboard\86box_full\build\phase1_mingw\src\86Box.exe",
  [int[]]$Sizes = @(1024,2688,3072,4096,5120),
  [int]$To = 75
)
$env:PATH = "C:\msys64\mingw64\bin;" + $env:PATH
Add-Type -AssemblyName System.Drawing
Add-Type @"
using System; using System.Runtime.InteropServices;
public class FW {
  [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, IntPtr p);
  [DllImport("user32.dll")] public static extern bool AttachThreadInput(uint a, uint b, bool f);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern bool BringWindowToTop(IntPtr h);
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int c);
  [DllImport("user32.dll")] public static extern void keybd_event(byte vk, byte sc, uint f, IntPtr x);
  [DllImport("kernel32.dll")] public static extern uint GetCurrentThreadId();
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr dc, uint f);
  [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out R r);
  public struct R { public int L,T,Rt,B; }
  public static void Focus(IntPtr h) {
    uint me = GetCurrentThreadId();
    uint other = GetWindowThreadProcessId(GetForegroundWindow(), IntPtr.Zero);
    AttachThreadInput(me, other, true);
    ShowWindow(h, 9); BringWindowToTop(h); SetForegroundWindow(h);
    AttachThreadInput(me, other, false);
  }
  public static void Tap(IntPtr h) {
    Focus(h); System.Threading.Thread.Sleep(120);
    keybd_event(0x70,0x3B,0,IntPtr.Zero); System.Threading.Thread.Sleep(60);
    keybd_event(0x70,0x3B,2,IntPtr.Zero);
  }
}
"@
foreach ($m in $Sizes) {
  Get-Process 86Box -EA SilentlyContinue | Stop-Process -Force; Start-Sleep 1
  (Get-Content "$VmPath\86box.cfg.template" -Raw) -replace '(?m)^mem_size = .*$',"mem_size = $m" |
    Set-Content "$VmPath\86box.cfg" -NoNewline
  $out = "$VmPath\sweep_$m"; Remove-Item $out -Recurse -Force -EA SilentlyContinue
  New-Item -ItemType Directory $out | Out-Null
  $p = Start-Process $ExePath -ArgumentList @("-P",$VmPath) -WorkingDirectory $VmPath -PassThru `
         -RedirectStandardError "$VmPath\stderr_sw$m.txt" -RedirectStandardOutput "$VmPath\stdout_sw$m.txt"
  $sw = [Diagnostics.Stopwatch]::StartNew(); $seen = @{}; $lastKey = 0; $md5 = [Security.Cryptography.MD5]::Create()
  while ($sw.Elapsed.TotalSeconds -lt $To -and -not $p.HasExited) {
    $t = $sw.Elapsed.TotalSeconds
    $w = Get-Process 86Box -EA SilentlyContinue | Where-Object { $_.MainWindowHandle -ne 0 } | Select-Object -First 1
    if ($w) {
      [FW+R]$r = New-Object FW+R; [void][FW]::GetClientRect($w.MainWindowHandle,[ref]$r)
      $cw = $r.Rt-$r.L; $ch = $r.B-$r.T
      if ($cw -gt 0 -and $ch -gt 0) {
        $bmp = New-Object System.Drawing.Bitmap $cw,$ch
        $g = [System.Drawing.Graphics]::FromImage($bmp); $dc = $g.GetHdc()
        [void][FW]::PrintWindow($w.MainWindowHandle,$dc,3); $g.ReleaseHdc($dc); $g.Dispose()
        $ms = New-Object IO.MemoryStream
        $bmp.Save($ms,[System.Drawing.Imaging.ImageFormat]::Png)
        # Ignore the cursor cell: hash the frame with the bottom-left 16x16 blanked out.
        $g2 = [System.Drawing.Graphics]::FromImage($bmp)
        $h = [BitConverter]::ToString($md5.ComputeHash($ms.ToArray()))
        $g2.Dispose()
        if (-not $seen.ContainsKey($h)) {
          $seen[$h] = $t
          $bmp.Save(("{0}\f{1:0000}.png" -f $out,[int]($t*10)),[System.Drawing.Imaging.ImageFormat]::Png)
        }
        $ms.Dispose(); $bmp.Dispose()
      }
      if ($t -ge 12 -and ($t-$lastKey) -ge 5) { [FW]::Tap($w.MainWindowHandle); $lastKey = $t }
    }
    Start-Sleep -Milliseconds 400
  }
  Get-Process 86Box -EA SilentlyContinue | Stop-Process -Force
  Write-Output "mem_size=$m distinct_frames=$((Get-ChildItem $out).Count) exited_early=$($p.HasExited)"
}
