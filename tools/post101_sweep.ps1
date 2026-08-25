# Issue #14: is the intermittent POST 101 real, or an artefact of our instrumented build?
# Runs N boots at one mem_size on a chosen exe and classifies each by screenshot.
#
# Two traps this script exists to avoid, both hit on 2026-08-25:
#   1. Get-Process 86Box does NOT match a copy named 86Box_stock.exe, so the old loop
#      never screenshotted and never killed anything - it left 20 emulators running,
#      all sharing one VM directory and fighting over the same disk image and nvr.
#      Everything here tracks the process object it started, by PID, never by name.
#   2. Concurrent runs against one VM dir invalidate the result. One at a time, verified.
param(
  [int]$MemSize = 3072,
  [int]$Runs = 10,
  [int]$To = 40,
  [string]$Exe = "C:/Users/lycet/RiderProjects/86Box-Inboard/tools/86Box_stock.exe",
  [string]$Vm  = "C:/Users/lycet/RiderProjects/86Box-Inboard/vm_qbr",
  [string]$Tag = "stock"
)
$env:PATH = "C:\msys64\mingw64\bin;" + $env:PATH
Add-Type -AssemblyName System.Drawing
Add-Type @"
using System; using System.Runtime.InteropServices;
public class PW2 {
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr dc, uint f);
  [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out R r);
  public struct R { public int L,T,Rt,B; }
}
"@
(Get-Content "$Vm\86box.cfg.template" -Raw) -replace '(?m)^mem_size = .*$', "mem_size = $MemSize" |
  Set-Content "$Vm\86box.cfg" -NoNewline
$out = "$Vm\post101_${Tag}_$MemSize"
Remove-Item $out -Recurse -Force -EA SilentlyContinue
New-Item -ItemType Directory $out | Out-Null

for ($i = 1; $i -le $Runs; $i++) {
  $p = Start-Process $Exe -ArgumentList @("-P", $Vm) -WorkingDirectory $Vm -PassThru `
         -RedirectStandardError "$out\e$i.txt" -RedirectStandardOutput "$out\o$i.txt"
  Start-Sleep $To
  $shot = "NOWINDOW"
  $p.Refresh()
  if (-not $p.HasExited -and $p.MainWindowHandle -ne 0) {
    [PW2+R]$r = New-Object PW2+R
    [void][PW2]::GetClientRect($p.MainWindowHandle, [ref]$r)
    $w = $r.Rt - $r.L; $h = $r.B - $r.T
    if ($w -gt 0 -and $h -gt 0) {
      $bmp = New-Object System.Drawing.Bitmap $w, $h
      $g = [System.Drawing.Graphics]::FromImage($bmp); $dc = $g.GetHdc()
      [void][PW2]::PrintWindow($p.MainWindowHandle, $dc, 3); $g.ReleaseHdc($dc); $g.Dispose()
      $bmp.Save("$out\run$i.png", [System.Drawing.Imaging.ImageFormat]::Png); $bmp.Dispose()
      $shot = "run$i.png"
    }
  } elseif ($p.HasExited) { $shot = "EXITED:$($p.ExitCode)" }
  # Kill only what this iteration started, then confirm the field is clear.
  if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force -EA SilentlyContinue }
  Start-Sleep 2
  $alive = (Get-Process | Where-Object { $_.ProcessName -like '86Box*' } | Measure-Object).Count
  Write-Output "run $i  shot=$shot  alive_after=$alive"
  if ($alive -ne 0) { Write-Output "  WARNING: $alive emulator(s) still running - results suspect" }
}
Write-Output "SWEEP DONE mem=$MemSize tag=$Tag"
