# Dense burst capture: the Inboard driver's status panel is on screen for only a few
# seconds when its self-test PASSES (it only pauses on failure), and the "extended
# memory detected" figure counts up while it probes - so a slow cadence catches a
# mid-count value and reads like a regression. Captures in-process rather than
# spawning pwsh per frame, which is what made the cadence too slow to be useful.
param(
  [Parameter(Mandatory=$true)][int]$MemSize,
  [Parameter(Mandatory=$true)][string]$Label,
  [string]$AliasBase = "",
  [int]$From = 44, [int]$To = 100
)
$root="C:\Users\lycet\RiderProjects\86Box-Inboard"; $vm="$root\vm_shadow"
$exe="$root\86box_full\build\phase1_mingw\src\86Box.exe"
Add-Type -AssemblyName System.Drawing
Add-Type @"
using System; using System.Runtime.InteropServices;
public class WB {
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr dc, uint f);
  [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out R r);
  public struct R { public int L, T, Rt, B; }
}
"@
Get-Process 86Box -EA SilentlyContinue | Stop-Process -Force; Start-Sleep 1
Remove-Item "$vm\inject_key.txt" -EA SilentlyContinue
if($AliasBase -ne ""){$env:INBOARD_ALIAS_BASE=$AliasBase}else{Remove-Item Env:\INBOARD_ALIAS_BASE -EA SilentlyContinue}
$env:INBOARD_MEM_TRACE="1"
(Get-Content "$vm\86box.cfg.template" -Raw) -replace '(?m)^mem_size = .*$',"mem_size = $MemSize" | Set-Content "$vm\86box.cfg" -NoNewline
$p=Start-Process $exe -ArgumentList @("-P",$vm) -WorkingDirectory $vm -PassThru `
     -RedirectStandardError "$vm\stderr_$Label.txt" -RedirectStandardOutput "$vm\stdout_$Label.txt"
$sw=[Diagnostics.Stopwatch]::StartNew(); $n=0; $lastKey=0
while($sw.Elapsed.TotalSeconds -lt $To -and -not $p.HasExited){
  $t=$sw.Elapsed.TotalSeconds
  if($t -ge 24 -and ($t-$lastKey) -ge 8){ Set-Content "$vm\inject_key.txt" "59" -NoNewline; $lastKey=$t }
  if($t -ge $From){
    $w=Get-Process 86Box -EA SilentlyContinue | Where-Object {$_.MainWindowHandle -ne 0} | Select-Object -First 1
    if($w){
      [WB+R]$r=New-Object WB+R; [void][WB]::GetClientRect($w.MainWindowHandle,[ref]$r)
      $cw=$r.Rt-$r.L; $ch=$r.B-$r.T
      if($cw -gt 0 -and $ch -gt 0){
        $bmp=New-Object System.Drawing.Bitmap $cw,$ch
        $g=[System.Drawing.Graphics]::FromImage($bmp); $dc=$g.GetHdc()
        [void][WB]::PrintWindow($w.MainWindowHandle,$dc,3); $g.ReleaseHdc($dc); $g.Dispose()
        $bmp.Save(("{0}\burst_{1}_{2:0000}.png" -f $vm,$Label,[int]($t*10)),[System.Drawing.Imaging.ImageFormat]::Png)
        $bmp.Dispose(); $n++
      }
    }
  }
  Start-Sleep -Milliseconds 250
}
Get-Process 86Box -EA SilentlyContinue | Stop-Process -Force
Write-Output "frames=$n writes=$((Select-String "$vm\stderr_$Label.txt" -Pattern '\[inbmem\] ALIAS write' -AllMatches|Measure-Object).Count) ring101=$((Select-String "$vm\stderr_$Label.txt" -Pattern 'ring101' -AllMatches|Measure-Object).Count) ringshadow=$((Select-String "$vm\stderr_$Label.txt" -Pattern 'ringshadow' -AllMatches|Measure-Object).Count)"
