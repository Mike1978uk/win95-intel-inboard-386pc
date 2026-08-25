# Boot the transplanted Win95 image with the DMA page-register trace on and capture
# whether floppy DMA (channel 2) is being truncated by the XT's 4-bit page latch.
param([int]$To = 300)
$vm  = "C:\Users\lycet\RiderProjects\86Box-Inboard\vm_fdd"
$exe = "C:\Users\lycet\RiderProjects\86Box-Inboard\86box_full\build\phase1_mingw\src\86Box.exe"
$env:PATH = "C:\msys64\mingw64\bin;" + $env:PATH
$env:INBOARD_DMA_TRACE = "1"
Get-Process 86Box -EA SilentlyContinue | Stop-Process -Force; Start-Sleep 1
$p = Start-Process $exe -ArgumentList @("-P",$vm) -WorkingDirectory $vm -PassThru `
       -RedirectStandardError "$vm\stderr_dma.txt" -RedirectStandardOutput "$vm\stdout_dma.txt"
$sw = [Diagnostics.Stopwatch]::StartNew()
while ($sw.Elapsed.TotalSeconds -lt $To -and -not $p.HasExited) { Start-Sleep 5 }
Get-Process 86Box -EA SilentlyContinue | Stop-Process -Force
Write-Output ("dmapage lines: " + (Select-String "$vm\stderr_dma.txt" -Pattern '\[dmapage\]' -AllMatches | Measure-Object).Count)
