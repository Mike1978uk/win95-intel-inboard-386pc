# Issue #14 asked for exactly this: 10 runs at one memory size on a build with no debug
# instrumentation, to decide whether the intermittent POST 101 is real or an artifact of
# the instrumented build's much lower guest instructions/sec.
param([int]$MemSize = 3072, [int]$Runs = 10, [int]$To = 40)
$vm  = "C:\Users\lycet\RiderProjects\86Box-Inboard\vm_qbr"
$exe = "C:/Users/lycet/RiderProjects/86Box-Inboard/tools/86Box_stock.exe"
$env:PATH = "C:\msys64\mingw64\bin;" + $env:PATH
Add-Type -AssemblyName System.Drawing
Add-Type @"
using System; using System.Runtime.InteropServices;
public class PW {
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr dc, uint f);
  [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out R r);
  public struct R { public int L,T,Rt,B; }
}
"@
(Get-Content "$vm\86box.cfg.template" -Raw) -replace '(?m)^mem_size = .*$',"mem_size = $MemSize" |
  Set-Content "$vm\86box.cfg" -NoNewline
$out = "$vm\post101_$MemSize"; Remove-Item $out -Recurse -Force -EA SilentlyContinue
New-Item -ItemType Directory $out | Out-Null
for ($i = 1; $i -le $Runs; $i++) {
  Get-Process 86Box -EA SilentlyContinue | Stop-Process -Force; Start-Sleep 1
  $p = Start-Process $exe -ArgumentList @("-P",$vm) -WorkingDirectory $vm -PassThru `
         -RedirectStandardError "$out\e$i.txt" -RedirectStandardOutput "$out\o$i.txt"
  Start-Sleep $To
  $w = Get-Process 86Box -EA SilentlyContinue | Where-Object { $_.MainWindowHandle -ne 0 } | Select-Object -First 1
  if ($w) {
    [PW+R]$r = New-Object PW+R; [void][PW]::GetClientRect($w.MainWindowHandle,[ref]$r)
    $bmp = New-Object System.Drawing.Bitmap ($r.Rt-$r.L),($r.B-$r.T)
    $g = [System.Drawing.Graphics]::FromImage($bmp); $dc = $g.GetHdc()
    [void][PW]::PrintWindow($w.MainWindowHandle,$dc,3); $g.ReleaseHdc($dc); $g.Dispose()
    $bmp.Save("$out\run$i.png",[System.Drawing.Imaging.ImageFormat]::Png); $bmp.Dispose()
  }
  Write-Output "run $i captured"
}
Get-Process 86Box -EA SilentlyContinue | Stop-Process -Force
