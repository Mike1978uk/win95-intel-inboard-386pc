# Run one shadow-RAM config under gdb to capture a backtrace on the crash.
#   pwsh -File tools/shadow_gdb.ps1 -MemSize 4096 -AliasBase 0x5F0000 -Label gdb4096
param(
  [Parameter(Mandatory=$true)][int]$MemSize,
  [Parameter(Mandatory=$true)][string]$Label,
  [string]$AliasBase = "",
  [int]$Wait = 240
)
$root = "C:\Users\lycet\RiderProjects\86Box-Inboard"
$vm   = "$root\vm_shadow"
$exe  = "$root\86box_full\build\phase1_mingw\src\86Box.exe"
$gdb  = "C:\msys64\mingw64\bin\gdb.exe"

Get-Process 86Box -ErrorAction SilentlyContinue | Stop-Process -Force
Get-Process gdb   -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Seconds 1
Remove-Item "$vm\inject_key.txt" -ErrorAction SilentlyContinue

$cfg = Get-Content "$vm\86box.cfg.template" -Raw
$cfg = $cfg -replace '(?m)^mem_size = .*$', "mem_size = $MemSize"
Set-Content -Path "$vm\86box.cfg" -Value $cfg -NoNewline

$env:INBOARD_MEM_TRACE = "1"
if ($AliasBase -ne "") { $env:INBOARD_ALIAS_BASE = $AliasBase } else { Remove-Item Env:\INBOARD_ALIAS_BASE -ErrorAction SilentlyContinue }

$args = @("-batch","-ex","run","-ex","bt 40","-ex","info registers","-ex","quit",
          "--args", $exe, "-P", $vm)
Start-Process -FilePath $gdb -ArgumentList $args -WorkingDirectory $vm `
  -RedirectStandardError "$vm\gdb_$Label.err" -RedirectStandardOutput "$vm\gdb_$Label.out"

$elapsed = 0
while ($elapsed -lt $Wait) {
  Start-Sleep -Seconds 8
  $elapsed += 8
  if ($elapsed -ge 24) { Set-Content -Path "$vm\inject_key.txt" -Value "59" -NoNewline }
  if (-not (Get-Process gdb -ErrorAction SilentlyContinue)) { break }
}
Get-Process 86Box -ErrorAction SilentlyContinue | Stop-Process -Force
Get-Process gdb   -ErrorAction SilentlyContinue | Stop-Process -Force
Write-Output "elapsed=$elapsed out=$((Get-Item "$vm\gdb_$Label.out").Length)B err=$((Get-Item "$vm\gdb_$Label.err").Length)B"
