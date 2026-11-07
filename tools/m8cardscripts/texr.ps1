param([string]$Label)
# M8TEXR on the card: one run after an idle wait, log to the silicon capture folder.
$root = 'C:\Users\lycet\RiderProjects\86Box-Inboard'
$ctl = 'C:\Users\lycet\tools\v9x-remote-agent\host\v9xremote-0.12.0\scripts\v9xctl.ps1'
$h = @{ Host = '192.168.68.62'; ConnectTimeoutSeconds = 60 }
$d = "$root\docs\captures\2026-11-07_silicon"
$W = 'C:\Users\lycet\tools\openwatcom'
$env:WATCOM = $W; $env:PATH = "$W\binnt64;$W\binnt;$env:PATH"; $env:INCLUDE = "$W\h;$W\h\nt"
Push-Location "$root\tools\m8probe"
& wcl386 -q -bt=nt -l=nt_win -ox "-i=$W\h" "-i=$W\h\nt" "-i=$W\h\nt\directx" '-fe=M8TEXR.EXE' M8TEXR.C ddraw.lib 2>&1 | Out-String
"build exit $LASTEXITCODE"
Pop-Location
if ($LASTEXITCODE -ne 0) { exit 1 }
New-Item -ItemType Directory -Force $d | Out-Null
& $ctl put -Source "$root\tools\m8probe\M8TEXR.EXE" -Destination 'C:\M8TEXR.EXE' @h 2>&1 | Out-Null
"put exit $LASTEXITCODE"
$null = & $ctl exec -Application 'C:\M8IDLE.EXE' -Arguments '10 10 120' -TimeoutSeconds 180 @h 2>&1
"idle exit $LASTEXITCODE"
$null = & $ctl exec -Application 'C:\M8TEXR.EXE' -Arguments $Label -TimeoutSeconds 120 @h 2>&1
"texr exit $LASTEXITCODE"
& $ctl get -Source 'C:\M8TEXR.TXT' -Destination "$d\M8TEXR.TXT" @h 2>&1 | Out-Null
Get-Content "$d\M8TEXR.TXT" | Out-String
