# Screenshot the running 86Box window to a PNG. Host-native; no guest agent needed.
#   pwsh -File tools/shot.ps1 <out.png>
# Uses PrintWindow so it works without bringing the window to the front. Screen-coordinate
# capture was tried before and multi-monitor DPI scaling made it unreliable (2026-07-31);
# this takes no coordinates at all.
param([Parameter(Mandatory=$true)][string]$Out)
Add-Type -AssemblyName System.Drawing
Add-Type @"
using System; using System.Runtime.InteropServices;
public class W {
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr dc, uint f);
  [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out R r);
  public struct R { public int L, T, Rt, B; }
}
"@
$p = Get-Process 86Box -ErrorAction SilentlyContinue | Where-Object { $_.MainWindowHandle -ne 0 } | Select-Object -First 1
if (-not $p) { Write-Error "no 86Box window found"; exit 1 }
$h = $p.MainWindowHandle
[W+R]$r = New-Object W+R
[void][W]::GetClientRect($h, [ref]$r)
$w = $r.Rt - $r.L; $ht = $r.B - $r.T
if ($w -le 0 -or $ht -le 0) { Write-Error "window has no client area ($w x $ht)"; exit 1 }
$bmp = New-Object System.Drawing.Bitmap $w, $ht
$g = [System.Drawing.Graphics]::FromImage($bmp)
$dc = $g.GetHdc()
[void][W]::PrintWindow($h, $dc, 3)   # PW_CLIENTONLY | PW_RENDERFULLCONTENT
$g.ReleaseHdc($dc); $g.Dispose()
$bmp.Save($Out, [System.Drawing.Imaging.ImageFormat]::Png); $bmp.Dispose()
Write-Output "wrote $Out ($w x $ht)"
