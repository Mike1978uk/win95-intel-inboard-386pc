# One boot of the REAL card image on the Inboard machine profile.
#
#   pwsh -File tools/pdr_inboard_run.ps1 [-Seconds 900] [-Tag name]
#
# The other harness (pdr_loadtest.ps1) runs a Deskpro 386 because "whether a VxD
# loads is machine-independent". That turned out to be false: on 2026-09-01 the
# same claim-nothing build gave Init Success on the Deskpro bed and Init Failure
# on the 5160. So the machine is a variable, and this bed removes it - the real
# card's own install, the Inboard machine, the 1986 5160 BIOS.
#
# It does NOT deploy anything. Put the driver in with tools/fatcp.py first, so
# the binary under test is chosen deliberately and its md5 is on the record.
#
# MSDOS.SYS in the image is armed with BootMenu=1 / BootMenuDefault=2, which is
# what "F8 -> Logged" picks by hand on the real machine - so BOOTLOG.TXT is
# written without anyone having to time a keystroke.

param(
    [string]$VmPath  = "C:\Users\lycet\RiderProjects\86Box-Inboard\vm_xtide_inboard",
    [string]$Image   = "prextide.img",
    [string]$ExePath = "C:\Users\lycet\RiderProjects\86Box-Inboard\86box_upstream\build\src\86Box.exe",
    # This machine POSTs far more slowly than the Deskpro bed - the Mach8 option
    # ROM alone takes tens of seconds - and Windows 95 on top of it is slower
    # again. A short window here is a false negative, not a result.
    [int]$Seconds    = 900,
    # How long to keep tapping Enter through the DOS phase.
    [int]$EnterSeconds = 300,
    [string]$Tag     = "inboard",
    # Drive a real Windows shutdown at the end of the boot window, then wait.
    # This is the ONLY way to exercise the shutdown AEP broadcasts - IOS sends
    # AEP_PEND_UNCONFIG_DCB / AEP_UNCONFIG_DCB / AEP_SYSTEM_SHUTDOWN /
    # AEP_SYSTEM_CRIT_SHUTDOWN at System_Exit and nowhere else.
    #
    # The readback is BOOTLOG.TXT's own Terminate=/EndTerminate= pairs, which
    # Windows appends as it tears each stage down. A stage that starts and
    # never ends NAMES the hang, which a screenshot of a frozen desktop does
    # not. It reaches the disk because C: is still the Real Mode Mapper's.
    [switch]$Shutdown,
    [int]$ShutdownSeconds = 240,
    # The shutdown is triggered from INSIDE the guest, by SHUT.BAT in the
    # StartUp folder, so the harness sends no keys at all.
    #
    # Why: host-side injection into 86Box could never be shown to reach the
    # guest, and the boot completing was never evidence that it did - the last
    # prompt in CONFIG.SYS (TSLCD) has been REM'd out since 2026-09-01, so
    # nothing in the DOS phase has needed a keystroke for days. A harness that
    # "clears prompts" which no longer exist proves only that it ran
    # (technique 71). In-guest is unattended, repeatable, and never takes focus
    # from whoever is using the host.
    [switch]$InGuest
)

$repo = "C:\Users\lycet\RiderProjects\86Box-Inboard"
$img  = Join-Path $VmPath $Image
$out  = Join-Path $VmPath "bootlog_$Tag.txt"
$shot = Join-Path $VmPath "screen_$Tag.png"

foreach ($t in @($img, $ExePath)) {
    if (-not (Test-Path $t)) { Write-Output "MISSING: $t"; exit 1 }
}
Get-Process 86Box -EA SilentlyContinue | Stop-Process -Force
Start-Sleep 1

# Restore the config from the master copy EVERY run. 86Box rewrites .cfg on
# every load, and a run that fails early - a missing ROM, say - gets normalised
# down to a plain 8088 ibmxt with 256K and that is what it writes back. The next
# run then boots a stock XT trying to start Windows 95, shows a black screen,
# and looks like a completely different problem. Technique 43.
$master = Join-Path $VmPath "86box.cfg.master"
if (-not (Test-Path $master)) { Write-Output "MISSING master config: $master"; exit 1 }
Copy-Item $master (Join-Path $VmPath "86box.cfg") -Force

