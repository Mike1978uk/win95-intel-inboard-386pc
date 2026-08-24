param([string]$Out = "C:\Users\lycet\RiderProjects\86Box-Inboard\vm_qbr\snap.png")
Add-Type -AssemblyName System.Drawing
Add-Type @"
using System; using System.Runtime.InteropServices;
public class SB {
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr dc, uint f);
  [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out R r);
  public struct R { public int L, T, Rt, B; }
}
"@
$w = Get-Process 86Box -EA SilentlyContinue | Where-Object {$_.MainWindowHandle -ne 0} | Select-Object -First 1
if(-not $w){ Write-Output "no 86Box window"; exit 1 }
[SB+R]$r = New-Object SB+R; [void][SB]::GetClientRect($w.MainWindowHandle,[ref]$r)
$cw=$r.Rt-$r.L; $ch=$r.B-$r.T
$bmp=New-Object System.Drawing.Bitmap $cw,$ch
$g=[System.Drawing.Graphics]::FromImage($bmp); $dc=$g.GetHdc()
[void][SB]::PrintWindow($w.MainWindowHandle,$dc,3); $g.ReleaseHdc($dc); $g.Dispose()
$bmp.Save($Out,[System.Drawing.Imaging.ImageFormat]::Png); $bmp.Dispose()
Write-Output "saved $Out ${cw}x${ch}"
