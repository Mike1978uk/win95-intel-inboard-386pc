# Restart the card the way a person does (Start, Shut Down, Restart): the agent's own reboot can end
# in a fatal exception here. Dot-source, then call Restart-Card. Keys sent by the agent can be lost
# while the Shut Down dialog is opening, leaving it waiting: so each attempt checks that the agent
# stopped answering, and if it did not within 60 s, closes the dialog and tries again.
#
#   . "$PSScriptRoot\card_restart.ps1"
#   $r = Restart-Card -Ctl $ctl -Target $Target -Name $Name    # .Ok, .Boot, .Before

function Restart-Card([string]$Ctl, [string]$Target, [string]$Name, [int]$Attempts = 3) {
  $h = @{ Host = $Target; ConnectTimeoutSeconds = 30 }
  $before = (& $Ctl info @h -Json | ConvertFrom-Json).BootCounter
  $down = $false
  for ($a = 1; $a -le $Attempts -and -not $down; $a++) {
    & $Ctl input -Sequence 'key ESCAPE; delay 500; key CTRL+ESC; delay 1500; key U' @h 2>&1 | Out-Null
    Start-Sleep -Seconds 25
    & $Ctl input -Sequence 'key R; delay 500; key ENTER' @h 2>&1 | Out-Null
    for ($t = 0; $t -lt 12; $t++) {
      Start-Sleep -Seconds 5
      $i = & $Ctl info -Host $Target -ConnectTimeoutSeconds 5 -Json 2>$null 6>$null
      if ($LASTEXITCODE -ne 0 -or ($i | ConvertFrom-Json).BootCounter -ne $before) { $down = $true; break }
    }
    if (-not $down) { "restart attempt ${a}: card still up after 60 s, retrying" }
  }
  $ok = $false
  $boot = '-'
  if ($down) {
    for ($t = 0; $t -lt 30 -and -not $ok; $t++) {
      Start-Sleep -Seconds 10
      $i = & $Ctl info -Host $Target -ConnectTimeoutSeconds 5 -Json 2>$null 6>$null
      if ($LASTEXITCODE -eq 0) { $i = $i | ConvertFrom-Json; $ok = $i.BootCounter -ne $before -and $i.DesktopReady; $boot = $i.BootCounter }
    }
  }
  Add-Content -Path (Join-Path $env:TEMP 'm8_card_restarts.log') -Value ('{0}  {1}  boot {2} -> {3}  {4}' -f (Get-Date -Format 'yyyy-MM-dd HH:mm'), $Name, $before, $(if ($ok) { $boot } else { '-' }), $(if ($ok) { 'ok' } elseif ($down) { 'did not come back' } else { 'never went down' }))
  [pscustomobject]@{ Ok = $ok; Boot = $boot; Before = $before; WentDown = $down }
}
