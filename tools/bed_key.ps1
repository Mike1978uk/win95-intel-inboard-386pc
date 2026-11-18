# Press keys in a bed through 86Box's automation socket (bed started with
# tools\bed_launch.ps1 -Automation <port>), or read its text screen.
#
#   tools\bed_key.ps1 -VmPath vm_6695_w2 -Port 9898 -Keys 0x31,0x1C     press N, then Enter
#   tools\bed_key.ps1 -VmPath vm_6695_w2 -Port 9898 -Text                print the text screen
# Keys are XT (set 1) make codes, 0x1xx for an E0-prefixed key; each is pressed and released.
param(
  [Parameter(Mandatory)][string]$VmPath,
  [Parameter(Mandatory)][int]$Port,
  [string[]]$Keys = @(),
  [switch]$Text
)
$ErrorActionPreference = 'Stop'
$token = (Get-Content (Join-Path $VmPath 'automation.token') -Raw).Trim()
$c = [Net.Sockets.TcpClient]::new('127.0.0.1', $Port)
try {
  $s = $c.GetStream()
  $w = [IO.StreamWriter]::new($s); $w.NewLine = "`n"; $w.AutoFlush = $true
  $r = [IO.StreamReader]::new($s)
  $w.WriteLine("auth $token"); $a = $r.ReadLine()
  if ($a -notmatch '"ok":true') { throw "auth: $a" }
  foreach ($k in $Keys) {
    foreach ($dir in 'down', 'up') {
      $w.WriteLine("key $dir $k"); $j = $r.ReadLine()
      if ($j -notmatch '"ok":true') { throw "key $dir ${k}: $j" }
    }
  }
  if ($Text) { $w.WriteLine('text'); Write-Output $r.ReadLine() }
} finally { $c.Close() }
