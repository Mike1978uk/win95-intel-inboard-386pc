<#
Switch the 5160 between the Windows boot and the DOS agent boot, unattended.

  tools\dosagent\bootmode.ps1 dos            # Windows -> DOSNET (packet driver + V9x DOS agent)
  tools\dosagent\bootmode.ps1 windows        # DOS agent -> Windows (menu entry LEAN unless -WindowsEntry)
  tools\dosagent\bootmode.ps1 status         # which agent answers, and CONFIG.SYS menudefault

Both agents listen on 9869 and both move files, so CONFIG.SYS's menudefault is edited through
whichever one is up: read, back up to C:\CONFIG.BMD, change one line, write, read back and
compare. Then reboot: the Windows agent's reboot verb, or the DOS agent's reboot probe
(v9xdos.py). Waits for the other agent to answer before returning.
#>
param(
    [Parameter(Mandatory = $true, Position = 0)][ValidateSet('dos', 'windows', 'status')][string]$Mode,
    [string]$EndpointHost = '192.168.68.62',
    [string]$WindowsEntry = 'LEAN',
    [int]$WaitSeconds = 600
)
$ErrorActionPreference = 'Stop'
$ctl = 'C:\Users\lycet\RiderProjects\V9x-Remote-Agent\scripts\v9xctl.ps1'
$probe = Join-Path $PSScriptRoot 'v9xdos.py'
$work = Join-Path $env:TEMP 'bootmode'
New-Item -ItemType Directory -Force $work | Out-Null

function Get-Agent {
    $r = & $ctl ping -EndpointHost $EndpointHost -Json 2>$null | ConvertFrom-Json
    if (-not $r -or -not $r.Success) { return $null }
    if ($r.AgentBuild -like 'dev-dos*') { 'dos' } else { 'windows' }
}

function Get-Config {
    $f = Join-Path $work 'CONFIG.SYS'
    $r = & $ctl get -EndpointHost $EndpointHost -Source 'C:\CONFIG.SYS' -Destination $f -Json | ConvertFrom-Json
    if (-not $r.Success) { throw 'could not read C:\CONFIG.SYS' }
    $f
}

$agent = Get-Agent
if (-not $agent) { throw "no agent answers at ${EndpointHost}:9869" }
$cfg = Get-Config
$latin = [Text.Encoding]::GetEncoding(28591)   # byte-preserving
$text = $latin.GetString([IO.File]::ReadAllBytes($cfg))
$m = [regex]::Matches($text, '(?im)^menudefault=([A-Z0-9]+)(,\d+)?')
if ($m.Count -ne 1) { throw "expected one menudefault line, found $($m.Count)" }
"agent: $agent   menudefault: $($m[0].Groups[1].Value)$($m[0].Groups[2].Value)"
if ($Mode -eq 'status') { return }

$want = if ($Mode -eq 'dos') { 'DOSNET' } else { $WindowsEntry }
if ($Mode -eq $agent) { "already in $Mode" }
if ($m[0].Groups[1].Value -ne $want) {
    $new = $text.Substring(0, $m[0].Index) + "menudefault=$want$($m[0].Groups[2].Value)" +
           $text.Substring($m[0].Index + $m[0].Length)
    $out = Join-Path $work 'CONFIG.NEW'
    [IO.File]::WriteAllBytes($out, $latin.GetBytes($new))
    $b = & $ctl put -EndpointHost $EndpointHost -Source $cfg -Destination 'C:\CONFIG.BMD' -Json | ConvertFrom-Json
    if (-not $b.Success) { throw 'backup to C:\CONFIG.BMD failed' }
    $p = & $ctl put -EndpointHost $EndpointHost -Source $out -Destination 'C:\CONFIG.SYS' -Json | ConvertFrom-Json
    if (-not $p.Success) { throw 'write of C:\CONFIG.SYS failed' }
    $back = Get-Config
    if ((Get-FileHash $back).Hash -ne (Get-FileHash $out).Hash) { throw 'C:\CONFIG.SYS read back different; backup is C:\CONFIG.BMD' }
    "menudefault -> $want (backup C:\CONFIG.BMD)"
}
if ($Mode -eq $agent) { return }

if ($agent -eq 'windows') {
    & $ctl reboot -EndpointHost $EndpointHost -WaitSeconds 30 -Json 2>$null | Out-Null
} else {
    python $probe --host $EndpointHost reboot | Out-Null
}
"rebooting into $Mode ..."
$t = [Diagnostics.Stopwatch]::StartNew()
Start-Sleep 20
while ($t.Elapsed.TotalSeconds -lt $WaitSeconds) {
    if ((Get-Agent) -eq $Mode) { "$Mode agent up after $([int]$t.Elapsed.TotalSeconds) s"; return }
    Start-Sleep 10
}
throw "no $Mode agent after $WaitSeconds s"
