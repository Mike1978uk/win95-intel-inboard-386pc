# One driver iteration in one call: build both variants of M8DISP.DRV, put the
# trace build into the bed image, commit the given paths, push. Prints one line.
#
#   tools\m8_cycle.ps1 -Message "D9: ..." [-Paths a,b,...] [-NoCommit] [-NoImage]
#
# -Paths defaults to the driver source folder and the tally script. The image
# write refuses while 86Box runs (tools\fatcp.py), so close the VM first.

param(
  [string]$Message,
  [string[]]$Paths = @('drivers/mach8_display/src', 'tools/m8_fallback_tally.py'),
  [switch]$NoCommit,
  [switch]$NoImage
)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot
$src = Join-Path $root 'drivers\mach8_display\src'
$plain = Join-Path $env:TEMP 'm8plain'
Set-Location $root

& "$src\build.ps1" -Out $plain | Out-Null
& "$src\build.ps1" -Trace | Out-Null
$drv = Join-Path $src 'build\M8DISP.DRV'
$size = (Get-Item $drv).Length

$img = 'skipped'
if (-not $NoImage) {
  $out = python -I tools/fatcp.py vm_5160_now/card.img 'WINDOWS/SYSTEM/M8DISP.DRV' $drv --yes 2>&1
  if ($LASTEXITCODE -ne 0) { throw "image write failed: $out" }
  $img = 'in image'
}

$commit = 'not committed'
if (-not $NoCommit) {
  if (-not $Message) { throw '-Message is required unless -NoCommit' }
  # From `pwsh -File`, a comma list arrives as one string.
  $list = @($Paths | ForEach-Object { $_ -split ',' } | Where-Object { $_ })
  git add -- $list
  $msg = "$Message`n`nCo-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
  git commit -q -m $msg
  if ($LASTEXITCODE -ne 0) { throw 'commit failed (nothing staged?)' }
  git push -q origin master 2>&1 | Out-Null
  $commit = (git log --oneline -1)
}
Write-Output "built $size bytes (both variants), $img, $commit"
