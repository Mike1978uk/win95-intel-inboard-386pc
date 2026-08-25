param([int]$MemSize = 1024)
$vm="C:\Users\lycet\RiderProjects\86Box-Inboard\vm_qbr"
$exe="C:\Users\lycet\RiderProjects\86Box-Inboard\86box_full\build\phase1_mingw\src\86Box.exe"
$env:PATH="C:\msys64\mingw64\bin;"+$env:PATH
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
}
"@
Get-Process 86Box -EA SilentlyContinue | Stop-Process -Force; Start-Sleep 1
(Get-Content "$vm\86box.cfg.template" -Raw) -replace '(?m)^mem_size = .*$',"mem_size = $MemSize" | Set-Content "$vm\86box.cfg" -NoNewline
$p=Start-Process $exe -ArgumentList @("-P",$vm) -WorkingDirectory $vm -PassThru -RedirectStandardError "$vm\e_f1.txt" -RedirectStandardOutput "$vm\o_f1.txt"
Start-Sleep 16
$w=Get-Process 86Box -EA SilentlyContinue | Where-Object {$_.MainWindowHandle -ne 0} | Select-Object -First 1
[FW]::Focus($w.MainWindowHandle); Start-Sleep -Milliseconds 300
$fg = [FW]::GetForegroundWindow()
Write-Output "target=$($w.MainWindowHandle) foreground=$fg match=$($fg -eq $w.MainWindowHandle)"
1..3 | ForEach-Object { [FW]::keybd_event(0x70,0x3B,0,[IntPtr]::Zero); Start-Sleep -Milliseconds 80; [FW]::keybd_event(0x70,0x3B,2,[IntPtr]::Zero); Start-Sleep -Milliseconds 700 }
Start-Sleep 8
[FW+R]$r=New-Object FW+R; [void][FW]::GetClientRect($w.MainWindowHandle,[ref]$r)
$bmp=New-Object System.Drawing.Bitmap ($r.Rt-$r.L),($r.B-$r.T)
$g=[System.Drawing.Graphics]::FromImage($bmp); $dc=$g.GetHdc(); [void][FW]::PrintWindow($w.MainWindowHandle,$dc,3); $g.ReleaseHdc($dc); $g.Dispose()
$bmp.Save("$vm\f1_test.png",[System.Drawing.Imaging.ImageFormat]::Png); $bmp.Dispose()
Get-Process 86Box -EA SilentlyContinue | Stop-Process -Force
