# Launch one 86Box bed run with the pre-flight checks enforced (inboard-hw-debug technique 131).
# Refuses to launch rather than let Windows raise a dialog on the owner's desktop.
# Stops only the process it started.
#
#   tools\bed_launch.ps1 -Exe <86Box.exe> -VmPath <bed dir> [-Seconds 15] [-Log <file>] [-AllowMouse]
#
# -Seconds 0 leaves the VM running and prints its PID. -DryRun checks everything and launches nothing.

param(
  [Parameter(Mandatory)] [string]$Exe,
  [Parameter(Mandatory)] [string]$VmPath,
  [int]$Seconds = 0,
  [string]$Log = "",
  [switch]$AllowMouse,
  [switch]$DryRun
)
$ErrorActionPreference = 'Stop'
$msys = 'C:\msys64\mingw64\bin'
function Fail($m) { Write-Host "REFUSED: $m"; exit 2 }

$Exe = (Resolve-Path $Exe).Path; $VmPath = (Resolve-Path $VmPath).Path
$exeDir = Split-Path $Exe
$cfg = Join-Path $VmPath '86box.cfg'
if (-not (Test-Path $cfg)) { Fail "no 86box.cfg in $VmPath" }
if (-not $Log) { $Log = Join-Path $VmPath '86box.log' }

# 1. Which binary, built when, from which commit.
$tree = (Resolve-Path (Join-Path $exeDir '..\..')).Path
$head = git -C $tree log -1 --format='%h %ci' 2>$null
Write-Host ("exe   {0}`n      built {1:yyyy-MM-dd HH:mm:ss}, tree HEAD {2}" -f $Exe, (Get-Item $Exe).LastWriteTime, $head)
$headTime = git -C $tree log -1 --format='%ct' 2>$null
if ($headTime -and ([DateTimeOffset]::FromUnixTimeSeconds([int64]$headTime).LocalDateTime -gt (Get-Item $Exe).LastWriteTime)) {
  Write-Host "      WARNING: the exe predates HEAD - this run does not test the newest commit (gate G9)"
}
$cache = Join-Path (Split-Path $exeDir) 'CMakeCache.txt'
if (Test-Path $cache) {
  $flags = @(Select-String $cache -Pattern '^(QT|CMAKE_BUILD_TYPE|ENABLE_\w+_LOG)\b[^=]*=' | ForEach-Object { $_.Line })
  $flags += (Select-String $cache -Pattern '-D(ENABLE_\w+_LOG)=1' -AllMatches | ForEach-Object { $_.Matches } | ForEach-Object { $_.Groups[1].Value + '=1' } | Sort-Object -Unique)
  Write-Host ("      " + ($flags -join '  '))
}

# 2. Every imported DLL must resolve, or Windows shows a system-error dialog.
$objdump = Join-Path $msys 'objdump.exe'
$dlls = & $objdump -p $Exe | Select-String 'DLL Name: (.+)$' | ForEach-Object { $_.Matches[0].Groups[1].Value.Trim() }
$search = @($exeDir, $msys, "$env:WINDIR\System32")
$missing = $dlls | Where-Object { $d = $_; -not ($search | Where-Object { Test-Path (Join-Path $_ $d) }) -and $d -notmatch '^(api-ms-|ext-ms-)' }
if ($missing) { Fail "DLLs not found in $($search -join '; '): $($missing -join ', ')" }

# 3. Config: no mouse, ROMs reachable, every image inside the bed and present.
$ini = Get-Content $cfg
function Get-CfgValue($key) {
  $m = $ini | Select-String "^\s*$key\s*=\s*(\S+)" | Select-Object -First 1
  if ($m) { $m.Matches[0].Groups[1].Value } else { $null }
}
$mouse = Get-CfgValue 'mouse_type'
if (-not $AllowMouse -and $mouse -ne 'none') { Fail "mouse_type = '$mouse'; set it to none (or pass -AllowMouse)" }
$roms = @(@((Join-Path $VmPath 'roms\machines'), (Join-Path $exeDir 'roms\machines')) | Where-Object { Test-Path $_ })
if (-not $roms) { Fail "no roms\machines under the bed or the exe" }
foreach ($m in ($ini | Select-String '^\s*\w+_(fn|image_path)\s*=\s*(.+)$')) {
  $f = $m.Matches[0].Groups[2].Value.Trim()
  if (-not $f) { continue }
  $p = if ([IO.Path]::IsPathRooted($f)) { $f } else { Join-Path $VmPath $f }
  if (-not (Test-Path $p)) { Fail "image not found: $f" }
  $full = (Resolve-Path $p).Path
  if (-not $full.StartsWith($VmPath, 'OrdinalIgnoreCase')) { Fail "image outside the bed: $full" }
}
# A copied bed keeps the original's uuid, and 86Box then stops on a
# "This machine might have been moved or copied" dialog.
$uuid = Get-CfgValue 'uuid'
if ($uuid) {
  $repo = Split-Path $PSScriptRoot
  $twins = Get-ChildItem $repo -Filter 86box.cfg -Recurse -Depth 4 -EA SilentlyContinue |
    Where-Object { $_.FullName -ne $cfg -and (Select-String -Path $_.FullName -Pattern "^\s*uuid\s*=\s*$uuid" -Quiet) }
  if ($twins) { Fail "uuid $uuid is shared with $($twins[0].FullName) - delete the uuid line from a copied bed" }
}
Write-Host "cfg   $cfg (mouse $mouse, roms $($roms[0]))"
if ($DryRun) { Write-Host "DRY RUN: all checks passed, nothing launched"; exit 0 }

# 4. Launch with the MSYS2 runtime on PATH; flags are -P and -L only (see src/86box.c).
$env:PATH = "$msys;$env:PATH"
if (Test-Path $Log) { Remove-Item $Log }
$p = Start-Process $Exe -ArgumentList @('-P', $VmPath, '-L', $Log) -WorkingDirectory $VmPath -PassThru
Write-Host "pid   $($p.Id)"
if ($Seconds -le 0) { exit 0 }
Start-Sleep $Seconds
if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force; $p.WaitForExit(5000) | Out-Null }
$n = if (Test-Path $Log) { (Get-Content $Log).Count } else { 0 }
Write-Host "log   $Log ($n lines)"
if ($n -le 20) { Write-Host "NOTE: little past the header - an absent line means nothing without a positive control run" }
