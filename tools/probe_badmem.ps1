# The Inboard driver, when its memory diagnostic fails, offers:
#   "Some extended memory on the Inboard 386/PC ... board has failed.
#    Press any key to display the location of the [failure]"
# That location is the one measurement nobody has taken for the `bad extended
# memory: 128k` report (86Box#7638). This drives that prompt and captures the
# screen after it. The fork's inject_key.txt hook does NOT exist in an upstream
# build, so the key has to go through the real window.
param(
  [Parameter(Mandatory=$true)][int]$MemSize,
  [Parameter(Mandatory=$true)][string]$Label,
  [string]$Bios = "",
  [int]$KeyAt = 52, [int]$From = 45, [int]$To = 95,
  [string]$ExePath = "", [string]$VmPath = ""
)
$root = "C:\Users\lycet\RiderProjects\86Box-Inboard"
$vm   = if ($VmPath  -ne "") { $VmPath  } else { "$root\vm_qbr" }
$exe  = if ($ExePath -ne "") { $ExePath } else { "C:\Users\lycet\RiderProjects\86Box-upstream-work\upstream\build\src\86Box.exe" }
$env:PATH = "C:\msys64\mingw64\bin;" + $env:PATH
Add-Type -AssemblyName System.Drawing
Add-Type @"
using System; using System.Runtime.InteropServices;
public class WB {
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr dc, uint f);
  [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out R r);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern void keybd_event(byte vk, byte scan, uint flags, IntPtr extra);
  public struct R { public int L, T, Rt, B; }
}
"@
Get-Process 86Box -EA SilentlyContinue | Stop-Process -Force; Start-Sleep 1
$cfg = (Get-Content "$vm\86box.cfg.template" -Raw) -replace '(?m)^mem_size = .*$',"mem_size = $MemSize"
if($Bios -ne ""){ $cfg = $cfg -replace '(?m)^bios = .*$',"bios = $Bios" }
$cfg | Set-Content "$vm\86box.cfg" -NoNewline
$p=Start-Process $exe -ArgumentList @("-P",$vm) -WorkingDirectory $vm -PassThru `
     -RedirectStandardError "$vm\stderr_$Label.txt" -RedirectStandardOutput "$vm\stdout_$Label.txt"
$sw=[Diagnostics.Stopwatch]::StartNew(); $n=0; $sent=$false
while($sw.Elapsed.TotalSeconds -lt $To -and -not $p.HasExited){
  $t=$sw.Elapsed.TotalSeconds
  $w=Get-Process 86Box -EA SilentlyContinue | Where-Object {$_.MainWindowHandle -ne 0} | Select-Object -First 1
  if($t -ge $KeyAt -and -not $sent -and $w){
    [void][WB]::SetForegroundWindow($w.MainWindowHandle); Start-Sleep -Milliseconds 400
    # SendKeys/WM_KEYDOWN do not reach 86Box's input layer; keybd_event does.
    [WB]::keybd_event(0x20,0,0,[IntPtr]::Zero); Start-Sleep -Milliseconds 80
    [WB]::keybd_event(0x20,0,2,[IntPtr]::Zero)
    $sent=$true; Write-Output "key sent at t=$([int]$t)s"
  }
  if($t -ge $From -and $w){
    [WB+R]$r=New-Object WB+R; [void][WB]::GetClientRect($w.MainWindowHandle,[ref]$r)
    $cw=$r.Rt-$r.L; $ch=$r.B-$r.T
    if($cw -gt 0 -and $ch -gt 0){
      $bmp=New-Object System.Drawing.Bitmap $cw,$ch
      $g=[System.Drawing.Graphics]::FromImage($bmp); $dc=$g.GetHdc()
      [void][WB]::PrintWindow($w.MainWindowHandle,$dc,3); $g.ReleaseHdc($dc); $g.Dispose()
      $bmp.Save(("{0}\bad_{1}_{2:0000}.png" -f $vm,$Label,[int]($t*10)),[System.Drawing.Imaging.ImageFormat]::Png)
      $bmp.Dispose(); $n++
    }
  }
  Start-Sleep -Milliseconds 250
}
Get-Process 86Box -EA SilentlyContinue | Stop-Process -Force
Write-Output "frames=$n keysent=$sent"
