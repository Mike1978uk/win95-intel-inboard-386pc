# Drive a guest through an interactive DOS/Windows installer unattended.
#
# Keys are given as a script: "<seconds>:<keys>" pairs, fired at those elapsed times.
# <keys> is plain text sent a character at a time, with {ENTER} {ESC} {TAB} {F1}..{F10}
# {UP} {DOWN} {BS} as named keys. Frames are deduplicated as captured.
#
# SetForegroundWindow alone is refused across processes - AttachThreadInput to the current
# foreground thread first is what makes it stick (Technique 71).
param(
  [Parameter(Mandatory=$true)][string]$VmPath,
  [Parameter(Mandatory=$true)][string[]]$Script,
  [string]$ExePath = "C:\Users\lycet\RiderProjects\86Box-Inboard\86box_full\build\phase1_mingw\src\86Box.exe",
  [string]$Label = "run",
  [int]$To = 180
)
$env:PATH = "C:\msys64\mingw64\bin;" + $env:PATH
Add-Type -AssemblyName System.Drawing
Add-Type -AssemblyName System.Windows.Forms
Add-Type @"
using System; using System.Runtime.InteropServices;
public class GD {
  [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, IntPtr p);
  [DllImport("user32.dll")] public static extern bool AttachThreadInput(uint a, uint b, bool f);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern bool BringWindowToTop(IntPtr h);
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int c);
  [DllImport("kernel32.dll")] public static extern uint GetCurrentThreadId();
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr dc, uint f);
  [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out R r);
  public struct R { public int L,T,Rt,B; }
  [DllImport("user32.dll")] public static extern void keybd_event(byte vk, byte sc, uint f, IntPtr x);
  public static void Key(int sc, bool up) {
    /* KEYEVENTF_SCANCODE (0x0008) - vk is ignored, the scancode is authoritative. */
    keybd_event(0, (byte) sc, (uint)(0x0008 | (up ? 0x0002 : 0)), IntPtr.Zero);
  }
  public static void Focus(IntPtr h) {
    uint me = GetCurrentThreadId();
    uint other = GetWindowThreadProcessId(GetForegroundWindow(), IntPtr.Zero);
    AttachThreadInput(me, other, true);
    ShowWindow(h, 9); BringWindowToTop(h); SetForegroundWindow(h);
    AttachThreadInput(me, other, false);
  }
}
"@

# Type by RAW XT SCANCODE, not by virtual key.
#
# SendKeys/VkKeyScan go through the HOST layout: on a UK host, "\" is scancode 0x56 - a key
# that only exists on 102-key European keyboards. An 83-key IBM XT (Model F) has none, and
# 86Box's scancode_xt entry 056 emits nothing, so the guest sees absolutely nothing. This is
# the same trap as issue #2 (# in place of \) and it silently ate every path this harness
# typed. Sending scancodes directly makes host layout irrelevant.
$XT = @{
  'a'=0x1E;'b'=0x30;'c'=0x2E;'d'=0x20;'e'=0x12;'f'=0x21;'g'=0x22;'h'=0x23;'i'=0x17;'j'=0x24
  'k'=0x25;'l'=0x26;'m'=0x32;'n'=0x31;'o'=0x18;'p'=0x19;'q'=0x10;'r'=0x13;'s'=0x1F;'t'=0x14
  'u'=0x16;'v'=0x2F;'w'=0x11;'x'=0x2D;'y'=0x15;'z'=0x2C
  '1'=0x02;'2'=0x03;'3'=0x04;'4'=0x05;'5'=0x06;'6'=0x07;'7'=0x08;'8'=0x09;'9'=0x0A;'0'=0x0B
  '-'=0x0C;'='=0x0D;'['=0x1A;']'=0x1B;';'=0x27;"'"=0x28;'`'=0x29;'\'=0x2B;','=0x33;'.'=0x34
  '/'=0x35;' '=0x39
}
$XTSHIFT = @{ ':'=0x27;'?'=0x35;'*'=0x09;'+'=0x0D;'_'=0x0C;'"'=0x28;'!'=0x02;'>'=0x34;'<'=0x33 }
$XTNAMED = @{ 'ENTER'=0x1C;'ESC'=0x01;'TAB'=0x0F;'BS'=0x0E;'UP'=0x48;'DOWN'=0x50
              'LEFT'=0x4B;'RIGHT'=0x4D;'F1'=0x3B;'F2'=0x3C;'F3'=0x3D;'F4'=0x3E;'F5'=0x3F
              'F6'=0x40;'F7'=0x41;'F8'=0x42;'F9'=0x43;'F10'=0x44 }
