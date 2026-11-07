param([string]$Label, [string]$Out)
# The driver already running: bench with pictures of the card's two pages.
$root = 'C:\Users\lycet\RiderProjects\86Box-Inboard'
$W = 'C:\Users\lycet\tools\openwatcom'
$env:WATCOM = $W; $env:PATH = "$W\binnt64;$W\binnt;$env:PATH"; $env:INCLUDE = "$W\h;$W\h\nt"
Push-Location "$root\tools\m8d3dtest"
& wcl386 -q -bt=nt -l=nt_win -ox "-i=$W\h" "-i=$W\h\nt" "-i=$W\h\nt\directx" '-fe=M8D3DF.EXE' M8D3DF.C ddraw.lib 2>&1 | Select-String -Pattern 'Error' | ForEach-Object { $_.Line }
Pop-Location
$ctl = 'C:\Users\lycet\tools\v9x-remote-agent\host\v9xremote-0.12.0\scripts\v9xctl.ps1'
$h = @{ Host = '192.168.68.62'; ConnectTimeoutSeconds = 60 }
$exe = "$root\tools\m8d3dtest\M8D3DF.EXE"
$target = 'C:\M8D3DF.EXE'
& $ctl put -Source $exe -Destination $target @h 2>&1 | Out-Null
& $ctl exec -Application 'C:\M8IDLE.EXE' -Arguments '10 10 200' -TimeoutSeconds 260 @h 2>&1 | Out-Null
& $ctl exec -Application $target -Arguments "$Label 3 high grab" -TimeoutSeconds 600 @h 2>&1 | Select-String 'ExitCode|TimedOut' | ForEach-Object { $_.Line.Trim() }
New-Item -ItemType Directory -Force $Out | Out-Null
& $ctl get -Source 'C:\M8D3DF.GRB' -Destination "$Out\M8D3DF.GRB" @h 2>&1 | Out-Null
python -I "$root\tools\m8d3dtest\m8grab_png.py" "$Out\M8D3DF.GRB" $Out
