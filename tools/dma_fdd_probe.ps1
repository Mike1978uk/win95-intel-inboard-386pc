# Boot the transplanted Win95 image and capture whether floppy DMA (channel 2) is being
# truncated by the XT's 4-bit page latch.
#
# Self-retries on POST 101. That halt is a hard HLT with IF=0 - the guest never boots, so a
# run that hits it measures NOTHING (see issue #14). Detect it early and restart rather than
# burning the whole window on a dead run.
param([int]$Attempts = 5, [int]$PostCheck = 100, [int]$To = 900)
$vm  = "C:\Users\lycet\RiderProjects\86Box-Inboard\vm_fdd"
$exe = "C:\Users\lycet\RiderProjects\86Box-Inboard\86box_full\build\phase1_mingw\src\86Box.exe"
$env:PATH = "C:\msys64\mingw64\bin;" + $env:PATH
$env:INBOARD_DMA_TRACE = "1"
for ($a = 1; $a -le $Attempts; $a++) {
  Get-Process 86Box -EA SilentlyContinue | Stop-Process -Force; Start-Sleep 3
  $err = "$vm\stderr_dma.txt"
  Remove-Item $err -EA SilentlyContinue
  $p = Start-Process $exe -ArgumentList @("-P",$vm) -WorkingDirectory $vm -PassThru `
         -RedirectStandardError $err -RedirectStandardOutput "$vm\stdout_dma.txt"
  Start-Sleep $PostCheck
  # Absence of ring101 is only meaningful if the guest actually RAN. An empty stderr means
  # 86Box never started (it has failed to relaunch when the 2 GB image is still locked from a
  # previous run), and treating that as "POST clean" reports dmapage=0 / TRUNCATED=0 as though
  # nothing was truncated - a false negative indistinguishable from a pass. Check liveness first.
  $size = (Get-Item $err -EA SilentlyContinue).Length
  if ($p.HasExited -or $size -lt 1024) {
    Write-Output "attempt $a : 86Box did not run (exited=$($p.HasExited) stderr=$size bytes) - retrying"
    Start-Sleep 10
    continue
  }
  $halted = (Select-String $err -Pattern 'ring101' -List -EA SilentlyContinue) -ne $null
  if ($halted) {
    Write-Output "attempt $a : POST 101 - discarding and retrying"
    continue
  }
  Write-Output "attempt $a : POST clean, letting it boot ($To s)"
  $sw = [Diagnostics.Stopwatch]::StartNew()
  while ($sw.Elapsed.TotalSeconds -lt $To -and -not $p.HasExited) { Start-Sleep 10 }
  break
}
Get-Process 86Box -EA SilentlyContinue | Stop-Process -Force
$dp = (Select-String "$vm\stderr_dma.txt" -Pattern '\[dmapage\]' -AllMatches -EA SilentlyContinue | Measure-Object).Count
$tr = (Select-String "$vm\stderr_dma.txt" -Pattern 'TRUNCATED' -AllMatches -EA SilentlyContinue | Measure-Object).Count
Write-Output "dmapage lines: $dp   TRUNCATED: $tr"
