param([int]$Vk = 0x70)   # default VK_F1
Add-Type @"
using System; using System.Runtime.InteropServices;
public class SK {
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern void keybd_event(byte vk, byte scan, uint flags, IntPtr extra);
}
"@
$w = Get-Process 86Box -EA SilentlyContinue | Where-Object {$_.MainWindowHandle -ne 0} | Select-Object -First 1
if(-not $w){ Write-Output "no 86Box window"; exit 1 }
[void][SK]::SetForegroundWindow($w.MainWindowHandle); Start-Sleep -Milliseconds 500
[SK]::keybd_event([byte]$Vk,0,0,[IntPtr]::Zero); Start-Sleep -Milliseconds 90
[SK]::keybd_event([byte]$Vk,0,2,[IntPtr]::Zero)
Write-Output ("sent vk=0x{0:X}" -f $Vk)
