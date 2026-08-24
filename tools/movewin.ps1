param([int]$X=0,[int]$Y=0,[int]$W=1600,[int]$H=1100)
Add-Type @"
using System; using System.Runtime.InteropServices;
public class MW {
  [DllImport("user32.dll")] public static extern bool MoveWindow(IntPtr h,int x,int y,int w,int hh,bool r);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
}
"@
$w = Get-Process 86Box -EA SilentlyContinue | Where-Object {$_.MainWindowHandle -ne 0} | Select-Object -First 1
if(-not $w){ Write-Output "no window"; exit 1 }
[void][MW]::MoveWindow($w.MainWindowHandle,$X,$Y,$W,$H,$true)
Write-Output "moved"