# EVERY RUN GETS A FRESH IMAGE. Not an option, not a switch - the default.
#
# 2026-09-05: two runs were abandoned by force-killing 86Box while Windows 95
# was running and writing. The next boot froze on the Windows splash with
# IO.SYS looping at 0070:0465 into the BIOS at F000:ACxx, and there was no
# backup of this bed's image to go back to. ScanDisk is not a way out - it
# misbehaves under emulation on this machine - so an image damaged by a kill
# is simply gone. Technique 23 already warned that repeated forced kills
# poison the next boot; what it did not say is that the recovery has to exist
# BEFORE you need it.
#
# The master is never booted. It is copied in here, the run dirties the copy,
# and the copy is disposable. That also makes killing a run free, which is the
# whole point - a harness you cannot safely abandon is a harness that pressures
# you into keeping bad runs alive.
$imgMaster = [IO.Path]::ChangeExtension($img, $null).TrimEnd('.') + "_master.img"
if (-not (Test-Path $imgMaster)) {
    Write-Output "MISSING master image: $imgMaster"
    Write-Output "  Create it ONCE from a known-good image and never boot it:"
    Write-Output "    Copy-Item <clean>.img $imgMaster"
    exit 1
}
Write-Output "Restoring $([IO.Path]::GetFileName($img)) from master (fresh image every run)"
Copy-Item $imgMaster $img -Force

# And say out loud what machine is about to boot, so a silent downgrade cannot
# be mistaken for a driver result.
$mach = (Select-String -Path $master -Pattern '^machine = (.+)$').Matches[0].Groups[1].Value
$mem  = (Select-String -Path $master -Pattern '^mem_size = (.+)$').Matches[0].Groups[1].Value
Write-Output "machine: $mach   mem: $mem KB"
if ($mach -ne "ibmxt_inboard386") { Write-Output "WRONG MACHINE - this bed exists to test the Inboard"; exit 1 }

# Say which driver is actually in the image. A run whose binary nobody recorded
# is a run nobody can cite (technique 70).
# Delete the extraction target FIRST. Without this, a run whose driver is absent
# (renamed out for a bisect) still finds the PREVIOUS run's copy and reports its
# md5 - so the log claims a driver was under test when none was. Technique 23
# again, and the third instance of it in this harness alone.
$uut = Join-Path $env:TEMP "pdr_under_test.pdr"
Remove-Item $uut -EA SilentlyContinue
python "$repo\tools\fatls.py" $img --get "C:\WINDOWS\SYSTEM\IOSUBSYS\PORT.PDR" $uut | Out-Null
if (Test-Path $uut) {
    $m = (Get-FileHash $uut -Algorithm MD5).Hash.ToLower()
    Write-Output "driver in the image: md5 $m"
} else {
    Write-Output "driver in the image: NONE - PORT.PDR is not in IOSUBSYS (bisect build)"
}
python "$repo\tools\fatcp.py" $img --rm "C:\BOOTLOG.TXT" 2>&1 | Out-Null
# And the shutdown log. A stale one left by a previous run reads exactly like a
# fresh success - this harness reported one run's stages as another's before the
# line below existed. Technique 23, in our own tooling.
python "$repo\tools\fatcp.py" $img --rm "C:\SHUTLOG.TXT" 2>&1 | Out-Null
python "$repo\tools\fatclean.py" $img

$p = Start-Process $ExePath -ArgumentList @("-P", $VmPath) -WorkingDirectory $VmPath -PassThru `
        -RedirectStandardOutput (Join-Path $VmPath "stdout_$Tag.txt") `
        -RedirectStandardError  (Join-Path $VmPath "stderr_$Tag.txt")
Write-Output "86Box pid $($p.Id), running $Seconds s..."

