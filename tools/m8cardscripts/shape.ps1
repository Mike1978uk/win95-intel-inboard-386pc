param([string]$Label, [string]$Show = 'us/tri|waits|done|centres')
$root = 'C:\Users\lycet\RiderProjects\86Box-Inboard'
$W = 'C:\Users\lycet\tools\openwatcom'
$env:WATCOM = $W
$env:PATH = "$W\binnt64;$W\binnt;$env:PATH"
$env:INCLUDE = "$W\h;$W\h\nt"
Push-Location "$root\tools\m8probe"
& wcl386 -q -bt=nt -l=nt_win -ox "-i=$W\h" "-i=$W\h\nt" "-i=$W\h\nt\directx" '-fe=M8SHAPE.EXE' M8SHAPE.C ddraw.lib 2>&1 | Select-String -Pattern 'Error|Warning' | ForEach-Object { $_.Line }
Pop-Location
$ctl = 'C:\Users\lycet\tools\v9x-remote-agent\host\v9xremote-0.12.0\scripts\v9xctl.ps1'
$h = @{ Host = '192.168.68.62'; ConnectTimeoutSeconds = 60 }
$c = "$root\docs\captures\2026-11-07_shape"
$exe = "$root\tools\m8probe\M8SHAPE.EXE"
$target = 'C:\M8SHAPE.EXE'
& $ctl put -Source $exe -Destination $target @h 2>&1 | Out-Null
& $ctl exec -Application 'C:\M8IDLE.EXE' -Arguments '10 10 120' -TimeoutSeconds 180 @h 2>&1 | Out-Null
& $ctl exec -Application $target -Arguments $Label -TimeoutSeconds 300 @h 2>&1 | Select-String 'ExitCode|TimedOut' | ForEach-Object { $_.Line }
& $ctl get -Source 'C:\M8SHAPE.TXT' -Destination "$c\M8SHAPE.TXT" @h 2>&1 | Out-Null
$s = Get-Content "$c\M8SHAPE.TXT"
$k = ($s | Select-String "^== M8SHAPE $Label" | Select-Object -Last 1).LineNumber
$s[($k - 1)..($s.Count - 1)] | Select-String -Pattern $Show | ForEach-Object { $_.Line }
