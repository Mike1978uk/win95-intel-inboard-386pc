# The frame-rate scoreboard (docs/mach8_frame_rate_plan.md): on the real 5160, through the V9x
# Remote Agent, run M8CUBE (3D through the polygon escape), M8DDB (DirectDraw, 10 tests) and M8P0
# (RAM and port costs) twice each against the driver already running, and print one table.
# -Gdi adds the 35-test M8BENCH through m8_card_cycle.ps1 -BenchOnly.
#
#   tools\m8_score.ps1 -Label solid [-Cubes 3] [-Seconds 10] [-Gdi]
#
# Results stay on the card in C:\M8CUBE.TXT and C:\M8DDB.TXT (appended); the table goes to the
# console and to %TEMP%\m8_score_<Label>.txt. Needs the card idle: run it after a bench, not during.
param(
  [Parameter(Mandatory)][string]$Label,
  [int]$Cubes = 3,
  [int]$Seconds = 10,
  [string]$CardHost = '192.168.68.62',
  [switch]$Gdi
)
$ErrorActionPreference = 'Stop'
$ctl = 'C:\Users\lycet\tools\v9x-remote-agent\host\v9xremote-0.12.0\scripts\v9xctl.ps1'
$tmp = Join-Path $env:TEMP 'm8_score'
New-Item -ItemType Directory -Force $tmp | Out-Null

function Agent([string]$verb, [hashtable]$a) {
  $out = & $ctl $verb @a -Host $CardHost -ConnectTimeoutSeconds 60 -Json
  if ($LASTEXITCODE -ne 0) { throw "v9xctl $verb failed ($LASTEXITCODE): $out" }
  return ($out | ConvertFrom-Json)
}
function Run([string]$exe, [string]$args_, [int]$timeout) {
  return Agent exec @{ Application = $exe; Arguments = $args_; TimeoutSeconds = $timeout }
}
function Fetch([string]$guest) {
  $local = Join-Path $tmp ([IO.Path]::GetFileName($guest))
  Agent get @{ Source = $guest; Destination = $local } | Out-Null
  return Get-Content $local
}

$lines = @("== $Label  $(Get-Date -Format 'yyyy-MM-dd HH:mm')")

# 3D: two runs, the last two lines of the card's log are ours.
foreach ($i in 1, 2) { Run 'C:\M8CUBE.EXE' "$Seconds $Cubes" ($Seconds + 60) | Out-Null }
$cube = Fetch 'C:\M8CUBE.TXT' | Select-Object -Last 2
foreach ($c in $cube) {
  if ($c -match '([\d.]+) fps\s+(\d+) faces/s') { $lines += ('3D   M8CUBE {0} cubes   {1,7} fps {2,6} faces/s' -f $Cubes, $Matches[1], $Matches[2]) }
}

# DirectDraw: two labelled runs.
foreach ($i in 1, 2) { Run 'C:\M8DDB.EXE' "$Label$i" 120 | Out-Null }
$dd = Fetch 'C:\M8DDB.TXT'
$in = $false; $run = ''
foreach ($l in $dd) {
  if ($l -match "^== ?(\S+)") { $in = $Matches[1] -like "$Label[12]"; $run = $Matches[1]; continue }
  if ($l -match "^$Label[12]\b") { $in = $true; $run = ($l -split '\s+')[0] }
  if ($in -and $l -match '^(\S+)\s+([\d.]+) ops/s') { $lines += ('DD   {0,-16} {1,10} ops/s  ({2})' -f $Matches[1], $Matches[2], $run) }
}

# Costs: two runs of M8P0's R and O arms.
foreach ($i in 1, 2) {
  $r = Run 'C:\M8P0.EXE' '' 120
  foreach ($l in ($r.Stdout -split "`r?`n")) { if ($l -match '^[RO] ') { $lines += "P0   $l" } }
}

if ($Gdi) {
  $lines += (& (Join-Path $PSScriptRoot 'm8_card_cycle.ps1') -Label "gdi_$Label" -BenchOnly 2>&1 |
    Where-Object { $_ -match 'faster in|geometric|<--' })
}

$lines | Tee-Object -FilePath (Join-Path $env:TEMP "m8_score_$Label.txt")