# The real card's CONFIG.SYS stops for keypresses this bed has nobody to give
# it - TSLCD prints "Driver aborting... Press [return]" when no CD answers, and
# the Startup Menu wants a choice. On the 5160 a human clears both. Tap Enter
# through the DOS phase; it also accepts the menu's own default, item 2/Logged.
Add-Type @"
using System; using System.Runtime.InteropServices;
public class PT2 {
  [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, IntPtr p);
  [DllImport("user32.dll")] public static extern bool AttachThreadInput(uint a, uint b, bool f);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern bool BringWindowToTop(IntPtr h);
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int c);
  [DllImport("user32.dll")] public static extern void keybd_event(byte vk, byte sc, uint f, IntPtr x);
  [DllImport("kernel32.dll")] public static extern uint GetCurrentThreadId();
  public static int skipped = 0;
  // Grab focus ONCE, at launch, when the VM has it anyway. Never again.
  public static void Focus(IntPtr h) {
    uint me = GetCurrentThreadId();
    uint other = GetWindowThreadProcessId(GetForegroundWindow(), IntPtr.Zero);
    AttachThreadInput(me, other, true);
    ShowWindow(h, 9); BringWindowToTop(h); SetForegroundWindow(h);
    AttachThreadInput(me, other, false);
    System.Threading.Thread.Sleep(150);
  }
  // keybd_event is GLOBAL input injection - it goes wherever focus is. So the
  // only safe key send is one made while the VM already HAS focus. The old
  // version grabbed focus back every 5 s for the whole DOS phase, which makes
  // the host unusable for whoever is sitting at it. If the user takes focus,
  // they have taken over; stop touching it and count what we skipped, so a run
  // that missed its prompts is recognisable as VOID rather than as a result.
  public static void Tap(IntPtr h, byte vk, byte sc) {
    if (GetForegroundWindow() != h) { skipped++; return; }
    keybd_event(vk, sc, 0, IntPtr.Zero); System.Threading.Thread.Sleep(60);
    keybd_event(vk, sc, 2, IntPtr.Zero);
  }
  // Ctrl+Esc opens the Start menu without a mouse. Held as a real chord -
  // the guest reads make/break codes, so Ctrl must still be down when Esc
  // arrives or Win95 just eats an Escape.
  // Alt+F4 on the desktop opens "Shut Down Windows" directly. Preferred over
  // Ctrl+Esc -> U: it needs no taskbar, no Start menu, and no DOS box.
  //
  // The batch-in-StartUp approach that this replaces cannot work by
  // construction - Windows 95 will not shut down while an MS-DOS box is open,
  // so a batch that triggers the shutdown is the thing blocking it, and
  // START/EXIT did not get out of the way either (the box hung instead).
  public static void AltF4(IntPtr h) {
    if (GetForegroundWindow() != h) { skipped++; return; }
    keybd_event(0x12, 0x38, 0, IntPtr.Zero); System.Threading.Thread.Sleep(80);
    keybd_event(0x73, 0x3E, 0, IntPtr.Zero); System.Threading.Thread.Sleep(80);
    keybd_event(0x73, 0x3E, 2, IntPtr.Zero); System.Threading.Thread.Sleep(80);
    keybd_event(0x12, 0x38, 2, IntPtr.Zero);
  }
}
"@
# One focus grab, here, while the freshly launched VM has it anyway. After
# this the harness never takes focus again - see PT2::Tap.
Start-Sleep 3
$p.Refresh()
if ($p.MainWindowHandle -ne 0) { [PT2]::Focus($p.MainWindowHandle) }
Write-Output "focus given to the VM once; the host is yours from here."
Write-Output "  (if you click away during the DOS phase, keys stop being sent -"
Write-Output "   the run then reports how many it skipped and is VOID, not a result)"

$deadline = (Get-Date).AddSeconds($Seconds)
$enteruntil = (Get-Date).AddSeconds($EnterSeconds)
while ((Get-Date) -lt $deadline -and -not $p.HasExited) {
    if ((Get-Date) -lt $enteruntil) {
        $p.Refresh()
        if ($p.MainWindowHandle -ne 0) { [PT2]::Tap($p.MainWindowHandle, 0x0D, 0x1C) }
    }
    Start-Sleep 5
}
if ([PT2]::skipped -gt 0) { Write-Output "NOTE: $([PT2]::skipped) keystrokes skipped - VM did not have focus." }

