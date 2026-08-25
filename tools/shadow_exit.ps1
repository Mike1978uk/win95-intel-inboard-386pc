# Watch one run to the moment 86Box exits and record when, and with what code.
param(
  [Parameter(Mandatory=$true)][int]$MemSize,
  [Parameter(Mandatory=$true)][string]$Label,
  [string]$AliasBase = "",
  [int]$Wait = 240,
  [string]$ExePath = ""    # NOTE: not $Exe - PowerShell vars are case-insensitive,
                          # so a param named $Exe IS the script's own $exe and gets
                          # silently overwritten before it can be read.
)
$root = "C:\Users\lycet\RiderProjects\86Box-Inboard"
$vm   = "$root\vm_shadow"
$exe  = if ($ExePath -ne "") { $ExePath } else { "C:\Users\lycet\RiderProjects\86Box-Inboard\86box_full\build\phase1_mingw\src\86Box.exe" }
$env:PATH = "C:\msys64\mingw64\bin;" + $env:PATH   # a MinGW-built 86Box needs these on PATH or it exits 0xC0000135 (DLL not found)
Get-Process 86Box -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Seconds 1
Remove-Item "$vm\inject_key.txt" -ErrorAction SilentlyContinue
$cfg = Get-Content "$vm\86box.cfg.template" -Raw
$cfg = $cfg -replace '(?m)^mem_size = .*$', "mem_size = $MemSize"
Set-Content -Path "$vm\86box.cfg" -Value $cfg -NoNewline
$env:INBOARD_MEM_TRACE = "1"
if ($AliasBase -ne "") { $env:INBOARD_ALIAS_BASE = $AliasBase } else { Remove-Item Env:\INBOARD_ALIAS_BASE -ErrorAction SilentlyContinue }
$p = Start-Process -FilePath $exe -ArgumentList @("-P", $vm) -WorkingDirectory $vm -PassThru `
       -RedirectStandardError "$vm\stderr_$Label.txt" -RedirectStandardOutput "$vm\stdout_$Label.txt"
$t = 0
while (-not $p.HasExited -and $t -lt $Wait) {
  Start-Sleep -Seconds 2; $t += 2
  if ($t -ge 24 -and ($t % 8) -eq 0) { Set-Content -Path "$vm\inject_key.txt" -Value "59" -NoNewline }
  if ($t % 10 -eq 0 -and -not $p.HasExited) { pwsh -File "$root\tools\shot.ps1" "$vm\live_${Label}_t$t.png" 2>$null | Out-Null }
}
if ($p.HasExited) { Write-Output "EXITED at t=${t}s exitcode=$($p.ExitCode) (0x$('{0:X}' -f $p.ExitCode))" }
else { Write-Output "still running at t=${t}s"; pwsh -File "$root\tools\shot.ps1" "$vm\live_${Label}_final.png" }
Get-Process 86Box -ErrorAction SilentlyContinue | Stop-Process -Force
Write-Output "ALIAS=$((Select-String -Path "$vm\stderr_$Label.txt" -Pattern '\[inbmem\] ALIAS' -AllMatches | Measure-Object).Count) ringshadow=$((Select-String -Path "$vm\stderr_$Label.txt" -Pattern 'ringshadow' -AllMatches | Measure-Object).Count)"
