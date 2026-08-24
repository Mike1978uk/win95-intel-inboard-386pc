# One shadow-RAM bisect run: set mem_size, boot the reporter's disk, screenshot, kill.
#   pwsh -File tools/shadow_run.ps1 -MemSize 2688 -Label ctl [-AliasBase 0x5F0000] [-Wait 200]
#
# The 5160 POST stops on ERROR. (RESUME = "F1" KEY) and the moment it does varies with
# mem_size, so F1 (XT scancode 59) is re-offered on a timer rather than once at a guess.
# It goes through the build's own inject_key.txt channel - SendInput/SendKeys never reach
# 86Box's SDL keyboard handling (confirmed 2026-07-24/25, see 386_dynarec.c [injectfix]).
param(
  [Parameter(Mandatory=$true)][int]$MemSize,
  [Parameter(Mandatory=$true)][string]$Label,
  [string]$AliasBase = "",
  [int]$Wait = 200
)
$root = "C:\Users\lycet\RiderProjects\86Box-Inboard"
$vm   = "$root\vm_shadow"
$exe  = "$root\86box_full\build\phase1_mingw\src\86Box.exe"

Get-Process 86Box -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Seconds 1
Remove-Item "$vm\inject_key.txt" -ErrorAction SilentlyContinue

$cfg = Get-Content "$vm\86box.cfg.template" -Raw
$cfg = $cfg -replace '(?m)^mem_size = .*$', "mem_size = $MemSize"
Set-Content -Path "$vm\86box.cfg" -Value $cfg -NoNewline

$env:INBOARD_MEM_TRACE = "1"
if ($AliasBase -ne "") { $env:INBOARD_ALIAS_BASE = $AliasBase } else { Remove-Item Env:\INBOARD_ALIAS_BASE -ErrorAction SilentlyContinue }

$err = "$vm\stderr_$Label.txt"; $out = "$vm\stdout_$Label.txt"
Start-Process -FilePath $exe -ArgumentList @("-P", $vm) -WorkingDirectory $vm `
  -RedirectStandardError $err -RedirectStandardOutput $out

$elapsed = 0
while ($elapsed -lt $Wait) {
  Start-Sleep -Seconds 8
  $elapsed += 8
  if ($elapsed -ge 24) { Set-Content -Path "$vm\inject_key.txt" -Value "59" -NoNewline }
  if ($elapsed -eq 96 -or $elapsed -eq 152) {
    pwsh -File "$root\tools\shot.ps1" "$vm\shot_${Label}_t$elapsed.png" | Out-Null
  }
}
pwsh -File "$root\tools\shot.ps1" "$vm\shot_$Label.png"
Get-Process 86Box -ErrorAction SilentlyContinue | Stop-Process -Force

$aliasHits = (Select-String -Path $err -Pattern 'ALIAS' -AllMatches -ErrorAction SilentlyContinue | Measure-Object).Count
$keys      = (Select-String -Path $err -Pattern 'keyinject' -AllMatches -ErrorAction SilentlyContinue | Measure-Object).Count
Write-Output "mem_size=$MemSize label=$Label aliasBase='$AliasBase' ALIAS-hits=$aliasHits keyinject=$keys stderr=$((Get-Item $err).Length)B"