function Tap-Scan([int]$sc, [bool]$shift) {
  if ($shift) { [GD]::Key(0x2A, $false) ; Start-Sleep -Milliseconds 25 }
  [GD]::Key($sc, $false); Start-Sleep -Milliseconds 35; [GD]::Key($sc, $true)
  if ($shift) { Start-Sleep -Milliseconds 25; [GD]::Key(0x2A, $true) }
  Start-Sleep -Milliseconds 55
}
function Send-Guest([string]$text) {
  $i = 0
  while ($i -lt $text.Length) {
    if ($text[$i] -eq '{') {
      $j = $text.IndexOf('}', $i)
      $name = $text.Substring($i+1, $j-$i-1).ToUpper()
      if ($XTNAMED.ContainsKey($name)) { Tap-Scan $XTNAMED[$name] $false }
      $i = $j + 1
      continue
    }
    $ch = $text[$i]
    $lower = [string]$ch
    if ($XTSHIFT.ContainsKey($lower))      { Tap-Scan $XTSHIFT[$lower] $true }
    elseif ([char]::IsUpper($ch))          { Tap-Scan $XT[([string]$ch).ToLower()] $true }
    elseif ($XT.ContainsKey($lower))       { Tap-Scan $XT[$lower] $false }
    $i++
  }
}

Get-Process 86Box -EA SilentlyContinue | Stop-Process -Force; Start-Sleep 1
$out = "$VmPath\$Label"; Remove-Item $out -Recurse -Force -EA SilentlyContinue
New-Item -ItemType Directory $out | Out-Null
$p = Start-Process $ExePath -ArgumentList @("-P",$VmPath) -WorkingDirectory $VmPath -PassThru `
       -RedirectStandardError "$VmPath\stderr_$Label.txt" -RedirectStandardOutput "$VmPath\stdout_$Label.txt"
$steps = @()
foreach ($e in $Script) { $i = $e.IndexOf(':'); $steps += ,@([int]$e.Substring(0,$i), $e.Substring($i+1)) }
$si = 0
$sw = [Diagnostics.Stopwatch]::StartNew(); $seen = @{}; $md5 = [Security.Cryptography.MD5]::Create()
while ($sw.Elapsed.TotalSeconds -lt $To -and -not $p.HasExited) {
  $t = $sw.Elapsed.TotalSeconds
  $w = Get-Process 86Box -EA SilentlyContinue | Where-Object { $_.MainWindowHandle -ne 0 } | Select-Object -First 1
  if ($w) {
    [GD+R]$r = New-Object GD+R; [void][GD]::GetClientRect($w.MainWindowHandle,[ref]$r)
    $cw = $r.Rt-$r.L; $ch = $r.B-$r.T
    if ($cw -gt 0 -and $ch -gt 0) {
      $bmp = New-Object System.Drawing.Bitmap $cw,$ch
      $g = [System.Drawing.Graphics]::FromImage($bmp); $dc = $g.GetHdc()
      [void][GD]::PrintWindow($w.MainWindowHandle,$dc,3); $g.ReleaseHdc($dc); $g.Dispose()
      $ms = New-Object IO.MemoryStream; $bmp.Save($ms,[System.Drawing.Imaging.ImageFormat]::Png)
      $h = [BitConverter]::ToString($md5.ComputeHash($ms.ToArray()))
      if (-not $seen.ContainsKey($h)) {
        $seen[$h] = $t
        $bmp.Save(("{0}\f{1:0000}.png" -f $out,[int]$t),[System.Drawing.Imaging.ImageFormat]::Png)
      }
      $ms.Dispose(); $bmp.Dispose()
    }
    while ($si -lt $steps.Count -and $t -ge $steps[$si][0]) {
      [GD]::Focus($w.MainWindowHandle); Start-Sleep -Milliseconds 200
      $keys = $steps[$si][1]
      if ($keys -ne "") { Send-Guest $keys }
      Write-Output ("t={0:0} sent: {1}" -f $t, $keys)
      $si++
    }
  }
  Start-Sleep -Milliseconds 700
}
Get-Process 86Box -EA SilentlyContinue | Stop-Process -Force
Write-Output "distinct=$((Get-ChildItem $out).Count)"
