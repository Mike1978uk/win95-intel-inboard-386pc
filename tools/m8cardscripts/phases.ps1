param([string]$Label, [string]$File = (Join-Path $env:TEMP 'card_tunnel\M8CENSUS.TXT'))
# Per-flip figures between census readings <Label>-a and <Label>-b, phases included.
$t = Get-Content $File
$a = ($t | Select-String "^== M8CENSUS $Label-a" | Select-Object -Last 1).LineNumber
$b = ($t | Select-String "^== M8CENSUS $Label-b" | Select-Object -Last 1).LineNumber
if (-not $a -or -not $b) { "no census for $Label"; exit 1 }
function Nums($start, $p) { $l = $t[$start..($start + 15)] | Where-Object { $_ -match "^$p" } | Select-Object -First 1; [regex]::Matches($l, '\d+') | ForEach-Object { [double]$_.Value } }
$ca = Nums $a 'clock'; $cb = Nums $b 'clock'; $f = $ca[0]; $n = $cb[4] - $ca[4]
function Ms($x, $y) { [math]::Round(($y - $x) * 1000 / $f / $n, 1) }
"flips $n  ours $(Ms $ca[1] $cb[1]) ms  flip $(Ms $ca[2] $cb[2]) ms"
$pa = Nums $a 'phases'; $pb = Nums $b 'phases'
if ($pa) {
  $names = 'erase+check', 'edge build', 'outlines', 'fills', 'redraw+clear', 'singles', 'final send', 'engine wait'
  for ($k = 0; $k -lt 8; $k++) { '{0,-13} {1,6} ms' -f $names[$k], (Ms $pa[$k] $pb[$k]) }
  $fa = Nums $a 'fifo'; $fb = Nums $b 'fifo'
  "blocking sends $(Ms $fa[2] $fb[2]) ms; room checks $([int](($fb[1]-$fa[1])/$n)); full reads $([int](($fb[0]-$fa[0])/$n))"
}
$wa = Nums $a 'writes'; $wb = Nums $b 'writes'
"writes $([int](($wb[0]-$wa[0])/$n))  pieces $([int](($wb[1]-$wa[1])/$n)) a flip"