# Drive Start -> Shut Down -> OK with the keyboard only, then let Windows run
# the teardown to whatever end it reaches. Screenshots either side, because a
# frozen desktop and a finished shutdown look identical in a bootlog that was
# never flushed - and because if Ctrl+Esc never opened the menu, the whole run
# is VOID rather than a result about shutdown (technique 86's prompt trap).
if ($Shutdown -and -not $p.HasExited) {
    & pwsh -File "$repo\tools\shot.ps1" (Join-Path $VmPath "screen_${Tag}_desktop.png") 2>&1 | Out-Null
    $p.Refresh()
    if ($InGuest) {
        Write-Output "in-guest trigger: SHUT.BAT drives it, no keys sent from here"
    }
    else {
        # NO CHORD IS SENT FROM THE HOST. Alt+F4 and Ctrl+Esc are host-reserved:
        # they never reach the guest. Alt+F4 closes 86Box itself, and Ctrl+Esc
        # opens the HOST's Start menu - which is why the first shutdown attempt
        # of the evening "did nothing" in the guest. Plain keys (Enter, letters)
        # forward fine when the VM has focus; system chords do not.
        Write-Output "SHUT DOWN THE GUEST BY HAND: Start -> Shut Down -> OK."
        Write-Output "  Waiting $ShutdownSeconds s, then reading the teardown log."
    }
    $end = (Get-Date).AddSeconds($ShutdownSeconds)
    while ((Get-Date) -lt $end -and -not $p.HasExited) { Start-Sleep 5 }
    & pwsh -File "$repo\tools\shot.ps1" (Join-Path $VmPath "screen_${Tag}_after.png") 2>&1 | Out-Null
}

if (-not $p.HasExited) {
    & pwsh -File "$repo\tools\shot.ps1" $shot 2>&1 | Out-Null
    $p | Stop-Process -Force
}
Start-Sleep 2

# SHUT.BAT's own stage ladder. Written before Windows starts tearing down, so
# it survives even a shutdown that never completes - and it separates "the
# trigger never fired" from "the trigger fired and Windows hung", which the
# bootlog alone cannot do.
$slog = Join-Path $VmPath "shutlog_$Tag.txt"
Remove-Item $slog -EA SilentlyContinue
python "$repo\tools\fatls.py" $img --get "C:\SHUTLOG.TXT" $slog 2>&1 | Out-Null
if (Test-Path $slog) {
    Write-Output ""
    Write-Output "---- SHUT.BAT stages ----"
    Get-Content $slog | ForEach-Object { $_ }
} elseif ($InGuest) {
    Write-Output ""
    Write-Output "---- SHUT.BAT stages ----"
    Write-Output "NO SHUTLOG - the batch never ran. Check it is in the StartUp folder"
    Write-Output "  and that Explorer got far enough to launch it. Run is VOID."
}

python "$repo\tools\fatls.py" $img --get "C:\BOOTLOG.TXT" $out
if (-not (Test-Path $out)) {
    Write-Output "NO BOOTLOG - Windows never wrote one."
    Write-Output "  Read $shot before concluding anything. On this machine the"
    Write-Output "  usual causes are POST stopping for a prompt, or simply not"
    Write-Output "  having had long enough - it is much slower than the Deskpro bed."
    exit 1
}

Write-Output ""
Write-Output "---- port.pdr ----"
Select-String -Path $out -Pattern "port\.pdr|hsflop|Init Success|Init Failure|LoadFailed" |
    ForEach-Object { $_.Line }

# Pair up Terminate=/EndTerminate= and report the ones that never ended. On a
# clean shutdown every stage pairs. On a hang the LAST unpaired stage is where
# Windows stopped, which is the whole reason for driving the shutdown at all.
if ($Shutdown) {
    Write-Output ""
    Write-Output "---- shutdown ----"
    $started = @(); $ended = @()
    foreach ($l in (Get-Content $out)) {
        if ($l -match 'EndTerminate\s*=\s*(.+)$')  { $ended   += $Matches[1].Trim(); continue }
        if ($l -match 'Terminate\s*=\s*(.+)$')     { $started += $Matches[1].Trim() }
    }
    if ($started.Count -eq 0) {
        Write-Output "NO Terminate LINES - Windows never began a shutdown."
        Write-Output "  This run says NOTHING about the shutdown path. Check"
        Write-Output "  screen_${Tag}_startmenu.png: if the Start menu is not open,"
        Write-Output "  the keystrokes never landed and the run is VOID."
    } else {
        Write-Output "stages started: $($started.Count)   ended: $($ended.Count)"
        $open = $started | Where-Object { $_ -notin $ended }
        if ($open.Count -eq 0) { Write-Output "CLEAN - every stage paired." }
        else { Write-Output "HUNG IN: $($open -join ', ')" }
    }
}
