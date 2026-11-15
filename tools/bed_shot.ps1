# Full-frame picture of a bed's screen at any guest depth, from 86Box's automation socket
# (bed started with tools\bed_launch.ps1 -Automation <port>). The agent's own screenshot fails at
# 16 bpp, and a host window grab shows only what the window shows.
#
#   tools\bed_shot.ps1 -VmPath vm_5160_mcp -Port 9890 -Out shot.png
param(
  [Parameter(Mandatory)][string]$VmPath,
  [Parameter(Mandatory)][int]$Port,
  [Parameter(Mandatory)][string]$Out
)
$ErrorActionPreference = 'Stop'
$token = (Get-Content (Join-Path $VmPath 'automation.token') -Raw).Trim()
$Out = [IO.Path]::GetFullPath($Out)
$c = [Net.Sockets.TcpClient]::new('127.0.0.1', $Port)
try {
  $s = $c.GetStream()
  $w = [IO.StreamWriter]::new($s); $w.NewLine = "`n"; $w.AutoFlush = $true
  $r = [IO.StreamReader]::new($s)
  $w.WriteLine("auth $token"); $a = $r.ReadLine()
  if ($a -notmatch '"ok":true') { throw "auth: $a" }
  # The path is written by 86Box on this machine; forward slashes keep it one word.
  $w.WriteLine("screenshot $($Out.Replace('\', '/'))"); $j = $r.ReadLine()
  if ($j -notmatch '"ok":true') { throw "screenshot: $j" }
  Write-Output "$Out $j"
} finally { $c.Close() }
